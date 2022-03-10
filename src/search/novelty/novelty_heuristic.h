#ifndef NOVELTY_NOVELTY_HEURISTIC_H
#define NOVELTY_NOVELTY_HEURISTIC_H

#include "../heuristic.h"

namespace novelty {
enum class AggregationFunction {
    MIN,
    MAX,
    SUM,
};

class NoveltyHeuristic : public Heuristic {
    const int width;
    const AggregationFunction aggregation_function;
    const bool debug;

    std::vector<int> fact_id_offsets;
    std::vector<int> fact_novelty;
    std::vector<std::vector<int>> fact_pair_novelty;

    int get_fact_id(FactPair fact) const {
        return fact_id_offsets[fact.var] + fact.value;
    }

    int get_and_increase_fact_pair_novelty(int fact_id1, int fact_id2);
    int compute_and_increase_novelty(const State &state);

protected:
    virtual int compute_heuristic(const State &ancestor_state) override;

public:
    explicit NoveltyHeuristic(const options::Options &opts);
};
}

#endif
