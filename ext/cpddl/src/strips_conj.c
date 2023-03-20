/***
 * Copyright (c)2022 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

#include "internal.h"
#include "pddl/strips_conj.h"

void pddlStripsConjConfigInit(pddl_strips_conj_config_t *cfg)
{
    ZEROIZE(cfg);
    pddlSetISetInit(&cfg->conj);
}

void pddlStripsConjConfigFree(pddl_strips_conj_config_t *cfg)
{
    pddlSetISetFree(&cfg->conj);
}

void pddlStripsConjConfigAddConj(pddl_strips_conj_config_t *cfg,
                                 const pddl_iset_t *conj)
{
    if (pddlISetSize(conj) > 1)
        pddlSetISetAdd(&cfg->conj, conj);
}


static void setToMetaSet(const pddl_iset_t *set,
                         const pddl_iset_t *fact_to_conj,
                         int num_singletons,
                         int num_meta_facts,
                         pddl_iset_t *meta_set)
{
    // Clear the output set
    pddlISetEmpty(meta_set);
    // Copy singletons
    pddlISetUnion(meta_set, set);
    // Check non-singletons
    for (int i = num_singletons; i < num_meta_facts; ++i){
        if (pddlISetIsSubset(fact_to_conj + i, set))
            pddlISetAdd(meta_set, i);
    }
}

static char *metaFactName(const pddl_facts_t *facts,
                          const pddl_iset_t *fact_set)
{
    ASSERT(pddlISetSize(fact_set) > 1);
    int slen = pddlISetSize(fact_set) - 1;
    int fact_id;
    PDDL_ISET_FOR_EACH(fact_set, fact_id)
        slen += strlen(facts->fact[fact_id]->name);

    char *name = ZALLOC_ARR(char, slen + 1);
    char *dst = name;
    ssize_t inc;
    inc = sprintf(dst, "%s", facts->fact[pddlISetGet(fact_set, 0)]->name);
    dst += inc;
    for (int i = 1; i < pddlISetSize(fact_set); ++i){
        inc = sprintf(dst, "+%s", facts->fact[pddlISetGet(fact_set, i)]->name);
        dst += inc;
    }

    return name;
}

static void genAllDownwardClosedSubsets(const pddl_iset_t *C_pottrue,
                                        const pddl_set_iset_t *downward_closed,
                                        const pddl_iset_t *start_set,
                                        int start_idx,
                                        pddl_set_iset_t *subsets)
{
    int size = pddlISetSize(C_pottrue);
    for (int i = start_idx; i < size; ++i){
        int si = pddlISetGet(C_pottrue, i);
        PDDL_ISET(set);
        pddlISetUnion(&set, start_set);
        pddlISetUnion(&set, pddlSetISetGet(downward_closed, si));
        pddlSetISetAdd(subsets, &set);
        if (i + 1 < size){
            genAllDownwardClosedSubsets(C_pottrue, downward_closed, &set,
                                        i + 1, subsets);
        }
        pddlISetFree(&set);
    }
}

static void addFullOpX(pddl_strips_conj_t *task,
                       const pddl_strips_t *in_task,
                       const pddl_strips_op_t *in_op,
                       const pddl_iset_t *C_true,
                       const pddl_iset_t *C_false,
                       const pddl_iset_t *X,
                       const pddl_mutex_pairs_t *mutex,
                       pddl_err_t *err)
{
    pddl_strips_op_t op;
    pddlStripsOpInit(&op);
    pddlStripsOpCopy(&op, in_op);

    PDDL_ISET(pre_base);
    int fact_id;
    PDDL_ISET_FOR_EACH(X, fact_id)
        pddlISetUnion(&pre_base, task->fact_to_conj + fact_id);
    pddlISetMinus(&pre_base, &in_op->add_eff);
    pddlISetUnion(&pre_base, &in_op->pre);

    PDDL_ISET_FOR_EACH(X, fact_id){
        if (pddlISetIsSubset(task->fact_to_conj + fact_id, &pre_base))
            pddlISetAdd(&op.pre, fact_id);
    }
    pddlISetUnion(&op.pre, &pre_base);
    pddlISetFree(&pre_base);

    if (mutex != NULL && pddlMutexPairsIsMutexSet(mutex, &op.pre)){
        pddlStripsOpFree(&op);
        return;
    }

    pddlISetUnion(&op.del_eff, C_false);

    pddlISetUnion(&op.add_eff, C_true);
    pddlISetUnion(&op.add_eff, X);

    pddlStripsOpNormalize(&op);
    if (pddlISetSize(&op.add_eff) > 0 || pddlISetSize(&op.del_eff) > 0)
        pddlStripsOpsAdd(&task->strips.op, &op);
    pddlStripsOpFree(&op);
}

static void addFullOp(pddl_strips_conj_t *task,
                      const pddl_strips_t *in_task,
                      const pddl_strips_op_t *op,
                      const pddl_set_iset_t *downward_closed,
                      const pddl_mutex_pairs_t *mutex,
                      pddl_err_t *err)
{
    PDDL_ISET(C_false); // Conjunctions made false
    PDDL_ISET(C_true); // Conjunctions made true
    PDDL_ISET(C_pottrue); // Conjunctions potentially made true

    // Construct regression (pre \setminus del) \cup add
    PDDL_ISET(regr);
    pddlISetUnion(&regr, &op->pre);
    pddlISetMinus(&regr, &op->del_eff);
    pddlISetUnion(&regr, &op->add_eff);

    // Construct C_false, C_true, C_pottrue
    int num_meta_facts = task->strips.fact.fact_size;
    for (int fi = task->num_singletons; fi < num_meta_facts; ++fi){
        const pddl_iset_t *facts = task->fact_to_conj + fi;
        int is_regr_subset = pddlISetIsSubset(facts, &regr);
        int is_del_intersect = pddlISetIntersectionSizeAtLeast(facts, &op->del_eff, 1);
        int is_add_intersect = pddlISetIntersectionSizeAtLeast(facts, &op->add_eff, 1);
        if (is_regr_subset && is_add_intersect)
            pddlISetAdd(&C_true, fi);
        if (is_del_intersect)
            pddlISetAdd(&C_false, fi);
        if (!is_regr_subset && !is_del_intersect && is_add_intersect)
            pddlISetAdd(&C_pottrue, fi);
    }

    PDDL_ISET(empty);
    pddl_set_iset_t C_pottrue_subsets;
    pddlSetISetInit(&C_pottrue_subsets);
    genAllDownwardClosedSubsets(&C_pottrue, downward_closed, &empty, 0,
                                &C_pottrue_subsets);

    addFullOpX(task, in_task, op, &C_true, &C_false, &empty, mutex, err);

    int num_subsets = pddlSetISetSize(&C_pottrue_subsets);
    for (int si = 0; si < num_subsets; ++si){
        const pddl_iset_t *X = pddlSetISetGet(&C_pottrue_subsets, si);
        addFullOpX(task, in_task, op, &C_true, &C_false, X, mutex, err);
    }
    pddlSetISetFree(&C_pottrue_subsets);
    pddlISetFree(&empty);

    pddlISetFree(&regr);
    pddlISetFree(&C_false);
    pddlISetFree(&C_true);
    pddlISetFree(&C_pottrue);
}

static void addFullOps(pddl_strips_conj_t *task,
                       const pddl_strips_t *in_task,
                       const pddl_mutex_pairs_t *mutex,
                       pddl_err_t *err)
{
    // Prepare downward closed sets
    pddl_set_iset_t downward_closed;
    pddlSetISetInit(&downward_closed);
    for (int fi = 0; fi < task->num_singletons; ++fi){
        PDDL_ISET(set);
        pddlISetAdd(&set, fi);
        pddlSetISetAdd(&downward_closed, &set);
        pddlISetFree(&set);
    }

    int num_meta_facts = task->strips.fact.fact_size;
    for (int fi = task->num_singletons; fi < num_meta_facts; ++fi){
        const pddl_iset_t *facts = task->fact_to_conj + fi;

        PDDL_ISET(set);
        pddlISetAdd(&set, fi);

        for (int fi2 = task->num_singletons; fi2 < num_meta_facts; ++fi2){
            if (fi == fi2)
                continue;
            const pddl_iset_t *facts2 = task->fact_to_conj + fi2;
            if (pddlISetIsSubset(facts2, facts))
                pddlISetAdd(&set, fi2);
        }

        pddlSetISetAdd(&downward_closed, &set);
        pddlISetFree(&set);
    }

    for (int opi = 0; opi < in_task->op.op_size; ++opi){
        addFullOp(task, in_task, in_task->op.op[opi], &downward_closed,
                  mutex, err);
    }
    pddlSetISetFree(&downward_closed);
}

void pddlStripsConjInit(pddl_strips_conj_t *task,
                        const pddl_strips_t *in_task,
                        const pddl_strips_conj_config_t *cfg,
                        pddl_err_t *err)
{
    PANIC_IF(in_task->has_cond_eff, "Conditional effects are not supported yet.");

    ZEROIZE(task);
    task->num_singletons = in_task->fact.fact_size;

    // Create mapping between facts and meta-facts
    int num_meta_facts = task->num_singletons + pddlSetISetSize(&cfg->conj);
    task->fact_to_conj = ZALLOC_ARR(pddl_iset_t, num_meta_facts);
    pddlSetISetInit(&task->conj_to_fact);
    for (int fi = 0; fi < in_task->fact.fact_size; ++fi){
        pddlISetAdd(task->fact_to_conj + fi, fi);
        int id = pddlSetISetAdd(&task->conj_to_fact, task->fact_to_conj + fi);
        PANIC_IF(id != fi, "Invalid ID of a singleton");
    }
    for (int i = 0; i < pddlSetISetSize(&cfg->conj); ++i){
        int fi = i + task->num_singletons;
        pddlISetUnion(task->fact_to_conj + fi, pddlSetISetGet(&cfg->conj, i));
        int id = pddlSetISetAdd(&task->conj_to_fact, task->fact_to_conj + fi);
        PANIC_IF(id != fi, "Invalid ID of a conjuction");
    }

    // Init empty strips task
    pddlStripsInit(&task->strips);

    // Create a set of meta-facts
    pddlFactsCopy(&task->strips.fact, &in_task->fact);
    for (int fi = task->num_singletons; fi < num_meta_facts; ++fi){
        pddl_fact_t fact;
        pddlFactInit(&fact);
        fact.name = metaFactName(&in_task->fact, task->fact_to_conj + fi);
        int id = pddlFactsAdd(&task->strips.fact, &fact);
        PANIC_IF(id != fi, "Invalid fact-ID of a conjunction");
        pddlFactFree(&fact);
    }

    // Initial state
    setToMetaSet(&in_task->init, task->fact_to_conj, task->num_singletons,
                 task->strips.fact.fact_size, &task->strips.init);

    // Goal condition
    setToMetaSet(&in_task->goal, task->fact_to_conj, task->num_singletons,
                 task->strips.fact.fact_size, &task->strips.goal);

    addFullOps(task, in_task, cfg->mutex, err);
}

void pddlStripsConjFree(pddl_strips_conj_t *task)
{
    if (task->fact_to_conj != NULL){
        for (int fi = 0; fi < task->strips.fact.fact_size; ++fi)
            pddlISetFree(task->fact_to_conj + fi);
        FREE(task->fact_to_conj);
    }
    pddlSetISetFree(&task->conj_to_fact);
    pddlStripsFree(&task->strips);
}

void pddlStripsConjMutexPairsInitCopy(pddl_mutex_pairs_t *mutex,
                                      const pddl_mutex_pairs_t *in_mutex,
                                      const pddl_strips_conj_t *task)
{
    pddlMutexPairsInitStrips(mutex, &task->strips);
    PDDL_MUTEX_PAIRS_FOR_EACH(in_mutex, f1, f2)
        pddlMutexPairsAdd(mutex, f1, f2);

    for (int fi = task->num_singletons; fi < task->strips.fact.fact_size; ++fi){
        const pddl_iset_t *conj = task->fact_to_conj + fi;
        int fact_id;
        PDDL_ISET_FOR_EACH(conj, fact_id){
            for (int fi2 = 0; fi2 < in_mutex->fact_size; ++fi2){
                if (pddlMutexPairsIsMutex(in_mutex, fact_id, fi2))
                    pddlMutexPairsAdd(mutex, fact_id, fi2);
            }
        }
    }
}
