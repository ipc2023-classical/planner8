/***
 * cpddl
 * -------
 * Copyright (c)2019 Daniel Fiser <danfis@danfis.cz>,
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

#ifndef __PDDL_HFLOW_H__
#define __PDDL_HFLOW_H__

#include <pddl/lp.h>
#include <pddl/fdr.h>
#include <pddl/set.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// TODO: Use ranged constraints instead of two constraints
// https://www.ibm.com/support/knowledgecenter/en/SSSA5P_12.5.0/ilog.odms.cplex.help/refcallablelibrary/html/functions/CPXnewrows.html
// TODO: If .cause_incomplete_op is true, use only one constraint
// TODO: Add option to turn upper bounds off
// TODO: Reset only the constraints that are necessary instead of going
// over all constraints every time
struct pddl_hflow_fact {
    // Precomputed data that does not ever change:
    int var;                /*!< Variable ID */
    int is_mutex_with_goal; /*!< True if the fact is mutex with the goal */
    int is_goal;            /*!< True if the fact is in goal */
    int is_goal_var;        /*!< True if it is one value of a goal variable */
    int cause_incomplete_op;/*!< True if the fact causes incompleteness of
                                 an operator */
    int *constr_idx;        /*!< Index of the constraint variable */
    double *constr_coef;    /*!< Coeficient of the constraint variable */
    int constr_len;         /*!< Number of constraint variables */

    // These values must be changed according to the state for which is
    // computed heuristic value:
    int is_init;            /*!< True if the fact is in initial state */
    double lower_bound;     /*!< Lower bound on constraint */
    double upper_bound;     /*!< Upper bound on constraint */
};
typedef struct pddl_hflow_fact pddl_hflow_fact_t;

/**
 * Main structure for Flow heuristic
 */
struct pddl_hflow {
    const pddl_fdr_t *fdr;
    const pddl_fdr_vars_t *vars;
    int use_ilp; /*!< True if ILP instead of LP should be used */
    pddl_hflow_fact_t *facts; /*!< Array of fact related structures */
    pddl_lp_t *lp; /*!< (I)LP solver */
};
typedef struct pddl_hflow pddl_hflow_t;


/**
 * Initialize Flow heuristic.
 * If use_ilp is True Integer LP is used instead of LP.
 */
void pddlHFlowInit(pddl_hflow_t *h, const pddl_fdr_t *fdr, int use_ilp);

/**
 * Free allocated memory.
 */
void pddlHFlowFree(pddl_hflow_t *h);

/**
 * Computes heuristic value for the given state.
 * If ldms is non-NULL the landmarks are integrated.
 */
int pddlHFlow(pddl_hflow_t *h,
              const int *fdr_state,
              const pddl_set_iset_t *ldms);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_HFLOW_H__ */
