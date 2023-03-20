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

#include "internal.h"
#include "pddl/iarr.h"
#include "pddl/dtg.h"

void pddlUnreachableInMGroupDTG(int init_fact,
                                const pddl_mgroup_t *mgroup,
                                const pddl_strips_ops_t *ops,
                                const pddl_strips_fact_cross_ref_t *cref,
                                pddl_iset_t *unreachable_facts,
                                pddl_iset_t *unreachable_ops)
{
    if (ops->op_size == 0
            || pddlISetSize(&mgroup->mgroup) == 0
            || !pddlISetIn(init_fact, &mgroup->mgroup)){
        return;
    }

    int size = pddlISetSize(&mgroup->mgroup);
    int max_fact = pddlISetGet(&mgroup->mgroup, size - 1);
    pddl_iset_t *reaches = CALLOC_ARR(pddl_iset_t, size);

    int *fact_to_id = CALLOC_ARR(int, max_fact + 1);
    for (int mi = 0; mi < size; ++mi){
        int fact = pddlISetGet(&mgroup->mgroup, mi);
        fact_to_id[fact] = mi;
    }

    PDDL_ISET(pre);
    for (int mi = 0; mi < size; ++mi){
        int to = pddlISetGet(&mgroup->mgroup, mi);
        int op_id;
        PDDL_ISET_FOR_EACH(&cref->fact[to].op_add, op_id){
            const pddl_strips_op_t *op = ops->op[op_id];
            pddlISetIntersect2(&pre, &op->pre, &mgroup->mgroup);
            if (pddlISetSize(&pre) == 0){
                for (int from = 0; from < size; ++from)
                    pddlISetAdd(reaches + from, mi);

            }else if (pddlISetSize(&pre) > 1){
                pddlISetAdd(unreachable_ops, op_id);

            }else if (pddlISetGet(&pre, 0) != to){ // pddlISetSize(&pre) == 1
                int from = pddlISetGet(&pre, 0);
                pddlISetAdd(reaches + fact_to_id[from], mi);
            }
        }
    }
    pddlISetFree(&pre);

    PDDL_IARR(queue);
    int *reached = CALLOC_ARR(int, size);
    pddlIArrAdd(&queue, fact_to_id[init_fact]);
    reached[fact_to_id[init_fact]] = 1;
    while (pddlIArrSize(&queue) > 0){
        int fid = queue.arr[--queue.size];
        int to;
        PDDL_ISET_FOR_EACH(reaches + fid, to){
            if (!reached[to]){
                pddlIArrAdd(&queue, to);
                reached[to] = 1;
            }
        }
    }

    for (int mi = 0; mi < size; ++mi){
        if (!reached[mi]){
            int fact = pddlISetGet(&mgroup->mgroup, mi);
            pddlISetAdd(unreachable_facts, fact);
            // Unreachable operators are those that have unreachable facts
            // in their preconditions
            pddlISetUnion(unreachable_ops, &cref->fact[fact].op_pre);
            pddlISetUnion(unreachable_ops, &cref->fact[fact].op_add);
        }
    }

    FREE(reached);
    pddlIArrFree(&queue);
    for (int mi = 0; mi < size; ++mi)
        pddlISetFree(reaches + mi);
    FREE(fact_to_id);
    FREE(reaches);
}

void pddlUnreachableInMGroupsDTGs(const pddl_strips_t *strips,
                                  const pddl_mgroups_t *mgroups,
                                  pddl_iset_t *unreachable_facts,
                                  pddl_iset_t *unreachable_ops,
                                  pddl_err_t *err)
{
    if (mgroups->mgroup_size == 0)
        return;

    PDDL_INFO(err, "Pruning using mutex group DTGs...");
    pddl_strips_fact_cross_ref_t cref;
    pddlStripsFactCrossRefInit(&cref, strips, 0, 0, 1, 1, 0);

    PDDL_ISET(init);
    for (int mgi = 0; mgi < mgroups->mgroup_size; ++mgi){
        const pddl_mgroup_t *mg = mgroups->mgroup + mgi;
        pddlISetIntersect2(&init, &strips->init, &mg->mgroup);
        if (pddlISetSize(&init) != 1)
            continue;

        int init_fact = pddlISetGet(&init, 0);
        pddlUnreachableInMGroupDTG(init_fact, mg, &strips->op, &cref,
                                   unreachable_facts, unreachable_ops);
    }
    pddlISetFree(&init);

    pddlStripsFactCrossRefFree(&cref);
    PDDL_INFO(err, "Pruning using mutex group DTGs DONE."
              " unreachable facts: %d, unreachable ops: %d",
              pddlISetSize(unreachable_facts),
              pddlISetSize(unreachable_ops));
}
