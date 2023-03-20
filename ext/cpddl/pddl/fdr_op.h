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

#ifndef __PDDL_FDR_OP_H__
#define __PDDL_FDR_OP_H__

#include <pddl/fdr_part_state.h>
#include <pddl/mgroup.h>
#include <pddl/mutex_pair.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct pddl_fdr_op_cond_eff {
    pddl_fdr_part_state_t pre;
    pddl_fdr_part_state_t eff;
};
typedef struct pddl_fdr_op_cond_eff pddl_fdr_op_cond_eff_t;

struct pddl_fdr_op {
    char *name;
    int cost;
    pddl_fdr_part_state_t pre;
    pddl_fdr_part_state_t eff;
    pddl_fdr_op_cond_eff_t *cond_eff;
    int cond_eff_size;
    int cond_eff_alloc;

    int id;
};
typedef struct pddl_fdr_op pddl_fdr_op_t;

struct pddl_fdr_ops {
    pddl_fdr_op_t **op;
    int op_size;
    int op_alloc;
};
typedef struct pddl_fdr_ops pddl_fdr_ops_t;


/**
 * Allocate empty FDR operator
 */
pddl_fdr_op_t *pddlFDROpNewEmpty(void);

/**
 * Creates a deep copy of op_in.
 */
pddl_fdr_op_t *pddlFDROpClone(const pddl_fdr_op_t *op_in);

/**
 * Free allocated memory
 */
void pddlFDROpDel(pddl_fdr_op_t *op);

/**
 * Adds empty conditional effect
 */
pddl_fdr_op_cond_eff_t *pddlFDROpAddEmptyCondEff(pddl_fdr_op_t *op);

/**
 * Remap facts
 */
void pddlFDROpRemapFacts(pddl_fdr_op_t *op, const pddl_fdr_vars_remap_t *rmp);

/**
 * Remap variable IDs.
 */
void pddlFDROpRemapVars(pddl_fdr_op_t *op, const int *remap);

/**
 * Apply the effects of the operator on the given state.
 */
void pddlFDROpApplyOnState(const pddl_fdr_op_t *op,
                           int num_vars,
                           const int *in_state,
                           int *out_state);
void pddlFDROpApplyOnStateInPlace(const pddl_fdr_op_t *op,
                                  int num_vars,
                                  int *out_state);

/**
 * Returns true if the operator is applicable in the state.
 */
int pddlFDROpIsApplicable(const pddl_fdr_op_t *op, const int *state);

/**
 * Initialize empty set of operators.
 */
void pddlFDROpsInit(pddl_fdr_ops_t *ops);

/**
 * Initialize ops as a deep copy of ops_in.
 */
void pddlFDROpsInitCopy(pddl_fdr_ops_t *ops, const pddl_fdr_ops_t *ops_in);

/**
 * Free allocated memory.
 */
void pddlFDROpsFree(pddl_fdr_ops_t *ops);

/**
 * Delete the specified set of operators.
 * This function may change IDs of operators.
 */
void pddlFDROpsDelSet(pddl_fdr_ops_t *ops, const pddl_iset_t *set);

/**
 * Remap facts.
 */
void pddlFDROpsRemapFacts(pddl_fdr_ops_t *ops, const pddl_fdr_vars_remap_t *r);

/**
 * Remap variable IDs.
 */
void pddlFDROpsRemapVars(pddl_fdr_ops_t *ops, const int *remap);

/**
 * Adds the given operator to the list of operators.
 */
void pddlFDROpsAddSteal(pddl_fdr_ops_t *ops, pddl_fdr_op_t *op);

/**
 * Sort operators by name (first).
 */
void pddlFDROpsSortByName(pddl_fdr_ops_t *ops);

/**
 * Sort operators first by effects, then preconditions, then name.
 */
void pddlFDROpsSortByEffPreName(pddl_fdr_ops_t *ops);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_FDR_OP_H__ */
