/***
 * cpddl
 * -------
 * Copyright (c)2020 Daniel Fiser <danfis@danfis.cz>,
 * AIC, Department of Computer Science,
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

#include "pddl/sort.h"
#include "pddl/cost.h"
#include "pddl/hff.h"
#include "internal.h"

#define FID(heur, f) ((f) - (heur)->fact)
#define FVALUE(fact) (fact)->heap.key
#define FVALUE_SET(fact, val) do { (fact)->heap.key = val; } while(0)
#define FVALUE_INIT(fact) FVALUE_SET((fact), INT_MAX)
#define FVALUE_IS_SET(fact) (FVALUE(fact) != INT_MAX)

#define FPUSH(pq, value, fact) \
    do { \
    if (FVALUE_IS_SET(fact)){ \
        pddlPQUpdate((pq), (value), &(fact)->heap); \
    }else{ \
        pddlPQPush((pq), (value), &(fact)->heap); \
    } \
    } while (0)

void pddlHFFInit(pddl_hff_t *h, const pddl_fdr_t *fdr)
{
    if (fdr->has_cond_eff)
        PANIC("h^ff does not support conditional effects\n");
    ZEROIZE(h);

    // Allocate facts and add one for empty-precondition fact and one for
    // goal fact
    h->fact_size = fdr->var.global_id_size + 2;
    h->fact = CALLOC_ARR(pddl_hff_fact_t, h->fact_size);
    h->fact_goal = h->fact_size - 2;
    h->fact_nopre = h->fact_size - 1;

    // Allocate operators and add one artificial for goal
    h->op_size = fdr->op.op_size + 1;
    h->op = CALLOC_ARR(pddl_hff_op_t, h->op_size);
    h->op_goal = h->op_size - 1;

    for (int op_id = 0; op_id < fdr->op.op_size; ++op_id){
        const pddl_fdr_op_t *src = fdr->op.op[op_id];
        pddl_hff_op_t *op = h->op + op_id;

        pddlFDRPartStateToGlobalIDs(&src->pre, &fdr->var, &op->pre);
        pddlFDRPartStateToGlobalIDs(&src->eff, &fdr->var, &op->eff);
        op->cost = src->cost;

        int fact;
        PDDL_ISET_FOR_EACH(&op->pre, fact)
            pddlISetAdd(&h->fact[fact].pre_op, op_id);
        op->pre_size = pddlISetSize(&op->pre);
        PDDL_ISET_FOR_EACH(&op->eff, fact)
            pddlISetAdd(&h->fact[fact].eff_op, op_id);

        // Record operator with no preconditions
        if (op->pre_size == 0){
            pddlISetAdd(&h->fact[h->fact_nopre].pre_op, op_id);
            op->pre_size = 1;
        }
    }

    // Set up goal operator
    pddl_hff_op_t *op = h->op + h->op_goal;
    pddlISetAdd(&op->eff, h->fact_goal);
    op->cost = 0;

    pddlFDRPartStateToGlobalIDs(&fdr->goal, &fdr->var, &op->pre);
    int fact;
    PDDL_ISET_FOR_EACH(&op->pre, fact)
        pddlISetAdd(&h->fact[fact].pre_op, h->op_goal);
    op->pre_size = pddlISetSize(&op->pre);
    PDDL_ISET_FOR_EACH(&op->eff, fact)
        pddlISetAdd(&h->fact[fact].eff_op, h->op_goal);
}

void pddlHFFInitStrips(pddl_hff_t *h, const pddl_strips_t *strips)
{
    if (strips->has_cond_eff)
        PANIC("h^ff does not support conditional effects\n");
    ZEROIZE(h);

    // Allocate facts and add one for empty-precondition fact and one for
    // goal fact
    h->fact_size = strips->fact.fact_size + 2;
    h->fact = CALLOC_ARR(pddl_hff_fact_t, h->fact_size);
    h->fact_goal = h->fact_size - 2;
    h->fact_nopre = h->fact_size - 1;

    // Allocate operators and add one artificial for goal
    h->op_size = strips->op.op_size + 1;
    h->op = CALLOC_ARR(pddl_hff_op_t, h->op_size);
    h->op_goal = h->op_size - 1;

    for (int op_id = 0; op_id < strips->op.op_size; ++op_id){
        const pddl_strips_op_t *src = strips->op.op[op_id];
        pddl_hff_op_t *op = h->op + op_id;

        pddlISetUnion(&op->pre, &src->pre);
        pddlISetUnion(&op->eff, &src->add_eff);
        op->cost = src->cost;

        int fact;
        PDDL_ISET_FOR_EACH(&op->pre, fact)
            pddlISetAdd(&h->fact[fact].pre_op, op_id);
        op->pre_size = pddlISetSize(&op->pre);
        PDDL_ISET_FOR_EACH(&op->eff, fact)
            pddlISetAdd(&h->fact[fact].eff_op, op_id);

        // Record operator with no preconditions
        if (op->pre_size == 0){
            pddlISetAdd(&h->fact[h->fact_nopre].pre_op, op_id);
            op->pre_size = 1;
        }
    }

    // Set up goal operator
    pddl_hff_op_t *op = h->op + h->op_goal;
    pddlISetAdd(&op->eff, h->fact_goal);
    op->cost = 0;

    pddlISetUnion(&op->pre, &strips->goal);
    int fact;
    PDDL_ISET_FOR_EACH(&op->pre, fact)
        pddlISetAdd(&h->fact[fact].pre_op, h->op_goal);
    op->pre_size = pddlISetSize(&op->pre);
    PDDL_ISET_FOR_EACH(&op->eff, fact)
        pddlISetAdd(&h->fact[fact].eff_op, h->op_goal);
}

void pddlHFFFree(pddl_hff_t *hff)
{
    for (int i = 0; i < hff->fact_size; ++i){
        pddlISetFree(&hff->fact[i].pre_op);
        pddlISetFree(&hff->fact[i].eff_op);
    }
    if (hff->fact != NULL)
        FREE(hff->fact);

    for (int i = 0; i < hff->op_size; ++i){
        pddlISetFree(&hff->op[i].pre);
        pddlISetFree(&hff->op[i].eff);
    }
    if (hff->op != NULL)
        FREE(hff->op);
}

static void initFacts(pddl_hff_t *h)
{
    int i;

    for (i = 0; i < h->fact_size; ++i){
        FVALUE_INIT(h->fact + i);
        h->fact[i].marked = 0;
        h->fact[i].reached_by_op = -1;
    }
}

static void initOps(pddl_hff_t *h)
{
    int i;

    for (i = 0; i < h->op_size; ++i){
        h->op[i].unsat = h->op[i].pre_size;
        h->op[i].value = h->op[i].cost;
        h->op[i].marked = 0;
        h->op[i].order = -1;
    }
}

static void addInitState(pddl_hff_t *h, const pddl_iset_t *state, pddl_pq_t *pq)
{
    int fact_id;
    PDDL_ISET_FOR_EACH(state, fact_id)
        FPUSH(pq, 0, h->fact + fact_id);
    FPUSH(pq, 0, h->fact + h->fact_nopre);
}

static void enqueueOpEffects(pddl_hff_t *h,
                             int op_id,
                             pddl_hff_op_t *op,
                             pddl_pq_t *pq)
{
    int fid;
    PDDL_ISET_FOR_EACH(&op->eff, fid){
        pddl_hff_fact_t *fact = h->fact + fid;
        if (FVALUE(fact) > op->value){
            h->fact[fid].reached_by_op = op_id;
            FPUSH(pq, op->value, fact);
        }
    }
}

static int hadd(pddl_hff_t *h, const pddl_iset_t *state)
{
    pddl_pq_t pq;

    pddlPQInit(&pq);
    initFacts(h);
    initOps(h);
    addInitState(h, state, &pq);
    int order = 0;
    while (!pddlPQEmpty(&pq)){
        int value;
        pddl_pq_el_t *el = pddlPQPop(&pq, &value);
        pddl_hff_fact_t *fact = pddl_container_of(el, pddl_hff_fact_t, heap);

        int fact_id = FID(h, fact);
        if (fact_id == h->fact_goal)
            break;

        int op_id;
        PDDL_ISET_FOR_EACH(&fact->pre_op, op_id){
            pddl_hff_op_t *op = h->op + op_id;
            op->value = pddlSumSat(op->value, value);
            if (--op->unsat == 0){
                op->order = order++;
                enqueueOpEffects(h, op_id, op, &pq);
            }
        }
    }
    pddlPQFree(&pq);

    int heur = PDDL_COST_DEAD_END;
    if (FVALUE_IS_SET(h->fact + h->fact_goal))
        heur = FVALUE(h->fact + h->fact_goal);
    return heur;
}

static void relaxedPlanMarkFact(pddl_hff_t *h, int fact)
{
    if (h->fact[fact].marked)
        return;
    h->fact[fact].marked = 1;
    ASSERT(h->fact[fact].reached_by_op >= 0);
    int op = h->fact[fact].reached_by_op;
    if (op < 0)
        return;

    h->op[op].marked = 1;
    int pre;
    PDDL_ISET_FOR_EACH(&h->op[op].pre, pre){
        if (!h->fact[pre].marked)
            relaxedPlanMarkFact(h, pre);
    }
}

static void markRelaxedPlan(pddl_hff_t *h, const pddl_iset_t *state)
{
    for (int oi = 0; oi < h->op_size; ++oi)
        h->op[oi].marked = 0;
    for (int fi = 0; fi < h->fact_size; ++fi)
        h->fact[fi].marked = 0;

    h->fact[h->fact_nopre].marked = 1;
    int fact_id;
    PDDL_ISET_FOR_EACH(state, fact_id)
        h->fact[fact_id].marked = 1;

    relaxedPlanMarkFact(h, h->fact_goal);
}

static int cmpExtractPlan(const void *a, const void *b, void *u)
{
    int op1 = *(int *)a;
    int op2 = *(int *)b;
    const pddl_hff_t *h = u;
    int cmp = h->op[op2].marked - h->op[op1].marked;
    if (cmp == 0){
        cmp = h->op[op1].order - h->op[op2].order;
    }
    return cmp;
}

static void extractPlan(const pddl_hff_t *h, pddl_iarr_t *plan)
{
    int *ops = ALLOC_ARR(int, h->op_size);
    for (int i = 0; i < h->op_size; ++i)
        ops[i] = i;
    pddlSort(ops, h->op_size, sizeof(int), cmpExtractPlan, (void *)h);
    for (int i = 0; i < h->op_size; ++i){
        if (!h->op[ops[i]].marked)
            break;
        if (ops[i] != h->op_goal)
            pddlIArrAdd(plan, ops[i]);
    }
    FREE(ops);
}

static int hff(pddl_hff_t *h, const pddl_iset_t *state)
{
    int add_heur = hadd(h, state);
    if (add_heur == PDDL_COST_DEAD_END)
        return PDDL_COST_DEAD_END;
    markRelaxedPlan(h, state);
    int heur = 0;
    for (int i = 0; i < h->op_size; ++i){
        if (h->op[i].marked)
            heur = pddlSumSat(heur, h->op[i].cost);
    }
    return heur;
}


int pddlHFF(pddl_hff_t *h,
            const int *fdr_state,
            const pddl_fdr_vars_t *vars)
{
    PDDL_ISET(state);
    for (int var = 0; var < vars->var_size; ++var){
        int val = fdr_state[var];
        int fact_id = vars->var[var].val[val].global_id;
        pddlISetAdd(&state, fact_id);
    }
    int ret = hff(h, &state);
    pddlISetFree(&state);
    return ret;
}

int pddlHFFPlan(pddl_hff_t *h,
                const int *fdr_state,
                const pddl_fdr_vars_t *vars,
                pddl_iarr_t *plan)
{
    int heur = pddlHFF(h, fdr_state, vars);
    if (heur == PDDL_COST_DEAD_END)
        return PDDL_COST_DEAD_END;
    extractPlan(h, plan);
    return heur;
}

int pddlHFFStrips(pddl_hff_t *h, const pddl_iset_t *state)
{
    return hff(h, state);
}

int pddlHFFStripsPlan(pddl_hff_t *h,
                      const pddl_iset_t *state,
                      pddl_iarr_t *plan)
{
    int heur = pddlHFFStrips(h, state);
    if (heur == PDDL_COST_DEAD_END)
        return PDDL_COST_DEAD_END;
    extractPlan(h, plan);
    return heur;
}
