#include "novelty_evaluator.h"

#include "../option_parser.h"
#include "../plugin.h"

#include "../task_utils/task_properties.h"
#include "../utils/logging.h"
#include "../utils/rng.h"
#include "../utils/rng_options.h"

using namespace std;

namespace novelty {
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
    if (!does_cache_estimates()) {
        cerr << "NoveltyEvaluator needs cache_estimates=true" << endl;
        utils::exit_with(utils::ExitCode::SEARCH_INPUT_ERROR);
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
        seen_fact_pairs.resize(num_facts);
        // We could store only the "triangle" of values instead of the full square.
        for (int fact_id = 0; fact_id < num_facts; ++fact_id) {
            seen_fact_pairs[fact_id].resize(num_facts, false);
        }
    }
}

void NoveltyEvaluator::get_path_dependent_evaluators(std::set<Evaluator *> &evals) {
    evals.insert(this);
}

void NoveltyEvaluator::notify_initial_state(const State &initial_state) {
    int novelty = compute_and_set_novelty(initial_state);
    assert(heuristic_cache[initial_state].dirty);
    heuristic_cache[initial_state].h = novelty;
    heuristic_cache[initial_state].dirty = false;
    if (debug) {
        dump_state_and_novelty(initial_state, novelty);
    }
}

void NoveltyEvaluator::notify_state_transition(
    const State &, OperatorID op_id, const State &state) {
    // Only compute novelty for new states.
    if (heuristic_cache[state].dirty) {
        int novelty = compute_and_set_novelty(op_id, state);
        heuristic_cache[state].h = novelty;
        heuristic_cache[state].dirty = false;
        if (debug) {
            dump_state_and_novelty(state, novelty);
        }
    }
}

bool NoveltyEvaluator::get_and_set_fact_pair_seen(int fact_id1, int fact_id2) {
    if (fact_id1 > fact_id2) {
        swap(fact_id1, fact_id2);
    }
    assert(fact_id1 < fact_id2);
    bool seen = seen_fact_pairs[fact_id1][fact_id2];
    seen_fact_pairs[fact_id1][fact_id2] = true;
    return seen;
}

int NoveltyEvaluator::compute_and_set_novelty(const State &state) {
    int num_vars = state.size();
    int novelty = 3;

    // Check for novelty 2.
    if (width == 2) {
        for (int var1 = 0; var1 < num_vars; ++var1) {
            FactPair fact1 = state[var1].get_pair();
            int fact_id1 = get_fact_id(fact1);
            for (int var2 = var1 + 1; var2 < num_vars; ++var2) {
                FactPair fact2 = state[var2].get_pair();
                int fact_id2 = get_fact_id(fact2);
                bool seen = get_and_set_fact_pair_seen(fact_id1, fact_id2);
                if (!seen) {
                    novelty = 2;
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

    // Order states with same width randomly.
    return novelty * 1000 + rng->random(1000);
}

int NoveltyEvaluator::compute_and_set_novelty(OperatorID op_id, const State &succ_state) {
    int novelty = 3;

    // Check for novelty 2.
    if (width == 2) {
        int num_vars = fact_id_offsets.size();
        for (EffectProxy effect : task_proxy.get_operators()[op_id].get_effects()) {
            FactPair fact1 = effect.get_fact().get_pair();
            int fact_id1 = get_fact_id(fact1);
            for (int var2 = 0; var2 < num_vars; ++var2) {
                if (fact1.var == var2) {
                    continue;
                }
                FactPair fact2 = succ_state[var2].get_pair();
                int fact_id2 = get_fact_id(fact2);
                bool seen = get_and_set_fact_pair_seen(fact_id1, fact_id2);
                if (!seen) {
                    novelty = 2;
                }
            }
        }
    }

    // Check for novelty 1.
    for (EffectProxy effect : task_proxy.get_operators()[op_id].get_effects()) {
        FactPair fact = effect.get_fact().get_pair();
        int fact_id = get_fact_id(fact);
        if (!seen_facts[fact_id]) {
            seen_facts[fact_id] = true;
            novelty = 1;
        }
    }

    // Order states with same width randomly.
    return novelty * 1000 + rng->random(1000);
}

void NoveltyEvaluator::dump_state_and_novelty(const State &state, int novelty) const {
    string sep;
    cout << state.get_id() << " [";
    for (FactProxy fact_proxy : state) {
        FactPair fact = fact_proxy.get_pair();
        cout << sep << fact.value;
        sep = ", ";
    }
    cout << "]: " << novelty << endl;
}

int NoveltyEvaluator::compute_heuristic(const State &) {
    ABORT("Novelty should already be stored in heuristic cache.");
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
