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

#ifndef __PDDL_LM_CUT_H__
#define __PDDL_LM_CUT_H__

#include <pddl/iset.h>
#include <pddl/iarr.h>
#include <pddl/fdr.h>
#include <pddl/pq.h>
#include <pddl/set.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct pddl_lm_cut_op {
    int op_id;
    pddl_iset_t eff; /*!< Facts in its effect */
    pddl_iset_t pre; /*!< Facts in its preconditions */
    int op_cost; /*!< Original operator's cost */

    int cost; /*!< Current cost of the operator */
    int unsat; /*!< Number of unsatisfied preconditions */
    int supp; /*!< Supporter fact (that maximizes h^max) */
    int supp_cost; /*!< Cost of the supported -- needed for hMaxInc() */
    int cut_candidate; /*!< True if the operator is candidate for a cut */
};
typedef struct pddl_lm_cut_op pddl_lm_cut_op_t;

struct pddl_lm_cut_fact {
    pddl_iset_t pre_op; /*!< Operators having this fact as its precond */
    pddl_iset_t eff_op; /*!< Operators having this fact as its effect */
    int value;
    pddl_pq_el_t heap; /*!< Connection to priority heap */
    int supp_cnt; /*!< Number of operators that have this fact as a supporter */
};
typedef struct pddl_lm_cut_fact pddl_lm_cut_fact_t;

struct pddl_lm_cut {
    pddl_lm_cut_fact_t *fact;
    int fact_size;
    int fact_goal;
    int fact_nopre;

    pddl_lm_cut_op_t *op;
    int op_size;
    int op_goal;

    pddl_iset_t state; /*!< Current state from which heur is computed */
    pddl_iset_t cut;   /*!< Current cut */

    /** Auxiliary structures to avoid re-allocation */
    int *fact_state;
    pddl_iarr_t queue;
    pddl_pq_t pq;
};
typedef struct pddl_lm_cut pddl_lm_cut_t;


/**
 * Initializes lm_cut structure so it can be repeatedly called for a
 * different states.
 * If op_unit_cost is True all operators will be considered to have unit
 * cost.
 * op_cost_plus is added to all operators' costs.
 */
void pddlLMCutInit(pddl_lm_cut_t *lmc,
                   const pddl_fdr_t *fdr,
                   int op_unit_cost,
                   int op_cost_plus);
void pddlLMCutInitStrips(pddl_lm_cut_t *lmc,
                         const pddl_strips_t *strips,
                         int op_unit_cost,
                         int op_cost_plus);

/**
 * Free allocated memory.
 */
void pddlLMCutFree(pddl_lm_cut_t *lmc);

/**
 * Compute LM-Cut heuristic on the given state and returns heuristic
 * estimate.
 * If ldms_in is non-NULL, these landmarks are applied as a first step.
 * If ldms is non-NULL it is filled with the found landmarks.
 */
int pddlLMCut(pddl_lm_cut_t *lmc,
              const int *fdr_state,
              const pddl_fdr_vars_t *vars,
              const pddl_set_iset_t *ldms_in,
              pddl_set_iset_t *ldms);
int pddlLMCutStrips(pddl_lm_cut_t *lmc,
                    const pddl_iset_t *state,
                    const pddl_set_iset_t *ldms_in,
                    pddl_set_iset_t *ldms);


#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_LM_CUT_H__ */
