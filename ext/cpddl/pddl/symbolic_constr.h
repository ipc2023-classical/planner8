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

#ifndef __PDDL_SYMBOLIC_CONSTR_H__
#define __PDDL_SYMBOLIC_CONSTR_H__

#include <pddl/mutex_pair.h>
#include <pddl/mgroup.h>
#include <pddl/bdds.h>
#include <pddl/symbolic_vars.h>
#include <pddl/disambiguation.h>

struct pddl_symbolic_constr {
    pddl_symbolic_vars_t *vars;

    pddl_mgroups_t mgroup;
    pddl_iset_t *fact_mutex;
    pddl_iset_t *fact_mutex_fw;
    pddl_iset_t *fact_mutex_bw;
    pddl_disambiguate_t disambiguate;

    pddl_bdds_t fw_mutex;
    pddl_bdds_t fw_mgroup;
    pddl_bdds_t bw_mutex;
    pddl_bdds_t bw_mgroup;
    pddl_bdd_t **group_mutex;
    pddl_bdd_t **group_mgroup;
};
typedef struct pddl_symbolic_constr pddl_symbolic_constr_t;

typedef void (*pddl_symbolic_constr_apply_fn)(pddl_symbolic_constr_t *constr,
                                              pddl_bdd_t **bdd);

void pddlSymbolicConstrInit(pddl_symbolic_constr_t *constr,
                            pddl_symbolic_vars_t *vars,
                            const pddl_mutex_pairs_t *mutex,
                            const pddl_mgroups_t *mgroup,
                            int max_nodes,
                            float max_time,
                            pddl_err_t *err);

void pddlSymbolicConstrFree(pddl_symbolic_constr_t *constr);

void pddlSymbolicConstrApplyFw(pddl_symbolic_constr_t *constr, pddl_bdd_t **bdd);
void pddlSymbolicConstrApplyBw(pddl_symbolic_constr_t *constr, pddl_bdd_t **bdd);
int pddlSymbolicConstrApplyBwLimit(pddl_symbolic_constr_t *constr,
                                   pddl_bdd_t **bdd,
                                   float max_time);

#endif /* __PDDL_SYMBOLIC_CONSTR_H__ */
