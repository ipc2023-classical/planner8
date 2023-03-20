/***
 * cpddl
 * -------
 * Copyright (c)2022 Daniel Fiser <danfis@danfis.cz>,
 * FAI Group at Saarland University, and
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

#ifndef __PDDL_SYMBOLIC_SPLIT_GOAL_H__
#define __PDDL_SYMBOLIC_SPLIT_GOAL_H__

#include <pddl/bdds.h>
#include <pddl/mg_strips.h>
#include <pddl/symbolic_vars.h>
#include <pddl/symbolic_constr.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct pddl_symbolic_states_split_by_pot_bdd {
    double h;
    int h_int;
    pddl_bdd_t *state;
};
typedef struct pddl_symbolic_states_split_by_pot_bdd
    pddl_symbolic_states_split_by_pot_bdd_t;

struct pddl_symbolic_states_split_by_pot {
    pddl_symbolic_states_split_by_pot_bdd_t *state;
    int state_size;
    int state_alloc;
};
typedef struct pddl_symbolic_states_split_by_pot
    pddl_symbolic_states_split_by_pot_t;

void pddlSymbolicStatesSplitByPotDel(pddl_symbolic_states_split_by_pot_t *s,
                                     pddl_bdd_manager_t *mgr);

pddl_symbolic_states_split_by_pot_t *
pddlSymbolicStatesSplitByPot(const pddl_iset_t *state,
                             const pddl_mgroups_t *mgroups,
                             const pddl_mutex_pairs_t *mutex,
                             const double *pot,
                             pddl_symbolic_vars_t *symb_vars,
                             pddl_bdd_manager_t *mgr,
                             pddl_err_t *err);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_SYMBOLIC_SPLIT_GOAL_H__ */
