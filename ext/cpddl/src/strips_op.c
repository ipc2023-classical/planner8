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

#include "internal.h"
#include "pddl/sort.h"
#include "pddl/iarr.h"
#include "pddl/hfunc.h"
#include "pddl/pddl.h"
#include "pddl/strips_op.h"

void pddlStripsOpInit(pddl_strips_op_t *op)
{
    ZEROIZE(op);
    op->pddl_action_id = -1;
}

static void condEffFree(pddl_strips_op_cond_eff_t *ce)
{
    pddlISetFree(&ce->pre);
    pddlISetFree(&ce->add_eff);
    pddlISetFree(&ce->del_eff);
}

void pddlStripsOpFree(pddl_strips_op_t *op)
{
    if (op->name)
        FREE(op->name);
    pddlISetFree(&op->pre);
    pddlISetFree(&op->del_eff);
    pddlISetFree(&op->add_eff);
    for (int i = 0; i < op->cond_eff_size; ++i)
        condEffFree(&op->cond_eff[i]);
    if (op->cond_eff != NULL)
        FREE(op->cond_eff);
    if (op->action_args != NULL)
        FREE(op->action_args);
}

void pddlStripsOpFreeAllCondEffs(pddl_strips_op_t *op)
{
    for (int i = 0; i < op->cond_eff_size; ++i)
        condEffFree(&op->cond_eff[i]);
    op->cond_eff_size = 0;
}

pddl_strips_op_t *pddlStripsOpNew(void)
{
    pddl_strips_op_t *op;
    op = ALLOC(pddl_strips_op_t);
    pddlStripsOpInit(op);
    return op;
}

void pddlStripsOpDel(pddl_strips_op_t *op)
{
    pddlStripsOpFree(op);
    FREE(op);
}

static pddl_strips_op_cond_eff_t *addCondEff(pddl_strips_op_t *op)
{
    pddl_strips_op_cond_eff_t *ce;

    if (op->cond_eff_size >= op->cond_eff_alloc){
        if (op->cond_eff_alloc == 0)
            op->cond_eff_alloc = 1;
        op->cond_eff_alloc *= 2;
        op->cond_eff = REALLOC_ARR(op->cond_eff,
                                   pddl_strips_op_cond_eff_t,
                                   op->cond_eff_alloc);
    }

    ce = op->cond_eff + op->cond_eff_size++;
    ZEROIZE(ce);
    return ce;
}

pddl_strips_op_cond_eff_t *pddlStripsOpAddCondEff(pddl_strips_op_t *op,
                                                  const pddl_strips_op_t *f)
{
    pddl_strips_op_cond_eff_t *ce = addCondEff(op);
    pddlISetUnion(&ce->pre, &f->pre);
    pddlISetUnion(&ce->add_eff, &f->add_eff);
    pddlISetUnion(&ce->del_eff, &f->del_eff);
    return ce;
}

void pddlStripsOpNormalize(pddl_strips_op_t *op)
{
    pddlISetMinus(&op->del_eff, &op->add_eff);
    pddlISetMinus(&op->add_eff, &op->pre);
}

int pddlStripsOpFinalize(pddl_strips_op_t *op, char *name)
{
    op->name = name;
    pddlStripsOpNormalize(op);
    if (op->add_eff.size == 0 && op->del_eff.size == 0)
        return -1;
    return 0;
}

void pddlStripsOpAddEffFromOp(pddl_strips_op_t *dst,
                              const pddl_strips_op_t *src)
{
    pddlISetUnion(&dst->add_eff, &src->add_eff);
    pddlISetUnion(&dst->del_eff, &src->del_eff);
    pddlStripsOpNormalize(dst);
}

void pddlStripsOpCopy(pddl_strips_op_t *dst, const pddl_strips_op_t *src)
{
    pddl_strips_op_cond_eff_t *ce;

    pddlStripsOpCopyWithoutCondEff(dst, src);
    for (int i = 0; i < src->cond_eff_size; ++i){
        const pddl_strips_op_cond_eff_t *f = src->cond_eff + i;
        ce = addCondEff(dst);
        pddlISetUnion(&ce->pre, &f->pre);
        pddlISetUnion(&ce->add_eff, &f->add_eff);
        pddlISetUnion(&ce->del_eff, &f->del_eff);
    }

    dst->pddl_action_id = src->pddl_action_id;
    if (src->action_args != NULL){
        dst->action_args = ALLOC_ARR(pddl_obj_id_t, src->action_args_size);
        dst->action_args_size = src->action_args_size;
        memcpy(dst->action_args, src->action_args,
               sizeof(pddl_obj_id_t) * src->action_args_size);
    }
}

void pddlStripsOpCopyWithoutCondEff(pddl_strips_op_t *dst,
                                    const pddl_strips_op_t *src)
{
    dst->name = STRDUP(src->name);
    dst->cost = src->cost;
    pddlISetUnion(&dst->pre, &src->pre);
    pddlISetUnion(&dst->add_eff, &src->add_eff);
    pddlISetUnion(&dst->del_eff, &src->del_eff);
    dst->pddl_action_id = src->pddl_action_id;
}

void pddlStripsOpCopyDual(pddl_strips_op_t *dst, const pddl_strips_op_t *src)
{
    pddl_strips_op_cond_eff_t *ce;

    dst->name = STRDUP(src->name);
    dst->cost = src->cost;
    pddlISetUnion(&dst->pre, &src->del_eff);
    pddlISetUnion(&dst->add_eff, &src->add_eff);
    pddlISetUnion(&dst->del_eff, &src->pre);
    for (int i = 0; i < src->cond_eff_size; ++i){
        const pddl_strips_op_cond_eff_t *f = src->cond_eff + i;
        ce = addCondEff(dst);
        pddlISetUnion(&ce->pre, &f->del_eff);
        pddlISetUnion(&ce->add_eff, &f->add_eff);
        pddlISetUnion(&ce->del_eff, &f->pre);
    }
    dst->pddl_action_id = src->pddl_action_id;
    if (src->action_args != NULL){
        dst->action_args = ALLOC_ARR(pddl_obj_id_t, src->action_args_size);
        dst->action_args_size = src->action_args_size;
        memcpy(dst->action_args, src->action_args,
               sizeof(pddl_obj_id_t) * src->action_args_size);
    }
}

void pddlStripsOpRemapFacts(pddl_strips_op_t *op, const int *remap)
{
    pddl_strips_op_cond_eff_t *ce;

    pddlISetRemap(&op->pre, remap);
    pddlISetRemap(&op->add_eff, remap);
    pddlISetRemap(&op->del_eff, remap);
    for (int i = 0; i < op->cond_eff_size; ++i){
        ce = op->cond_eff + i;
        pddlISetRemap(&ce->pre, remap);
        pddlISetRemap(&ce->add_eff, remap);
        pddlISetRemap(&ce->del_eff, remap);
    }
}

static void iarrAppendISet(pddl_iarr_t *arr, const pddl_iset_t *set)
{
    int fact;
    PDDL_ISET_FOR_EACH(set, fact)
        pddlIArrAdd(arr, fact);
}

static uint64_t opHash(const pddl_strips_op_t *op)
{
    const int delim = INT_MAX;
    PDDL_IARR(buf);
    uint64_t hash;

    iarrAppendISet(&buf, &op->pre);
    pddlIArrAdd(&buf, delim);
    iarrAppendISet(&buf, &op->add_eff);
    pddlIArrAdd(&buf, delim);
    iarrAppendISet(&buf, &op->del_eff);
    pddlIArrAdd(&buf, delim);

    for (int cei = 0; cei < op->cond_eff_size; ++cei){
        const pddl_strips_op_cond_eff_t *ce = op->cond_eff + cei;
        iarrAppendISet(&buf, &ce->pre);
        pddlIArrAdd(&buf, delim);
        iarrAppendISet(&buf, &ce->add_eff);
        pddlIArrAdd(&buf, delim);
        iarrAppendISet(&buf, &ce->del_eff);
        pddlIArrAdd(&buf, delim);
    }

    hash = pddlCityHash_64(buf.arr, buf.size);
    pddlIArrFree(&buf);

    return hash;
}

static uint64_t opEq(const pddl_strips_op_t *op1,
                     const pddl_strips_op_t *op2)
{
    if (op1->cond_eff_size != op2->cond_eff_size)
        return 0;

    if (!pddlISetEq(&op1->pre, &op2->pre)
            || !pddlISetEq(&op1->add_eff, &op2->add_eff)
            || !pddlISetEq(&op2->del_eff, &op2->del_eff))
        return 0;

    for (int cei = 0; cei < op1->cond_eff_size; ++cei){
        const pddl_strips_op_cond_eff_t *ce1 = op1->cond_eff + cei;
        const pddl_strips_op_cond_eff_t *ce2 = op2->cond_eff + cei;
        if (!pddlISetEq(&ce1->pre, &ce2->pre)
                || !pddlISetEq(&ce1->add_eff, &ce2->add_eff)
                || !pddlISetEq(&ce1->del_eff, &ce2->del_eff))
            return 0;
    }
    return 1;
}

struct deduplicate {
    int id;
    int cost;
    uint64_t hash;
};
typedef struct deduplicate deduplicate_t;

static int opDeduplicateCmp(const void *_a, const void *_b, void *_)
{
    const deduplicate_t *a = _a;
    const deduplicate_t *b = _b;
    if (a->hash < b->hash)
        return -1;
    if (a->hash > b->hash)
        return 1;
    if (a->cost == b->cost)
        return a->id - b->id;
    return a->cost - b->cost;
}

static int deduplicateRange(const pddl_strips_ops_t *ops,
                            deduplicate_t *dedup,
                            int start,
                            int end,
                            int *remove)
{
    int change = 0;

    for (int di = start; di < end; ++di){
        const deduplicate_t *d1 = dedup + di;
        if (d1->id < 0)
            continue;

        for (int di2 = di + 1; di2 < end; ++di2){
            deduplicate_t *d2 = dedup + di2;
            if (d2->id < 0)
                continue;

            if (opEq(ops->op[d1->id], ops->op[d2->id])){
                // dedup is sorted by the cost so it always holds that
                // d1->cost <= d2->cost
                remove[d2->id] = 1;
                change = 1;
                d2->id = -1;
            }
        }
    }

    return change;
}

static int deduplicate(pddl_strips_ops_t *ops, int *remove)
{
    deduplicate_t *dedup;
    int change = 0;

    dedup = CALLOC_ARR(deduplicate_t, ops->op_size);
    for (int op_id = 0; op_id < ops->op_size; ++op_id){
        dedup[op_id].id = op_id;
        dedup[op_id].cost = ops->op[op_id]->cost;
        dedup[op_id].hash = opHash(ops->op[op_id]);
    }

    pddlSort(dedup, ops->op_size, sizeof(deduplicate_t),
             opDeduplicateCmp, NULL);

    int start, cur;
    for (start = 0, cur = 1; cur < ops->op_size; ++cur){
        if (dedup[cur].hash != dedup[start].hash){
            if (start < cur - 1)
                change |= deduplicateRange(ops, dedup, start, cur, remove);
            start = cur;
        }
    }
    if (start < cur - 1)
        change |= deduplicateRange(ops, dedup, start, cur, remove);

    FREE(dedup);
    return change;
}

void pddlStripsOpsDeduplicate(pddl_strips_ops_t *ops)
{
    int *remove = CALLOC_ARR(int, ops->op_size);
    if (deduplicate(ops, remove))
        pddlStripsOpsDelOps(ops, remove);
    if (remove != NULL)
        FREE(remove);
}

void pddlStripsOpsDeduplicateSet(pddl_strips_ops_t *ops, pddl_iset_t *rm_op)
{
    int *remove = CALLOC_ARR(int, ops->op_size);
    if (deduplicate(ops, remove)){
        for (int oi = 0; oi < ops->op_size; ++oi){
            if (remove[oi])
                pddlISetAdd(rm_op, oi);
        }
    }
    if (remove != NULL)
        FREE(remove);
}

void pddlStripsOpsSetUnitCost(pddl_strips_ops_t *ops)
{
    for (int i = 0; i < ops->op_size; ++i)
        ops->op[i]->cost = 1;
}

static int opCmp(const void *a, const void *b, void *_)
{
    pddl_strips_op_t *o1 = *(pddl_strips_op_t **)a;
    pddl_strips_op_t *o2 = *(pddl_strips_op_t **)b;
    int cmp = strcmp(o1->name, o2->name);
    if (cmp == 0)
        cmp = pddlISetCmp(&o1->pre, &o2->pre);
    if (cmp == 0)
        cmp = pddlISetCmp(&o1->add_eff, &o2->add_eff);
    if (cmp == 0)
        cmp = pddlISetCmp(&o1->del_eff, &o2->del_eff);
    return cmp;
}

void pddlStripsOpsSort(pddl_strips_ops_t *ops)
{
    pddlSort(ops->op, ops->op_size, sizeof(pddl_strips_op_t *), opCmp, NULL);
    for (int i = 0; i < ops->op_size; ++i)
        ops->op[i]->id = i;
}

static void reorderCondEffs(pddl_strips_op_t *op)
{
    int size = 0;
    for (int cei = 0; cei < op->cond_eff_size; ++cei){
        pddl_strips_op_cond_eff_t *ce = op->cond_eff + cei;
        if (pddlISetSize(&ce->pre) == 0){
            condEffFree(&op->cond_eff[cei]);
        }else{
            op->cond_eff[size++] = op->cond_eff[cei];
        }
    }
    op->cond_eff_size = size;
    pddlStripsOpNormalize(op);
}

void pddlStripsOpRemoveFact(pddl_strips_op_t *op, int fact_id)
{
    int reorder = 0;

    pddlISetRm(&op->pre, fact_id);
    pddlISetRm(&op->add_eff, fact_id);
    pddlISetRm(&op->del_eff, fact_id);

    for (int cei = 0; cei < op->cond_eff_size; ++cei){
        pddl_strips_op_cond_eff_t *ce = op->cond_eff + cei;
        pddlISetRm(&ce->pre, fact_id);
        pddlISetRm(&ce->add_eff, fact_id);
        pddlISetRm(&ce->del_eff, fact_id);
        if (pddlISetSize(&ce->pre) == 0){
            pddlISetUnion(&op->add_eff, &ce->add_eff);
            pddlISetUnion(&op->del_eff, &ce->del_eff);
            reorder = 1;
        }
    }

    if (reorder)
        reorderCondEffs(op);
}

void pddlStripsOpRemoveFacts(pddl_strips_op_t *op, const pddl_iset_t *facts)
{
    int reorder = 0;

    pddlISetMinus(&op->pre, facts);
    pddlISetMinus(&op->add_eff, facts);
    pddlISetMinus(&op->del_eff, facts);

    for (int cei = 0; cei < op->cond_eff_size; ++cei){
        pddl_strips_op_cond_eff_t *ce = op->cond_eff + cei;
        pddlISetMinus(&ce->pre, facts);
        pddlISetMinus(&ce->add_eff, facts);
        pddlISetMinus(&ce->del_eff, facts);
        if (pddlISetSize(&ce->pre) == 0){
            pddlISetUnion(&op->add_eff, &ce->add_eff);
            pddlISetUnion(&op->del_eff, &ce->del_eff);
            reorder = 1;
        }
    }

    if (reorder)
        reorderCondEffs(op);
}

void pddlStripsOpApplyOnState(const pddl_strips_op_t *op,
                              const pddl_iset_t *in_state,
                              pddl_iset_t *out_state)
{
    pddlISetMinus2(out_state, in_state, &op->del_eff);
    pddlISetUnion(out_state, &op->add_eff);

    for (int cei = 0; cei < op->cond_eff_size; ++cei){
        const pddl_strips_op_cond_eff_t *ce = op->cond_eff + cei;
        if (pddlISetIsSubset(&ce->pre, in_state)){
            pddlISetMinus(out_state, &ce->del_eff);
            pddlISetUnion(out_state, &ce->add_eff);
        }
    }
}


void pddlStripsOpsInit(pddl_strips_ops_t *ops)
{
    ZEROIZE(ops);
    ops->op_alloc = 4;
    ops->op = ALLOC_ARR(pddl_strips_op_t *, ops->op_alloc);
}

void pddlStripsOpsFree(pddl_strips_ops_t *ops)
{
    for (int i = 0; i < ops->op_size; ++i){
        if (ops->op[i])
            pddlStripsOpDel(ops->op[i]);
    }
    if (ops->op != NULL)
        FREE(ops->op);
}

void pddlStripsOpsCopy(pddl_strips_ops_t *dst, const pddl_strips_ops_t *src)
{
    for (int i = 0; i < src->op_size; ++i)
        pddlStripsOpsAdd(dst, src->op[i]);
}

static pddl_strips_op_t *nextNewOp(pddl_strips_ops_t *ops)
{
    pddl_strips_op_t *op;

    if (ops->op_size >= ops->op_alloc){
        ops->op_alloc *= 2;
        ops->op = REALLOC_ARR(ops->op, pddl_strips_op_t *, ops->op_alloc);
    }

    op = pddlStripsOpNew();
    op->id = ops->op_size;
    ops->op[ops->op_size] = op;
    ++ops->op_size;
    return op;
}

int pddlStripsOpsAdd(pddl_strips_ops_t *ops, const pddl_strips_op_t *add)
{
    pddl_strips_op_t *op;
    op = nextNewOp(ops);
    pddlStripsOpCopy(op, add);
    return op->id;
}

void pddlStripsOpsDelOps(pddl_strips_ops_t *ops, const int *m)
{
    int ins = 0;
    for (int op_id = 0; op_id < ops->op_size; ++op_id){
        if (m[op_id]){
            pddlStripsOpDel(ops->op[op_id]);
        }else{
            ops->op[op_id]->id = ins;
            ops->op[ins++] = ops->op[op_id];
        }
    }

    ops->op_size = ins;
}

void pddlStripsOpsDelOpsSet(pddl_strips_ops_t *ops, const pddl_iset_t *del_ops)
{
    int op_id;
    PDDL_ISET_FOR_EACH(del_ops, op_id){
        pddlStripsOpDel(ops->op[op_id]);
        ops->op[op_id] = NULL;
    }

    int ins = 0;
    for (int op_id = 0; op_id < ops->op_size; ++op_id){
        if (ops->op[op_id] != NULL){
            ops->op[op_id]->id = ins;
            ops->op[ins++] = ops->op[op_id];
        }
    }

    ops->op_size = ins;
}

void pddlStripsOpsRemapFacts(pddl_strips_ops_t *ops, const int *remap)
{
    for (int i = 0; i < ops->op_size; ++i)
        pddlStripsOpRemapFacts(ops->op[i], remap);
}

void pddlStripsOpsRemoveFacts(pddl_strips_ops_t *ops, const pddl_iset_t *facts)
{
    for (int i = 0; i < ops->op_size; ++i)
        pddlStripsOpRemoveFacts(ops->op[i], facts);
}

void pddlStripsOpPrintDebug(const pddl_strips_op_t *op,
                            const pddl_facts_t *fs,
                            FILE *fout)
{
    fprintf(fout, "  (%s), cost: %d\n", op->name, op->cost);

    fprintf(fout, "    pre:");
    pddlFactsPrintSet(&op->pre, fs, " ", "", fout);
    fprintf(fout, "\n");
    fprintf(fout, "    add:");
    pddlFactsPrintSet(&op->add_eff, fs, " ", "", fout);
    fprintf(fout, "\n");
    fprintf(fout, "    del:");
    pddlFactsPrintSet(&op->del_eff, fs, " ", "", fout);
    fprintf(fout, "\n");

    if (op->cond_eff_size > 0)
        fprintf(fout, "    cond-eff[%d]:\n", op->cond_eff_size);

    for (int j = 0; j < op->cond_eff_size; ++j){
        const pddl_strips_op_cond_eff_t *ce = op->cond_eff + j;

        fprintf(fout, "      pre:");
        pddlFactsPrintSet(&ce->pre, fs, " ", "", fout);
        fprintf(fout, "\n");
        fprintf(fout, "      add:");
        pddlFactsPrintSet(&ce->add_eff, fs, " ", "", fout);
        fprintf(fout, "\n");
        fprintf(fout, "      del:");
        pddlFactsPrintSet(&ce->del_eff, fs, " ", "", fout);
        fprintf(fout, "\n");
    }
}

void pddlStripsOpsPrintDebug(const pddl_strips_ops_t *ops,
                             const pddl_facts_t *fs,
                             FILE *fout)
{
    for (int i = 0; i < ops->op_size; ++i)
        pddlStripsOpPrintDebug(ops->op[i], fs, fout);
}
