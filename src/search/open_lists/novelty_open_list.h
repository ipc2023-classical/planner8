#ifndef OPEN_LISTS_NOVELTY_OPEN_LIST_H
#define OPEN_LISTS_NOVELTY_OPEN_LIST_H

#include "../open_list_factory.h"
#include "../option_parser_util.h"

namespace novelty_open_list {
class NoveltyOpenListFactory : public OpenListFactory {
    Options options;
public:
    explicit NoveltyOpenListFactory(const Options &options);

    virtual std::unique_ptr<StateOpenList> create_state_open_list() override;
    virtual std::unique_ptr<EdgeOpenList> create_edge_open_list() override;
};
}

#endif
