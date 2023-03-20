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

#ifndef __PDDL_SYMBOLIC_TRANS_H__
#define __PDDL_SYMBOLIC_TRANS_H__

#include <pddl/cost.h>
#include <pddl/symbolic_vars.h>
#include <pddl/symbolic_constr.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct pddl_symbolic_trans {
    pddl_bdd_t *bdd; /*!< BDD representing the transition(s) */
    pddl_iset_t eff_groups; /*!< Groups appearing in the effect(s) */
    pddl_bdd_t **var_pre; /*!< List of pre variables */
    pddl_bdd_t **var_eff; /*!< List of eff variables */
    int var_size; /*!< Size of .var_pre and .var_eff */
    pddl_bdd_t *exist_pre; /*!< Cube from .var_pre */
    pddl_bdd_t *exist_eff; /*!< Cube from .var_eff */
};
typedef struct pddl_symbolic_trans pddl_symbolic_trans_t;

struct pddl_symbolic_trans_set {
    pddl_symbolic_vars_t *vars;
    pddl_symbolic_trans_t *trans;
    int trans_size;

    pddl_iset_t op; /*!< List of covered operators */
    pddl_cost_t cost; /*!< Cost of the covered operatros */
    pddl_cost_t heur_change; /*!< Change of heuristic value induced by transitions */
};
typedef struct pddl_symbolic_trans_set pddl_symbolic_trans_set_t;

struct pddl_symbolic_trans_sets {
    pddl_symbolic_vars_t *vars;
    pddl_symbolic_trans_set_t *trans;
    int trans_size;
    int trans_alloc;
};
typedef struct pddl_symbolic_trans_sets pddl_symbolic_trans_sets_t;

void pddlSymbolicTransSetsInit(pddl_symbolic_trans_sets_t *tr,
                               pddl_symbolic_vars_t *vars,
                               pddl_symbolic_constr_t *constr,
                               const pddl_strips_t *strips,
                               int use_op_constr,
                               int max_nodes,
                               float max_time,
                               pddl_cost_t *op_heur_change,
                               int sum_op_heur_change_to_cost,
                               pddl_err_t *err);
void pddlSymbolicTransSetsFree(pddl_symbolic_trans_sets_t *tr);

typedef pddl_bdd_t *(*pddl_symbolic_trans_set_image_fn)(
        pddl_symbolic_trans_set_t *trset,
        pddl_bdd_t *state,
        pddl_time_limit_t *time_limit);

pddl_bdd_t *pddlSymbolicTransSetImage(pddl_symbolic_trans_set_t *trset,
                                      pddl_bdd_t *state,
                                      pddl_time_limit_t *time_limit);
pddl_bdd_t *pddlSymbolicTransSetPreImage(pddl_symbolic_trans_set_t *trset,
                                         pddl_bdd_t *state,
                                         pddl_time_limit_t *time_limit);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_SYMBOLIC_TRANS_H__ */
