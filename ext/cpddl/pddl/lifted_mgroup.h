/***
 * cpddl
 * -------
 * Copyright (c)2019 Daniel Fiser <danfis@danfis.cz>,
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

#ifndef __PDDL_LIFTED_MGROUP_H__
#define __PDDL_LIFTED_MGROUP_H__

#include <pddl/common.h>
#include <pddl/fm.h>
#include <pddl/fm_arr.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct pddl_lifted_mgroup {
    pddl_params_t param;
    pddl_fm_arr_t cond;
    int is_exactly_one;
    int is_static;
};
typedef struct pddl_lifted_mgroup pddl_lifted_mgroup_t;

struct pddl_lifted_mgroups {
    pddl_lifted_mgroup_t *mgroup;
    int mgroup_size;
    int mgroup_alloc;
};
typedef struct pddl_lifted_mgroups pddl_lifted_mgroups_t;

#define PDDL_LIFTED_MGROUPS_INIT { 0 }

/**
 * Initialize empty liftd mutex group.
 */
void pddlLiftedMGroupInitEmpty(pddl_lifted_mgroup_t *dst);

/**
 * Initialize lifted mgroup as a copy of src.
 */
void pddlLiftedMGroupInitCopy(pddl_lifted_mgroup_t *dst,
                              const pddl_lifted_mgroup_t *src);

/**
 * Initialize mutex group as a candidate from the given predicate and with
 * (at most) one counted variable specified as the index of the predicate's
 * parameter. If the index is -1 then no variables are counted.
 */
void pddlLiftedMGroupInitCandFromPred(pddl_lifted_mgroup_t *mgroup,
                                      const pddl_pred_t *pred,
                                      int counted_var);

/**
 * Free allocated memory.
 */
void pddlLiftedMGroupFree(pddl_lifted_mgroup_t *mgroup);

/**
 * Returns true if m1 equals to m2.
 */
int pddlLiftedMGroupEq(const pddl_lifted_mgroup_t *m1,
                       const pddl_lifted_mgroup_t *m2);

/**
 * Sort mutex group's atoms and parameters.
 */
void pddlLiftedMGroupSort(pddl_lifted_mgroup_t *m);

/**
 * Returns the number of counted variables (parameters).
 */
int pddlLiftedMGroupNumCountedVars(const pddl_lifted_mgroup_t *mg);

/**
 * Returns the number of fixed variables (parameters).
 */
int pddlLiftedMGroupNumFixedVars(const pddl_lifted_mgroup_t *mg);

/**
 * Remove atoms that does not contain any counted variable.
 */
void pddlLiftedMGroupRemoveFixedAtoms(pddl_lifted_mgroup_t *mg);

/**
 * Extend mg with a copy of itself but with fresh counted variables.
 */
void pddlLiftedMGroupDoubleCounted(pddl_lifted_mgroup_t *mg);

/**
 * Prints a formatted lifted mutex group (or a candidate if there are some
 * counted variables).
 */
void pddlLiftedMGroupPrint(const pddl_t *pddl,
                           const pddl_lifted_mgroup_t *mgroup,
                           FILE *fout);
void pddlLiftedMGroupLog(const pddl_t *pddl,
                         const pddl_lifted_mgroup_t *mgroup,
                         pddl_err_t *err);

const char *pddlLiftedMGroupFmt(const pddl_t *pddl,
                                const pddl_lifted_mgroup_t *mgroup,
                                char *s,
                                size_t s_size);



/**
 * Initialize empty structure.
 */
void pddlLiftedMGroupsInit(pddl_lifted_mgroups_t *lm);

/**
 * Initialize dst as a copy of src.
 */
void pddlLiftedMGroupsInitCopy(pddl_lifted_mgroups_t *dst,
                               const pddl_lifted_mgroups_t *src);

/**
 * Free allocated memory.
 */
void pddlLiftedMGroupsFree(pddl_lifted_mgroups_t *lm);

/**
 * Adds a copy of the given lifted mgroup.
 */
void pddlLiftedMGroupsAdd(pddl_lifted_mgroups_t *lm,
                          const pddl_lifted_mgroup_t *lmg);

/**
 * Adds (partially) instantiated copy of the given lifted group.
 */
void pddlLiftedMGroupsAddInst(pddl_lifted_mgroups_t *lm,
                              const pddl_lifted_mgroup_t *lmg,
                              const pddl_obj_id_t *args);

/**
 * Sort mgroups according to size and predicates and removes duplicates.
 */
void pddlLiftedMGroupsSortAndUniq(pddl_lifted_mgroups_t *lm);

/**
 * Returns true if the two sets of lifted mutex groups are the same.
 * It is assumed both lmg1 and lmg2 are sorted!
 */
int pddlLiftedMGroupsEq(const pddl_lifted_mgroups_t *lmg1,
                        const pddl_lifted_mgroups_t *lmg2);

/**
 * Extend each mutex group with a copy of itself but with fresh counted
 * variables.
 */
void pddlLiftedMGroupsDoubleCounted(pddl_lifted_mgroups_t *mgs);

void pddlLiftedMGroupsPrint(const pddl_t *pddl,
                            const pddl_lifted_mgroups_t *lm,
                            FILE *fout);
#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_LIFTED_MGROUP_H__ */
