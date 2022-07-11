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

enum class HandleProgress {
    IGNORE,
    CLEAR,
    MOVE,
};

namespace novelty_open_list {
template<class Entry>
class NoveltyOpenList : public OpenList<Entry> {
    using Bucket = deque<Entry>;
    deque<Bucket> novelty_buckets;
    int size;
    shared_ptr<Evaluator> novelty_evaluator;
    const bool break_ties_randomly;
    const HandleProgress handle_progress;
    shared_ptr<utils::RandomNumberGenerator> rng;

protected:
    virtual void do_insertion(
        EvaluationContext &eval_context, const Entry &entry) override;

public:
    explicit NoveltyOpenList(const Options &opts);

    virtual Entry remove_min() override;
    virtual bool empty() const override;
    virtual void clear() override;
    virtual void boost_preferred() override;
    virtual void get_path_dependent_evaluators(set<Evaluator *> &evals) override;
    virtual bool is_dead_end(
        EvaluationContext &eval_context) const override;
    virtual bool is_reliable_dead_end(
        EvaluationContext &eval_context) const override;
};


template<class Entry>
NoveltyOpenList<Entry>::NoveltyOpenList(const Options &opts)
    : OpenList<Entry>(opts.get<bool>("pref_only")),
      novelty_buckets(2),
      size(0),
      novelty_evaluator(opts.get<shared_ptr<Evaluator>>("evaluator")),
      break_ties_randomly(opts.get<bool>("break_ties_randomly")),
      handle_progress(opts.get<HandleProgress>("handle_progress")),
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

template<typename Container>
typename Container::value_type swap_and_pop_from_container(
    Container &container, size_t pos) {
    assert(utils::in_bounds(pos, container));
    typename Container::value_type element = container[pos];
    std::swap(container[pos], container.back());
    container.pop_back();
    return element;
}

template<class Entry>
Entry NoveltyOpenList<Entry>::remove_min() {
    assert(size > 0);
    // Choose bucket with lowest novelty value.
    for (auto it = novelty_buckets.begin(); it != novelty_buckets.end(); ++it) {
        auto &bucket = *it;
        if (!bucket.empty()) {
            --size;
            Entry entry = bucket.front();
            if (break_ties_randomly) {
                // Choose random state from bucket.
                int pos = rng->random(bucket.size());
                entry = swap_and_pop_from_container(bucket, pos);
            } else {
                bucket.pop_front();
            }
            // Remove bucket if it's empty, but always keep the first two buckets.
            int bucket_index = it - novelty_buckets.begin();
            if (bucket.empty() && bucket_index >= 2) {
                novelty_buckets.erase(it);
            }
            assert(novelty_buckets.size() >= 2);
            return entry;
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
    novelty_buckets.resize(2);
    for (auto &bucket : novelty_buckets) {
        bucket.clear();
    }
    size = 0;
}

template<class Entry>
void NoveltyOpenList<Entry>::boost_preferred() {
    novelty_evaluator->notify_progress();
    if (handle_progress == HandleProgress::CLEAR) {
        clear();
    } else if (handle_progress == HandleProgress::MOVE) {
        // Insert new buckets for novelty 1 and 2 states.
        novelty_buckets.emplace_front();
        novelty_buckets.emplace_front();
    }
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
    parser.add_option<bool>(
        "break_ties_randomly",
        "If false, use FIFO for states with equal novelty.",
        "false");
    parser.add_enum_option<HandleProgress>(
        "handle_progress",
        {"ignore", "clear", "move"},
        "What to do when a heuristic makes progress.",
        "ignore");
    parser.add_option<bool>(
        "pref_only",
        "insert only nodes generated by preferred operators",
        "false");

    utils::add_rng_options(parser);

    Options opts = parser.parse();
    if (parser.dry_run())
        return nullptr;
    else
        return make_shared<NoveltyOpenListFactory>(opts);
}

static Plugin<OpenListFactory> _plugin("novelty_open_list", _parse);
}
