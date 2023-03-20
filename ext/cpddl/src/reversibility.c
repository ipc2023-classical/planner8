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

#include "pddl/sort.h"
#include "pddl/reversibility.h"
#include "internal.h"

static void pddlConjFactFormulaFree(pddl_conj_fact_formula_t *f)
{
    pddlISetFree(&f->pos);
    pddlISetFree(&f->neg);
}

static void pddlReversePlanFree(pddl_reverse_plan_t *r)
{
    pddlConjFactFormulaFree(&r->formula);
    pddlIArrFree(&r->plan);
}

void pddlReversibilityUniformInit(pddl_reversibility_uniform_t *r)
{
    ZEROIZE(r);
}

void pddlReversibilityUniformFree(pddl_reversibility_uniform_t *r)
{
    for (int i = 0; i < r->plan_size; ++i){
        pddlReversePlanFree(r->plan + i);
    }
    if (r->plan != NULL)
        FREE(r->plan);
}

static int revPlanCmp(const void *a, const void *b, void *ud)
{
    const pddl_reverse_plan_t *p1 = a;
    const pddl_reverse_plan_t *p2 = b;
    int cmp = p1->reversible_op_id - p2->reversible_op_id;
    if (cmp == 0)
        cmp = pddlISetCmp(&p1->formula.pos, &p2->formula.pos);
    if (cmp == 0)
        cmp = pddlISetCmp(&p1->formula.neg, &p2->formula.neg);
    if (cmp == 0)
        cmp = pddlIArrSize(&p1->plan) - pddlIArrSize(&p2->plan);
    if (cmp == 0)
        cmp = pddlIArrCmp(&p1->plan, &p2->plan);
    return cmp;
}

void pddlReversibilityUniformSort(pddl_reversibility_uniform_t *r)
{
    pddlSort(r->plan, r->plan_size, sizeof(pddl_reverse_plan_t),
             revPlanCmp, NULL);
}

static int foundPlan(const pddl_iset_t *pre_a,
                     const pddl_iset_t *F0,
                     const pddl_iset_t *Fpos,
                     const pddl_iset_t *Fneg)
{
    return pddlISetIsSubset(pre_a, Fpos)
            && pddlISetIsDisjunct(F0, Fneg);
}

static int isApplicable(const pddl_iset_t *Fneg,
                        const pddl_strips_op_t *op,
                        const pddl_iset_t *F0pos,
                        const pddl_mutex_pairs_t *mutex)
{
    if (mutex != NULL && pddlMutexPairsIsMutexSetSet(mutex, &op->pre, F0pos))
        return 0;
    return pddlISetIsDisjunct(Fneg, &op->pre);
}

static void applyOp(pddl_iset_t *F0,
                    pddl_iset_t *Fpos,
                    pddl_iset_t *Fneg,
                    const pddl_strips_op_t *op)
{
    PDDL_ISET(pre_Fpos);
    pddlISetMinus2(&pre_Fpos, &op->pre, Fpos);
    pddlISetUnion(F0, &pre_Fpos);
    pddlISetFree(&pre_Fpos);

    pddlISetMinus(Fpos, &op->del_eff);
    pddlISetUnion(Fpos, &op->add_eff);
    pddlISetMinus(Fneg, &op->add_eff);
    pddlISetUnion(Fneg, &op->del_eff);
}

static pddl_reverse_plan_t *addEmptyPlan(pddl_reversibility_uniform_t *r)
{
    if (r->plan_size == r->plan_alloc){
        if (r->plan_alloc == 0)
            r->plan_alloc = 2;
        r->plan_alloc *= 2;
        r->plan = REALLOC_ARR(r->plan, pddl_reverse_plan_t, r->plan_alloc);
    }
    pddl_reverse_plan_t *plan = r->plan + r->plan_size++;
    ZEROIZE(plan);
    return plan;
}

static void addPlan(pddl_reversibility_uniform_t *r,
                    const pddl_strips_ops_t *ops,
                    const pddl_strips_op_t *a,
                    const pddl_iset_t *F0,
                    const pddl_iset_t *Fpos,
                    const pddl_iset_t *Fneg,
                    const pddl_iarr_t *plan,
                    const pddl_mutex_pairs_t *mutex)
{
    PDDL_ISET(pos);
    pddlISetUnion2(&pos, Fpos, F0);
    if (mutex != NULL && pddlMutexPairsIsMutexSet(mutex, &pos)){
        pddlISetFree(&pos);
        return;
    }

    pddl_reverse_plan_t *rplan = addEmptyPlan(r);
    rplan->reversible_op_id = a->id;
    pddlISetSet(&rplan->formula.pos, &pos);

    int fact;
    PDDL_ISET_FOR_EACH(Fneg, fact){
        if (mutex == NULL || !pddlMutexPairsIsMutexFactSet(mutex, fact, &pos))
            pddlISetAdd(&rplan->formula.neg, fact);
    }

    if (pddlISetSize(&rplan->formula.neg) == 0
            && pddlISetIsSubset(&rplan->formula.pos, &a->pre)){
        pddlISetEmpty(&rplan->formula.pos);
    }
    pddlIArrAppendArr(&rplan->plan, plan);
    pddlISetFree(&pos);
}

static void reversibleRec(pddl_reversibility_uniform_t *r,
                          const pddl_strips_ops_t *ops,
                          const pddl_strips_op_t *a,
                          const pddl_iset_t *F0,
                          const pddl_iset_t *Fpos,
                          const pddl_iset_t *Fneg,
                          pddl_iarr_t *plan,
                          int depth,
                          const pddl_mutex_pairs_t *mutex)
{
    if (foundPlan(&a->pre, F0, Fpos, Fneg)){
        addPlan(r, ops, a, F0, Fpos, Fneg, plan, mutex);
        return;
    }
    if (depth == 0)
        return;

    PDDL_ISET(F0pos);
    pddlISetUnion2(&F0pos, F0, Fpos);
    if (mutex != NULL && pddlMutexPairsIsMutexSet(mutex, &F0pos)){
        pddlISetFree(&F0pos);
        return;
    }

    PDDL_ISET(F0_next);
    PDDL_ISET(Fpos_next);
    PDDL_ISET(Fneg_next);
    for (int op_id = 0; op_id < ops->op_size; ++op_id){
        const pddl_strips_op_t *op = ops->op[op_id];
        if (isApplicable(Fneg, op, &F0pos, mutex)){
            pddlISetSet(&F0_next, F0);
            pddlISetSet(&Fpos_next, Fpos);
            pddlISetSet(&Fneg_next, Fneg);

            applyOp(&F0_next, &Fpos_next, &Fneg_next, op);
            pddlIArrAdd(plan, op_id);
            reversibleRec(r, ops, a, &F0_next, &Fpos_next, &Fneg_next,
                          plan, depth - 1, mutex);
            pddlIArrRmLast(plan);
        }
    }
    pddlISetFree(&F0pos);
    pddlISetFree(&F0_next);
    pddlISetFree(&Fpos_next);
    pddlISetFree(&Fneg_next);
}

void pddlReversibilityUniformInfer(pddl_reversibility_uniform_t *r,
                                   const pddl_strips_ops_t *ops,
                                   const pddl_strips_op_t *rev_op,
                                   int max_depth,
                                   const pddl_mutex_pairs_t *mutex)
{
    PDDL_ISET(F0);
    PDDL_ISET(Fpos);
    PDDL_ISET(Fneg);
    PDDL_IARR(plan);

    pddlISetMinus2(&Fpos, &rev_op->pre, &rev_op->del_eff);
    pddlISetUnion(&Fpos, &rev_op->add_eff);
    pddlISetUnion(&Fneg, &rev_op->del_eff);

    reversibleRec(r, ops, rev_op, &F0, &Fpos, &Fneg, &plan, max_depth, mutex);

    pddlIArrFree(&plan);
    pddlISetFree(&F0);
    pddlISetFree(&Fpos);
    pddlISetFree(&Fneg);
}

void pddlReversePlanUniformPrint(const pddl_reverse_plan_t *p,
                                 const pddl_strips_ops_t *ops,
                                 FILE *fout)
{
    const pddl_strips_op_t *op = ops->op[p->reversible_op_id];
    //printf("%d:(%s)", p->reversible_op_id, op->name);
    fprintf(fout, "%d", p->reversible_op_id);
    fprintf(fout, " | \\phi:");
    int fact;
    PDDL_ISET_FOR_EACH(&p->formula.pos, fact){
        fprintf(fout, " +%d", fact);
        if (pddlISetIn(fact, &op->pre))
            fprintf(fout, "*");
    }
    PDDL_ISET_FOR_EACH(&p->formula.neg, fact)
        fprintf(fout, " -%d", fact);

    fprintf(fout, " | plan:");
    int op_id;
    PDDL_IARR_FOR_EACH(&p->plan, op_id)
        fprintf(fout, " %d", op_id);

    fprintf(fout, " | (%s)", op->name);
    fprintf(fout, "\n");

    /*
    printf("\t%d: pre:", op->id);
    PDDL_ISET_FOR_EACH(&op->pre, fact)
        printf(" %d", fact);
    printf(", del:");
    PDDL_ISET_FOR_EACH(&op->del_eff, fact)
        printf(" %d", fact);
    printf(", add:");
    PDDL_ISET_FOR_EACH(&op->add_eff, fact)
        printf(" %d", fact);
    printf("\n");
    PDDL_IARR_FOR_EACH(&p->plan, op_id){
        const pddl_strips_op_t *op = ops->op[op_id];
        printf("\t%d: pre:", op->id);
        PDDL_ISET_FOR_EACH(&op->pre, fact)
            printf(" %d", fact);
        printf(", del:");
        PDDL_ISET_FOR_EACH(&op->del_eff, fact)
            printf(" %d", fact);
        printf(", add:");
        PDDL_ISET_FOR_EACH(&op->add_eff, fact)
            printf(" %d", fact);
        printf("\n");
    }
    */
}

void pddlReversibilityUniformPrint(const pddl_reversibility_uniform_t *r,
                                   const pddl_strips_ops_t *ops,
                                   FILE *fout)
{
    for (int i = 0; i < r->plan_size; ++i)
        pddlReversePlanUniformPrint(r->plan + i, ops, fout);
}
