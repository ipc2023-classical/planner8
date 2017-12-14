#include "cost_partitioning_generator_greedy.h"

#include "abstraction.h"
#include "utils.h"

#include "../option_parser.h"
#include "../plugin.h"
#include "../task_proxy.h"

#include "../utils/collections.h"
#include "../utils/countdown_timer.h"
#include "../utils/logging.h"
#include "../utils/rng.h"
#include "../utils/rng_options.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <unordered_set>

using namespace std;

namespace cost_saturation {
CostPartitioningGeneratorGreedy::CostPartitioningGeneratorGreedy(const Options &opts)
    : CostPartitioningGenerator(),
      reverse_order(opts.get<bool>("reverse_order")),
      scoring_function(static_cast<ScoringFunction>(opts.get_enum("scoring_function"))),
      use_negative_costs(opts.get<bool>("use_negative_costs")),
      use_exp(opts.get<bool>("use_exp")),
      dynamic(opts.get<bool>("dynamic")),
      rng(utils::parse_rng_from_options(opts)),
      num_returned_orders(0) {
}

static int compute_stolen_costs(int wanted_by_abs, int surplus_cost) {
    if (wanted_by_abs >= 0) {
        if (surplus_cost >= 0) {
            return 0;
        } else {
            return wanted_by_abs;
        }
    } else {
        if (wanted_by_abs == -INF || surplus_cost == INF) {
            return 0;
        } else {
            // Both operands are finite.
            int surplus_for_rest = surplus_cost + wanted_by_abs;
            if (surplus_for_rest >= 0) {
                return 0;
            } else {
                return max(surplus_for_rest, wanted_by_abs);
            }
        }
    }
}

static int compute_stolen_costs_by_abstraction(
    const vector<int> &saturated_costs,
    const vector<int> &surplus_costs) {
    int num_operators = surplus_costs.size();
    int sum_stolen_costs = 0;
    for (int op_id = 0; op_id < num_operators; ++op_id) {
        int stolen_costs = compute_stolen_costs(
            saturated_costs[op_id], surplus_costs[op_id]);
        assert(stolen_costs != -INF);
        sum_stolen_costs += stolen_costs;
    }
    return sum_stolen_costs;
}

static int compute_surplus_costs(
    const vector<vector<int>> &saturated_costs_by_abstraction,
    int op_id,
    int remaining_costs) {
    int num_abstractions = saturated_costs_by_abstraction.size();
    int sum_wanted = 0;
    for (int abs = 0; abs < num_abstractions; ++abs) {
        int wanted = saturated_costs_by_abstraction[abs][op_id];
        if (wanted == -INF) {
            return INF;
        } else {
            sum_wanted += wanted;
        }
    }
    assert(sum_wanted != -INF);
    if (remaining_costs == INF) {
        return INF;
    }
    return remaining_costs - sum_wanted;
}

static vector<int> compute_all_surplus_costs(
    const vector<int> &costs,
    const vector<vector<int>> &saturated_costs_by_abstraction) {
    int num_operators = costs.size();
    vector<int> surplus_costs;
    surplus_costs.reserve(num_operators);
    for (int op_id = 0; op_id < num_operators; ++op_id) {
        surplus_costs.push_back(
            compute_surplus_costs(saturated_costs_by_abstraction, op_id, costs[op_id]));
    }
    return surplus_costs;
}

static int compute_used_costs(const vector<int> &saturated_costs, bool use_negative_costs) {
    int sum = 0;
    for (int cost : saturated_costs) {
        assert(cost != INF);
        if (cost != -INF && (use_negative_costs || cost > 0)) {
            sum += cost;
        }
    }
    return sum;
}

static long double compute_score(
    int h, int used_costs, ScoringFunction scoring_function, bool use_exp) {
    assert(h >= 0);
    assert(h != INF);
    assert(used_costs != INF);
    assert(used_costs != -INF);
    if (scoring_function == ScoringFunction::MAX_HEURISTIC) {
        return h;
    } else if (scoring_function == ScoringFunction::MIN_COSTS ||
               scoring_function == ScoringFunction::MIN_STOLEN_COSTS) {
        return -used_costs;
    } else if (scoring_function == ScoringFunction::MAX_HEURISTIC_PER_COSTS ||
               scoring_function == ScoringFunction::MAX_HEURISTIC_PER_STOLEN_COSTS) {
        if (use_exp) {
            return exp(static_cast<long double>(h - used_costs));
        } else {
            return static_cast<long double>(h) / max(1, used_costs);
        }
    } else {
        ABORT("Invalid scoring_function");
    }
}

long double CostPartitioningGeneratorGreedy::rate_abstraction(
    const vector<int> &local_state_ids, int abs_id) const {
    assert(utils::in_bounds(abs_id, local_state_ids));
    int local_state_id = local_state_ids[abs_id];
    assert(utils::in_bounds(abs_id, h_values_by_abstraction));
    assert(utils::in_bounds(local_state_id, h_values_by_abstraction[abs_id]));
    int h = h_values_by_abstraction[abs_id][local_state_id];
    assert(h >= 0);

    assert(utils::in_bounds(abs_id, stolen_costs_by_abstraction));
    int stolen_costs = stolen_costs_by_abstraction[abs_id];

    if (scoring_function == ScoringFunction::MIN_COSTS ||
        scoring_function == ScoringFunction::MAX_HEURISTIC_PER_COSTS) {
        assert(utils::in_bounds(abs_id, used_costs_by_abstraction));
        stolen_costs = used_costs_by_abstraction[abs_id];
    }
    if (false) {
        long double diff = static_cast<long double>(h - stolen_costs);
        cout << "abs " << abs_id << ", h: " << h << ", costs: " << stolen_costs
             << ", h/costs: " << static_cast<long double>(h) / stolen_costs
             << ", exp(h-costs): " << exp(diff) << ", log: " << log(exp(diff))
             << endl;
    }
    return compute_score(h, stolen_costs, scoring_function, use_exp);
}

Order CostPartitioningGeneratorGreedy::compute_static_greedy_order_for_sample(
    const vector<int> &local_state_ids, bool verbose) const {
    assert(local_state_ids.size() == h_values_by_abstraction.size());
    assert(local_state_ids.size() == used_costs_by_abstraction.size());
    int num_abstractions = local_state_ids.size();
    Order order = get_default_order(num_abstractions);
    vector<long double> scores;
    scores.reserve(num_abstractions);
    for (int abs = 0; abs < num_abstractions; ++abs) {
        scores.push_back(rate_abstraction(local_state_ids, abs));
    }
    sort(order.begin(), order.end(), [&](int abs1, int abs2) {
            return scores[abs1] > scores[abs2];
        });
    if (verbose) {
        cout << "Scores: " << scores << endl;
        unordered_set<long double> unique_scores(scores.begin(), scores.end());
        cout << "Unique scores: " << unique_scores.size() << endl;
        cout << "Order: " << order << endl;
    }
    return order;
}

Order CostPartitioningGeneratorGreedy::compute_greedy_dynamic_order_for_sample(
    const vector<unique_ptr<Abstraction>> &abstractions,
    const vector<int> &local_state_ids,
    vector<int> remaining_costs) const {
    assert(abstractions.size() == local_state_ids.size());
    Order order;

    vector<int> remaining_abstractions = get_default_order(abstractions.size());

    while (!remaining_abstractions.empty()) {
        int num_remaining = remaining_abstractions.size();
        vector<int> current_h_values;
        vector<vector<int>> current_saturated_costs;
        current_h_values.reserve(num_remaining);
        current_saturated_costs.reserve(num_remaining);

        vector<int> saturated_costs_for_best_abstraction;
        for (int abs_id : remaining_abstractions) {
            assert(utils::in_bounds(abs_id, local_state_ids));
            int local_state_id = local_state_ids[abs_id];
            Abstraction &abstraction = *abstractions[abs_id];
            auto pair = abstraction.compute_goal_distances_and_saturated_costs(remaining_costs);
            vector<int> &h_values = pair.first;
            vector<int> &saturated_costs = pair.second;
            assert(utils::in_bounds(local_state_id, h_values));
            int h = h_values[local_state_id];
            current_h_values.push_back(h);
            current_saturated_costs.push_back(move(saturated_costs));
        }

        vector<int> surplus_costs = compute_all_surplus_costs(
            remaining_costs, current_saturated_costs);

        long double highest_score = -numeric_limits<long double>::max();
        int best_rem_id = -1;
        for (int rem_id = 0; rem_id < num_remaining; ++rem_id) {
            int used_costs;
            if (scoring_function == ScoringFunction::MIN_COSTS ||
                scoring_function == ScoringFunction::MAX_HEURISTIC_PER_COSTS) {
                used_costs = compute_used_costs(
                    current_saturated_costs[rem_id], use_negative_costs);
            } else {
                used_costs = compute_stolen_costs_by_abstraction(
                    current_saturated_costs[rem_id], surplus_costs);
            }
            long double score = compute_score(
                current_h_values[rem_id], used_costs, scoring_function, use_exp);
            if (score > highest_score) {
                best_rem_id = rem_id;
                highest_score = score;
            }
        }
        order.push_back(remaining_abstractions[best_rem_id]);
        reduce_costs(remaining_costs, current_saturated_costs[best_rem_id]);
        utils::swap_and_pop_from_vector(remaining_abstractions, best_rem_id);
    }
    return order;
}

void CostPartitioningGeneratorGreedy::initialize(
    const TaskProxy &,
    const vector<unique_ptr<Abstraction>> &abstractions,
    const vector<int> &costs) {
    utils::Log() << "Initialize greedy order generator" << endl;
    random_order = get_default_order(abstractions.size());
    if (dynamic) {
        return;
    }

    utils::Timer timer;

    vector<vector<int>> saturated_costs_by_abstraction;
    for (const unique_ptr<Abstraction> &abstraction : abstractions) {
        auto pair = abstraction->compute_goal_distances_and_saturated_costs(costs);
        vector<int> &h_values = pair.first;
        vector<int> &saturated_costs = pair.second;
        h_values_by_abstraction.push_back(move(h_values));
        int used_costs = compute_used_costs(saturated_costs, use_negative_costs);
        used_costs_by_abstraction.push_back(used_costs);
        saturated_costs_by_abstraction.push_back(move(saturated_costs));
    }
    utils::Log() << "Time for computing h values and saturated costs: " << timer << endl;

    vector<int> surplus_costs = compute_all_surplus_costs(costs, saturated_costs_by_abstraction);
    utils::Log() << "Done computing surplus costs" << endl;

    utils::Log() << "Compute stolen costs" << endl;
    int num_abstractions = abstractions.size();
    for (int abs = 0; abs < num_abstractions; ++abs) {
        int sum_stolen_costs = compute_stolen_costs_by_abstraction(
            saturated_costs_by_abstraction[abs], surplus_costs);
        stolen_costs_by_abstraction.push_back(sum_stolen_costs);
    }
    utils::Log() << "Time for initializing greedy order generator: " << timer << endl;
}

Order CostPartitioningGeneratorGreedy::get_next_order(
    const TaskProxy &,
    const vector<unique_ptr<Abstraction>> &abstractions,
    const vector<int> &costs,
    const vector<int> &local_state_ids,
    bool verbose) {
    utils::Timer greedy_timer;
    vector<int> order;
    if (scoring_function == ScoringFunction::RANDOM) {
        rng->shuffle(random_order);
        order = random_order;
    } else if (dynamic) {
        order = compute_greedy_dynamic_order_for_sample(
            abstractions, local_state_ids, costs);
    } else {
        // We can call compute_sum_h with unpartitioned h values since we only need
        // a safe, but not necessarily admissible estimate.
        assert(compute_sum_h(local_state_ids, h_values_by_abstraction) != INF);
        order = compute_static_greedy_order_for_sample(local_state_ids, verbose);
    }

    if (reverse_order) {
        reverse(order.begin(), order.end());
    }

    if (verbose) {
        utils::Log() << "Time for computing greedy order: " << greedy_timer << endl;
    }

    assert(order.size() == abstractions.size());
    ++num_returned_orders;
    return order;
}


void add_scoring_function_to_parser(OptionParser &parser) {
    vector<string> scoring_functions;
    scoring_functions.push_back("RANDOM");
    scoring_functions.push_back("MAX_HEURISTIC");
    scoring_functions.push_back("MIN_COSTS");
    scoring_functions.push_back("MAX_HEURISTIC_PER_COSTS");
    scoring_functions.push_back("MIN_STOLEN_COSTS");
    scoring_functions.push_back("MAX_HEURISTIC_PER_STOLEN_COSTS");
    parser.add_enum_option(
        "scoring_function", scoring_functions, "scoring function", "MAX_HEURISTIC_PER_COSTS");
}

static shared_ptr<CostPartitioningGenerator> _parse_greedy(OptionParser &parser) {
    parser.add_option<bool>(
        "reverse_order",
        "invert initial order",
        "false");
    add_scoring_function_to_parser(parser);
    parser.add_option<bool>(
        "use_negative_costs",
        "account for negative costs when computing used costs",
        "false");
    parser.add_option<bool>(
        "use_exp",
        "use exp(h-costs) instead of h/max(1,costs) for computing scores",
        "false");
    parser.add_option<bool>(
        "dynamic",
        "recompute ratios in each step",
        "false");
    utils::add_rng_options(parser);
    Options opts = parser.parse();
    if (parser.dry_run())
        return nullptr;
    else
        return make_shared<CostPartitioningGeneratorGreedy>(opts);
}

static PluginShared<CostPartitioningGenerator> _plugin_greedy(
    "greedy", _parse_greedy);
}
