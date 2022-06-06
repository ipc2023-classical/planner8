#include "novelty_evaluator.h"

#include "../option_parser.h"
#include "../plugin.h"

#include "../task_utils/task_properties.h"
#include "../utils/logging.h"
#include "../utils/rng.h"
#include "../utils/rng_options.h"

using namespace std;

namespace novelty {
template<typename Value>
FactPairMap<Value>::FactPairMap(TaskProxy task_proxy) {
    fact_id_offsets.reserve(task_proxy.get_variables().size());
    int num_facts = 0;
    for (VariableProxy var : task_proxy.get_variables()) {
        fact_id_offsets.push_back(num_facts);
        int domain_size = var.get_domain_size();
        num_facts += domain_size;
    }

    int num_vars = task_proxy.get_variables().size();
    int last_domain_size = task_proxy.get_variables()[num_vars - 1].get_domain_size();
    // We don't need offsets for facts of the last variable.
    int num_pair_offsets = num_facts - last_domain_size;
    pair_offsets.reserve(num_pair_offsets);
    int current_pair_offset = 0;
    int num_facts_in_higher_vars = num_facts;
    int num_pairs = 0;
    for (int var_id = 0; var_id < num_vars - 1; ++var_id) {  // Skip last var.
        int domain_size = task_proxy.get_variables()[var_id].get_domain_size();
        int var_last_fact_id = get_fact_id(FactPair(var_id, domain_size - 1));
        num_facts_in_higher_vars -= domain_size;
        num_pairs += (domain_size * num_facts_in_higher_vars);
        for (int value = 0; value < domain_size; ++value) {
            pair_offsets.push_back(current_pair_offset - (var_last_fact_id + 1));
            current_pair_offset += num_facts_in_higher_vars;
        }
    }
    values.resize(num_pairs);
    assert(static_cast<int>(pair_offsets.size()) == num_pair_offsets);
    assert(num_facts_in_higher_vars == last_domain_size);
#ifndef NDEBUG
    cout << "Pairs: " << num_pairs << endl;
    cout << "Pair offsets: " << pair_offsets << endl;
    int expected_id = 0;
    for (FactProxy fact_proxy1 : task_proxy.get_variables().get_facts()) {
        FactPair fact1 = fact_proxy1.get_pair();
        for (FactProxy fact_proxy2 : task_proxy.get_variables().get_facts()) {
            FactPair fact2 = fact_proxy2.get_pair();
            if (!(fact1 < fact2) || fact1.var == fact2.var) {
                continue;
            }
            cout << "Fact pair " << fact1 << " & " << fact2 << endl;
            cout << "Offset: " << pair_offsets[get_fact_id(fact1)] << endl;
            cout << "ID fact2: " << get_fact_id(fact2) << endl;
            int id = get_pair_id(fact1, fact2);
            cout << id << endl;
            assert(id == expected_id);
            ++expected_id;
        }
    }
#endif
}

NoveltyEvaluator::NoveltyEvaluator(const Options &opts)
    : Heuristic(opts),
      width(opts.get<int>("width")),
      rng(utils::parse_rng_from_options(opts)),
      debug(opts.get<utils::Verbosity>("verbosity") == utils::Verbosity::DEBUG) {
    use_for_reporting_minima = false;
    use_for_boosting = false;
    if (debug) {
        utils::g_log << "Initializing novelty heuristic..." << endl;
    }

    fact_id_offsets.reserve(task_proxy.get_variables().size());
    int num_facts = 0;
    for (VariableProxy var : task_proxy.get_variables()) {
        fact_id_offsets.push_back(num_facts);
        int domain_size = var.get_domain_size();
        num_facts += domain_size;
    }
    if (debug) {
        utils::g_log << "Facts: " << num_facts << endl;
    }

    seen_facts.resize(num_facts, false);
    if (width == 2) {
        seen_fact_pairs = utils::make_unique_ptr<FactPairMap<bool>>(task_proxy);
    }
}

int NoveltyEvaluator::compute_and_set_novelty(const State &state) {
    int num_vars = state.size();
    int novelty = 3;

    // Check for novelty 2.
    if (width == 2) {
        for (int var1 = 0; var1 < num_vars; ++var1) {
            FactPair fact1 = state[var1].get_pair();
            for (int var2 = var1 + 1; var2 < num_vars; ++var2) {
                FactPair fact2 = state[var2].get_pair();
                bool seen = seen_fact_pairs->get(fact1, fact2);
                if (!seen) {
                    novelty = 2;
                    seen_fact_pairs->set(fact1, fact2, true);
                }
            }
        }
    }

    // Check for novelty 1.
    for (FactProxy fact_proxy : state) {
        FactPair fact = fact_proxy.get_pair();
        int fact_id = get_fact_id(fact);
        if (!seen_facts[fact_id]) {
            seen_facts[fact_id] = true;
            novelty = 1;
        }
    }

    return novelty;
}

int NoveltyEvaluator::compute_heuristic(const State &state) {
    // No need to convert ancestor state since we only allow cost transformations.
    int novelty = compute_and_set_novelty(state);
    if (debug) {
        cout << novelty << endl;
    }
    return novelty;
}

static shared_ptr<Heuristic> _parse(OptionParser &parser) {
    parser.add_option<int>(
        "width", "maximum conjunction size", "2", Bounds("1", "2"));
    utils::add_rng_options(parser);
    Heuristic::add_options_to_parser(parser);
    utils::add_log_options_to_parser(parser);
    Options opts = parser.parse();
    if (parser.dry_run())
        return nullptr;
    else
        return make_shared<NoveltyEvaluator>(opts);
}

static Plugin<Evaluator> _plugin("novelty", _parse);
}
