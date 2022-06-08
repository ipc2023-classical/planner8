#include "novelty_open_list.h"

#include "../evaluator.h"
#include "../open_list.h"
#include "../option_parser.h"
#include "../plugin.h"

#include "../utils/collections.h"
#include "../utils/hash.h"
#include "../utils/logging.h"
#include "../utils/memory.h"
#include "../utils/rng.h"
#include "../utils/rng_options.h"

#include <cassert>
#include <memory>
#include <vector>

using namespace std;
using utils::ExitCode;

namespace novelty_open_list {
template<class Entry>
class NoveltyOpenList : public OpenList<Entry> {
    using Bucket = vector<Entry>;
    array<Bucket, 3> novelty_buckets;  // bucket order: novelty 1, 2, 3
    int size;
    shared_ptr<Evaluator> novelty_evaluator;
    shared_ptr<utils::RandomNumberGenerator> rng;

protected:
    virtual void do_insertion(
        EvaluationContext &eval_context, const Entry &entry) override;

public:
    explicit NoveltyOpenList(const Options &opts);

    virtual Entry remove_min() override;
    virtual bool empty() const override;
    virtual void clear() override;
    virtual void get_path_dependent_evaluators(set<Evaluator *> &evals) override;
    virtual bool is_dead_end(
        EvaluationContext &eval_context) const override;
    virtual bool is_reliable_dead_end(
        EvaluationContext &eval_context) const override;
};


template<class Entry>
NoveltyOpenList<Entry>::NoveltyOpenList(const Options &opts)
    : OpenList<Entry>(false),
      size(0),
      novelty_evaluator(opts.get<shared_ptr<Evaluator>>("evaluator")),
      rng(utils::parse_rng_from_options(opts)) {
}

template<class Entry>
void NoveltyOpenList<Entry>::do_insertion(
    EvaluationContext &eval_context, const Entry &entry) {
    int novelty = eval_context.get_evaluator_value(novelty_evaluator.get());
    int bucket_id = novelty - 1;
    assert(utils::in_bounds(bucket_id, novelty_buckets));
    novelty_buckets[bucket_id].push_back(entry);
    ++size;
}

template<class Entry>
Entry NoveltyOpenList<Entry>::remove_min() {
    assert(size > 0);
    // Choose bucket with lowest novelty value.
    for (auto &bucket : novelty_buckets) {
        if (!bucket.empty()) {
            // Choose random state from bucket.
            int pos = rng->random(bucket.size());
            --size;
            return utils::swap_and_pop_from_vector(bucket, pos);
        }
    }
    ABORT("All novelty buckets are empty.");
}

template<class Entry>
bool NoveltyOpenList<Entry>::empty() const {
    return size == 0;
}

template<class Entry>
void NoveltyOpenList<Entry>::clear() {
    for (auto &bucket : novelty_buckets) {
        bucket.clear();
    }
    size = 0;
}

template<class Entry>
void NoveltyOpenList<Entry>::get_path_dependent_evaluators(
    set<Evaluator *> &evals) {
    novelty_evaluator->get_path_dependent_evaluators(evals);
}

template<class Entry>
bool NoveltyOpenList<Entry>::is_dead_end(
    EvaluationContext &eval_context) const {
    return eval_context.is_evaluator_value_infinite(novelty_evaluator.get());
}

template<class Entry>
bool NoveltyOpenList<Entry>::is_reliable_dead_end(
    EvaluationContext &eval_context) const {
    return is_dead_end(eval_context) && novelty_evaluator->dead_ends_are_reliable();
}

NoveltyOpenListFactory::NoveltyOpenListFactory(const Options &options)
    : options(options) {
}

unique_ptr<StateOpenList>
NoveltyOpenListFactory::create_state_open_list() {
    return utils::make_unique_ptr<NoveltyOpenList<StateOpenListEntry>>(options);
}

unique_ptr<EdgeOpenList>
NoveltyOpenListFactory::create_edge_open_list() {
    return utils::make_unique_ptr<NoveltyOpenList<EdgeOpenListEntry>>(options);
}

static shared_ptr<OpenListFactory> _parse(OptionParser &parser) {
    parser.document_synopsis(
        "Novelty open list", "");
    parser.add_option<shared_ptr<Evaluator>>(
        "evaluator",
        "Novelty evaluator.");

    utils::add_rng_options(parser);

    Options opts = parser.parse();
    if (parser.dry_run())
        return nullptr;
    else
        return make_shared<NoveltyOpenListFactory>(opts);
}

static Plugin<OpenListFactory> _plugin("novelty_open_list", _parse);
}
