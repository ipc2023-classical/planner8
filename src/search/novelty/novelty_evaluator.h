#ifndef NOVELTY_NOVELTY_EVALUATOR_H
#define NOVELTY_NOVELTY_EVALUATOR_H

#include "novelty_table.h"

#include "../heuristic.h"

namespace novelty {
class NoveltyEvaluator : public Heuristic {
    const int width;
    const bool consider_only_novel_states;
    const bool debug;

    NoveltyTable novelty_table;

    void set_novelty(const State &state, int novelty);

protected:
    virtual int compute_heuristic(const State &ancestor_state) override;

public:
    NoveltyEvaluator(
        const options::Options &opts,
        const std::shared_ptr<FactIndexer> &fact_indexer = nullptr);

    virtual void get_path_dependent_evaluators(
        std::set<Evaluator *> &evals) override;
    virtual void notify_initial_state(const State &initial_state) override;
    virtual void notify_state_transition(
        const State &parent_state, OperatorID op_id, const State &state) override;
    virtual bool dead_ends_are_reliable() const override;
};
}

#endif
