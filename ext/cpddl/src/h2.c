/***
 * cpddl
 * -------
 * Copyright (c)2017 Daniel Fiser <danfis@danfis.cz>,
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
#include "pddl/timer.h"
#include "pddl/strips.h"
#include "pddl/critical_path.h"
#include "pddl/disambiguation.h"
#include "pddl/time_limit.h"

#define REACHED 1
#define FW_MUTEX 2
#define BW_MUTEX 3
#define IS_MUTEX(X) ((X) > 1)
#define PRUNED -1
#define _FACT(h2, x, y) ((h2)->fact[(size_t)(x) * (h2)->fact_size + (y)])

struct h2 {
    char *fact; /*!< 0/REACHED/MUTEX for each pair of facts */
    int fact_size;
    int op_size;
    char *op; /*!< 0/REACHED/PRUNED for each operator */
    char *op_fact;
    pddl_disambiguate_t *disambiguate;
};
typedef struct h2 h2_t;

_pddl_inline int setReached(h2_t *h2, int f1, int f2)
{
    if (_FACT(h2, f1, f2) == 0){
        _FACT(h2, f1, f2) = _FACT(h2, f2, f1) = REACHED;
        return 1;
    }
    return 0;
}

_pddl_inline void setMutex(h2_t *h2, int f1, int f2, int is_bw)
{
    if (is_bw){
        _FACT(h2, f1, f2) = _FACT(h2, f2, f1) = BW_MUTEX;
    }else{
        _FACT(h2, f1, f2) = _FACT(h2, f2, f1) = FW_MUTEX;
    }
}

_pddl_inline void reset(h2_t *h2, int f1, int f2)
{
    _FACT(h2, f1, f2) = _FACT(h2, f2, f1) = 0;
}

_pddl_inline int isNotReached(const h2_t *h2, int f1, int f2)
{
    return _FACT(h2, f1, f2) == 0;
}

_pddl_inline int isReached(const h2_t *h2, int f1, int f2)
{
    return _FACT(h2, f1, f2) == REACHED;
}

_pddl_inline int isMutex(const h2_t *h2, int f1, int f2)
{
    return IS_MUTEX(_FACT(h2, f1, f2));
}

_pddl_inline int isFwMutex(const h2_t *h2, int f1, int f2)
{
    return _FACT(h2, f1, f2) == FW_MUTEX;
}

_pddl_inline int isBwMutex(const h2_t *h2, int f1, int f2)
{
    return _FACT(h2, f1, f2) == BW_MUTEX;
}


_pddl_inline void setOpReached(h2_t *h2, int op_id)
{
    ASSERT(h2->op[op_id] == 0);
    h2->op[op_id] = REACHED;
}

_pddl_inline void setOpPruned(h2_t *h2, int op_id)
{
    h2->op[op_id] = PRUNED;
}

_pddl_inline void resetOp(h2_t *h2, int op_id)
{
    h2->op[op_id] = 0;
}

_pddl_inline int isOpNotReached(const h2_t *h2, int op_id)
{
    return h2->op[op_id] == 0;
}

_pddl_inline int isOpReached(const h2_t *h2, int op_id)
{
    return h2->op[op_id] == REACHED;
}

_pddl_inline int isOpPruned(const h2_t *h2, int op_id)
{
    return h2->op[op_id] == PRUNED;
}

static void setFwInit(h2_t *h2, const pddl_iset_t *init)
{
    int f1, f2;
    PDDL_ISET_FOR_EACH(init, f1){
        PDDL_ISET_FOR_EACH(init, f2){
            setReached(h2, f1, f2);
        }
    }
}

static int isMutexWith(const h2_t *h2, int fact_id, const pddl_iset_t *set)
{
    int fact_id2;
    PDDL_ISET_FOR_EACH(set, fact_id2){
        if (isMutex(h2, fact_id, fact_id2))
            return 1;
    }
    return 0;
}

static void h2Init(h2_t *h2,
                   const pddl_strips_t *strips,
                   const pddl_mutex_pairs_t *mutexes,
                   const pddl_iset_t *unreachable_facts,
                   const pddl_iset_t *unreachable_ops,
                   pddl_err_t *err)
{
    ZEROIZE(h2);
    h2->fact_size = strips->fact.fact_size;
    h2->op_size = strips->op.op_size;
    h2->fact = CALLOC_ARR(char, (size_t)h2->fact_size * h2->fact_size);
    h2->op = CALLOC_ARR(char, h2->op_size);

    // Copy mutexes into the table
    PDDL_MUTEX_PAIRS_FOR_EACH(mutexes, f1, f2){
        if (pddlMutexPairsIsBwMutex(mutexes, f1, f2)){
            setMutex(h2, f1, f2, 1);
        }else{
            setMutex(h2, f1, f2, 0);
        }
    }

    if (unreachable_facts != NULL){
        int fact_id;
        PDDL_ISET_FOR_EACH(unreachable_facts, fact_id){
            if (!isMutex(h2, fact_id, fact_id))
                setMutex(h2, fact_id, fact_id, 0);
        }
    }

    if (unreachable_ops != NULL){
        int op_id;
        PDDL_ISET_FOR_EACH(unreachable_ops, op_id){
            if (!isOpPruned(h2, op_id))
                setOpPruned(h2, op_id);
        }
    }
}

static void h2AllocOpFact(h2_t *h2, pddl_err_t *err)
{
    size_t op_fact_size = (size_t)h2->fact_size * h2->op_size;
    h2->op_fact = calloc(op_fact_size, 1);
    if (h2->op_fact != NULL){
        PDDL_INFO(err, "uses additional memory of %.2f MB",
                  op_fact_size / (1024. * 1024.));
    }
}

static void h2InitOpFact(h2_t *h2, const pddl_strips_ops_t *ops, pddl_err_t *err)
{
    h2AllocOpFact(h2, err);
    if (h2->op_fact != NULL){
        for (int op_id = 0; op_id < h2->op_size; ++op_id){
            const pddl_strips_op_t *op = ops->op[op_id];
            char *fact = h2->op_fact + (size_t)op_id * h2->fact_size;
            int fact_id;
            PDDL_ISET_FOR_EACH(&op->add_eff, fact_id)
                fact[fact_id] = -1;
            PDDL_ISET_FOR_EACH(&op->del_eff, fact_id)
                fact[fact_id] = -1;
        }
    }
}

static void h2ResetOpFact(h2_t *h2, const pddl_strips_ops_t *ops)
{
    if (h2->op_fact == NULL)
        return;
    ZEROIZE_ARR(h2->op_fact, (size_t)h2->fact_size * h2->op_size);
    for (int op_id = 0; op_id < h2->op_size; ++op_id){
        const pddl_strips_op_t *op = ops->op[op_id];
        char *fact = h2->op_fact + (size_t)op_id * h2->fact_size;
        int fact_id;
        PDDL_ISET_FOR_EACH(&op->add_eff, fact_id)
            fact[fact_id] = -1;
        PDDL_ISET_FOR_EACH(&op->del_eff, fact_id)
            fact[fact_id] = -1;
    }
}

static void h2Free(h2_t *h2)
{
    if (h2->fact != NULL)
        FREE(h2->fact);
    if (h2->op != NULL)
        FREE(h2->op);
    if (h2->op_fact != NULL)
        free(h2->op_fact);
}

/** Returns true if operator is applicable with the currently reachable facts */
static int isApplicable(const pddl_strips_op_t *op, h2_t *h2)
{
    int f1, f2;

    if (isOpPruned(h2, op->id))
        return 0;

    if (isOpReached(h2, op->id))
        return 1;

    PDDL_ISET_FOR_EACH(&op->pre, f1){
        PDDL_ISET_FOR_EACH(&op->pre, f2){
            if (!isReached(h2, f1, f2))
                return 0;
        }
    }

    return 1;
}

/** Returns true if operator is applicable with the additional fact_id */
static int isApplicable2(const pddl_strips_op_t *op, int fact_id, h2_t *h2)
{
    int f1;

    if (!isOpReached(h2, op->id))
        return 0;
    if (!isReached(h2, fact_id, fact_id))
        return 0;
    if (h2->op_fact == NULL
            && (pddlISetHas(&op->add_eff, fact_id)
                    || pddlISetHas(&op->del_eff, fact_id))){
        return 0;
    }

    PDDL_ISET_FOR_EACH(&op->pre, f1){
        if (!isReached(h2, f1, fact_id))
            return 0;
    }

    return 1;
}

/** Apply operator if currently applicable */
static int applyOp(const pddl_strips_op_t *op, h2_t *h2)
{
    int f1, f2;
    int updated = 0;
    char *op_fact = NULL;

    if (!isApplicable(op, h2))
        return 0;

    if (!isOpReached(h2, op->id)){
        // This needs to be run only the first time the operator is
        // applied.
        PDDL_ISET_FOR_EACH(&op->add_eff, f1){
            PDDL_ISET_FOR_EACH(&op->add_eff, f2){
                updated |= setReached(h2, f1, f2);
            }
        }
        // This needs to be set here because isApplicable2 depends on it
        setOpReached(h2, op->id);
    }

    for (int fact_id = 0; fact_id < h2->fact_size; ++fact_id){
        if (h2->op_fact != NULL)
            op_fact = h2->op_fact + (size_t)op->id * h2->fact_size;
        if (op_fact != NULL && op_fact[fact_id])
            continue;
        if (isApplicable2(op, fact_id, h2)){
            if (op_fact != NULL)
                op_fact[fact_id] = 1;
            PDDL_ISET_FOR_EACH(&op->add_eff, f1)
                updated |= setReached(h2, f1, fact_id);
        }
    }

    return updated;
}

static int h2Run(h2_t *h2,
                 const pddl_strips_ops_t *ops,
                 pddl_time_limit_t *time_limit,
                 int is_bw,
                 pddl_err_t *err)
{
    int updated;
    int ret = 0;

    do {
        if (pddlTimeLimitCheck(time_limit) != 0)
            return -2;

        updated = 0;
        for (int op_id = 0; op_id < ops->op_size; ++op_id){
            const pddl_strips_op_t *op = ops->op[op_id];
            updated |= applyOp(op, h2);
        }
    } while (updated);

    for (int f1 = 0; f1 < h2->fact_size; ++f1){
        for (int f2 = f1; f2 < h2->fact_size; ++f2){
            if (isNotReached(h2, f1, f2)){
                if (!isMutex(h2, f1, f2)){
                    setMutex(h2, f1, f2, is_bw);
                    if (h2->disambiguate != NULL)
                        pddlDisambiguateAddMutex(h2->disambiguate, f1, f2);
                    ret = 1;
                }
            }else if (isReached(h2, f1, f2)){
                reset(h2, f1, f2);
            }
        }
    }

    for (int op_id = 0; op_id < h2->op_size; ++op_id){
        if (isOpNotReached(h2, op_id)){
            if (!isOpPruned(h2, op_id)){
                setOpPruned(h2, op_id);
                ret = 1;
            }
        }else if (isOpReached(h2, op_id)){
            resetOp(h2, op_id);
        }
    }

    return ret;
}

static void outUnreachableOps(const h2_t *h2, pddl_iset_t *unreachable_ops)
{
    for (int op_id = 0; op_id < h2->op_size; ++op_id){
        if (isOpPruned(h2, op_id))
            pddlISetAdd(unreachable_ops, op_id);
    }
}

static void outMutexes(const h2_t *h2,
                       pddl_mutex_pairs_t *mutexes,
                       pddl_iset_t *unreachable_facts)
{
    for (int f1 = 0; f1 < h2->fact_size; ++f1){
        for (int f2 = f1; f2 < h2->fact_size; ++f2){
            if (isMutex(h2, f1, f2)){
                pddlMutexPairsAdd(mutexes, f1, f2);
                if (isFwMutex(h2, f1, f2)){
                    pddlMutexPairsSetFwMutex(mutexes, f1, f2);
                }else if (isBwMutex(h2, f1, f2)){
                    pddlMutexPairsSetBwMutex(mutexes, f1, f2);
                }
                if (f1 == f2 && unreachable_facts != NULL)
                    pddlISetAdd(unreachable_facts, f1);
            }
        }
    }
}

static void setOutput(const h2_t *h2,
                      pddl_mutex_pairs_t *mutexes,
                      pddl_iset_t *unreachable_facts,
                      pddl_iset_t *unreachable_ops)
{
    outMutexes(h2, mutexes, unreachable_facts);
    if (unreachable_ops != NULL)
        outUnreachableOps(h2, unreachable_ops);
}

static int h2StateFw(const pddl_strips_t *strips,
                     const pddl_iset_t *init_state,
                     pddl_mutex_pairs_t *m,
                     pddl_iset_t *unreachable_facts,
                     pddl_iset_t *unreachable_ops,
                     float time_limit_s,
                     pddl_err_t *err)
{
    if (strips->has_cond_eff)
        PDDL_ERR_RET(err, -1, "h^2: Conditional effects not supported!");

    PDDL_INFO(err, "facts: %d, ops: %d, mutex pairs: %lu, time-limit: %.2fs",
              strips->fact.fact_size,
              strips->op.op_size,
              (unsigned long)m->num_mutex_pairs,
              time_limit_s);

    pddl_time_limit_t time_limit;
    h2_t h2;

    pddlTimeLimitSet(&time_limit, time_limit_s);
    h2Init(&h2, strips, m, unreachable_facts, unreachable_ops, err);
    h2InitOpFact(&h2, &strips->op, err);

    setFwInit(&h2, init_state);
    int ret = h2Run(&h2, &strips->op, &time_limit, 0, err);
    if (ret >= 0)
        ret = 0;

    setOutput(&h2, m, unreachable_facts, unreachable_ops);

    PDDL_INFO(err, "DONE. mutex pairs: %lu, unreachable facts: %d,"
              " unreachable ops: %d, time-limit reached: %d",
              (unsigned long)m->num_mutex_pairs,
              (unreachable_facts != NULL ? pddlISetSize(unreachable_facts) : -1),
              (unreachable_ops != NULL ? pddlISetSize(unreachable_ops) : -1),
              (ret == -2 ? 1 : 0));

    h2Free(&h2);
    return ret;
}

int pddlH2(const pddl_strips_t *strips,
           pddl_mutex_pairs_t *m,
           pddl_iset_t *unreachable_facts,
           pddl_iset_t *unreachable_ops,
           float time_limit_in_s,
           pddl_err_t *err)
{
    CTX(err, "h2fw", "h^2 fw");
    int ret = h2StateFw(strips, &strips->init, m, unreachable_facts,
                        unreachable_ops, time_limit_in_s, err);
    CTXEND(err);
    return ret;
}

int pddlH2IsDeadEnd(const pddl_strips_t *strips, const pddl_iset_t *state)
{
    pddl_mutex_pairs_t mutex;
    pddlMutexPairsInitStrips(&mutex, strips);
    h2StateFw(strips, state, &mutex, NULL, NULL, 0, NULL);
    int is_dead = 0;
    for (int i = 0; i < pddlISetSize(&strips->goal) && !is_dead; ++i){
        int f1 = pddlISetGet(&strips->goal, i);
        if (pddlMutexPairsIsMutex(&mutex, f1, f1)){
            is_dead = 1;
            break;
        }
        for (int j = i + 1; j < pddlISetSize(&strips->goal) && !is_dead; ++j){
            int f2 = pddlISetGet(&strips->goal, j);
            if (pddlMutexPairsIsMutex(&mutex, f1, f2)){
                is_dead = 1;
                break;
            }
        }
    }
    pddlMutexPairsFree(&mutex);
    return is_dead;
}

static void setBwInit(h2_t *h2, const pddl_iset_t *goal_in)
{
    PDDL_ISET(goal);
    pddlISetUnion(&goal, goal_in);

    if (h2->disambiguate != NULL)
        pddlDisambiguateSet(h2->disambiguate, &goal);

    for (int fact_id = 0; fact_id < h2->fact_size; ++fact_id){
        if (isMutex(h2, fact_id, fact_id) || isMutexWith(h2, fact_id, &goal))
            continue;

        setReached(h2, fact_id, fact_id);
    }

    for (int fact_id = 0; fact_id < h2->fact_size; ++fact_id){
        if (!isReached(h2, fact_id, fact_id))
            continue;

        for (int fact_id2 = fact_id + 1; fact_id2 < h2->fact_size; ++fact_id2){
            if (!isReached(h2, fact_id2, fact_id2)
                    || isMutex(h2, fact_id, fact_id2)){
                continue;
            }

            setReached(h2, fact_id, fact_id2);
        }
    }

    pddlISetFree(&goal);
}

static void opSetEDeletes(pddl_strips_op_t *bw_op,
                          const pddl_strips_op_t *fw_op,
                          const h2_t *h2)
{
    // Set e-deletes -- fw_op->pre contains prevails and delete effects.
    // We can't iterate over fw_op->del_eff \setminus sop->pre!
    int pre_fact;
    PDDL_ISET_FOR_EACH(&fw_op->pre, pre_fact){
        for (int fact_id = 0; fact_id < h2->fact_size; ++fact_id){
            if (isMutex(h2, pre_fact, fact_id))
                pddlISetAdd(&bw_op->del_eff, fact_id);
        }
    }
    pddlStripsOpNormalize(bw_op);
}

static void opInitBw(pddl_strips_op_t *bw_op,
                     const pddl_strips_op_t *fw_op,
                     const h2_t *h2)
{
    // Erase bw operator
    pddlISetEmpty(&bw_op->pre);
    pddlISetEmpty(&bw_op->add_eff);
    pddlISetEmpty(&bw_op->del_eff);

    // Set precondition as prevail + add effect from sop
    pddlISetMinus2(&bw_op->pre, &fw_op->pre, &fw_op->del_eff);
    pddlISetUnion(&bw_op->pre, &fw_op->add_eff);

    // Set add effects as fw_op's delete effects
    pddlISetSet(&bw_op->add_eff, &fw_op->del_eff);

    opSetEDeletes(bw_op, fw_op, h2);
}

static void opsInitBw(pddl_strips_ops_t *bw_ops,
                      const pddl_strips_ops_t *fw_ops,
                      const h2_t *h2)
{
    pddlStripsOpsInit(bw_ops);
    pddlStripsOpsCopy(bw_ops, fw_ops);
    for (int op_id = 0; op_id < h2->op_size; ++op_id)
        opInitBw(bw_ops->op[op_id], fw_ops->op[op_id], h2);
}

static int opsUpdateBw(pddl_strips_ops_t *bw_ops,
                       const pddl_strips_ops_t *fw_ops,
                       h2_t *h2)
{
    int ret = 0;

    for (int op_id = 0; op_id < h2->op_size; ++op_id){
        if (isOpPruned(h2, op_id))
            continue;

        pddl_strips_op_t *bw_op = bw_ops->op[op_id];
        const pddl_strips_op_t *fw_op = fw_ops->op[op_id];
        if (h2->disambiguate != NULL){
            if (pddlDisambiguateSet(h2->disambiguate, &bw_op->pre) < 0){
                setOpPruned(h2, op_id);
                ret = 1;
                continue;
            }
        }
        opSetEDeletes(bw_op, fw_op, h2);
    }

    return ret;
}

static int opsUpdateFw(pddl_strips_ops_t *fw_ops, h2_t *h2)
{
    int ret = 0;

    if (h2->disambiguate == NULL)
        return 0;

    for (int op_id = 0; op_id < h2->op_size; ++op_id){
        if (isOpPruned(h2, op_id))
            continue;

        pddl_strips_op_t *op = fw_ops->op[op_id];
        if (pddlDisambiguateSet(h2->disambiguate, &op->pre) < 0){
            setOpPruned(h2, op_id);
            ret = 1;
            continue;
        }
    }

    return ret;
}


int pddlH2FwBw(const pddl_strips_t *strips,
               const pddl_mgroups_t *mgroup,
               pddl_mutex_pairs_t *mutex,
               pddl_iset_t *unreachable_facts,
               pddl_iset_t *unreachable_ops,
               float time_limit_in_s,
               pddl_err_t *err)
{
    if (strips->has_cond_eff)
        PDDL_ERR_RET(err, -1, "h^2 fw/bw: Conditional effects not supported!");

    CTX(err, "h2fwbw", "h^2 fw/bw");
    PDDL_INFO(err, "facts: %d, ops: %d, mutex pairs: %lu, time-limit: %.2fs",
              strips->fact.fact_size,
              strips->op.op_size,
              (unsigned long)mutex->num_mutex_pairs,
              time_limit_in_s);

    pddl_time_limit_t time_limit;
    pddlTimeLimitSet(&time_limit, time_limit_in_s);

    h2_t h2;
    int update_fw = 1;
    int update_bw = 1;
    pddl_strips_ops_t ops_fw, ops_bw;
    pddl_disambiguate_t disamb;

    h2Init(&h2, strips, mutex, unreachable_facts, unreachable_ops, err);

    pddlStripsOpsInit(&ops_fw);
    pddlStripsOpsCopy(&ops_fw, &strips->op);
    opsInitBw(&ops_bw, &ops_fw, &h2);

    if (pddlDisambiguateInit(&disamb, h2.fact_size, mutex, mgroup) == 0)
        h2.disambiguate = &disamb;

    h2AllocOpFact(&h2, err);

    int ret = 0;
    while (update_fw || update_bw){
        if (update_fw < 0
                || update_bw < 0
                || pddlTimeLimitCheck(&time_limit) != 0){
            ret = -2;
            break;
        }

        if (update_fw == 1){
            update_fw = 0;
            setFwInit(&h2, &strips->init);
            opsUpdateFw(&ops_fw, &h2);
            h2ResetOpFact(&h2, &ops_fw);
            update_bw |= h2Run(&h2, &ops_fw, &time_limit, 0, err);
        }

        if (update_bw == 1){
            update_bw = 0;
            setBwInit(&h2, &strips->goal);
            update_fw |= opsUpdateFw(&ops_fw, &h2);
            opsUpdateBw(&ops_bw, &ops_fw, &h2);
            h2ResetOpFact(&h2, &ops_bw);
            update_fw |= h2Run(&h2, &ops_bw, &time_limit, 1, err);
        }
    }

    pddlStripsOpsFree(&ops_fw);
    pddlStripsOpsFree(&ops_bw);
    if (h2.disambiguate != NULL)
        pddlDisambiguateFree(&disamb);

    setOutput(&h2, mutex, unreachable_facts, unreachable_ops);

    PDDL_INFO(err, "DONE. mutex pairs: %lu, unreachable facts: %d,"
              " unreachable ops: %d, time-limit reached: %d",
              (unsigned long)mutex->num_mutex_pairs,
              (unreachable_facts != NULL ? pddlISetSize(unreachable_facts) : -1),
              (unreachable_ops != NULL ? pddlISetSize(unreachable_ops) : -1),
              (ret == -2 ? 1 : 0));

    h2Free(&h2);
    CTXEND(err);
    return ret;
}
