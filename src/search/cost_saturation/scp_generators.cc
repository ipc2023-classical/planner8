#include "scp_generators.h"

#include "abstraction.h"
#include "types.h"
#include "utils.h"

#include "../option_parser.h"
#include "../plugin.h"
#include "../task_proxy.h"

#include "../utils/rng.h"
#include "../utils/rng_options.h"
#include "../utils/countdown_timer.h"

#include <algorithm>
#include <cassert>

using namespace std;

namespace cost_saturation {
static vector<int> get_default_order(int n) {
    vector<int> indices(n);
    iota(indices.begin(), indices.end(), 0);
    return indices;
}

static void reduce_costs(
    vector<int> &remaining_costs, const vector<int> &saturated_costs) {
    assert(remaining_costs.size() == saturated_costs.size());
    for (size_t i = 0; i < remaining_costs.size(); ++i) {
        int &remaining = remaining_costs[i];
        const int &saturated = saturated_costs[i];
        assert(saturated <= remaining);
        /* Since we ignore transitions from states s with h(s)=INF, all
           saturated costs (h(s)-h(s')) are finite or -INF. */
        assert(saturated != INF);
        if (remaining == INF) {
            // INF - x = INF for finite values x.
        } else if (saturated == -INF) {
            remaining = INF;
        } else {
            remaining -= saturated;
        }
        assert(remaining >= 0);
    }
}

static void print_indexed_vector(const vector<int> &vec) {
    for (size_t i = 0; i < vec.size(); ++i) {
        cout << i << ":" << vec[i] << ", ";
    }
    cout << endl;
}

static vector<vector<int>> compute_saturated_cost_partitioning(
    const vector<unique_ptr<Abstraction>> &abstractions,
    const vector<int> &order,
    const vector<int> &operator_costs) {
    assert(abstractions.size() == order.size());
    const bool debug = false;
    vector<int> remaining_costs = operator_costs;
    vector<vector<int>> h_values_by_abstraction(abstractions.size());
    for (int pos : order) {
        Abstraction &abstraction = *abstractions[pos];
        auto pair = abstraction.compute_goal_distances_and_saturated_costs(
            remaining_costs);
        vector<int> &h_values = pair.first;
        vector<int> &saturated_costs = pair.second;
        if (debug) {
            cout << "h-values: ";
            print_indexed_vector(h_values);
            cout << "saturated costs: ";
            print_indexed_vector(saturated_costs);
        }
        h_values_by_abstraction[pos] = move(h_values);
        reduce_costs(remaining_costs, saturated_costs);
        if (debug) {
            cout << "remaining costs: ";
            print_indexed_vector(remaining_costs);
        }
    }
    return h_values_by_abstraction;
}


RandomSCPGenerator::RandomSCPGenerator(const Options &opts)
    : num_orders(opts.get<int>("orders")),
      rng(utils::parse_rng_from_options(opts)) {
}

CostPartitionings RandomSCPGenerator::get_cost_partitionings(
    const TaskProxy &,
    const vector<unique_ptr<Abstraction>> &abstractions,
    const vector<StateMap> &,
    const vector<int> &costs) const {
    CostPartitionings cost_partitionings;
    vector<int> order = get_default_order(abstractions.size());
    for (int i = 0; i < num_orders; ++i) {
        rng->shuffle(order);
        cost_partitionings.push_back(
            compute_saturated_cost_partitioning(abstractions, order, costs));
    }
    return cost_partitionings;
}


DiverseSCPGenerator::DiverseSCPGenerator(const Options &opts)
    : max_time(opts.get<double>("max_time")),
      rng(utils::parse_rng_from_options(opts)) {
}

CostPartitionings DiverseSCPGenerator::get_cost_partitionings(
    const TaskProxy &task_proxy,
    const vector<unique_ptr<Abstraction>> &abstractions,
    const vector<StateMap> &state_maps,
    const vector<int> &costs) const {
    vector<int> portfolio_h_values(num_samples, -1);
    vector<int> order = get_default_order(abstractions.size());
    CostPartitioning scp_for_default_order =
        compute_saturated_cost_partitioning(abstractions, order, costs);

    function<int (const State &state)> default_order_heuristic =
            [&](const State &state) {
        vector<int> local_state_ids = get_local_state_ids(state_maps, state);
        return compute_sum_h(local_state_ids, scp_for_default_order);
    };

    vector<State> samples = sample_states(
        task_proxy, default_order_heuristic, num_samples);

    vector<vector<int>> local_state_ids_by_sample;
    for (const State &sample : samples) {
        local_state_ids_by_sample.push_back(
            get_local_state_ids(state_maps, sample));
    }
    utils::release_vector_memory(samples);
    assert(static_cast<int>(local_state_ids_by_sample.size()) == num_samples);

    int evaluated_orders = 0;
    CostPartitionings cost_partitionings;
    utils::CountdownTimer diversification_timer(max_time);
    while (!diversification_timer.is_expired()) {
        rng->shuffle(order);
        CostPartitioning scp = compute_saturated_cost_partitioning(
            abstractions, order, costs);
        ++evaluated_orders;
        bool scp_improves_portfolio = false;
        for (int sample_id = 0; sample_id < num_samples; ++sample_id) {
            int scp_h_value = compute_sum_h(local_state_ids_by_sample[sample_id], scp);
            assert(utils::in_bounds(sample_id, portfolio_h_values));
            int &portfolio_h_value = portfolio_h_values[sample_id];
            if (scp_h_value > portfolio_h_value) {
                scp_improves_portfolio = true;
                portfolio_h_value = scp_h_value;
            }
        }
        if (scp_improves_portfolio) {
            cost_partitionings.push_back(move(scp));
        }
    }
    cout << "Total evaluated orders: " << evaluated_orders << endl;
    return cost_partitionings;
}


static shared_ptr<SCPGenerator> _parse_random(OptionParser &parser) {
    parser.add_option<int>(
        "orders",
        "number of abstraction orders",
        "1",
        Bounds("1", "infinity"));
    utils::add_rng_options(parser);
    Options opts = parser.parse();
    if (parser.dry_run())
        return nullptr;
    else
        return make_shared<RandomSCPGenerator>(opts);
}

static shared_ptr<SCPGenerator> _parse_diverse(OptionParser &parser) {
    parser.add_option<double>(
        "max_time",
        "maximum time for finding cost partitionings",
        "10",
        Bounds("0", "infinity"));
    utils::add_rng_options(parser);
    Options opts = parser.parse();
    if (parser.dry_run())
        return nullptr;
    else
        return make_shared<DiverseSCPGenerator>(opts);
}

static PluginShared<SCPGenerator> _plugin_random(
    "random", _parse_random);

static PluginShared<SCPGenerator> _plugin_diverse(
    "diverse", _parse_diverse);

static PluginTypePlugin<SCPGenerator> _type_plugin(
    "SCPGenerator",
    "Saturated cost partitioning generator.");
}
