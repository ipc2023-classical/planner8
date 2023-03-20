/***
 * cpddl
 * -------
 * Copyright (c)2017 Daniel Fiser <danfis@danfis.cz>,
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

#ifndef __PDDL_STRIPS_OP_H__
#define __PDDL_STRIPS_OP_H__

#include <pddl/common.h>
#include <pddl/iset.h>
#include <pddl/fact.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct pddl_strips_op_cond_eff {
    pddl_iset_t pre;
    pddl_iset_t del_eff;
    pddl_iset_t add_eff;
};
typedef struct pddl_strips_op_cond_eff pddl_strips_op_cond_eff_t;

struct pddl_strips_op {
    char *name;
    int cost;
    pddl_iset_t pre;
    pddl_iset_t del_eff;
    pddl_iset_t add_eff;
    pddl_strips_op_cond_eff_t *cond_eff;
    int cond_eff_size;
    int cond_eff_alloc;

    int id;
    int pddl_action_id; /*!< ID of the corresponding PDDL action schema */
    pddl_obj_id_t *action_args;
    int action_args_size;
};
typedef struct pddl_strips_op pddl_strips_op_t;

/**
 * Initializes strips operator.
 */
void pddlStripsOpInit(pddl_strips_op_t *op);
pddl_strips_op_t *pddlStripsOpNew(void);

/**
 * Frees allocated memory
 */
void pddlStripsOpFree(pddl_strips_op_t *op);
void pddlStripsOpDel(pddl_strips_op_t *op);
void pddlStripsOpFreeAllCondEffs(pddl_strips_op_t *op);

pddl_strips_op_cond_eff_t *pddlStripsOpAddCondEff(pddl_strips_op_t *op,
                                                  const pddl_strips_op_t *f);

/**
 * Makes the operator well-formed.
 */
void pddlStripsOpNormalize(pddl_strips_op_t *op);

/**
 * Finalizes strips operator after all parts, except name, is filled.
 */
int pddlStripsOpFinalize(pddl_strips_op_t *op, char *name);

/**
 * Adds all del and delete effects from src to dst.
 */
void pddlStripsOpAddEffFromOp(pddl_strips_op_t *dst,
                              const pddl_strips_op_t *src);

/**
 * Copy the operator including conditional effects.
 */
void pddlStripsOpCopy(pddl_strips_op_t *dst, const pddl_strips_op_t *src);

/**
 * Copy the operator without conditional effects.
 */
void pddlStripsOpCopyWithoutCondEff(pddl_strips_op_t *dst,
                                    const pddl_strips_op_t *src);

/**
 * Copy dual variant of the operator (i.e., exchanged pre and del_eff)
 */
void pddlStripsOpCopyDual(pddl_strips_op_t *dst, const pddl_strips_op_t *src);

/**
 * Remaps fact IDs according to the provided map (old ID -> new ID).
 * It is assumed that the mapping is monotonically increasing.
 */
void pddlStripsOpRemapFacts(pddl_strips_op_t *op, const int *remap);

/**
 * Remove the fact from preconditions and effects including conditional
 * effects. If the precondition of a conditional effect is made empty the
 * effects are merged with the ordinary effects.
 * The operator is kept well-formed.
 */
void pddlStripsOpRemoveFact(pddl_strips_op_t *op, int fact_id);

/**
 * Remove all facts from the given set.
 */
void pddlStripsOpRemoveFacts(pddl_strips_op_t *op, const pddl_iset_t *facts);

/**
 * Apply operator on the given state.
 */
void pddlStripsOpApplyOnState(const pddl_strips_op_t *op,
                              const pddl_iset_t *in_state,
                              pddl_iset_t *out_state);

/**
 * Returns true if o enables p, i.e., if add(o) \cap pre(p) \neq \emptyset.
 */
_pddl_inline int pddlStripsOpEnable(const pddl_strips_op_t *o,
                                    const pddl_strips_op_t *p)
{
    return pddlISetIntersectionSizeAtLeast(&o->add_eff, &p->pre, 1);
}

/**
 * Returns true if o disables p, i.e., if del(o) \cap pre(p) \neq \emptyset.
 */
_pddl_inline int pddlStripsOpDisable(const pddl_strips_op_t *o,
                                     const pddl_strips_op_t *p)
{
    return pddlISetIntersectionSizeAtLeast(&o->del_eff, &p->pre, 1);
}

/**
 * Returns true if o is in conflict with p, i.e., if add(o) \cap del(p)
 * \neq \emptysetif or add(p) \cap del(o) \neq \emptyset.
 */
_pddl_inline int pddlStripsOpInConflict(const pddl_strips_op_t *o,
                                        const pddl_strips_op_t *p)

{
    return pddlISetIntersectionSizeAtLeast(&p->del_eff, &o->add_eff, 1)
            || pddlISetIntersectionSizeAtLeast(&o->del_eff, &p->add_eff, 1);
}

/**
 * Returns true if the operators interfere, i.e., if one disables the other
 * or they are in conflict.
 */
_pddl_inline int pddlStripsOpInterfere(const pddl_strips_op_t *o,
                                       const pddl_strips_op_t *p)
{
    return pddlStripsOpDisable(o, p)
            || pddlStripsOpDisable(p, o)
            || pddlStripsOpInConflict(p, o);
}


struct pddl_strips_ops {
    pddl_strips_op_t **op;
    int op_size;
    int op_alloc;
};
typedef struct pddl_strips_ops pddl_strips_ops_t;

#define PDDL_STRIPS_OPS_FOR_EACH(OPS, OP) \
    for (int __i = 0; __i < (OPS)->op_size && ((OP) = (OPS)->op[__i], 1); \
            ++__i) \
        if ((OP) != NULL)

void pddlStripsOpsInit(pddl_strips_ops_t *ops);
void pddlStripsOpsFree(pddl_strips_ops_t *ops);

/**
 * Copy all operators one by one from src to dst with pddlStripsOpsAdd().
 */
void pddlStripsOpsCopy(pddl_strips_ops_t *dst, const pddl_strips_ops_t *src);

/**
 * Adds a new operator if not already added, operator add has to be
 * finalized by pddlStripsOpFinalize().
 */
int pddlStripsOpsAdd(pddl_strips_ops_t *ops, const pddl_strips_op_t *add);

/**
 * Deletes all operators for which m[op_id] is set to true.
 */
void pddlStripsOpsDelOps(pddl_strips_ops_t *ops, const int *m);

/**
 * Deletes all operators from the set del_ops.
 */
void pddlStripsOpsDelOpsSet(pddl_strips_ops_t *ops, const pddl_iset_t *del_ops);

/**
 * Calls pddlStripsOpRemapFacts for each operator.
 */
void pddlStripsOpsRemapFacts(pddl_strips_ops_t *ops, const int *remap);

/**
 * Calls pddlStripsOpRemoveFacts for each operator.
 */
void pddlStripsOpsRemoveFacts(pddl_strips_ops_t *ops, const pddl_iset_t *facts);

/**
 * Removes duplicate operators, keeps the ones with the lowest cost.
 */
void pddlStripsOpsDeduplicate(pddl_strips_ops_t *ops);
void pddlStripsOpsDeduplicateSet(pddl_strips_ops_t *ops, pddl_iset_t *rm_op);

/**
 * Set costs of all operators to 1
 */
void pddlStripsOpsSetUnitCost(pddl_strips_ops_t *ops);

/**
 * Sort operators by name.
 */
void pddlStripsOpsSort(pddl_strips_ops_t *ops);

void pddlStripsOpPrintDebug(const pddl_strips_op_t *op,
                            const pddl_facts_t *fs,
                            FILE *fout);
void pddlStripsOpsPrintDebug(const pddl_strips_ops_t *ops,
                             const pddl_facts_t *fs,
                             FILE *fout);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_STRIPS_OP_H__ */
