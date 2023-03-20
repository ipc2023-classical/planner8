/***
 * cpddl
 * -------
 * Copyright (c)2018 Daniel Fiser <danfis@danfis.cz>,
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
#include "pddl/ts.h"
#include "internal.h"

void pddlTSInit(pddl_ts_t *ts, int num_states)
{
    ZEROIZE(ts);
    ts->num_states = num_states;
    ts->tr = CALLOC_ARR(pddl_iset_t, num_states * num_states);
    ts->init_state = -1;
}

void pddlTSFree(pddl_ts_t *ts)
{
    int s1, s2;
    pddl_iset_t *tr;
    PDDL_TS_FOR_EACH_TRANSITION(ts, s1, s2, tr)
        pddlISetFree(tr);
    if (ts->tr != NULL)
        FREE(ts->tr);
}

void pddlTSAddTransition(pddl_ts_t *ts, int s1, int s2, int l)
{
    pddlISetAdd(pddlTSTransitionW(ts, s1, s2), l);
}

#ifdef PDDL_DEBUG
static void checkProjToFAMGroup(const pddl_ts_t *ts)
{
    int s1, s2;
    const pddl_iset_t *tr;
    PDDL_ISET(labels);

    PDDL_TS_FOR_EACH_TRANSITION(ts, s1, s2, tr){
        int label;
        PDDL_ISET_FOR_EACH(tr, label){
            ASSERT(!pddlISetIn(label, &labels));
            pddlISetAdd(&labels, label);
        }
    }

    pddlISetFree(&labels);
}
#else /* PDDL_DEBUG */
static void checkProjToFAMGroup(const pddl_ts_t *ts)
{
}
#endif /* PDDL_DEBUG */

void pddlTSInitProjToFAMGroup(pddl_ts_t *ts,
                              const pddl_strips_t *strips,
                              const pddl_strips_fact_cross_ref_t *cr,
                              const pddl_iset_t *famgroup)
{
    PDDL_ISET(fset);
    PDDL_ISET(predel);
    int *fact_to_state;

    pddlTSInit(ts, pddlISetSize(famgroup) + 1);

    fact_to_state = CALLOC_ARR(int, strips->fact.fact_size);
    for (int state = 0; state < pddlISetSize(famgroup); ++state){
        int fact = pddlISetGet(famgroup, state);
        fact_to_state[fact] = state;
    }

    for (int state = 0; state < pddlISetSize(famgroup); ++state){
        int fact = pddlISetGet(famgroup, state);
        const pddl_iset_t *pre = &cr->fact[fact].op_pre;
        const pddl_iset_t *del_eff = &cr->fact[fact].op_del;
        pddlISetIntersect2(&predel, pre, del_eff);
        int opi;
        PDDL_ISET_FOR_EACH(&predel, opi){
            const pddl_strips_op_t *op = strips->op.op[opi];
            pddlISetIntersect2(&fset, &op->add_eff, famgroup);
            // Skip operators that are not reachable
            if (pddlISetSize(&fset) > 1
                    || pddlISetIntersectionSizeAtLeast(&op->pre, famgroup, 2)){
                continue;
            }

            int dst_state = pddlISetSize(famgroup);
            if (pddlISetSize(&fset) == 1)
                dst_state = fact_to_state[pddlISetGet(&fset, 0)];

            pddlTSAddTransition(ts, state, dst_state, opi);
        }

        // Add loops that have precondition on this abstract state
        pddlISetMinus2(&fset, &cr->fact[fact].op_pre, del_eff);
        PDDL_ISET_FOR_EACH(&fset, opi){
            const pddl_strips_op_t *op = strips->op.op[opi];
            if (pddlISetIntersectionSizeAtLeast(&op->pre, famgroup, 2))
                continue;
            pddlTSAddTransition(ts, state, state, opi);
        }
    }

    // Set up the initial state
    pddlISetIntersect2(&fset, famgroup, &strips->init);
    ASSERT(pddlISetSize(&fset) == 1);
    ts->init_state = fact_to_state[pddlISetGet(&fset, 0)];

    checkProjToFAMGroup(ts);

    if (fact_to_state != NULL)
        FREE(fact_to_state);
    pddlISetFree(&fset);
    pddlISetFree(&predel);
}

/** Strongly connected components */
struct cond_scc {
    pddl_iset_t *comp; /*!< List of components */
    int comp_size;    /*!< Number of components */
    int comp_alloc;
};
typedef struct cond_scc cond_scc_t;

/** Context for DFS during computing SCC */
struct cond_scc_dfs {
    int cur_index;
    int *index;
    int *lowlink;
    int *in_stack;
    int *stack;
    int stack_size;
};
typedef struct cond_scc_dfs cond_scc_dfs_t;

static void sccTarjanStrongconnect(cond_scc_t *scc, cond_scc_dfs_t *dfs,
                                   const pddl_ts_t *ts, int state)
{
    dfs->index[state] = dfs->lowlink[state] = dfs->cur_index++;
    dfs->stack[dfs->stack_size++] = state;
    dfs->in_stack[state] = 1;

    for (int w = 0; w < ts->num_states; ++w){
        if (w == state || pddlISetSize(pddlTSTransition(ts, state, w)) == 0)
            continue;
        if (dfs->index[w] == -1){
            sccTarjanStrongconnect(scc, dfs, ts, w);
            dfs->lowlink[state] = PDDL_MIN(dfs->lowlink[state], dfs->lowlink[w]);
        }else if (dfs->in_stack[w]){
            dfs->lowlink[state] = PDDL_MIN(dfs->lowlink[state], dfs->lowlink[w]);
        }
    }

    if (dfs->index[state] == dfs->lowlink[state]){
        // Find how deep unroll stack
        int i;
        for (i = dfs->stack_size - 1; dfs->stack[i] != state; --i)
            dfs->in_stack[dfs->stack[i]] = 0;
        dfs->in_stack[dfs->stack[i]] = 0;

        // Create new component
        if (scc->comp_size == scc->comp_alloc){
            if (scc->comp_alloc == 0)
                scc->comp_alloc = 1;
            scc->comp_alloc *= 2;
            scc->comp = REALLOC_ARR(scc->comp, pddl_iset_t, scc->comp_alloc);
        }
        pddl_iset_t *comp = scc->comp + scc->comp_size++;
        pddlISetInit(comp);
        for (int j = i; j < dfs->stack_size; ++j)
            pddlISetAdd(comp, dfs->stack[j]);

        // Shrink stack
        dfs->stack_size = i;
    }
}

static void sccTarjan(cond_scc_t *scc, const pddl_ts_t *ts)
{
    cond_scc_dfs_t dfs;

    // Initialize structure for Tarjan's algorithm
    dfs.cur_index = 0;
    dfs.index    = ALLOC_ARR(int, 4 * ts->num_states);
    dfs.lowlink  = dfs.index + ts->num_states;
    dfs.in_stack = dfs.lowlink + ts->num_states;
    dfs.stack    = dfs.in_stack + ts->num_states;
    dfs.stack_size = 0;
    for (int i = 0; i < ts->num_states; ++i){
        dfs.index[i] = dfs.lowlink[i] = -1;
        dfs.in_stack[i] = 0;
    }

    for (int s = 0; s < ts->num_states; ++s){
        if (dfs.index[s] == -1)
            sccTarjanStrongconnect(scc, &dfs, ts, s);
    }

    FREE(dfs.index);
}

static void condCreateLoop(pddl_ts_t *cond,
                           int state,
                           const pddl_ts_t *ts,
                           const pddl_iset_t *comp)
{
    pddl_iset_t *loop = pddlTSTransitionW(cond, state, state);
    int s1, s2;
    PDDL_ISET_FOR_EACH(comp, s1){
        PDDL_ISET_FOR_EACH(comp, s2){
            pddlISetUnion(loop, pddlTSTransition(ts, s1, s2));
        }
    }
}

void pddlTSCondensate(pddl_ts_t *cond, const pddl_ts_t *ts)
{
    cond_scc_t scc;
    ZEROIZE(&scc);
    sccTarjan(&scc, ts);

    pddlTSInit(cond, scc.comp_size);
    for (int ci1 = 0; ci1 < scc.comp_size; ++ci1){
        const pddl_iset_t *comp1 = scc.comp + ci1;
        if (pddlISetIn(ts->init_state, comp1))
            cond->init_state = ci1;

        condCreateLoop(cond, ci1, ts, comp1);

        for (int ci2 = 0; ci2 < scc.comp_size; ++ci2){
            if (ci1 == ci2)
                continue;
            const pddl_iset_t *comp2 = scc.comp + ci2;
            pddl_iset_t *tr = pddlTSTransitionW(cond, ci1, ci2);
            int s1, s2;
            PDDL_ISET_FOR_EACH(comp1, s1){
                PDDL_ISET_FOR_EACH(comp2, s2)
                    pddlISetUnion(tr, pddlTSTransition(ts, s1, s2));
            }
        }
    }

    for (int i = 0; i < scc.comp_size; ++i)
        pddlISetFree(&scc.comp[i]);
    if (scc.comp != NULL)
        FREE(scc.comp);
}

void pddlTSPruneUnreachableStates(pddl_ts_t *ts, int state)
{
    PDDL_IARR(queue);
    int *reach;

    reach = CALLOC_ARR(int, ts->num_states);
    reach[state] = 1;
    pddlIArrAdd(&queue, state);
    while (pddlIArrSize(&queue) > 0){
        int cur_state = queue.arr[--queue.size];
        for (int s = 0; s < ts->num_states; ++s){
            const pddl_iset_t *tr = pddlTSTransition(ts, cur_state, s);
            if (pddlISetSize(tr) > 0 && !reach[s]){
                reach[s] = 1;
                pddlIArrAdd(&queue, s);
            }
        }
    }

    int states = 0;
    for (int i = 0; i < ts->num_states; ++i){
        if (reach[i]){
            reach[i] = states++;
        }else{
            reach[i] = -1;
        }
    }
    if (states == ts->num_states){
        FREE(reach);
        return;
    }

    pddl_ts_t newts;
    pddlTSInit(&newts, states);
    for (int i = 0; i < ts->num_states; ++i){
        if (reach[i] == -1)
            continue;
        for (int j = 0; j < ts->num_states; ++j){
            if (reach[j] == -1)
                continue;
            pddl_iset_t *dst = newts.tr + reach[i] * newts.num_states + reach[j];
            const pddl_iset_t *src = pddlTSTransition(ts, i, j);
            pddlISetUnion(dst, src);
        }
    }
    newts.init_state = reach[ts->init_state];

    pddlTSFree(ts);
    *ts = newts;

    FREE(reach);
}

void pddlTSPrintDebug(const pddl_ts_t *ts, FILE *fout)
{
    int s1, s2;
    const pddl_iset_t *tr;
    int op_id;

    fprintf(fout, "Init: %d\n", ts->init_state);
    PDDL_TS_FOR_EACH_NONEMPTY_TRANSITION(ts, s1, s2, tr){
        fprintf(fout, "%d -> %d:", s1, s2);
        PDDL_ISET_FOR_EACH(tr, op_id)
            fprintf(fout, " %d", op_id);
        fprintf(fout, "\n");
    }
}

