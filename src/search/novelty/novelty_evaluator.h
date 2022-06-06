#ifndef NOVELTY_NOVELTY_EVALUATOR_H
#define NOVELTY_NOVELTY_EVALUATOR_H

#include "../heuristic.h"

namespace utils {
class RandomNumberGenerator;
}

namespace novelty {
/* The values vector stores entries contiguously in the following order:
    0=0: 1=0 1=1 1=2 2=0 2=1
    0=1: 1=0 1=1 1=2 2=0 2=1
    1=0: 2=0 2=1
    1=1: 2=0 2=1
    1=2: 2=0 2=1
*/
template<typename Value>
class FactPairMap {
    std::vector<int> fact_id_offsets;
    std::vector<int> pair_offsets;
    std::vector<Value> values;

    int get_fact_id(FactPair fact) const {
        return fact_id_offsets[fact.var] + fact.value;
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

public:
    explicit FactPairMap(TaskProxy task_proxy);

    Value get(FactPair fact1, FactPair fact2) const {
        return values[get_pair_id(fact1, fact2)];
    }
    void set(FactPair fact1, FactPair fact2, Value value) {
        values[get_pair_id(fact1, fact2)] = value;
    }

    void dump() const;
};

class NoveltyEvaluator : public Heuristic {
    const int width;
    const std::shared_ptr<utils::RandomNumberGenerator> rng;
    const bool debug;

    std::vector<int> fact_id_offsets;
    std::vector<int> seen_facts;
    std::unique_ptr<FactPairMap<bool>> seen_fact_pairs;

    int get_fact_id(FactPair fact) const {
        return fact_id_offsets[fact.var] + fact.value;
    }

    int compute_and_set_novelty(const State &state);

protected:
    virtual int compute_heuristic(const State &ancestor_state) override;

public:
    explicit NoveltyEvaluator(const options::Options &opts);
};
}

#endif
