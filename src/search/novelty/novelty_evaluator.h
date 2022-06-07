#ifndef NOVELTY_NOVELTY_EVALUATOR_H
#define NOVELTY_NOVELTY_EVALUATOR_H

#include "../heuristic.h"

namespace utils {
class RandomNumberGenerator;
}

namespace novelty {
/* Assign indices in the following order:
    0=0: 1=0 1=1 1=2 2=0 2=1
    0=1: 1=0 1=1 1=2 2=0 2=1
    1=0: 2=0 2=1
    1=1: 2=0 2=1
    1=2: 2=0 2=1
*/
class FactIndexer {
    std::vector<int> fact_offsets;
    std::vector<int> pair_offsets;
    int num_facts;
    int num_pairs;

public:
    explicit FactIndexer(TaskProxy task_proxy);

    int get_fact_id(FactPair fact) const {
        return fact_offsets[fact.var] + fact.value;
    }

    int get_pair_id(FactPair fact1, FactPair fact2) const {
        assert(fact1.var != fact2.var);
        if (!(fact1 < fact2)) {
            std::swap(fact1, fact2);
        }
        assert(fact1 < fact2);
        assert(utils::in_bounds(get_fact_id(fact1), pair_offsets));
        return pair_offsets[get_fact_id(fact1)] + get_fact_id(fact2);
    }

    int get_num_facts() const {
        return num_facts;
    }

    int get_num_pairs() const {
        return num_pairs;
    }

    void dump() const;
};

class NoveltyEvaluator : public Heuristic {
    const int width;
    const bool consider_only_novel_states;
    const std::shared_ptr<utils::RandomNumberGenerator> rng;
    const bool debug;

    std::shared_ptr<FactIndexer> fact_indexer;
    std::vector<bool> seen_facts;
    std::vector<bool> seen_fact_pairs;

    int compute_and_set_novelty(const State &state);
    int compute_and_set_novelty(OperatorID op_id, const State &succ_state);

    void dump_state_and_novelty(const State &state, int novelty) const;

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
};
}

#endif
