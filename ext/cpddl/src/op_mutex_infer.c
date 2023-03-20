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
#include "pddl/famgroup.h"
#include "pddl/critical_path.h"
#include "pddl/op_mutex_infer.h"
#include "internal.h"

static void tsReachabilityState(const pddl_ts_t *ts, int state, int *reach)
{
    for (int s = 0; s < ts->num_states; ++s){
        if (reach[s])
            continue;
        const pddl_iset_t *tr = pddlTSTransition(ts, state, s);
        if (pddlISetSize(tr) == 0)
            continue;
        reach[s] = 1;
        tsReachabilityState(ts, s, reach);
    }
}

/** Computes reachability of abstract states. */
static int *tsReachability(const pddl_ts_t *ts)
{
    int *reach = CALLOC_ARR(int, ts->num_states * ts->num_states);

    for (int state = 0; state < ts->num_states; ++state){
        int *rline = reach + state * ts->num_states;
        rline[state] = 1;
        tsReachabilityState(ts, state, rline);
    }

    return reach;
}

static void opMutexesFromSingleTransitions(pddl_op_mutex_pairs_t *m,
                                           const pddl_ts_t *ts)
{
    int s1, s2;
    const pddl_iset_t *tr;

    PDDL_TS_FOR_EACH_NONEMPTY_TRANSITION(ts, s1, s2, tr){
        if (s1 != s2 && pddlISetSize(tr) > 1)
            pddlOpMutexPairsAddGroup(m, tr);
    }
}

static void opMutexesFromCondensedTS(pddl_op_mutex_pairs_t *m,
                                     const pddl_ts_t *ts,
                                     pddl_err_t *err)
{
    int *reach;

    // First add an op-mutex for every pair of labels that share both start
    // and end state.
    opMutexesFromSingleTransitions(m, ts);

    // Compute reachability between states
    reach = tsReachability(ts);

    // Iterate over all pairs of states
    for (int s = 0; s < ts->num_states; ++s){
        const int *s_reach = reach + s * ts->num_states;
        for (int t = 0; t < ts->num_states; ++t){
            if (s == t || s_reach[t])
                continue;
            // Now t is not reachable from s.
            // Next, for each transition (s_start, s), we check every
            // transition (t, t_end). If s_start is not reachable from
            // t_end then we have found an op-mutex.

            int s_start;
            const pddl_iset_t *s_tr;
            PDDL_TS_FOR_EACH_NONEMPTY_TRANSITION_TO(ts, s, s_start, s_tr){
                int t_end;
                const pddl_iset_t *t_tr;
                PDDL_TS_FOR_EACH_NONEMPTY_TRANSITION_FROM(ts, t, t_end, t_tr){
                    if (t_tr == s_tr
                            || reach[t_end * ts->num_states + s_start]){
                        continue;
                    }
                    ASSERT(pddlISetIsDisjunct(s_tr, t_tr));

                    int o1, o2;
                    PDDL_ISET_FOR_EACH(s_tr, o1){
                        PDDL_ISET_FOR_EACH(t_tr, o2){
                            pddlOpMutexPairsAdd(m, o1, o2);
                        }
                    }
                }
            }
        }
    }

    if (reach != NULL)
        FREE(reach);
}

int pddlOpMutexInferFAMGroups(pddl_op_mutex_pairs_t *m,
                              const pddl_strips_t *strips,
                              const pddl_mgroups_t *mgroup,
                              pddl_err_t *err)
{
    CTX(err, "opm", "OPM");
    PDDL_INFO(err, "Op-mutexes from fam-groups:");

    pddl_strips_fact_cross_ref_t cr;
    pddlStripsFactCrossRefInit(&cr, strips, 0, 0, 1, 1, 1);

    for (int i = 0; i < mgroup->mgroup_size; ++i){
        const pddl_mgroup_t *mg = mgroup->mgroup + i;
        if (!mg->is_fam_group)
            continue;

        pddl_ts_t ts, tsc;
        pddlTSInitProjToFAMGroup(&ts, strips, &cr, &mg->mgroup);
        pddlTSCondensate(&tsc, &ts);

        opMutexesFromCondensedTS(m, &tsc, err);

        pddlTSFree(&ts);
        pddlTSFree(&tsc);
    }

    pddlStripsFactCrossRefFree(&cr);
    PDDL_INFO(err, "  --> Found %d op-mutexes from fam-groups",
              pddlOpMutexPairsSize(m));
    CTXEND(err);
    return 0;
}

int pddlOpMutexInferUncoveredFacts(pddl_op_mutex_pairs_t *m,
                                   const pddl_strips_t *strips,
                                   const pddl_mgroups_t *mgroup,
                                   pddl_err_t *err)
{
    PDDL_ISET(covered);
    PDDL_ISET(op_mgroup);

    PDDL_INFO(err, "Op-mutexes from uncovered facts:");

    pddl_strips_fact_cross_ref_t cr;
    pddlStripsFactCrossRefInit(&cr, strips, 0, 0, 1, 1, 1);

    for (int i = 0; mgroup != NULL && i < mgroup->mgroup_size; ++i)
        pddlISetUnion(&covered, &mgroup->mgroup[i].mgroup);

    int mi = 0;
    for (int fact_id = 0; fact_id < strips->fact.fact_size; ++fact_id){
        const pddl_strips_fact_cross_ref_fact_t *fact = cr.fact + fact_id;
        if (mi < pddlISetSize(&covered) && fact_id == pddlISetGet(&covered, mi)){
            ++mi;
        }else if (pddlISetSize(&fact->op_del) == 0){
            pddlISetEmpty(&op_mgroup);
            int opi;
            PDDL_ISET_FOR_EACH(&fact->op_add, opi){
                const pddl_strips_op_t *op = strips->op.op[opi];
                if (pddlISetSize(&op->add_eff) != 1)
                    continue;
                pddlISetAdd(&op_mgroup, opi);
            }

            if (pddlISetSize(&op_mgroup) > 1)
                pddlOpMutexPairsAddGroup(m, &op_mgroup);
        }
    }

    pddlISetFree(&op_mgroup);
    pddlISetFree(&covered);

    pddlStripsFactCrossRefFree(&cr);
    PDDL_INFO(err, "  --> Found %d op-mutexes", pddlOpMutexPairsSize(m));
    return 0;
}

int pddlOpMutexInferHmOpFactCompilation(pddl_op_mutex_pairs_t *opm,
                                        int m,
                                        const pddl_strips_t *strips,
                                        pddl_err_t *err)
{
    pddl_strips_op_t *op;
    pddl_strips_t P2;
    int op_fact_offset;

    CTX(err, "opm", "OPM");
    PDDL_INFO(err, "Op-mutexes using h^%d compilation:", m);

    pddlStripsInitCopy(&P2, strips);

    // Remember ID of the first op-fact
    op_fact_offset = P2.fact.fact_size;

    // Create a fact for each operator
    PDDL_STRIPS_OPS_FOR_EACH(&P2.op, op){
        pddl_fact_t fact;
        pddlFactInit(&fact);
        char name[128];
        sprintf(name, "o%d", op->id);
        fact.name = name;
        int fid = pddlFactsAdd(&P2.fact, &fact);

        // add the fact to the corresponding operator's add effect
        pddlISetAdd(&op->add_eff, fid);
        fact.name = NULL;
        pddlFactFree(&fact);
    }
    PDDL_INFO(err, "  --> Modified problem created.");

    pddl_mutex_pairs_t mutex;

    pddlMutexPairsInitStrips(&mutex, &P2);
    int o1, o2;
    PDDL_OP_MUTEX_PAIRS_FOR_EACH(opm, o1, o2)
        pddlMutexPairsAdd(&mutex, op_fact_offset + o1, op_fact_offset + o2);

    if (pddlHm(m, &P2, &mutex, NULL, NULL, 0, 0, err) == 0){
        PDDL_INFO(err, "  --> h^%d computed with %d mutex pairs.",
                  m, mutex.num_mutex_pairs);
        int fact_size = P2.fact.fact_size;
        for (int i = op_fact_offset; i < fact_size; ++i){
            for (int j = i + 1; j < fact_size; ++j){
                if (pddlMutexPairsIsMutex(&mutex, i, j)){
                    pddlOpMutexPairsAdd(opm, i - op_fact_offset,
                                             j - op_fact_offset);
                }
            }
        }
    }else{
        PDDL_ERR(err, "h^%d failed!", m);
    }
    pddlMutexPairsFree(&mutex);

    pddlStripsFree(&P2);

    PDDL_INFO(err, "  --> Found %d op-mutexes", pddlOpMutexPairsSize(opm));
    CTXEND(err);
    return 0;
}


static void opMutexHmFromOp(pddl_op_mutex_pairs_t *opm,
                            int m,
                            const pddl_strips_op_t *op,
                            const pddl_strips_t *strips_in,
                            const pddl_mutex_pairs_t *mutex,
                            pddl_iset_t *unreach_map,
                            pddl_err_t *err)
{
    pddl_strips_t strips;
    pddl_mutex_pairs_t hm_mutex;

    pddlStripsInitCopy(&strips, strips_in);

    PDDL_ISET(init);
    pddlISetMinus2(&init, &op->pre, &op->del_eff);
    pddlISetUnion(&init, &op->add_eff);

    pddlISetEmpty(&strips.init);
    pddlISetUnion(&strips.init, &init);

    for (int fact_id = 0; fact_id < strips_in->fact.fact_size; ++fact_id){
        if (!pddlISetIn(fact_id, &op->del_eff)
                && !pddlMutexPairsIsMutexFactSet(mutex, fact_id, &init)
                && !pddlMutexPairsIsMutexFactSet(mutex, fact_id, &op->pre)){
            pddlISetAdd(&strips.init, fact_id);
        }
    }
    pddlISetFree(&init);

    PDDL_ISET(unreach_ops);
    pddlMutexPairsInitCopy(&hm_mutex, mutex);
    if (pddlHm(m, &strips, &hm_mutex, NULL, &unreach_ops, 0, 0, err) != 0){
        // TODO
        PDDL_ERR(err, "h^2 failed!");
        pddlErrPrint(err, 1, stderr);
        exit(-1);
    }

    int op_unreach_id;
    PDDL_ISET_FOR_EACH(&unreach_ops, op_unreach_id){
        if (op->id == op_unreach_id)
            continue;
        pddlISetAdd(&unreach_map[op->id], op_unreach_id);
        if (pddlISetIn(op->id, &unreach_map[op_unreach_id])){
            pddlOpMutexPairsAdd(opm, op->id, op_unreach_id);
        }
    }
    pddlISetFree(&unreach_ops);

    pddlMutexPairsFree(&hm_mutex);
    pddlStripsFree(&strips);
}

int pddlOpMutexInferHmFromEachOp(pddl_op_mutex_pairs_t *opm,
                                 int m,
                                 const pddl_strips_t *strips_in,
                                 const pddl_mutex_pairs_t *mutex,
                                 const pddl_iset_t *ops,
                                 pddl_err_t *err)
{
    pddl_iset_t *unreach_map;

    CTX(err, "opm", "OPM");
    PDDL_INFO(err, "Op-mutexes using h^%d from each operator:", m);

    unreach_map = CALLOC_ARR(pddl_iset_t, strips_in->op.op_size);

    if (ops != NULL){
        int opi;
        PDDL_ISET_FOR_EACH(ops, opi){
            const pddl_strips_op_t *op = strips_in->op.op[opi];
            opMutexHmFromOp(opm, m, op, strips_in, mutex, unreach_map, err);
        }
    }else{
        const pddl_strips_op_t *op;
        PDDL_STRIPS_OPS_FOR_EACH(&strips_in->op, op)
            opMutexHmFromOp(opm, m, op, strips_in, mutex, unreach_map, err);
    }

    const pddl_strips_op_t *op;
    PDDL_STRIPS_OPS_FOR_EACH(&strips_in->op, op)
        pddlISetFree(unreach_map + op->id);
    if (unreach_map != NULL)
        FREE(unreach_map);

    PDDL_INFO(err, "  --> Found %d op-mutexes", pddlOpMutexPairsSize(opm));
    CTXEND(err);
    return 0;
}
