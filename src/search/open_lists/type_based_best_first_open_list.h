#ifndef OPEN_LISTS_TYPE_BASED_BEST_FIRST_OPEN_LIST_H
#define OPEN_LISTS_TYPE_BASED_BEST_FIRST_OPEN_LIST_H

#include "../open_list_factory.h"
#include "../option_parser_util.h"

namespace type_based_best_first_open_list {
class TypeBasedBestFirstOpenListFactory : public OpenListFactory {
    Options options;
public:
    explicit TypeBasedBestFirstOpenListFactory(const Options &options);
    virtual ~TypeBasedBestFirstOpenListFactory() override = default;

    virtual std::unique_ptr<StateOpenList> create_state_open_list() override;
    virtual std::unique_ptr<EdgeOpenList> create_edge_open_list() override;
};
}

#endif
