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

#include "internal.h"
#include "pddl/cost.h"
#include "pddl/hadd.h"

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

static int fdrNumCondEffs(const pddl_fdr_t *fdr)
{
    int num = 0;
    for (int op_id = 0; op_id < fdr->op.op_size; ++op_id){
        const pddl_fdr_op_t *op = fdr->op.op[op_id];
        num += op->cond_eff_size;
    }
    return num;
}

static int stripsNumCondEffs(const pddl_strips_t *strips)
{
    int num = 0;
    for (int op_id = 0; op_id < strips->op.op_size; ++op_id){
        const pddl_strips_op_t *op = strips->op.op[op_id];
        num += op->cond_eff_size;
    }
    return num;
}

void pddlHAddInit(pddl_hadd_t *h, const pddl_fdr_t *fdr)
{
    ZEROIZE(h);

    // Allocate facts and add one for empty-precondition fact and one for
    // goal fact
    h->fact_size = fdr->var.global_id_size + 2;
    h->fact = CALLOC_ARR(pddl_hadd_fact_t, h->fact_size);
    h->fact_goal = h->fact_size - 2;
    h->fact_nopre = h->fact_size - 1;

    // Allocate operators and add one artificial for goal
    h->op_size = fdr->op.op_size + 1 + fdrNumCondEffs(fdr);
    h->op = CALLOC_ARR(pddl_hadd_op_t, h->op_size);
    h->op_goal = h->op_size - 1;

    int cond_eff_ins = fdr->op.op_size;
    PDDL_ISET(pre);
    for (int op_id = 0; op_id < fdr->op.op_size; ++op_id){
        const pddl_fdr_op_t *src = fdr->op.op[op_id];
        pddl_hadd_op_t *op = h->op + op_id;

        pddlFDRPartStateToGlobalIDs(&src->eff, &fdr->var, &op->eff);
        op->cost = src->cost;

        pddlISetEmpty(&pre);
        pddlFDRPartStateToGlobalIDs(&src->pre, &fdr->var, &pre);
        int fact;
        PDDL_ISET_FOR_EACH(&pre, fact)
            pddlISetAdd(&h->fact[fact].pre_op, op_id);
        op->pre_size = pddlISetSize(&pre);

        // Record operator with no preconditions
        if (op->pre_size == 0){
            pddlISetAdd(&h->fact[h->fact_nopre].pre_op, op_id);
            op->pre_size = 1;
        }

        for (int cei = 0; cei < src->cond_eff_size; ++cei){
            const pddl_fdr_op_cond_eff_t *ce = src->cond_eff + cei;
            pddl_hadd_op_t *op = h->op + cond_eff_ins;
            pddlFDRPartStateToGlobalIDs(&ce->eff, &fdr->var, &op->eff);
            op->cost = src->cost;

            PDDL_ISET(ce_pre);
            pddlISetUnion(&ce_pre, &pre);
            pddlFDRPartStateToGlobalIDs(&ce->pre, &fdr->var, &ce_pre);
            int fact;
            PDDL_ISET_FOR_EACH(&ce_pre, fact)
                pddlISetAdd(&h->fact[fact].pre_op, cond_eff_ins);
            op->pre_size = pddlISetSize(&ce_pre);
            ASSERT_RUNTIME(op->pre_size > 0);

            ++cond_eff_ins;
        }
    }

    // Set up goal operator
    pddl_hadd_op_t *op = h->op + h->op_goal;
    pddlISetAdd(&op->eff, h->fact_goal);
    op->cost = 0;

    pddlISetEmpty(&pre);
    pddlFDRPartStateToGlobalIDs(&fdr->goal, &fdr->var, &pre);
    int fact;
    PDDL_ISET_FOR_EACH(&pre, fact)
        pddlISetAdd(&h->fact[fact].pre_op, h->op_goal);
    op->pre_size = pddlISetSize(&pre);

    pddlISetFree(&pre);
}

void pddlHAddInitStrips(pddl_hadd_t *h, const pddl_strips_t *strips)
{
    ZEROIZE(h);

    // Allocate facts and add one for empty-precondition fact and one for
    // goal fact
    h->fact_size = strips->fact.fact_size + 2;
    h->fact = CALLOC_ARR(pddl_hadd_fact_t, h->fact_size);
    h->fact_goal = h->fact_size - 2;
    h->fact_nopre = h->fact_size - 1;

    // Allocate operators and add one artificial for goal
    h->op_size = strips->op.op_size + 1 + stripsNumCondEffs(strips);
    h->op = CALLOC_ARR(pddl_hadd_op_t, h->op_size);
    h->op_goal = h->op_size - 1;

    int cond_eff_ins = strips->op.op_size;
    for (int op_id = 0; op_id < strips->op.op_size; ++op_id){
        const pddl_strips_op_t *src = strips->op.op[op_id];
        pddl_hadd_op_t *op = h->op + op_id;
        pddlISetUnion(&op->eff, &src->add_eff);
        op->cost = src->cost;

        int fact;
        PDDL_ISET_FOR_EACH(&src->pre, fact)
            pddlISetAdd(&h->fact[fact].pre_op, op_id);
        op->pre_size = pddlISetSize(&src->pre);

        // Record operator with no preconditions
        if (op->pre_size == 0){
            pddlISetAdd(&h->fact[h->fact_nopre].pre_op, op_id);
            op->pre_size = 1;
        }

        for (int cei = 0; cei < src->cond_eff_size; ++cei){
            const pddl_strips_op_cond_eff_t *ce = src->cond_eff + cei;
            pddl_hadd_op_t *op = h->op + cond_eff_ins;
            pddlISetUnion(&op->eff, &ce->add_eff);
            op->cost = src->cost;

            int fact;
            PDDL_ISET_FOR_EACH(&src->pre, fact)
                pddlISetAdd(&h->fact[fact].pre_op, cond_eff_ins);
            PDDL_ISET_FOR_EACH(&ce->pre, fact)
                pddlISetAdd(&h->fact[fact].pre_op, cond_eff_ins);
            op->pre_size = pddlISetSize(&src->pre) + pddlISetSize(&ce->pre);
            ASSERT_RUNTIME(op->pre_size > 0);

            ++cond_eff_ins;
        }
    }

    // Set up goal operator
    pddl_hadd_op_t *op = h->op + h->op_goal;
    pddlISetAdd(&op->eff, h->fact_goal);
    op->cost = 0;

    int fact;
    PDDL_ISET_FOR_EACH(&strips->goal, fact)
        pddlISetAdd(&h->fact[fact].pre_op, h->op_goal);
    op->pre_size = pddlISetSize(&strips->goal);
}

void pddlHAddFree(pddl_hadd_t *hadd)
{
    for (int i = 0; i < hadd->fact_size; ++i)
        pddlISetFree(&hadd->fact[i].pre_op);
    if (hadd->fact != NULL)
        FREE(hadd->fact);

    for (int i = 0; i < hadd->op_size; ++i)
        pddlISetFree(&hadd->op[i].eff);
    if (hadd->op != NULL)
        FREE(hadd->op);
}

static void initFacts(pddl_hadd_t *h)
{
    int i;

    for (i = 0; i < h->fact_size; ++i){
        FVALUE_INIT(h->fact + i);
    }
}

static void initOps(pddl_hadd_t *h)
{
    int i;

    for (i = 0; i < h->op_size; ++i){
        h->op[i].unsat = h->op[i].pre_size;
        h->op[i].value = h->op[i].cost;
    }
}

static void addInitState(pddl_hadd_t *h,
                         const int *fdr_state,
                         const pddl_fdr_vars_t *vars,
                         pddl_pq_t *pq)
{
    for (int var = 0; var < vars->var_size; ++var){
        int fact_id = vars->var[var].val[fdr_state[var]].global_id;
        FPUSH(pq, 0, h->fact + fact_id);
    }
    FPUSH(pq, 0, h->fact + h->fact_nopre);
}

static void addStripsInitState(pddl_hadd_t *h,
                               const pddl_iset_t *state,
                               pddl_pq_t *pq)
{
    int fact_id;
    PDDL_ISET_FOR_EACH(state, fact_id)
        FPUSH(pq, 0, h->fact + fact_id);
    FPUSH(pq, 0, h->fact + h->fact_nopre);
}

static void enqueueOpEffects(pddl_hadd_t *h, pddl_hadd_op_t *op, pddl_pq_t *pq)
{
    int fid;
    PDDL_ISET_FOR_EACH(&op->eff, fid){
        pddl_hadd_fact_t *fact = h->fact + fid;
        if (FVALUE(fact) > op->value)
            FPUSH(pq, op->value, fact);
    }
}

static int _pddlHAdd(pddl_hadd_t *h,
                     const int *fdr_state,
                     const pddl_fdr_vars_t *vars,
                     const pddl_iset_t *strips_state)
{
    pddl_pq_t pq;

    pddlPQInit(&pq);
    initFacts(h);
    initOps(h);
    if (fdr_state != NULL){
        addInitState(h, fdr_state, vars, &pq);
    }else{
        addStripsInitState(h, strips_state, &pq);
    }
    while (!pddlPQEmpty(&pq)){
        int value;
        pddl_pq_el_t *el = pddlPQPop(&pq, &value);
        pddl_hadd_fact_t *fact = pddl_container_of(el, pddl_hadd_fact_t, heap);

        int fact_id = FID(h, fact);
        if (fact_id == h->fact_goal)
            break;

        int op_id;
        PDDL_ISET_FOR_EACH(&fact->pre_op, op_id){
            pddl_hadd_op_t *op = h->op + op_id;
            op->value = pddlSumSat(op->value, value);
            if (--op->unsat == 0)
                enqueueOpEffects(h, op, &pq);
        }
    }
    pddlPQFree(&pq);

    int heur = PDDL_COST_DEAD_END;
    if (FVALUE_IS_SET(h->fact + h->fact_goal))
        heur = FVALUE(h->fact + h->fact_goal);
    return heur;
}

int pddlHAdd(pddl_hadd_t *h,
             const int *fdr_state,
             const pddl_fdr_vars_t *vars)
{
    return _pddlHAdd(h, fdr_state, vars, NULL);
}

int pddlHAddStrips(pddl_hadd_t *h, const pddl_iset_t *state)
{
    return _pddlHAdd(h, NULL, NULL, state);
}
