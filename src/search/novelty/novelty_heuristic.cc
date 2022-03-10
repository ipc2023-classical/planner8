#include "novelty_heuristic.h"

#include "../option_parser.h"
#include "../plugin.h"

#include "../task_utils/task_properties.h"
#include "../utils/logging.h"

using namespace std;

namespace novelty {
NoveltyHeuristic::NoveltyHeuristic(const Options &opts)
    : Heuristic(opts),
      width(opts.get<int>("width")),
      aggregation_function(opts.get<AggregationFunction>("aggregate")),
      debug(opts.get<utils::Verbosity>("verbosity") == utils::Verbosity::DEBUG) {
    utils::g_log << "Initializing novelty heuristic..." << endl;

    fact_id_offsets.reserve(task_proxy.get_variables().size());
    int num_facts = 0;
    for (VariableProxy var : task_proxy.get_variables()) {
        fact_id_offsets.push_back(num_facts);
        int domain_size = var.get_domain_size();
        num_facts += domain_size;
    }
    utils::g_log << "Facts: " << num_facts << endl;

    fact_novelty.resize(num_facts, 0);
    if (width == 2) {
        fact_pair_novelty.resize(num_facts);
        // We could store only the "triangle" of values instead of the full square.
        for (int fact_id = 0; fact_id < num_facts; ++fact_id) {
            fact_pair_novelty[fact_id].resize(num_facts, 0);
        }
    }
}

int NoveltyHeuristic::get_and_increase_fact_pair_novelty(int fact_id1, int fact_id2) {
    if (fact_id1 > fact_id2) {
        swap(fact_id1, fact_id2);
    }
    assert(fact_id1 < fact_id2);
    int novelty = fact_pair_novelty[fact_id1][fact_id2]++;
    return novelty;
}

static u_int64_t combine_values(uint64_t val1, uint64_t val2, AggregationFunction func) {
    if (func == AggregationFunction::MIN) {
        return min(val1, val2);
    } else if (func == AggregationFunction::MAX) {
        return max(val1, val2);
    } else if (func == AggregationFunction::SUM) {
        return val1 + val2;
    } else {
        ABORT("unknown aggregation function");
    }
}

int NoveltyHeuristic::compute_and_increase_novelty(const State &state) {
    int num_vars = state.size();
    uint64_t novelty = (aggregation_function == AggregationFunction::MIN)
        ? numeric_limits<uint64_t>::max()
        : 0;
    for (FactProxy fact_proxy : state) {
        FactPair fact = fact_proxy.get_pair();
        int fact_id = get_fact_id(fact);
        novelty = combine_values(novelty, fact_novelty[fact_id]++, aggregation_function);
    }
    if (debug) {
        cout << "width-1 novelty: " << novelty << endl;
    }
    if (width == 2) {
        for (int var1 = 0; var1 < num_vars; ++var1) {
            FactPair fact1 = state[var1].get_pair();
            int fact_id1 = get_fact_id(fact1);
            for (int var2 = var1 + 1; var2 < num_vars; ++var2) {
                FactPair fact2 = state[var2].get_pair();
                int fact_id2 = get_fact_id(fact2);
                novelty = combine_values(
                    novelty,
                    get_and_increase_fact_pair_novelty(fact_id1, fact_id2),
                    aggregation_function);
                if (debug) {
                    cout << fact1 << " " << fact2 << endl;
                    cout << "width-2 novelty: " << novelty << endl;
                }
            }
        }
    }
    if (novelty > static_cast<uint64_t>(numeric_limits<int>::max())) {
        ABORT("novelty sum overflow");
    }
    return novelty;
}

int NoveltyHeuristic::compute_heuristic(const State &state) {
    // No need to convert ancestor state since we only allow cost transformations.
    int novelty = compute_and_increase_novelty(state);
    if (debug) {
        cout << novelty << endl;
    }
    return novelty;
}

static shared_ptr<Heuristic> _parse(OptionParser &parser) {
    parser.add_option<int>(
        "width", "maximum conjunction size", "2", Bounds("1", "2"));
    parser.add_enum_option<AggregationFunction>(
        "aggregate", {"min", "max", "sum"}, "aggregation function", "min");
    Heuristic::add_options_to_parser(parser);
    utils::add_log_options_to_parser(parser);
    Options opts = parser.parse();
    if (parser.dry_run())
        return nullptr;
    else
        return make_shared<NoveltyHeuristic>(opts);
}

static Plugin<Evaluator> _plugin("novelty", _parse);
}
