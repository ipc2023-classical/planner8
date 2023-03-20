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

#include "pddl/sort.h"
#include "pddl/symbolic_trans.h"
#include "internal.h"

struct op {
    int op_id;
    char *name;
    pddl_iset_t pre;
    pddl_iset_t neg_pre;
    pddl_iset_t eff;
    pddl_iset_t neg_eff;
    pddl_cost_t cost;
    pddl_cost_t heur_change;
    int is_dead;
};
typedef struct op op_t;

static void opInit(pddl_symbolic_constr_t *constr,
                   const pddl_strips_op_t *op_in,
                   int use_op_constr,
                   int op_id,
                   op_t *op,
                   pddl_cost_t *op_heur_change,
                   int sum_op_heur_change_to_cost,
                   pddl_err_t *err)
{
    ZEROIZE(op);
    op->op_id = op_id;
    pddlISetUnion(&op->pre, &op_in->pre);
    pddlISetUnion(&op->eff, &op_in->add_eff);
    pddlISetUnion(&op->neg_eff, &op_in->del_eff);
    pddlCostSetOp(&op->cost, op_in->cost);
    if (op_in->name != NULL)
        op->name = STRDUP(op_in->name);

    // Disambiguate preconditions
    if (pddlDisambiguate(&constr->disambiguate, &op->pre, NULL,
                         1, 0, NULL, &op->pre) < 0){
        PDDL_INFO(err, "Operator %d:(%s) skipped, because it"
                  " is unreachable or dead-end", op->op_id, op->name);
        op->is_dead = 1;
        return;
    }
    pddlISetMinus(&op->eff, &op->pre);

    int fact_id;
    PDDL_ISET_FOR_EACH(&op->pre, fact_id)
        pddlISetMinus(&op->neg_eff, constr->fact_mutex + fact_id);

    if (use_op_constr){
        int fact;

        PDDL_ISET(fw_neg_pre);
        // Find negative preconditions
        PDDL_ISET_FOR_EACH(&op->pre, fact){
            pddlISetUnion(&op->neg_pre, constr->fact_mutex_bw + fact);
            pddlISetUnion(&fw_neg_pre, constr->fact_mutex_fw + fact);
        }

        // E-delete facts that are mutex with the add effect
        PDDL_ISET_FOR_EACH(&op->eff, fact)
            pddlISetUnion(&op->neg_eff, constr->fact_mutex_fw + fact);
        pddlISetMinus(&op->neg_eff, &fw_neg_pre);
        pddlISetMinus(&op->neg_eff, &op->neg_pre);
        pddlISetFree(&fw_neg_pre);
    }

    if (!pddlISetIsDisjoint(&op->neg_pre, &op->pre)
            || !pddlISetIsDisjoint(&op->neg_eff, &op->eff)){
        PDDL_INFO(err, "Operator %d:(%s) skipped, because it"
                  " is unreachable or dead-end", op->op_id, op->name);
        op->is_dead = 1;
    }

    if (op_heur_change != NULL){
        if (sum_op_heur_change_to_cost){
            pddlCostSum(&op->cost, op_heur_change + op_id);
            if (op->cost.cost == 0 && op->cost.zero_cost == 0)
                op->cost.zero_cost = 1;
        }else{
            op->heur_change = op_heur_change[op_id];
            //DBG(err, "%d:(%s) --> %s / %d", op->op_id, op->name,
            //    F_COST(&op->heur_change), op->cost);
        }
    }
}

static void opFree(op_t *op)
{
    if (op->name != NULL)
        FREE(op->name);
    pddlISetFree(&op->pre);
    pddlISetFree(&op->neg_pre);
    pddlISetFree(&op->eff);
    pddlISetFree(&op->neg_eff);
}

static void opsInit(pddl_symbolic_constr_t *constr,
                    const pddl_strips_t *strips,
                    int use_op_constr,
                    op_t *ops,
                    pddl_cost_t *op_heur_change,
                    int sum_op_heur_change_to_cost,
                    pddl_err_t *err)
{
    for (int op_id = 0; op_id < strips->op.op_size; ++op_id){
        opInit(constr, strips->op.op[op_id], use_op_constr,
               op_id, ops + op_id, op_heur_change, sum_op_heur_change_to_cost, err);
    }
}

static void transFree(pddl_bdd_manager_t *mgr, pddl_symbolic_trans_t *tr)
{
    pddlBDDDel(mgr, tr->bdd);
    for (int i = 0; i < tr->var_size; ++i){
        pddlBDDDel(mgr, tr->var_pre[i]);
        pddlBDDDel(mgr, tr->var_eff[i]);
    }
    if (tr->var_pre != NULL)
        FREE(tr->var_pre);
    if (tr->var_eff != NULL)
        FREE(tr->var_eff);
    pddlBDDDel(mgr, tr->exist_pre);
    pddlBDDDel(mgr, tr->exist_eff);
    pddlISetFree(&tr->eff_groups);
    ZEROIZE(tr);
}

static void transSetFree(pddl_bdd_manager_t *mgr,
                         pddl_symbolic_trans_set_t *trset)
{
    for (int i = 0; i < trset->trans_size; ++i)
        transFree(mgr, trset->trans + i);
    if (trset->trans != NULL)
        FREE(trset->trans);
    pddlISetFree(&trset->op);
}

static void transInitEffVars(pddl_symbolic_vars_t *vars,
                             pddl_symbolic_trans_t *tr)
{
    pddlSymbolicVarsGroupsBDDVars(vars, &tr->eff_groups,
                                  &tr->var_pre, &tr->var_eff, &tr->var_size);
    tr->exist_pre = pddlBDDCube(vars->mgr, tr->var_pre, tr->var_size);
    tr->exist_eff = pddlBDDCube(vars->mgr, tr->var_eff, tr->var_size);
}

static void transInit(pddl_symbolic_vars_t *vars,
                      pddl_symbolic_constr_t *constr,
                      const op_t *op,
                      pddl_symbolic_trans_t *tr,
                      int use_op_constr,
                      pddl_err_t *err)
{
    ASSERT(!op->is_dead);

    // Build the BDD from bottom up by first filling the array bdds and
    // then going over it from last to the first item
    pddl_bdd_t **bdds = CALLOC_ARR(pddl_bdd_t *, 2 * vars->group_size);
    int *pre_set = CALLOC_ARR(int, vars->group_size);
    int *eff_set = CALLOC_ARR(int, vars->group_size);

    int fact_id;
    PDDL_ISET_FOR_EACH(&op->pre, fact_id){
        int group_id = pddlSymbolicVarsFactGroup(vars, fact_id);
        pre_set[group_id] = 1;
        ASSERT(bdds[2 * group_id] == NULL);
        bdds[2 * group_id] = pddlSymbolicVarsFactPreBDD(vars, fact_id);
    }

    PDDL_ISET_FOR_EACH(&op->neg_pre, fact_id){
        ASSERT(!pddlISetIn(fact_id, &op->pre));
        int group_id = pddlSymbolicVarsFactGroup(vars, fact_id);
        pddl_bdd_t *bdd = pddlSymbolicVarsFactPreBDDNeg(vars, fact_id);
        if (bdds[2 * group_id] == NULL){
            bdds[2 * group_id] = bdd;
        }else{
            pddlBDDAndUpdate(vars->mgr, &bdds[2 * group_id], bdd);
            pddlBDDDel(vars->mgr, bdd);
        }
    }

    PDDL_ISET_FOR_EACH(&op->eff, fact_id){
        int group_id = pddlSymbolicVarsFactGroup(vars, fact_id);
        eff_set[group_id] = 1;
        ASSERT(bdds[2 * group_id + 1] == NULL);
        bdds[2 * group_id + 1] = pddlSymbolicVarsFactEffBDD(vars, fact_id);
    }

    PDDL_ISET_FOR_EACH(&op->neg_eff, fact_id){
        ASSERT(!pddlISetIn(fact_id, &op->eff));
        int group_id = pddlSymbolicVarsFactGroup(vars, fact_id);
        pddl_bdd_t *bdd = pddlSymbolicVarsFactEffBDDNeg(vars, fact_id);
        if (bdds[2 * group_id + 1] == NULL){
            bdds[2 * group_id + 1] = bdd;
        }else{
            pddlBDDAndUpdate(vars->mgr, &bdds[2 * group_id + 1], bdd);
            pddlBDDDel(vars->mgr, bdd);
        }

    }

    tr->bdd = pddlBDDOne(vars->mgr);
    for (int i = 2 * vars->group_size - 1; i >= 0; --i){
        if (bdds[i] != NULL){
            pddlBDDAndUpdate(vars->mgr, &tr->bdd, bdds[i]);
            pddlBDDDel(vars->mgr, bdds[i]);
        }
    }
    FREE(bdds);


    if (use_op_constr){
        int fact_id;


        PDDL_ISET_FOR_EACH(&op->eff, fact_id){
            int group_id = vars->fact[fact_id].group_id;
            if (pre_set[group_id]){
                pddlBDDAndUpdate(vars->mgr, &tr->bdd,
                                 constr->group_mutex[group_id]);
            }

            pddlBDDAndUpdate(vars->mgr, &tr->bdd,
                             constr->group_mgroup[group_id]);
        }
    }

    for (int i = 0; i < vars->group_size; ++i){
        if (eff_set[i])
            pddlISetAdd(&tr->eff_groups, i);
    }
    transInitEffVars(vars, tr);

    FREE(pre_set);
    FREE(eff_set);
}

static int transMerge(pddl_symbolic_vars_t *vars,
                      pddl_symbolic_trans_t *dst,
                      pddl_symbolic_trans_t *tr1,
                      pddl_symbolic_trans_t *tr2,
                      size_t max_nodes)
{
    ZEROIZE(dst);

    if (pddlBDDSize(tr1->bdd) >= max_nodes
            || pddlBDDSize(tr2->bdd) >= max_nodes){
        return -1;
    }

    pddl_bdd_t *bdd1 = pddlBDDClone(vars->mgr, tr1->bdd);
    pddl_bdd_t *bdd2 = pddlBDDClone(vars->mgr, tr2->bdd);

    pddlISetUnion2(&dst->eff_groups, &tr1->eff_groups, &tr2->eff_groups);
    int e1 = 0, esize1 = pddlISetSize(&tr1->eff_groups);
    int e2 = 0, esize2 = pddlISetSize(&tr2->eff_groups);
    int group_id;
    PDDL_ISET_FOR_EACH(&dst->eff_groups, group_id){
        if (e1 < esize1 && pddlISetGet(&tr1->eff_groups, e1) == group_id){
            ++e1;
        }else{
            pddl_bdd_t *biimp;
            biimp = pddlSymbolicVarsCreateBiimp(vars, group_id);
            pddlBDDAndUpdate(vars->mgr, &bdd1, biimp);
            pddlBDDDel(vars->mgr, biimp);
        }

        if (e2 < esize2 && pddlISetGet(&tr2->eff_groups, e2) == group_id){
            ++e2;
        }else{
            pddl_bdd_t *biimp;
            biimp = pddlSymbolicVarsCreateBiimp(vars, group_id);
            pddlBDDAndUpdate(vars->mgr, &bdd2, biimp);
            pddlBDDDel(vars->mgr, biimp);
        }
    }

    if (max_nodes > 0){
        dst->bdd = pddlBDDOrLimit(vars->mgr, bdd1, bdd2, max_nodes, NULL);
    }else{
        dst->bdd = pddlBDDOr(vars->mgr, bdd1, bdd2);
    }
    pddlBDDDel(vars->mgr, bdd1);
    pddlBDDDel(vars->mgr, bdd2);
    if (dst->bdd == NULL){
        pddlISetFree(&dst->eff_groups);
        return -1;
    }

    transInitEffVars(vars, dst);

    return 0;
}

static void transSetsAddRange(pddl_symbolic_vars_t *vars,
                              pddl_symbolic_constr_t *constr,
                              pddl_symbolic_trans_set_t *trset,
                              op_t *ops,
                              const int *op_ids,
                              int op_ids_size,
                              int use_op_constr,
                              int max_nodes,
                              float max_time,
                              pddl_err_t *err)
{
    ZEROIZE(trset);
    trset->vars = vars;
    for (int i = 0; i < op_ids_size; ++i)
        pddlISetAdd(&trset->op, op_ids[i]);
    ASSERT(pddlISetSize(&trset->op) > 0);
    trset->cost = ops[op_ids[0]].cost;
    trset->heur_change = ops[op_ids[0]].heur_change;

    int T_size = pddlISetSize(&trset->op);
    pddl_symbolic_trans_t *T = CALLOC_ARR(pddl_symbolic_trans_t, T_size);
    int Tres_size = 0;
    pddl_symbolic_trans_t *Tres = CALLOC_ARR(pddl_symbolic_trans_t, T_size);
    for (int i = 0; i < T_size; ++i)
        transInit(vars, constr, ops + op_ids[i], T + i, use_op_constr, err);

    PDDL_INFO(err, "Initialized individual trans BDDs: cost: %s,"
              " heur change: %s ops: %d",
              F_COST(&trset->cost),
              F_COST(&trset->heur_change),
              pddlISetSize(&trset->op));

    pddl_time_limit_t time_limit;
    pddlTimeLimitInit(&time_limit);
    pddlTimeLimitSet(&time_limit, max_time);
    while (T_size > 1){
        if (pddlTimeLimitCheck(&time_limit) < 0)
            break;

        int ins = 0;
        for (int i = 0; i < T_size; i = i + 2){
            if (i + 1 >= T_size){
                T[ins] = T[i];

            }else{
                if (T[i].bdd == NULL && T[i + 1].bdd == NULL){
                    ZEROIZE(T + ins);

                }else if (T[i].bdd == NULL){
                    T[ins] = T[i + 1];

                }else if (T[i + 1].bdd == NULL){
                    T[ins] = T[i];

                }else{
                    pddl_symbolic_trans_t restr;
                    int res = transMerge(vars, &restr, T + i, T + i + 1,
                                         max_nodes);
                    if (res < 0){
                        Tres[Tres_size++] = T[i];
                        Tres[Tres_size++] = T[i + 1];
                        ZEROIZE(T + ins);

                    }else{
                        transFree(vars->mgr, T + i);
                        transFree(vars->mgr, T + i + 1);
                        T[ins] = restr;
                    }
                }
            }
            ++ins;
        }
        T_size = ins;
    }

    for (int i = 0; i < T_size; ++i){
        if (T[i].bdd != NULL)
            Tres[Tres_size++] = T[i];
    }

    trset->trans_size = Tres_size;
    trset->trans = CALLOC_ARR(pddl_symbolic_trans_t, trset->trans_size);
    memcpy(trset->trans, Tres, sizeof(pddl_symbolic_trans_t) * Tres_size);

    FREE(T);
    FREE(Tres);

    long nodes = 0;
    for (int i = 0; i < trset->trans_size; ++i)
        nodes += pddlBDDSize(trset->trans[i].bdd);
    PDDL_INFO(err, "created trans BDDs: cost: %d, ops: %d, bdds: %d,"
              " nodes: %lu, %s",
              trset->cost, pddlISetSize(&trset->op), trset->trans_size,
              nodes, (T_size > 1 ? "(time limit reached)" : ""));
}

static int opIdCostCmp(const void *a, const void *b, void *_ops)
{
    const int id1 = *(const int *)a;
    const int id2 = *(const int *)b;
    const op_t *ops = _ops;
    int cmp = pddlCostCmp(&ops[id1].cost, &ops[id2].cost);
    if (cmp == 0)
        cmp = pddlCostCmp(&ops[id1].heur_change, &ops[id2].heur_change);
    if (cmp == 0)
        cmp = strcmp(ops[id1].name, ops[id2].name);
    if (cmp == 0)
        cmp = pddlISetCmp(&ops[id1].pre, &ops[id2].pre);
    if (cmp == 0)
        cmp = pddlISetCmp(&ops[id1].neg_pre, &ops[id2].neg_pre);
    if (cmp == 0)
        cmp = pddlISetCmp(&ops[id1].eff, &ops[id2].eff);
    if (cmp == 0)
        cmp = pddlISetCmp(&ops[id1].neg_eff, &ops[id2].neg_eff);
    if (cmp == 0)
        return id1 - id2;
    return cmp;
}

static int add(pddl_symbolic_trans_sets_t *trset)
{
    if (trset->trans_size == trset->trans_alloc){
        trset->trans_alloc *= 2;
        trset->trans = REALLOC_ARR(trset->trans,
                                   pddl_symbolic_trans_set_t,
                                   trset->trans_alloc);
    }
    ZEROIZE(trset->trans + trset->trans_size);
    return trset->trans_size++;
}

void pddlSymbolicTransSetsInit(pddl_symbolic_trans_sets_t *trset,
                               pddl_symbolic_vars_t *vars,
                               pddl_symbolic_constr_t *constr,
                               const pddl_strips_t *strips,
                               int use_op_constr,
                               int max_nodes,
                               float max_time,
                               pddl_cost_t *op_heur_change,
                               int sum_op_heur_change_to_cost,
                               pddl_err_t *err)
{
    ZEROIZE(trset);
    trset->vars = vars;

    op_t *ops = CALLOC_ARR(op_t, strips->op.op_size);
    opsInit(constr, strips, use_op_constr, ops,
            op_heur_change, sum_op_heur_change_to_cost, err);

    trset->trans_size = 0;
    trset->trans_alloc = 2;
    trset->trans = CALLOC_ARR(pddl_symbolic_trans_set_t, trset->trans_alloc);

    int op_ids_size = strips->op.op_size;
    int *op_ids = ALLOC_ARR(int, op_ids_size);
    int ins = 0;
    for (int op_id = 0; op_id < strips->op.op_size; ++op_id){
        if (!ops[op_id].is_dead)
            op_ids[ins++] = op_id;
    }
    op_ids_size = ins;
    pddlSort(op_ids, op_ids_size, sizeof(int), opIdCostCmp, (void *)ops);

    int start = 0, end = 1;
    for (end = 1; end < op_ids_size; ++end){
        pddl_cost_t cost_start = ops[op_ids[start]].cost;
        pddl_cost_t heur_change_start = ops[op_ids[start]].heur_change;
        pddl_cost_t cost_end = ops[op_ids[end]].cost;
        pddl_cost_t heur_change_end = ops[op_ids[end]].heur_change;
        if (pddlCostCmp(&cost_start, &cost_end) != 0
                || pddlCostCmp(&heur_change_start, &heur_change_end) != 0){
            ASSERT(end > start);
            int tr_id = add(trset);
            ASSERT(tr_id < trset->trans_size);
            transSetsAddRange(vars, constr, trset->trans + tr_id, ops,
                              op_ids + start, end - start,
                              use_op_constr, max_nodes, max_time, err);
            start = end;
        }
    }
    if (end > start && op_ids_size > 0){
        int tr_id = add(trset);
        transSetsAddRange(vars, constr, trset->trans + tr_id, ops,
                          op_ids + start, end - start,
                          use_op_constr, max_nodes, max_time, err);
    }

    FREE(op_ids);

    for (int op_id = 0; op_id < strips->op.op_size; ++op_id)
        opFree(ops + op_id);
    FREE(ops);
}

void pddlSymbolicTransSetsFree(pddl_symbolic_trans_sets_t *trset)
{
    for (int i = 0; i < trset->trans_size; ++i)
        transSetFree(trset->vars->mgr, trset->trans + i);
    if (trset->trans != NULL)
        FREE(trset->trans);
}


static pddl_bdd_t *transImage(pddl_bdd_manager_t *mgr,
                              pddl_symbolic_trans_t *tr,
                              pddl_bdd_t *state,
                              pddl_time_limit_t *time_limit)
{
    pddl_bdd_t *bdd1, *bdd;
    bdd1 = pddlBDDAndAbstractLimit(mgr, state, tr->bdd, tr->exist_pre, 0,
                                   time_limit);
    if (bdd1 == NULL)
        return NULL;
    bdd = pddlBDDSwapVars(mgr, bdd1, tr->var_pre, tr->var_eff, tr->var_size);
    pddlBDDDel(mgr, bdd1);
    return bdd;
}

static pddl_bdd_t *transPreImage(pddl_bdd_manager_t *mgr,
                                 pddl_symbolic_trans_t *tr,
                                 pddl_bdd_t *state,
                                 pddl_time_limit_t *time_limit)
{
    pddl_bdd_t *bdd1, *bdd;
    bdd1 = pddlBDDSwapVars(mgr, state, tr->var_eff, tr->var_pre, tr->var_size);
    bdd = pddlBDDAndAbstractLimit(mgr, bdd1, tr->bdd, tr->exist_eff, 0,
                                  time_limit);
    pddlBDDDel(mgr, bdd1);
    return bdd;
}

static pddl_bdd_t *transSetApply(pddl_bdd_manager_t *mgr,
                                 pddl_symbolic_trans_set_t *trset,
                                 pddl_bdd_t *state,
                                 pddl_time_limit_t *time_limit,
                                 pddl_bdd_t *(*f)(pddl_bdd_manager_t *,
                                                  pddl_symbolic_trans_t *,
                                                  pddl_bdd_t *,
                                                  pddl_time_limit_t *))
{
    if (trset->trans_size == 0)
        return NULL;

    pddl_bdd_t *bdd = f(mgr, trset->trans + 0, state, time_limit);
    if (bdd == NULL)
        return NULL;
    for (int i = 1; i < trset->trans_size; ++i){
        pddl_bdd_t *bdd2 = f(mgr, trset->trans + i, state, time_limit);
        if (bdd2 == NULL){
            pddlBDDDel(mgr, bdd);
            return NULL;
        }
        pddlBDDOrUpdate(mgr, &bdd, bdd2);
        pddlBDDDel(mgr, bdd2);
    }
    return bdd;
}

pddl_bdd_t *pddlSymbolicTransSetImage(pddl_symbolic_trans_set_t *trset,
                                      pddl_bdd_t *state,
                                      pddl_time_limit_t *time_limit)
{
    return transSetApply(trset->vars->mgr, trset, state, time_limit, transImage);
}

pddl_bdd_t *pddlSymbolicTransSetPreImage(pddl_symbolic_trans_set_t *trset,
                                         pddl_bdd_t *state,
                                         pddl_time_limit_t *time_limit)
{
    return transSetApply(trset->vars->mgr, trset, state, time_limit, transPreImage);
}
