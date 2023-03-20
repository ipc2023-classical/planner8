/***
 * cpddl
 * -------
 * Copyright (c)2021 Daniel Fiser <danfis@danfis.cz>,
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

#include "pddl/iarr.h"
#include "pddl/mgroup_projection.h"
#include "internal.h"

void pddlMGroupProjectionInit(pddl_mgroup_projection_t *p,
                              const pddl_strips_t *strips,
                              const pddl_iset_t *mgroup,
                              const pddl_mutex_pairs_t *mutex,
                              const pddl_strips_fact_cross_ref_t *cref)
{
    ZEROIZE(p);
    p->num_states = pddlISetSize(mgroup) + 1;
    pddlISetUnion(&p->mgroup, mgroup);
    p->tr = CALLOC_ARR(pddl_iset_t, p->num_states * p->num_states);

    PDDL_ISET(ops_add);
    PDDL_ISET(ops_del);
    for (int to = 0; to < p->mgroup.size; ++to){
        int fact_to = pddlISetGet(&p->mgroup, to);
        pddlISetEmpty(&ops_add);
        pddlISetUnion(&ops_add, &cref->fact[fact_to].op_add);

        // Keeps track of operators that delete fact_to but do not add any
        // other fact from this mutex group
        pddlISetEmpty(&ops_del);
        pddlISetUnion(&ops_del, &cref->fact[fact_to].op_del);

        // Transitions from -> to, where from is in the precondition and to
        // is in the add effect
        for (int from = 0; from < p->mgroup.size; ++from){
            if (from == to)
                continue;
            int fact_from = pddlISetGet(&p->mgroup, from);
            pddl_iset_t *tr = p->tr + from * p->num_states + to;
            pddlISetIntersect2(tr, &cref->fact[fact_from].op_pre, &ops_add);
            pddlISetMinus(&ops_add, &cref->fact[fact_from].op_pre);

            pddlISetMinus(&ops_del, &cref->fact[fact_from].op_add);
        }

        // Now ops_add contains operators whose precondition has empty
        // intersection with mgroup, but adding fact_to.

        // Transitions \emptyset -> to
        pddlISetUnion(p->tr + (p->num_states - 1) * p->num_states + to, &ops_add);

        // Transitions from -> to if from is not in the precondition and it
        // is also not a mutex with the precondition
        int op_id;
        PDDL_ISET_FOR_EACH(&ops_add, op_id){
            const pddl_strips_op_t *op = strips->op.op[op_id];
            for (int from = 0; from < p->mgroup.size; ++from){
                if (from == to)
                    continue;
                int fact_from = pddlISetGet(&p->mgroup, from);
                if (!pddlMutexPairsIsMutexFactSet(mutex, fact_from, &op->pre))
                    pddlISetAdd(p->tr + from * p->num_states + to, op_id);
            }
        }

        // Transitions to -> \emptyset
        pddlISetUnion(p->tr + to * p->num_states + (p->num_states - 1), &ops_del);
    }
    pddlISetFree(&ops_add);
    pddlISetFree(&ops_del);

    pddlMGroupProjectionPruneUnreachableFromInit(p, strips);
    pddlMGroupProjectionPruneUnreachableFromGoal(p, strips, mutex);
}

void pddlMGroupProjectionInitCopy(pddl_mgroup_projection_t *p,
                                  const pddl_mgroup_projection_t *src)
{
    ZEROIZE(p);
    p->num_states = src->num_states;
    pddlISetUnion(&p->mgroup, &src->mgroup);
    p->tr = CALLOC_ARR(pddl_iset_t, p->num_states * p->num_states);
    for (int i = 0; i < p->num_states; ++i){
        for (int j = 0; j < p->num_states; ++j){
            pddlISetUnion(p->tr + i * p->num_states + j,
                          src->tr + i * p->num_states + j);
        }
    }
}

void pddlMGroupProjectionFree(pddl_mgroup_projection_t *p)
{
    pddlISetFree(&p->mgroup);
    for (int i = 0; i < p->num_states; ++i){
        for (int j = 0; j < p->num_states; ++j){
            pddlISetFree(p->tr + i * p->num_states + j);
        }
    }
    if (p->tr != NULL)
        FREE(p->tr);
}

static int outdegree(const pddl_mgroup_projection_t *p, int state)
{
    int outdegree = 0;
    for (int i = 0; i < p->num_states; ++i){
        if (i != state && pddlISetSize(p->tr + state * p->num_states + i) > 0)
            outdegree += pddlISetSize(p->tr + state * p->num_states + i);
            //outdegree += 1;
    }
    return outdegree;
}

int pddlMGroupProjectionMaxOutdegree(const pddl_mgroup_projection_t *p)
{
    int max = -1;
    for (int state = 0; state < p->num_states; ++state){
        int deg = outdegree(p, state);
        max = PDDL_MAX(max, deg);
    }
    return max;
}

void pddlMGroupProjectionPruneUnreachable(pddl_mgroup_projection_t *p,
                                          const pddl_iset_t *states,
                                          int backward)
{
    int *visited = CALLOC_ARR(int, p->num_states);
    PDDL_IARR(queue);
    int state;
    PDDL_ISET_FOR_EACH(states, state){
        pddlIArrAdd(&queue, state);
        visited[state] = 1;
    }
    for (int cur = 0; cur < pddlIArrSize(&queue); ++cur){
        int state = pddlIArrGet(&queue, cur);
        for (int to = 0; to < p->num_states; ++to){
            if (visited[to])
                continue;
            const pddl_iset_t *tr;
            if (backward){
                tr = p->tr + to * p->num_states + state;
            }else{
                tr = p->tr + state * p->num_states + to;
            }
            if (pddlISetSize(tr) > 0){
                pddlIArrAdd(&queue, to);
                visited[to] = 1;
            }
        }
    }
    pddlIArrFree(&queue);

    for (int state = 0; state < p->num_states; ++state){
        if (visited[state])
            continue;
        for (int to = 0; to < p->num_states; ++to){
            pddlISetEmpty(p->tr + state * p->num_states + to);
            pddlISetEmpty(p->tr + to * p->num_states + state);
        }
    }
    FREE(visited);
}

void pddlMGroupProjectionPruneUnreachableFromInit(pddl_mgroup_projection_t *p,
                                                  const pddl_strips_t *strips)
{
    PDDL_ISET(from_state);
    PDDL_ISET(init);
    pddlISetIntersect2(&init, &p->mgroup, &strips->init);

    if (pddlISetSize(&init) > 1)
        PANIC("The set of facts is not a mutex group!");

    if (pddlISetSize(&init) == 0){
        pddlISetAdd(&from_state, p->num_states - 1);
    }else{
        int init_fact = pddlISetGet(&init, 0);
        for (int i = 0; i < pddlISetSize(&p->mgroup); ++i){
            if (pddlISetGet(&p->mgroup, i) == init_fact){
                pddlISetAdd(&from_state, i);
                break;
            }
        }
    }
    ASSERT_RUNTIME(pddlISetSize(&from_state) == 1);
    pddlMGroupProjectionPruneUnreachable(p, &from_state, 0);
    pddlISetFree(&from_state);
    pddlISetFree(&init);
}

void pddlMGroupProjectionPruneUnreachableFromGoal(pddl_mgroup_projection_t *p,
                                                  const pddl_strips_t *strips,
                                                  const pddl_mutex_pairs_t *mx)
{
    PDDL_ISET(from_state);
    PDDL_ISET(goal);
    pddlISetIntersect2(&goal, &p->mgroup, &strips->goal);
    if (pddlISetSize(&goal) > 0){
        for (int i = 0; i < pddlISetSize(&p->mgroup); ++i){
            if (pddlISetIn(pddlISetGet(&p->mgroup, i), &goal))
                pddlISetAdd(&from_state, i);
        }

    }else{
        pddlISetAdd(&from_state, p->num_states - 1);
        for (int i = 0; i < pddlISetSize(&p->mgroup); ++i){
            int fact = pddlISetGet(&p->mgroup, i);
            if (!pddlMutexPairsIsMutexFactSet(mx, fact, &strips->goal))
                pddlISetAdd(&from_state, i);
        }
    }

    ASSERT_RUNTIME(pddlISetSize(&from_state) > 0);
    pddlMGroupProjectionPruneUnreachable(p, &from_state, 1);
    pddlISetFree(&from_state);
    pddlISetFree(&goal);
}

void pddlMGroupProjectionRestrictOps(pddl_mgroup_projection_t *p,
                                     const pddl_iset_t *ops)
{
    for (int i = 0; i < p->num_states; ++i){
        for (int j = 0; j < p->num_states; ++j){
            pddlISetIntersect(p->tr + i * p->num_states + j, ops);
        }
    }
}

void pddlMGroupProjectionPrint(const pddl_mgroup_projection_t *p,
                               const pddl_strips_t *strips,
                               FILE *fout)
{
    fprintf(fout, "Proj (%d)\n", p->num_states);
    for (int i = 0; i < p->num_states; ++i){
        for (int j = 0; j < p->num_states; ++j){
            if (pddlISetSize(p->tr + i * p->num_states + j) == 0)
                continue;
            int fact_from = -1;
            if (i < p->num_states - 1)
                fact_from = pddlISetGet(&p->mgroup, i);
            int fact_to = -1;
            if (j < p->num_states - 1)
                fact_to = pddlISetGet(&p->mgroup, j);

            fprintf(fout, "%d[%d:(%s)] -> %d[%d:(%s)] (%d)\n",
                    i, fact_from,
                    (fact_from >= 0 ?  strips->fact.fact[fact_from]->name : ""),
                    j, fact_to,
                    (fact_to >= 0 ?  strips->fact.fact[fact_to]->name : ""),
                    pddlISetSize(p->tr + i * p->num_states + j));
            int op_id;
            PDDL_ISET_FOR_EACH(p->tr + i * p->num_states + j, op_id){
                const pddl_strips_op_t *op = strips->op.op[op_id];
                fprintf(fout, "  %d:", op_id);
                pddlStripsOpPrintDebug(op, &strips->fact, fout);
            }
        }
    }
}
