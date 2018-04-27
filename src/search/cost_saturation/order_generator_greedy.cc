#include "order_generator_greedy.h"

#include "abstraction.h"
#include "utils.h"

#include "../option_parser.h"
#include "../plugin.h"
#include "../task_proxy.h"

#include "../utils/collections.h"
#include "../utils/logging.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <unordered_set>

using namespace std;

namespace cost_saturation {
OrderGeneratorGreedy::OrderGeneratorGreedy(const Options &opts)
    : scoring_function(
        static_cast<ScoringFunction>(opts.get_enum("scoring_function"))) {
}

double OrderGeneratorGreedy::rate_abstraction(
    const vector<int> &local_state_ids, int abs_id) const {
    assert(utils::in_bounds(abs_id, local_state_ids));
    int local_state_id = local_state_ids[abs_id];
    assert(utils::in_bounds(abs_id, h_values_by_abstraction));
    assert(utils::in_bounds(local_state_id, h_values_by_abstraction[abs_id]));
    int h = h_values_by_abstraction[abs_id][local_state_id];
    assert(h >= 0);

    assert(utils::in_bounds(abs_id, stolen_costs_by_abstraction));
    int stolen_costs = stolen_costs_by_abstraction[abs_id];

    return compute_score(h, stolen_costs, scoring_function);
}

Order OrderGeneratorGreedy::compute_static_greedy_order_for_sample(
    const vector<int> &local_state_ids, bool verbose) const {
    assert(local_state_ids.size() == h_values_by_abstraction.size());
    int num_abstractions = local_state_ids.size();
    Order order = get_default_order(num_abstractions);
    vector<double> scores;
    scores.reserve(num_abstractions);
    for (int abs = 0; abs < num_abstractions; ++abs) {
        scores.push_back(rate_abstraction(local_state_ids, abs));
    }
    sort(order.begin(), order.end(), [&](int abs1, int abs2) {
            return scores[abs1] > scores[abs2];
        });
    if (verbose) {
        cout << "Static greedy scores: " << scores << endl;
        unordered_set<double> unique_scores(scores.begin(), scores.end());
        cout << "Static greedy unique scores: " << unique_scores.size() << endl;
        cout << "Static greedy order: " << order << endl;
    }
    return order;
}

void OrderGeneratorGreedy::initialize(
    const TaskProxy &,
    const Abstractions &abstractions,
    const vector<int> &costs) {
    utils::Timer timer;
    utils::Log() << "Initialize greedy order generator" << endl;

    vector<vector<int>> saturated_costs_by_abstraction;
    for (const unique_ptr<Abstraction> &abstraction : abstractions) {
        auto pair = abstraction->compute_goal_distances_and_saturated_costs(costs);
        vector<int> &h_values = pair.first;
        vector<int> &saturated_costs = pair.second;
        h_values_by_abstraction.push_back(move(h_values));
        saturated_costs_by_abstraction.push_back(move(saturated_costs));
    }
    utils::Log() << "Time for computing h values and saturated costs: "
                 << timer << endl;

    vector<int> surplus_costs = compute_all_surplus_costs(
        costs, saturated_costs_by_abstraction);
    utils::Log() << "Done computing surplus costs" << endl;

    utils::Log() << "Compute stolen costs" << endl;
    int num_abstractions = abstractions.size();
    for (int abs = 0; abs < num_abstractions; ++abs) {
        int sum_stolen_costs = compute_costs_stolen_by_heuristic(
            saturated_costs_by_abstraction[abs], surplus_costs);
        stolen_costs_by_abstraction.push_back(sum_stolen_costs);
    }
    utils::Log() << "Time for initializing greedy order generator: " << timer << endl;
}

Order OrderGeneratorGreedy::get_next_order(
    const TaskProxy &,
    const Abstractions &abstractions,
    const vector<int> &,
    const vector<int> &local_state_ids,
    bool verbose) {
    utils::Timer greedy_timer;
    vector<int> order = compute_static_greedy_order_for_sample(local_state_ids, verbose);

    if (verbose) {
        utils::Log() << "Time for computing greedy order: " << greedy_timer << endl;
    }

    assert(order.size() == abstractions.size());
    return order;
}


static shared_ptr<OrderGenerator> _parse_greedy(OptionParser &parser) {
    add_scoring_function_to_parser(parser);
    Options opts = parser.parse();
    if (parser.dry_run())
        return nullptr;
    else
        return make_shared<OrderGeneratorGreedy>(opts);
}

static PluginShared<OrderGenerator> _plugin_greedy("greedy_orders", _parse_greedy);
}
