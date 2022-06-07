#include "novelty_evaluator.h"

#include "../option_parser.h"
#include "../plugin.h"

#include "../task_utils/task_properties.h"
#include "../utils/logging.h"

using namespace std;

namespace novelty {
NoveltyEvaluator::NoveltyEvaluator(
    const Options &opts, const shared_ptr<FactIndexer> &fact_indexer)
    : Heuristic(opts),
      width(opts.get<int>("width")),
      consider_only_novel_states(opts.get<bool>("consider_only_novel_states")),
      debug(opts.get<utils::Verbosity>("verbosity") == utils::Verbosity::DEBUG),
      novelty_table(task_proxy, width, fact_indexer) {
    use_for_reporting_minima = false;
    use_for_boosting = false;
    if (debug) {
        utils::g_log << "Initializing novelty heuristic..." << endl;
    }
    if (!does_cache_estimates()) {
        cerr << "NoveltyEvaluator needs cache_estimates=true" << endl;
        utils::exit_with(utils::ExitCode::SEARCH_INPUT_ERROR);
    }
}

void NoveltyEvaluator::set_novelty(const State &state, int novelty) {
    assert(heuristic_cache[state].dirty);
    if (consider_only_novel_states && novelty == 3) {
        novelty = DEAD_END;
    }
    heuristic_cache[state].h = novelty;
    heuristic_cache[state].dirty = false;
}

void NoveltyEvaluator::get_path_dependent_evaluators(std::set<Evaluator *> &evals) {
    evals.insert(this);
}

void NoveltyEvaluator::notify_initial_state(const State &initial_state) {
    int novelty = novelty_table.compute_novelty_and_update_table(initial_state);
    set_novelty(initial_state, novelty);
}

void NoveltyEvaluator::notify_state_transition(
    const State &, OperatorID op_id, const State &state) {
    // Only compute novelty for new states.
    if (heuristic_cache[state].dirty) {
        OperatorProxy op = task_proxy.get_operators()[op_id];
        int novelty = novelty_table.compute_novelty_and_update_table(op, state);
        set_novelty(state, novelty);
    }
}

int NoveltyEvaluator::compute_heuristic(const State &) {
    ABORT("Novelty should already be stored in heuristic cache.");
}

static shared_ptr<Heuristic> _parse(OptionParser &parser) {
    parser.add_option<int>(
        "width", "maximum conjunction size", "2", Bounds("1", "2"));
    parser.add_option<bool>(
        "consider_only_novel_states", "assign infinity to non-novel states", "false");
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
