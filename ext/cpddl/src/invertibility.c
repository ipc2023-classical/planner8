/***
 * cpddl
 * -------
 * Copyright (c)2020 Daniel Fiser <danfis@danfis.cz>,
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

#include "pddl/disambiguation.h"
#include "pddl/strips_fact_cross_ref.h"
#include "pddl/invertibility.h"
#include "internal.h"

static int existOpWithPreSubsetPreAdd(const pddl_strips_t *strips,
                                      const pddl_iset_t *ops,
                                      int op_pre_add_id)
{
    const pddl_strips_op_t *op = strips->op.op[op_pre_add_id];
    PDDL_ISET(pre_add);
    pddlISetUnion2(&pre_add, &op->pre, &op->add_eff);

    int op_id;
    PDDL_ISET_FOR_EACH(ops, op_id){
        const pddl_strips_op_t *op = strips->op.op[op_id];
        if (pddlISetIsSubset(&op->pre, &pre_add)){
            pddlISetFree(&pre_add);
            return 1;
        }
    }

    pddlISetFree(&pre_add);
    return 0;
}

static int forEveryOpExistsOpWithPreSubsetPreAdd(const pddl_strips_t *strips,
                                                 const pddl_iset_t *for_every,
                                                 const pddl_iset_t *exist)
{
    int op_id;
    PDDL_ISET_FOR_EACH(for_every, op_id){
        if (!existOpWithPreSubsetPreAdd(strips, exist, op_id))
            return 0;
    }
    return 1;
}

static int factDelIsInvertible(const pddl_strips_t *strips,
                               const pddl_strips_fact_cross_ref_fact_t *cref)
{
    return forEveryOpExistsOpWithPreSubsetPreAdd(
                strips, &cref->op_del, &cref->op_add);
}

static int factAddIsInvertible(const pddl_strips_t *strips,
                               const pddl_strips_fact_cross_ref_fact_t *cref)
{
    return forEveryOpExistsOpWithPreSubsetPreAdd(
                strips, &cref->op_add, &cref->op_del);
}

int pddlRSEInvertibleFacts(const pddl_strips_t *strips,
                           const pddl_mgroups_t *fam_groups,
                           pddl_iset_t *invertible_facts,
                           pddl_err_t *err)
{
    pddl_strips_fact_cross_ref_t cref;
    pddlStripsFactCrossRefInit(&cref, strips, 0, 0, 0, 1, 1);

    int fact_size = strips->fact.fact_size;
    for (int fact = 0; fact < fact_size; ++fact){
        const pddl_strips_fact_cross_ref_fact_t *cref_fact = &cref.fact[fact];
        if (pddlISetSize(&cref_fact->op_del) > 0
                && pddlISetSize(&cref_fact->op_add) > 0
                && factDelIsInvertible(strips, cref_fact)
                && factAddIsInvertible(strips, cref_fact)){
            pddlISetAdd(invertible_facts, fact);
        }
    }

    pddlStripsFactCrossRefFree(&cref);
    return 0;
}
