/***
 * cpddl
 * -------
 * Copyright (c)2020 Daniel Fiser <danfis@danfis.cz>,
 * AI Center, Department of Computer Science,
 * Faculty of Electrical Engineering, Czech Technical University in Prague.
 * All rights reserved.
 *
 * This file is part of cpddl.
 *
 * Distributed under the OSI-approved BSD License (the "License");
 * see accompanying file LICENSE for details or see
 * <http://www.opensource.org/licenses/bsd-license.php>.
 *
 * This software is distributed WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the License for more information.
 */

#ifndef __PDDL_SYMBOLIC_TASK_H__
#define __PDDL_SYMBOLIC_TASK_H__

#include <pddl/iarr.h>
#include <pddl/strips.h>
#include <pddl/mgroup.h>
#include <pddl/mutex_pair.h>
#include <pddl/hpot.h>
#include <pddl/bdd.h>
#include <pddl/bdds.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define PDDL_SYMBOLIC_CONT 0
#define PDDL_SYMBOLIC_PLAN_FOUND 1
#define PDDL_SYMBOLIC_PLAN_NOT_EXIST 2
#define PDDL_SYMBOLIC_FAIL -1
#define PDDL_SYMBOLIC_ABORT_TIME_LIMIT -2

struct pddl_symbolic_search_config {
    int enabled;
    size_t trans_merge_max_nodes;
    float trans_merge_max_time;
    int use_constr;
    int use_op_constr;
    int use_pot_heur;
    int use_pot_heur_inconsistent;
    int use_pot_heur_sum_op_cost;
    int use_goal_splitting;
    float step_time_limit;
    pddl_hpot_config_t pot_heur_config;
};
typedef struct pddl_symbolic_search_config pddl_symbolic_search_config_t;

#define __PDDL_SYMBOLIC_SEARCH_CONFIG_INIT(Enabled) \
    { \
        (Enabled), /* .enabled */ \
        100000ul, /* .trans_merge_max_nodes */ \
        -1.f, /* .trans_merge_max_time */ \
        0, /* .use_constr */ \
        1, /* .use_op_constr */ \
        0, /* .use_pot_heur */ \
        0, /* .use_pot_heur_inconsistent */ \
        0, /* .use_pot_heur_sum_op_cost */ \
        1, /* .use_goal_splitting */ \
        0.f, /* .step_time_limit */ \
        PDDL_HPOT_CONFIG_INIT, \
    }

struct pddl_symbolic_task_config {
    int cache_size;
    size_t constr_max_nodes;
    float constr_max_time;
    float goal_constr_max_time;
    int fam_groups;
    int log_every_step;

    pddl_symbolic_search_config_t fw;
    pddl_symbolic_search_config_t bw;
};
typedef struct pddl_symbolic_task_config pddl_symbolic_task_config_t;

#define PDDL_SYMBOLIC_TASK_CONFIG_INIT \
    { \
        16000000, /* .cache_size */ \
        100000ul, /* .constr_max_nodes */ \
        -1.f, /* .constr_max_time */ \
        -1., /* .goal_constr_max_time */ \
        0, /* .fam_groups */ \
        0, /* .log_every_step */ \
        __PDDL_SYMBOLIC_SEARCH_CONFIG_INIT(1), /* .fw */ \
        __PDDL_SYMBOLIC_SEARCH_CONFIG_INIT(0), /* .bw */ \
    }

typedef struct pddl_symbolic_task pddl_symbolic_task_t;

pddl_symbolic_task_t *pddlSymbolicTaskNew(const pddl_fdr_t *fdr,
                                          const pddl_symbolic_task_config_t *c,
                                          pddl_err_t *err);

void pddlSymbolicTaskDel(pddl_symbolic_task_t *states);

/**
 * Returns true if applying constraints on the goal failed.
 */
int pddlSymbolicTaskGoalConstrFailed(const pddl_symbolic_task_t *task);

int pddlSymbolicTaskSearchFw(pddl_symbolic_task_t *ss,
                             pddl_iarr_t *plan,
                             pddl_err_t *err);
int pddlSymbolicTaskSearchBw(pddl_symbolic_task_t *ss,
                             pddl_iarr_t *plan,
                             pddl_err_t *err);
int pddlSymbolicTaskSearchFwBw(pddl_symbolic_task_t *ss,
                               pddl_iarr_t *plan,
                               pddl_err_t *err);
int pddlSymbolicTaskSearch(pddl_symbolic_task_t *ss,
                           pddl_iarr_t *plan,
                           pddl_err_t *err);

int pddlSymbolicTaskCheckApplyFw(pddl_symbolic_task_t *ss,
                                 const int *state,
                                 const int *res_state,
                                 int op_id);
int pddlSymbolicTaskCheckApplyBw(pddl_symbolic_task_t *ss,
                                 const int *state,
                                 const int *res_state,
                                 int op_id);
int pddlSymbolicTaskCheckPlan(pddl_symbolic_task_t *ss,
                              const pddl_iarr_t *op,
                              int plan_size);
#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_SYMBOLIC_TASK_H__ */
