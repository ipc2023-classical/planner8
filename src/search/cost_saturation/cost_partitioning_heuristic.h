#ifndef COST_SATURATION_COST_PARTITIONING_HEURISTIC_H
#define COST_SATURATION_COST_PARTITIONING_HEURISTIC_H

#include "cost_partitioned_heuristic.h"
#include "types.h"

#include "../heuristic.h"

#include <memory>
#include <vector>

namespace options {
class Options;
}

namespace cost_saturation {
class CostPartitioningCollectionGenerator;

class CostPartitioningHeuristic : public Heuristic {
protected:
    std::vector<CostPartitionedHeuristic> cp_heuristics;
    Abstractions abstractions;
    const bool debug;

    std::vector<int> abstractions_per_generator;

    // For statistics.
    mutable std::vector<int> num_best_order;

    int compute_max_h_with_statistics(const std::vector<int> &local_state_ids) const;
    virtual int compute_heuristic(const State &state);
    virtual int compute_heuristic(const GlobalState &global_state) override;

public:
    explicit CostPartitioningHeuristic(const options::Options &opts);
    virtual ~CostPartitioningHeuristic() override;

    virtual void print_statistics() const override;
};

extern void add_cost_partitioning_collection_options_to_parser(
    options::OptionParser &parser);

extern void prepare_parser_for_cost_partitioning_heuristic(
    options::OptionParser &parser);

extern CostPartitioningCollectionGenerator get_cp_collection_generator_from_options(
    const options::Options &opts);
}

#endif
