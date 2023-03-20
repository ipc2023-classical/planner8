/***
 * Copyright (c)2022 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

#ifndef __PDDL_LIFTED_SEARCH_H__
#define __PDDL_LIFTED_SEARCH_H__

#include <pddl/search.h>
#include <pddl/lifted_heur.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct pddl_lifted_search_stat {
    /** Number of calls to *Step() */
    size_t steps;
    /** Number of expansions of states */
    size_t expanded;
    /** Same as .expanded, but before the last f layer */
    size_t expanded_before_last_f_layer;
    /** Number of times heuristic function was evaluated */
    size_t evaluated;
    /** Number of different states created so far */
    size_t generated;
    /** Number of states currently in the open list */
    size_t open;
    /** Number of closed states so far */
    size_t closed;
    /** Number of times a state was re-opened. */
    size_t reopen;
    /** Number of states detected as dead-end states */
    size_t dead_end;
    /** Same as .dead_end, but before the last f layer */
    size_t dead_end_before_last_f_layer;
    int last_f_value;
};
typedef struct pddl_lifted_search_stat pddl_lifted_search_stat_t;

enum pddl_lifted_search_status {
    PDDL_LIFTED_SEARCH_CONT = 0,
    PDDL_LIFTED_SEARCH_UNSOLVABLE,
    PDDL_LIFTED_SEARCH_FOUND,
    PDDL_LIFTED_SEARCH_ABORT,
};
typedef enum pddl_lifted_search_status pddl_lifted_search_status_t;

struct pddl_lifted_plan {
    char **plan;
    int plan_len;
    int plan_cost;
    int plan_alloc;
};
typedef struct pddl_lifted_plan pddl_lifted_plan_t;

enum pddl_lifted_search_alg {
    PDDL_LIFTED_SEARCH_ASTAR,
    PDDL_LIFTED_SEARCH_GBFS,
    PDDL_LIFTED_SEARCH_LAZY,
};
typedef enum pddl_lifted_search_alg pddl_lifted_search_alg_t;

struct pddl_lifted_search_config {
    /** Input task */
    const pddl_t *pddl;
    /** Algorithm used for the lifted search */
    pddl_lifted_search_alg_t alg;
    /** Which backed to use for the successor generator */
    pddl_lifted_app_action_backend_t succ_gen;
    /** Lifted heuristic */
    pddl_lifted_heur_t *heur;
};
typedef struct pddl_lifted_search_config pddl_lifted_search_config_t;

#define PDDL_LIFTED_SEARCH_CONFIG_INIT \
    { \
        NULL, /* .pddl */ \
        PDDL_LIFTED_SEARCH_ASTAR, /* .alg */ \
        PDDL_LIFTED_APP_ACTION_DL, /* .succ_gen */ \
        NULL, /* .heur */ \
    }

typedef struct pddl_lifted_search pddl_lifted_search_t;

pddl_lifted_search_t *pddlLiftedSearchNew(const pddl_lifted_search_config_t *cfg,
                                          pddl_err_t *err);

void pddlLiftedSearchDel(pddl_lifted_search_t *s);
pddl_lifted_search_status_t pddlLiftedSearchInitStep(pddl_lifted_search_t *s);
pddl_lifted_search_status_t pddlLiftedSearchStep(pddl_lifted_search_t *s);

void pddlLiftedSearchStat(const pddl_lifted_search_t *s,
                          pddl_lifted_search_stat_t *stat);
void pddlLiftedSearchStatLog(const pddl_lifted_search_t *s, pddl_err_t *err);

const pddl_lifted_plan_t *pddlLiftedSearchPlan(const pddl_lifted_search_t *s);
void pddlLiftedSearchPlanPrint(const pddl_lifted_search_t *s, FILE *fout);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_LIFTED_SEARCH_H__ */
