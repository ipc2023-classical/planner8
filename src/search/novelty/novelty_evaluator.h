#ifndef NOVELTY_NOVELTY_EVALUATOR_H
#define NOVELTY_NOVELTY_EVALUATOR_H

#include "../heuristic.h"

namespace utils {
class RandomNumberGenerator;
}

namespace novelty {
class NoveltyEvaluator : public Heuristic {
    const int width;
    const std::shared_ptr<utils::RandomNumberGenerator> rng;
    const bool debug;

    std::vector<int> fact_id_offsets;
    std::vector<int> seen_facts;
    std::vector<std::vector<bool>> seen_fact_pairs;

    int get_fact_id(FactPair fact) const {
        return fact_id_offsets[fact.var] + fact.value;
    }

    bool get_and_set_fact_pair_seen(int fact_id1, int fact_id2);
    int compute_and_set_novelty(const State &state);
    int compute_and_set_novelty(OperatorID op_id, const State &succ_state);

    void dump_state_and_novelty(const State &state, int novelty) const;

protected:
    virtual int compute_heuristic(const State &ancestor_state) override;

public:
    explicit NoveltyEvaluator(const options::Options &opts);

    virtual void get_path_dependent_evaluators(
        std::set<Evaluator *> &evals) override;
    virtual void notify_initial_state(const State &initial_state) override;
    virtual void notify_state_transition(
        const State &parent_state, OperatorID op_id, const State &state) override;
};
}

#endif
