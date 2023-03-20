/***
 * cpddl
 * -------
 * Copyright (c)2018 Daniel Fiser <danfis@danfis.cz>,
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

#ifndef __PDDL_PLAN_H__
#define __PDDL_PLAN_H__

#include <pddl/iarr.h>
#include <pddl/fdr_state_space.h>
#include <pddl/fdr_op.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct pddl_search;

struct pddl_plan {
    pddl_state_id_t *state;
    int state_size;
    int state_alloc;

    pddl_iarr_t op;
    int cost;
    int length;
};
typedef struct pddl_plan pddl_plan_t;


/**
 * Initializes empty structure.
 */
void pddlPlanInit(pddl_plan_t *plan);

/**
 * Copy the plan.
 */
void pddlPlanInitCopy(pddl_plan_t *dst, const pddl_plan_t *src);

/**
 * Frees allocated memory.
 */
void pddlPlanFree(pddl_plan_t *plan);

/**
 * Load the plan by backtracking from the goal state.
 */
void pddlPlanLoadBacktrack(pddl_plan_t *plan,
                           pddl_state_id_t goal_state_id,
                           const pddl_fdr_state_space_t *state_space);

/**
 * Prints out the found plan.
 */
void pddlPlanPrint(const pddl_plan_t *plan,
                   const pddl_fdr_ops_t *ops,
                   FILE *fout);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_PLAN_H__ */
