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
#include "pddl/iarr.h"
#include "pddl/critical_path.h"
#include "pddl/strips.h"
#include "pddl/time_limit.h"

typedef int16_t fact_id_t;

struct set_range_pair {
    fact_id_t from;
    fact_id_t to;
} pddl_packed;
typedef struct set_range_pair set_range_pair_t;

struct set_range {
    set_range_pair_t *v;
    fact_id_t size;
    fact_id_t alloc;
} pddl_packed;
typedef struct set_range set_range_t;

_pddl_inline int setRangeIsSet(const set_range_t *s, int v)
{
    int left = 0, right = s->size - 1;
    int idx = (left + right) / 2;
    while (left <= right){
        if (s->v[idx].from <= v && v <= s->v[idx].to){
            return 1;
        }else if (v < s->v[idx].from){
            right = idx - 1;
        }else{ // f3 > mf->range[idx].to
            left = idx + 1;
        }

        idx = (left + right) / 2;
    }
    return 0;
}

_pddl_inline void setRangeAlloc(set_range_t *s)
{
    if (s->size == s->alloc){
        if (s->alloc == 0)
            s->alloc = 1;
        s->alloc *= 2;
        s->v = REALLOC_ARR(s->v, set_range_pair_t, s->alloc);
    }
}

_pddl_inline void setRangeSet(set_range_t *s, int v)
{
    for (int i = 0; i < s->size; ++i){
        if (s->v[i].from >= v && v >= s->v[i].to){
            return;

        }else if (s->v[i].from == v + 1){
            s->v[i].from = v;
            if (i > 0 && s->v[i - 1].to >= s->v[i].from - 1){
                s->v[i - 1].to = s->v[i].to;
                for (; i < s->size - 1; ++i)
                    s->v[i] = s->v[i + 1];
                --s->size;
            }
            return;

        }else if (s->v[i].to == v - 1){
            s->v[i].to = v;
            if (i < s->size - 1
                    && s->v[i].to >= s->v[i + 1].from - 1){
                s->v[i].to = s->v[i + 1].to;
                for (++i; i < s->size - 1; ++i)
                    s->v[i] = s->v[i + 1];
                --s->size;
            }
            return;

        }else if (s->v[i].from > v){
            setRangeAlloc(s);
            for (int j = s->size; j > i; --j)
                s->v[j] = s->v[j - 1];
            s->v[i].from = s->v[i].to = v;
            ++s->size;
            return;
        }
    }

    setRangeAlloc(s);
    s->v[s->size].from = s->v[s->size].to = v;
    ++s->size;
}

struct h3 {
    char *meta_fact1;
    char *meta_fact2;
    char *meta_fact3;
    set_range_t *meta_fact3_set;
    char *op_fact1;
    char *op_fact2;
    int *ext;
    int fact_size;
    int op_size;
    int *op_applied;
};
typedef struct h3 h3_t;

// Assumes f1 < f2 < f3
_pddl_inline int metaFactIsSet3(const h3_t *h3, int f1, int f2, int f3)
{
    if (h3->meta_fact3 != NULL){
        int idx = (f1 * h3->fact_size + f2) * h3->fact_size + f3;
        return h3->meta_fact3[idx];
    }else{
        set_range_t *s = h3->meta_fact3_set + f1 * h3->fact_size + f2;
        return setRangeIsSet(s, f3);
    }
}

// Assumes f1 <= f2
_pddl_inline int metaFactIsSet2(const h3_t *h3, int f1, int f2)
{
    return h3->meta_fact2[f1 * h3->fact_size + f2];
}

_pddl_inline int metaFactIsSet1(const h3_t *h3, int fid)
{
    return h3->meta_fact1[fid];
}

// Assumes f1 <= f2 <= f3
_pddl_inline void metaFactSet3(h3_t *h3, int f1, int f2, int f3)
{
    if (h3->meta_fact3 != NULL){
        int idx = (f1 * h3->fact_size + f2) * h3->fact_size + f3;
        h3->meta_fact3[idx] = 1;
    }else{
        set_range_t *s = h3->meta_fact3_set + f1 * h3->fact_size + f2;
        setRangeSet(s, f3);
    }
}

// Assumes f1 <= f2
_pddl_inline void metaFactSet2(h3_t *h3, int f1, int f2)
{
    h3->meta_fact2[f1 * h3->fact_size + f2] = 1;
    h3->meta_fact2[f2 * h3->fact_size + f1] = 1;
}

_pddl_inline void metaFactSet1(h3_t *h3, int fid)
{
    h3->meta_fact1[fid] = 1;
}

static void h3Init(h3_t *h3,
                   const pddl_strips_t *strips,
                   size_t excess_mem,
                   pddl_err_t *err)
{
    ZEROIZE(h3);
    h3->fact_size = strips->fact.fact_size;
    h3->op_size = strips->op.op_size;
    h3->meta_fact1 = CALLOC_ARR(char, h3->fact_size);
    h3->meta_fact2 = CALLOC_ARR(char, h3->fact_size * h3->fact_size);

    size_t needed_mem = h3->fact_size; // h3->meta_fact1
    needed_mem -= h3->fact_size * h3->fact_size; // h3->meta_fact2
    needed_mem -= sizeof(int) * h3->fact_size; // h3->ext
    needed_mem -= sizeof(int) * h3->op_size; // h3->op_applied
    size_t max_mem = 0;
    if (needed_mem < excess_mem)
        max_mem = excess_mem - needed_mem;
    size_t used_excess_mem = 0;

    size_t meta_fact3_size = h3->fact_size;
    meta_fact3_size *= h3->fact_size;
    meta_fact3_size *= h3->fact_size;
    if (meta_fact3_size <= max_mem){
        h3->meta_fact3 = CALLOC_ARR(char, meta_fact3_size);
        max_mem -= meta_fact3_size;
        used_excess_mem += meta_fact3_size;
    }else{
        h3->meta_fact3_set = CALLOC_ARR(set_range_t,
                                        h3->fact_size * h3->fact_size);
        if (max_mem >= h3->fact_size * h3->fact_size)
            max_mem -= h3->fact_size * h3->fact_size;
    }

    size_t op_fact1_size = h3->fact_size;
    op_fact1_size *= h3->op_size;
    if (op_fact1_size <= max_mem){
        h3->op_fact1 = CALLOC_ARR(char, op_fact1_size);
        for (int op_id = 0; op_id < strips->op.op_size; ++op_id){
            const pddl_strips_op_t *op = strips->op.op[op_id];
            char *dst = h3->op_fact1 + op->id * h3->fact_size;
            int f;
            PDDL_ISET_FOR_EACH(&op->add_eff, f)
                dst[f] = -1;
            PDDL_ISET_FOR_EACH(&op->del_eff, f)
                dst[f] = -1;
        }
        max_mem -= op_fact1_size;
        used_excess_mem += op_fact1_size;
    }

    size_t op_fact2_size = h3->fact_size;
    op_fact2_size *= h3->fact_size;
    op_fact2_size *= h3->op_size;
    if (op_fact2_size <= max_mem){
        h3->op_fact2 = CALLOC_ARR(char, op_fact2_size);
        max_mem -= op_fact2_size;
        used_excess_mem += op_fact2_size;
    }

    if (h3->meta_fact3 != NULL || h3->op_fact1 != NULL || h3->op_fact2 != NULL){
        PDDL_INFO(err, "uses additional memory of %.2f MB"
                  "(meta-fact3: %d, op-fact1: %d, op-fact2: %d",
                  used_excess_mem / (1024. * 1024.),
                  (h3->meta_fact3 != NULL ? 1 : 0),
                  (h3->op_fact1 != NULL ? 1 : 0),
                  (h3->op_fact2 != NULL ? 1 : 0));
    }

    h3->ext = ALLOC_ARR(int, h3->fact_size);
    h3->op_applied = CALLOC_ARR(int, h3->op_size);

    for (int i = 0; i < pddlISetSize(&strips->init); ++i){
        int f1 = pddlISetGet(&strips->init, i);
        metaFactSet1(h3, f1);
        for (int j = i + 1; j < pddlISetSize(&strips->init); ++j){
            int f2 = pddlISetGet(&strips->init, j);
            metaFactSet2(h3, f1, f2);
            for (int k = j + 1; k < pddlISetSize(&strips->init); ++k){
                int f3 = pddlISetGet(&strips->init, k);
                metaFactSet3(h3, f1, f2, f3);
            }
        }
    }
}

static void h3Free(h3_t *h3)
{
    if (h3->meta_fact1 != NULL)
        FREE(h3->meta_fact1);
    if (h3->meta_fact2 != NULL)
        FREE(h3->meta_fact2);
    if (h3->meta_fact3 != NULL)
        FREE(h3->meta_fact3);
    if (h3->meta_fact3_set != NULL){
        for (int i = 0; i < h3->fact_size; ++i){
            for (int j = i + 1; j < h3->fact_size; ++j){
                if (h3->meta_fact3_set[i * h3->fact_size + j].v != NULL)
                    FREE(h3->meta_fact3_set[i * h3->fact_size + j].v);
            }
        }
        FREE(h3->meta_fact3_set);
    }
    if (h3->op_fact1 != NULL)
        FREE(h3->op_fact1);
    if (h3->op_fact2 != NULL)
        FREE(h3->op_fact2);
    if (h3->ext != NULL)
        FREE(h3->ext);

    if (h3->op_applied != NULL)
        FREE(h3->op_applied);
}

static int testSet(const h3_t *h3, const pddl_iset_t *set)
{
    for (int i = 0; i < pddlISetSize(set); ++i){
        int f1 = pddlISetGet(set, i);
        if (!metaFactIsSet1(h3, f1))
            return 0;
        for (int j = i + 1; j < pddlISetSize(set); ++j){
            int f2 = pddlISetGet(set, j);
            if (!metaFactIsSet2(h3, f1, f2))
                return 0;
            for (int k = j + 1; k < pddlISetSize(set); ++k){
                int f3 = pddlISetGet(set, k);
                if (!metaFactIsSet3(h3, f1, f2, f3))
                    return 0;
            }
        }
    }

    return 1;
}

static int testSet2(const h3_t *h3, const pddl_iset_t *set, int f)
{
    if (pddlISetIn(f, set))
        return 1;

    for (int i = 0; i < pddlISetSize(set); ++i){
        int f1 = f;
        int f2 = pddlISetGet(set, i);
        if (f > f2){
            f1 = f2;
            f2 = f;
        }

        if (!metaFactIsSet2(h3, f1, f2))
            return 0;

        for (int j = i + 1; j < pddlISetSize(set); ++j){
            int t = pddlISetGet(set, j);
            int t1 = f1, t2 = f2, t3 = t;
            if (t < t2){
                t3 = t2;
                t2 = t;
            }
            if (t < t1){
                t2 = t1;
                t1 = t;
            }

            if (!metaFactIsSet3(h3, t1, t2, t3))
                return 0;
        }
    }

    return 1;
}

static int testSet3(const h3_t *h3, const pddl_iset_t *set, int f1, int f2)
{
    int f;

    if (pddlISetIn(f1, set) || pddlISetIn(f2, set))
        return 1;

    PDDL_ISET_FOR_EACH(set, f){
        if (f < f2){
            if (f < f1){
                if (!metaFactIsSet3(h3, f, f1, f2))
                    return 0;
            }else if (!metaFactIsSet3(h3, f1, f, f2)){
                return 0;
            }
        }else if (!metaFactIsSet3(h3, f1, f2, f)){
            return 0;
        }
    }

    return 1;
}

static int addSet(h3_t *h3, const pddl_iset_t *set)
{
    int updated = 0;

    for (int i = 0; i < pddlISetSize(set); ++i){
        int f1 = pddlISetGet(set, i);
        if (!metaFactIsSet1(h3, f1)){
            metaFactSet1(h3, f1);
            updated = 1;
        }
        for (int j = i + 1; j < pddlISetSize(set); ++j){
            int f2 = pddlISetGet(set, j);
            if (!metaFactIsSet2(h3, f1, f2)){
                metaFactSet2(h3, f1, f2);
                updated = 1;
            }
            for (int k = j + 1; k < pddlISetSize(set); ++k){
                int f3 = pddlISetGet(set, k);
                if (!metaFactIsSet3(h3, f1, f2, f3)){
                    metaFactSet3(h3, f1, f2, f3);
                    updated = 1;
                }
            }
        }
    }

    return updated;
}

static int addSet2(h3_t *h3, const pddl_iset_t *set, int f)
{
    int updated = 0;

    for (int i = 0; i < pddlISetSize(set); ++i){
        int f1 = f;
        int f2 = pddlISetGet(set, i);
        if (f2 < f){
            f1 = f2;
            f2 = f;
        }
        if (!metaFactIsSet2(h3, f1, f2)){
            metaFactSet2(h3, f1, f2);
            updated = 1;
        }
        for (int j = i + 1; j < pddlISetSize(set); ++j){
            int t = pddlISetGet(set, j);
            int t1 = f1, t2 = f2, t3 = t;
            if (t < t2){
                t3 = t2;
                t2 = t;
            }
            if (t < t1){
                t2 = t1;
                t1 = t;
            }
            if (!metaFactIsSet3(h3, t1, t2, t3)){
                metaFactSet3(h3, t1, t2, t3);
                updated = 1;
            }
        }
    }

    return updated;
}

static int addSet3(h3_t *h3, const pddl_iset_t *set, int f1, int f2)
{
    int f;
    int updated = 0;

    PDDL_ISET_FOR_EACH(set, f){
        if (f < f2){
            if (f < f1){
                if (!metaFactIsSet3(h3, f, f1, f2)){
                    metaFactSet3(h3, f, f1, f2);
                    updated = 1;
                }
            }else{
                if (!metaFactIsSet3(h3, f1, f, f2)){
                    metaFactSet3(h3, f1, f, f2);
                    updated = 1;
                }
            }
        }else{
            if (!metaFactIsSet3(h3, f1, f2, f)){
                metaFactSet3(h3, f1, f2, f);
                updated = 1;
            }
        }
    }

    return updated;
}

/** Returns true if operator is applicable with the currently reachable facts */
static int isApplicable(const pddl_strips_op_t *op, h3_t *h3)
{
    if (h3->op_applied[op->id])
        return 1;

    return testSet(h3, &op->pre);
}

/** Apply operator if currently applicable */
static int applyOp(const pddl_strips_op_t *op, h3_t *h3)
{
    int updated = 0;

    if (!isApplicable(op, h3))
        return 0;

    if (!h3->op_applied[op->id]){
        // This needs to be run only the first time the operator is
        // applied.
        updated = addSet(h3, &op->add_eff);
    }
    // This needs to be set here because isApplicable2 depends on it
    h3->op_applied[op->id] = 1;

    if (h3->op_fact1 != NULL){
        char *fact1 = h3->op_fact1 + op->id * h3->fact_size;
        for (int f1 = 0; f1 < h3->fact_size; ++f1){
            if (fact1[f1] || !metaFactIsSet1(h3, f1))
                continue;
            fact1[f1] = testSet2(h3, &op->pre, f1);
            if (fact1[f1])
                updated |= addSet2(h3, &op->add_eff, f1);
        }

        if (h3->op_fact2 != NULL){
            char *fact2 = h3->op_fact2;
            fact2 += (size_t)op->id
                        * (size_t)h3->fact_size
                        * (size_t)h3->fact_size;
            for (int f1 = 0; f1 < h3->fact_size; ++f1){
                if (fact1[f1] != 1)
                    continue;
                for (int f2 = f1 + 1; f2 < h3->fact_size; ++f2){
                    if (fact1[f2] != 1
                            || fact2[f1 * h3->fact_size + f2]
                            || !metaFactIsSet2(h3, f1, f2))
                        continue;
                    if (testSet3(h3, &op->pre, f1, f2)){
                        fact2[f1 * h3->fact_size + f2] = 1;
                        updated |= addSet3(h3, &op->add_eff, f1, f2);
                    }
                }
            }

        }else{
            for (int f1 = 0; f1 < h3->fact_size; ++f1){
                if (fact1[f1] != 1)
                    continue;
                for (int f2 = f1 + 1; f2 < h3->fact_size; ++f2){
                    if (fact1[f2] != 1
                            || !metaFactIsSet2(h3, f1, f2)
                            || !testSet3(h3, &op->pre, f1, f2))
                        continue;
                    updated |= addSet3(h3, &op->add_eff, f1, f2);
                }
            }
        }

    }else{
        ZEROIZE_ARR(h3->ext, h3->fact_size);
        for (int f1 = 0; f1 < h3->fact_size; ++f1){
            if (pddlISetIn(f1, &op->add_eff)
                    || pddlISetIn(f1, &op->del_eff)
                    || !metaFactIsSet1(h3, f1)
                    || !testSet2(h3, &op->pre, f1))
                continue;
            updated |= addSet2(h3, &op->add_eff, f1);
            h3->ext[f1] = 1;
        }

        for (int f1 = 0; f1 < h3->fact_size; ++f1){
            if (!h3->ext[f1])
                continue;
            for (int f2 = f1 + 1; f2 < h3->fact_size; ++f2){
                if (!h3->ext[f2]
                        || !metaFactIsSet2(h3, f1, f2)
                        || !testSet3(h3, &op->pre, f1, f2))
                    continue;
                updated |= addSet3(h3, &op->add_eff, f1, f2);
            }
        }
    }

    return updated;
}

int pddlH3(const pddl_strips_t *strips,
           pddl_mutex_pairs_t *ms,
           pddl_iset_t *unreachable_facts,
           pddl_iset_t *unreachable_ops,
           float time_limit_s,
           size_t excess_memory,
           pddl_err_t *err)
{
    if (strips->has_cond_eff)
        PDDL_ERR_RET(err, -1, "h^3: Conditional effects not supported!");

    pddl_time_limit_t time_limit;
    h3_t h3;
    int updated, ret = 0;

    CTX(err, "h3fw", "h^3 fw");
    PDDL_INFO(err, "facts: %d, ops: %d, mutex pairs: %lu,"
              " time-limit: %.2f, excess-memory: %lu",
              strips->fact.fact_size,
              strips->op.op_size,
              (unsigned long)ms->num_mutex_pairs,
              time_limit_s,
              (unsigned long)excess_memory);

    pddlTimeLimitSet(&time_limit, time_limit_s);
    h3Init(&h3, strips, excess_memory, err);

    do {
        if (pddlTimeLimitCheck(&time_limit) != 0){
            ret = -2;
            goto mutex_h3_end;
        }

        updated = 0;
        for (int op_id = 0; op_id < strips->op.op_size; ++op_id){
            const pddl_strips_op_t *op = strips->op.op[op_id];
            updated |= applyOp(op, &h3);
        }
    } while (updated);

    for (int f1 = 0; f1 < h3.fact_size; ++f1){
        if (!metaFactIsSet1(&h3, f1)){
            pddlMutexPairsAdd(ms, f1, f1);
            if (unreachable_facts != NULL)
                pddlISetAdd(unreachable_facts, f1);
            continue;
        }

        for (int f2 = f1 + 1; f2 < h3.fact_size; ++f2){
            if (!metaFactIsSet2(&h3, f1, f2))
                pddlMutexPairsAdd(ms, f1, f2);
        }
    }

    for (int op_id = 0;
            unreachable_ops != NULL && op_id < strips->op.op_size; ++op_id){
        if (!h3.op_applied[op_id])
            pddlISetAdd(unreachable_ops, op_id);
    }

mutex_h3_end:
    h3Free(&h3);

    PDDL_INFO(err, "DONE. mutex pairs: %lu, unreachable facts: %d,"
              " unreachable ops: %d, time-limit reached: %d",
              (unsigned long)ms->num_mutex_pairs,
              (unreachable_facts != NULL ? pddlISetSize(unreachable_facts) : -1),
              (unreachable_ops != NULL ? pddlISetSize(unreachable_ops) : -1),
              (ret == -2 ? 1 : 0));
    CTXEND(err);

    return ret;
}
