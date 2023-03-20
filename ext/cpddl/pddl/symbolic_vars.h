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

#ifndef __PDDL_SYMBOLIC_VARS_H__
#define __PDDL_SYMBOLIC_VARS_H__

#include <pddl/bdd.h>
#include <pddl/mgroup.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * A mutex group and the corresponding BDD variables
 */
struct pddl_symbolic_fact_group {
    int id;
    pddl_iset_t fact; /*!< Facts forming this mutex group */
    pddl_iset_t pre_var; /*!< Corresponding precondition BDD variables */
    pddl_iset_t eff_var; /*!< Corresponding effect BDD variables */
};
typedef struct pddl_symbolic_fact_group pddl_symbolic_fact_group_t;

/**
 * A strips fact and the corresponding BDD encoding
 */
struct pddl_symbolic_fact {
    int id; /*!< ID of the fact */
    int group_id; /*!< ID of the mutex group it belongs to */
    int val; /*!< Value of the fact within the mutex group, i.e,, value, if we
                  consider the mutex group to be variable */
    pddl_bdd_t *pre_bdd; /*!< Encoding of the fact in precondition BDD vars */
    pddl_bdd_t *eff_bdd; /*!< Encoding of the fact in effect BDD vars */
};
typedef struct pddl_symbolic_fact pddl_symbolic_fact_t;

struct pddl_symbolic_vars {
    int group_size; /*!< Number of mutex groups that encode the task */
    pddl_symbolic_fact_group_t *group; /*!< Array of mutex groups */
    int fact_size; /*!< Number of strips facts */
    pddl_symbolic_fact_t *fact; /*!< Facts and their corresponding BDD encoding */
    pddl_bdd_manager_t *mgr; /*!< Corresponding BDD manager */
    pddl_bdd_t *valid_states; /*!< BDD encoding all valid states */
    int bdd_var_size; /*!< Number of BDD variables (both pre and eff) */
    int *ordered_facts; /*!< Facts as they are ordered in BDDs */
};
typedef struct pddl_symbolic_vars pddl_symbolic_vars_t;

/**
 * Initialize mapping from facts to BDD variables without constructing any
 * BDDs.
 */
void pddlSymbolicVarsInit(pddl_symbolic_vars_t *vars,
                          int fact_size,
                          const pddl_mgroups_t *mgroups);

/**
 * Initialize BDD nodes -- needs to be run *after* *Init()
 */
void pddlSymbolicVarsInitBDD(pddl_bdd_manager_t *mgr,
                             pddl_symbolic_vars_t *vars);

/**
 * Free allocated memory.
 */
void pddlSymbolicVarsFree(pddl_symbolic_vars_t *vars);

/**
 * Construct BDD representing the given STRIPS state
 */
pddl_bdd_t *pddlSymbolicVarsCreateState(pddl_symbolic_vars_t *vars,
                                        const pddl_iset_t *state);

/**
 * Construct BDD partial state
 */
pddl_bdd_t *pddlSymbolicVarsCreatePartialState(pddl_symbolic_vars_t *vars,
                                               const pddl_iset_t *part_state);

/**
 * Construct bi-implication of the corresponding mutex group
 */
pddl_bdd_t *pddlSymbolicVarsCreateBiimp(pddl_symbolic_vars_t *vars,
                                        int group_id);

/**
 * Create a mutex formed by the given facts as BDD of precondition
 * variables.
 */
pddl_bdd_t *pddlSymbolicVarsCreateMutexPre(pddl_symbolic_vars_t *vars,
                                           int fact1, int fact2);

pddl_bdd_t *pddlSymbolicVarsCreateExactlyOneMGroupPre(pddl_symbolic_vars_t *vars,
                                                      const pddl_iset_t *mgroup);
pddl_bdd_t *pddlSymbolicVarsCreateExactlyOneMGroupEff(pddl_symbolic_vars_t *vars,
                                                      const pddl_iset_t *mgroup);

int pddlSymbolicVarsFactFromBDDCube(const pddl_symbolic_vars_t *vars,
                                    int group_id,
                                    const char *cube);

void pddlSymbolicVarsGroupsBDDVars(pddl_symbolic_vars_t *vars,
                                   const pddl_iset_t *groups,
                                   pddl_bdd_t ***var_pre,
                                   pddl_bdd_t ***var_eff,
                                   int *var_size);

_pddl_inline int pddlSymbolicVarsFactGroup(const pddl_symbolic_vars_t *vars,
                                           int fact)
{
    return vars->fact[fact].group_id;
}

_pddl_inline pddl_bdd_t *pddlSymbolicVarsFactPreBDD(pddl_symbolic_vars_t *vars,
                                                    int fact)
{
    return pddlBDDClone(vars->mgr, vars->fact[fact].pre_bdd);
}

_pddl_inline pddl_bdd_t *pddlSymbolicVarsFactPreBDDNeg(pddl_symbolic_vars_t *vars,
                                                       int fact)
{
    return pddlBDDNot(vars->mgr, vars->fact[fact].pre_bdd);
}

_pddl_inline pddl_bdd_t *pddlSymbolicVarsFactEffBDD(pddl_symbolic_vars_t *vars,
                                                    int fact)
{
    return pddlBDDClone(vars->mgr, vars->fact[fact].eff_bdd);
}

_pddl_inline pddl_bdd_t *pddlSymbolicVarsFactEffBDDNeg(pddl_symbolic_vars_t *vars,
                                                       int fact)
{
    return pddlBDDNot(vars->mgr, vars->fact[fact].eff_bdd);
}

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_SYMBOLIC_VARS_H__ */
