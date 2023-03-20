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

#include "pddl/set.h"
#include "pddl/trans_system.h"
#include "internal.h"

/** Returns index of the fact within the mutex groups with mgroup_id */
static int indexOfFactWithinMGroup(const pddl_trans_systems_t *tss,
                                   int mgroup_id,
                                   int fact);
/** Adds transition from->to labeled with the set of labels */
static void addTrans(pddl_trans_system_t *ts,
                     const pddl_iset_t *label,
                     int from,
                     int to);
/** Creates transitions in the projection to the specified mutex group */
static void createTransitionsInProjection(pddl_trans_system_t *ts,
                                          const pddl_mg_strips_t *mg_strips,
                                          const pddl_mutex_pairs_t *mutex,
                                          const pddl_iset_t *mgroup);
/** Sets the initial state in the projection to the specified mutex group */
static void setInitState(pddl_trans_system_t *ts,
                         const pddl_iset_t *mgroup,
                         const pddl_strips_t *strips);
/** Sets the goal states in the projection to the specified mutex group */
static void setGoalStates(pddl_trans_system_t *ts,
                          const pddl_iset_t *mgroup,
                          const pddl_strips_t *strips,
                          const pddl_mutex_pairs_t *mutex);
/** Make a deep copy of labeled transitions */
static void copyLabeledTransitions(pddl_trans_system_t *ts,
                                   pddl_labeled_transitions_set_t *dst,
                                   const pddl_labeled_transitions_set_t *src);
/** Merge transitions from tr1 and tr2 into t */
static void mergeTransitions(pddl_trans_system_t *t,
                             const pddl_labeled_transitions_t *tr1,
                             const pddl_labeled_transitions_t *tr2);
/** Free labeled transitions and makes sure that labels are dereferenced */
static void freeLabeledTransitions(pddl_trans_system_t *ts);
/** Adds a new transition system to tss and returns its ID */
static int transSystemsAddTS(pddl_trans_systems_t *tss,
                             pddl_trans_system_t *ts);


void pddlMGroupIdxPairsFree(pddl_mgroup_idx_pairs_t *p)
{
    if (p->mgroup_idx != NULL)
        FREE(p->mgroup_idx);
}

void pddlMGroupIdxPairsAdd(pddl_mgroup_idx_pairs_t *p, int mg_id, int idx)
{
    if (p->mgroup_idx_size == p->mgroup_idx_alloc){
        if (p->mgroup_idx_alloc == 0){
            p->mgroup_idx_alloc = 1;
        }else{
            p->mgroup_idx_alloc *= 2;
        }
        p->mgroup_idx = REALLOC_ARR(p->mgroup_idx,
                                    pddl_mgroup_idx_pair_t,
                                    p->mgroup_idx_alloc);
    }
    pddl_mgroup_idx_pair_t *pair = p->mgroup_idx + p->mgroup_idx_size++;
    pair->mg_id = mg_id;
    pair->fact_idx = idx;
}


pddl_trans_system_t *pddlTransSystemNewMGroup(pddl_trans_systems_t *tss,
                                              const pddl_mg_strips_t *mg_strips,
                                              const pddl_mutex_pairs_t *mutex,
                                              int mg_id)
{
    const pddl_mgroup_t *mg = mg_strips->mg.mgroup + mg_id;
    ASSERT_RUNTIME(mg->is_exactly_one);

    pddl_trans_system_t *ts = ZALLOC(pddl_trans_system_t);
    ts->trans_systems = tss;
    pddlISetAdd(&ts->mgroup_ids, mg_id);
    ts->num_states = pddlISetSize(&mg->mgroup);
    ts->repr = pddlCascadingTableNewLeaf(mg_id, pddlISetSize(&mg->mgroup));
    ASSERT(pddlCascadingTableSize(ts->repr) == ts->num_states);
    pddlLabeledTransitionsSetInit(&ts->trans);

    setInitState(ts, &mg->mgroup, &mg_strips->strips);
    setGoalStates(ts, &mg->mgroup, &mg_strips->strips, mutex);

    createTransitionsInProjection(ts, mg_strips, mutex, &mg->mgroup);
    pddlLabeledTransitionsSetSort(&ts->trans);

    return ts;
}


pddl_trans_system_t *pddlTransSystemClone(pddl_trans_systems_t *tss,
                                          const pddl_trans_system_t *ts_in)
{
    pddl_trans_system_t *ts = ZALLOC(pddl_trans_system_t);
    ts->trans_systems = tss;
    pddlISetUnion(&ts->mgroup_ids, &ts_in->mgroup_ids);
    ts->num_states = ts_in->num_states;
    ts->repr = pddlCascadingTableClone(ts_in->repr);

    pddlLabeledTransitionsSetInit(&ts->trans);
    copyLabeledTransitions(ts, &ts->trans, &ts_in->trans);

    ts->init_state = ts_in->init_state;
    pddlISetUnion(&ts->goal_states, &ts_in->goal_states);
    return ts;
}

pddl_trans_system_t *pddlTransSystemNewMerge(pddl_trans_systems_t *tss,
                                             const pddl_trans_system_t *t1,
                                             const pddl_trans_system_t *t2)
{
    pddl_trans_system_t *ts = ZALLOC(pddl_trans_system_t);
    ts->trans_systems = tss;
    pddlISetUnion2(&ts->mgroup_ids, &t1->mgroup_ids, &t2->mgroup_ids);
    ts->num_states = t1->num_states * t2->num_states;
    ts->repr = pddlCascadingTableMerge(t1->repr, t2->repr);
    ASSERT(ts->num_states == pddlCascadingTableSize(ts->repr));

    // Construct transitions by iterating over all pairs of transitions
    // from both t1 and t2
    pddlLabeledTransitionsSetInit(&ts->trans);
    for (int t1i = 0; t1i < t1->trans.trans_size; ++t1i){
        const pddl_labeled_transitions_t *tr1 = t1->trans.trans + t1i;
        for (int t2i = 0; t2i < t2->trans.trans_size; ++t2i){
            const pddl_labeled_transitions_t *tr2 = t2->trans.trans + t2i;
            mergeTransitions(ts, tr1, tr2);
        }
    }
    pddlLabeledTransitionsSetSort(&ts->trans);

    // Set initial state
    ts->init_state = pddlCascadingTableMergeValue(ts->repr, t1->init_state,
                                                  t2->init_state);
    // Find all goal states
    int g1, g2;
    PDDL_ISET_FOR_EACH(&t1->goal_states, g1){
        PDDL_ISET_FOR_EACH(&t2->goal_states, g2){
            int g = pddlCascadingTableMergeValue(ts->repr, g1, g2);
            pddlISetAdd(&ts->goal_states, g);
        }
    }
    return ts;
}

void pddlTransSystemDel(pddl_trans_system_t *ts)
{
    pddlISetFree(&ts->mgroup_ids);
    pddlCascadingTableDel(ts->repr);
    freeLabeledTransitions(ts);
    pddlISetFree(&ts->goal_states);
    FREE(ts);
}

int pddlTransSystemMGroupState(const pddl_trans_system_t *ts, int *state)
{
    return pddlCascadingTableValueFromState(ts->repr, state);
}


void pddlTransSystemsInit(pddl_trans_systems_t *tss,
                          const pddl_mg_strips_t *mg_strips,
                          const pddl_mutex_pairs_t *mutex)
{
    if (mg_strips->strips.has_cond_eff){
        PANIC("trans_system module does not support conditional effects!");
    }

    ZEROIZE(tss);
    tss->fact_size = mg_strips->strips.fact.fact_size;
    pddlMGroupsInitCopy(&tss->mgroup, &mg_strips->mg);

    tss->fact_to_mgroup = CALLOC_ARR(pddl_mgroup_idx_pairs_t,
                                     tss->fact_size);
    for (int mgi = 0; mgi < mg_strips->mg.mgroup_size; ++mgi){
        const pddl_mgroup_t *mg = mg_strips->mg.mgroup + mgi;
        ASSERT_RUNTIME(mg->is_exactly_one);
        for (int i = 0; i < pddlISetSize(&mg->mgroup); ++i){
            int fact = pddlISetGet(&mg->mgroup, i);
            pddlMGroupIdxPairsAdd(tss->fact_to_mgroup + fact, mgi, i);
        }
    }

    pddlLabelsInitFromStripsOps(&tss->label, &mg_strips->strips.op);

    tss->ts_alloc = 1;
    while (tss->ts_alloc < mg_strips->mg.mgroup_size)
        tss->ts_alloc *= 2;
    tss->ts_size = mg_strips->mg.mgroup_size;
    tss->ts = ALLOC_ARR(pddl_trans_system_t *, tss->ts_alloc);
    for (int i = 0; i < mg_strips->mg.mgroup_size; ++i)
        tss->ts[i] = pddlTransSystemNewMGroup(tss, mg_strips, mutex, i);
}

void pddlTransSystemsFree(pddl_trans_systems_t *tss)
{
    for (int i = 0; i < tss->ts_size; ++i)
        pddlTransSystemDel(tss->ts[i]);
    if (tss->ts != NULL)
        FREE(tss->ts);

    for (int f = 0; f < tss->fact_size; ++f)
        pddlMGroupIdxPairsFree(tss->fact_to_mgroup + f);
    if (tss->fact_to_mgroup != NULL)
        FREE(tss->fact_to_mgroup);

    pddlLabelsFree(&tss->label);
    pddlISetFree(&tss->dead_labels);
    pddlMGroupsFree(&tss->mgroup);
}

int pddlTransSystemsCloneTransSystem(pddl_trans_systems_t *tss, int tid)
{
    pddl_trans_system_t *ts = pddlTransSystemClone(tss, tss->ts[tid]);
    return transSystemsAddTS(tss, ts);
}

void pddlTransSystemsDelTransSystem(pddl_trans_systems_t *tss, int tid)
{
    if (tss->ts[tid] != NULL)
        pddlTransSystemDel(tss->ts[tid]);
    tss->ts[tid] = NULL;
}

void pddlTransSystemsCleanDeletedTransSystems(pddl_trans_systems_t *tss)
{
    int inc = 0;
    for (int i = 0; i < tss->ts_size; ++i){
        if (tss->ts[i] != NULL)
            tss->ts[inc++] = tss->ts[i];
    }
    tss->ts_size = inc;
}

int pddlTransSystemsMerge(pddl_trans_systems_t *tss, int t1, int t2)
{
    pddl_trans_system_t *ts;
    ts = pddlTransSystemNewMerge(tss, tss->ts[t1], tss->ts[t2]);
    return transSystemsAddTS(tss, ts);
}

void pddlTransSystemsAbstract(pddl_trans_systems_t *tss,
                              int ts_id,
                              const pddl_trans_system_abstr_map_t *map)
{
    if (map->is_identity)
        return;

    pddl_trans_system_t *ts = tss->ts[ts_id];
    ASSERT_RUNTIME(map->num_states == ts->num_states);
    ASSERT_RUNTIME(map->map_num_states >= 1);
    pddlCascadingTableAbstract(ts->repr, map->map);
    ASSERT_RUNTIME(map->map_num_states == pddlCascadingTableSize(ts->repr));

    // Merge labels for the transformed transitions
    int labels_size = map->map_num_states * map->map_num_states;
    pddl_iset_t *labels = CALLOC_ARR(pddl_iset_t, labels_size);
    for (int ltri = 0; ltri < ts->trans.trans_size; ++ltri){
        const pddl_labeled_transitions_t *ltr = ts->trans.trans + ltri;
        const pddl_iset_t *cur_label = &ltr->label->label;
        for (int tri = 0; tri < ltr->trans.trans_size; ++tri){
            const pddl_transition_t *tr = ltr->trans.trans + tri;
            int from = map->map[tr->from];
            int to = map->map[tr->to];
            if (from < 0 || to < 0)
                continue;
            pddlISetUnion(labels + from * map->map_num_states + to, cur_label);
        }
    }

    // Create a new transition table
    pddl_labeled_transitions_set_t trans;
    pddlLabeledTransitionsSetInit(&trans);
    for (int from = 0; from < map->map_num_states; ++from){
        for (int to = 0; to < map->map_num_states; ++to){
            int idx = from * map->map_num_states + to;
            if (pddlISetSize(labels + idx) == 0)
                continue;

            pddl_label_set_t *lb = pddlLabelsAddSet(&tss->label, labels + idx);
            if (pddlLabeledTransitionsSetAdd(&trans, lb, from, to) == 1)
                pddlLabelsSetDecRef(&tss->label, lb);
        }
    }
    freeLabeledTransitions(ts);
    ts->trans = trans;
    pddlLabeledTransitionsSetSort(&ts->trans);

    for (int i = 0; i < labels_size; ++i)
        pddlISetFree(labels + i);
    FREE(labels);

    ts->init_state = map->map[ts->init_state];
    PDDL_ISET(goal_states);
    int state;
    PDDL_ISET_FOR_EACH(&ts->goal_states, state){
        if (map->map[state] >= 0)
            pddlISetAdd(&goal_states, map->map[state]);
    }
    pddlISetFree(&ts->goal_states);
    ts->goal_states = goal_states;

    ts->num_states = map->map_num_states;
    ts->dead_labels_collected = 0;
}

int pddlTransSystemsCollectDeadLabels(pddl_trans_systems_t *tss, int ts_id)
{
    pddl_trans_system_t *ts = tss->ts[ts_id];
    if (ts->dead_labels_collected)
        return 0;
    ts->dead_labels_collected = 1;

    int *label = CALLOC_ARR(int, tss->label.label_size);
    for (int ltri = 0; ltri < ts->trans.trans_size; ++ltri){
        const pddl_labeled_transitions_t *ltr = ts->trans.trans + ltri;
        int lbl;
        PDDL_ISET_FOR_EACH(&ltr->label->label, lbl)
            label[lbl] = 1;
    }

    int num_dead_labels = pddlISetSize(&tss->dead_labels);
    for (int i = 0; i < tss->label.label_size; ++i){
        if (!label[i])
            pddlISetAdd(&tss->dead_labels, i);
    }
    FREE(label);

    return num_dead_labels > pddlISetSize(&tss->dead_labels);
}

int pddlTransSystemsCollectDeadLabelsFromAll(pddl_trans_systems_t *tss)
{
    int ret = 0;
    for (int tsi = 0; tsi < tss->ts_size; ++tsi)
        ret |= pddlTransSystemsCollectDeadLabels(tss, tsi);
    return ret;
}

void pddlTransSystemsRemoveDeadLabels(pddl_trans_systems_t *tss, int ts_id)
{
    if (pddlISetSize(&tss->dead_labels) == 0)
        return;

    pddl_trans_system_t *ts = tss->ts[ts_id];
    pddl_labeled_transitions_set_t trans;
    pddlLabeledTransitionsSetInit(&trans);

    PDDL_ISET(labels);
    for (int ti = 0; ti < ts->trans.trans_size; ++ti){
        pddl_labeled_transitions_t *t = ts->trans.trans + ti;
        pddlISetMinus2(&labels, &t->label->label, &tss->dead_labels);
        if (pddlISetSize(&labels) == 0)
            continue;

        // Replace the current label with the label set without dead labels
        pddl_label_set_t *l = pddlLabelsAddSet(&tss->label, &labels);
        int added;
        pddl_labeled_transitions_t *ltr;
        ltr = pddlLabeledTransitionsSetAddLabel(&trans, l, &added);
        if (!added){
            ASSERT(l->ref > 1);
            pddlLabelsSetDecRef(&tss->label, l);
        }
        pddlTransitionsUnion(&ltr->trans, &t->trans);
    }
    pddlISetFree(&labels);

    // Replace old set with the new one
    freeLabeledTransitions(ts);
    ts->trans = trans;
    pddlLabeledTransitionsSetSort(&ts->trans);
}

void pddlTransSystemsRemoveDeadLabelsFromAll(pddl_trans_systems_t *tss)
{
    for (int tsi = 0; tsi < tss->ts_size; ++tsi)
        pddlTransSystemsRemoveDeadLabels(tss, tsi);
}

void pddlTransSystemPrintDebug1(const pddl_trans_systems_t *tss,
                                const pddl_strips_t *strips,
                                int ts_id,
                                FILE *fout)
{
    const pddl_trans_system_t *ts = tss->ts[ts_id];
    fprintf(fout, "TS: id: %d, num-states: %d\n", ts_id, ts->num_states);
    int mg_id;
    PDDL_ISET_FOR_EACH(&ts->mgroup_ids, mg_id){
        int fact;
        fprintf(fout, "  MG %d:", mg_id);
        PDDL_ISET_FOR_EACH(&tss->mgroup.mgroup[mg_id].mgroup, fact)
            fprintf(fout, " %d:(%s)", fact, strips->fact.fact[fact]->name);
        fprintf(fout, "\n");
    }

    fprintf(fout, "  Init: %d\n", ts->init_state);
    fprintf(fout, "  Goal:");
    int state;
    PDDL_ISET_FOR_EACH(&ts->goal_states, state)
        fprintf(fout, " %d", state);
    fprintf(fout, "\n");

    fprintf(fout, "  Trans[%d]:\n", ts->trans.trans_size);
    for (int ti = 0; ti < ts->trans.trans_size; ++ti){
        const pddl_labeled_transitions_t *trans = ts->trans.trans + ti;
        int op_id;
        PDDL_ISET_FOR_EACH(&trans->label->label, op_id){
            const pddl_strips_op_t *op = strips->op.op[op_id];
            fprintf(fout, "    %d:(%s)\n", op_id, op->name);
        }
        fprintf(fout, "     ");
        for (int i = 0; i < trans->trans.trans_size; ++i){
            fprintf(fout, " %d->%d", trans->trans.trans[i].from,
                    trans->trans.trans[i].to);
        }
        fprintf(fout, "\n");
    }
}

void pddlTransSystemPrintDebug2(const pddl_trans_systems_t *tss,
                                int ts_id,
                                FILE *fout)
{
    const pddl_trans_system_t *ts = tss->ts[ts_id];
    fprintf(fout, "TS: id: %d, num-states: %d", ts_id, ts->num_states);
    fprintf(fout, ", init: %d", ts->init_state);
    fprintf(fout, ", goal:");
    int state;
    PDDL_ISET_FOR_EACH(&ts->goal_states, state)
        fprintf(fout, " %d", state);

    fprintf(fout, ", trans:");
    for (int ti = 0; ti < ts->trans.trans_size; ++ti){
        const pddl_labeled_transitions_t *trans = ts->trans.trans + ti;
        fprintf(fout, " (");
        pddlISetPrintCompressed(&trans->label->label, fout);
        fprintf(fout, ")/%d:[", trans->label->cost);
        //fprintf(fout, " %d:[", pddlISetSize(&trans->label->label));
        for (int i = 0; i < trans->trans.trans_size; ++i){
            if (i != 0)
                fprintf(fout, " ");
            fprintf(fout, "%d->%d", trans->trans.trans[i].from,
                    trans->trans.trans[i].to);
        }
        fprintf(fout, "]");
    }
    fprintf(fout, "\n");
}

void pddlTransSystemsPrintDebug1(const pddl_trans_systems_t *tss,
                                 const pddl_strips_t *strips,
                                 FILE *fout)
{
    for (int i = 0; i < tss->ts_size; ++i)
        pddlTransSystemPrintDebug1(tss, strips, i, fout);
}

void pddlTransSystemsPrintDebug2(const pddl_trans_systems_t *tss, FILE *fout)
{
    for (int i = 0; i < tss->ts_size; ++i)
        pddlTransSystemPrintDebug2(tss, i, fout);
}

void pddlTransSystemsSetMGroupState(const pddl_trans_systems_t *tss,
                                    const pddl_iset_t *state,
                                    int *mgroup_state)
{
    for (int i = 0; i < tss->mgroup.mgroup_size; ++i)
        mgroup_state[i] = -1;
    int fact;
    PDDL_ISET_FOR_EACH(state, fact){
        const pddl_mgroup_idx_pairs_t *mp = tss->fact_to_mgroup + fact;
        for (int i = 0; i < mp->mgroup_idx_size; ++i){
            const pddl_mgroup_idx_pair_t *p = mp->mgroup_idx + i;
            mgroup_state[p->mg_id] = p->fact_idx;
        }
    }
}

int pddlTransSystemsStripsState(const pddl_trans_systems_t *tss,
                                int ts_id,
                                const pddl_iset_t *strips_state)
{
    int state[tss->mgroup.mgroup_size];
    pddlTransSystemsSetMGroupState(tss, strips_state, state);

    const pddl_trans_system_t *ts = tss->ts[ts_id];
    return pddlTransSystemMGroupState(ts, state);
}


static int indexOfFactWithinMGroup(const pddl_trans_systems_t *tss,
                                   int mgroup_id,
                                   int fact)
{
    const pddl_mgroup_idx_pairs_t *p = tss->fact_to_mgroup + fact;
    for (int i = 0; i < p->mgroup_idx_size; ++i){
        if (p->mgroup_idx[i].mg_id == mgroup_id)
            return p->mgroup_idx[i].fact_idx;
    }
    return -1;
}

static void addTrans(pddl_trans_system_t *ts,
                     const pddl_iset_t *label,
                     int from,
                     int to)
{
    pddl_trans_systems_t *tss = ts->trans_systems;
    pddl_label_set_t *l = pddlLabelsAddSet(&tss->label, label);
    if (pddlLabeledTransitionsSetAdd(&ts->trans, l, from, to) == 1)
        pddlLabelsSetDecRef(&tss->label, l);
}

static void _findInOutLoopOps(pddl_trans_systems_t *tss,
                              pddl_trans_system_t *ts,
                              const pddl_mg_strips_t *mg_strips,
                              const pddl_mutex_pairs_t *mutex,
                              const pddl_iset_t *mgroup,
                              pddl_iset_t *in_ops,
                              pddl_iset_t *out_ops,
                              pddl_iset_t *loop)
{
    PDDL_ISET(fset);
    PDDL_ISET(eff);
    int mgroup_id = pddlISetGet(&ts->mgroup_ids, 0);
    int mgroup_size = pddlISetSize(mgroup);
    const pddl_iset_t *dead_labels = &tss->dead_labels;

    int dead_size = pddlISetSize(dead_labels);
    int dead_cur = 0;
    for (int op_id = 0; op_id < mg_strips->strips.op.op_size; ++op_id){
        // Skip dead labels
        if (dead_cur < dead_size && pddlISetGet(dead_labels, dead_cur) == op_id){
            ++dead_cur;
            continue;
        }

        const pddl_strips_op_t *op = mg_strips->strips.op.op[op_id];

        // First find out if it deletes any fact from the current mutex
        // group
        pddlISetIntersect2(&fset, &op->del_eff, mgroup);
        if (pddlISetSize(&fset) > 0){
            //ASSERT(pddlISetSize(&fset) == 1);
            int fact;
            PDDL_ISET_FOR_EACH(&fset, fact){
                int deleted_idx = indexOfFactWithinMGroup(tss, mgroup_id, fact);
                // This operator can be outgoing label for the deleted fact
                pddlISetAdd(out_ops + deleted_idx, op_id);
            }
        }

        if (pddlISetSize(&fset) == 0 || pddlISetIsDisjoint(mgroup, &op->pre)){
            // If no fact from this mutex group is deleted, then this
            // operator is on the loop of all facts that are not mutex with
            // the precondition or the effect.
            // But also, if a fact from this mutex group is deleted, but
            // this fact is not in the precondition, then we don't know
            // where the operator will be applied, so it still can be on a
            // loop of other facts.
            // Note, that we assume that mutex contains all mutex pairs
            // from all mutex groups (at least).
            pddlISetMinus2(&eff, &op->pre, &op->del_eff);
            pddlISetUnion(&eff, &op->add_eff);
            for (int idx = 0; idx < mgroup_size; ++idx){
                int fact = pddlISetGet(mgroup, idx);
                if (!pddlISetIn(fact, &fset)
                        && !pddlMutexPairsIsMutexFactSet(mutex, fact, &op->pre)
                        && !pddlMutexPairsIsMutexFactSet(mutex, fact, &eff)){
                    pddlISetAdd(loop + idx, op_id);
                }
            }
        }

        // Incoming transitions are only those that add some fact from this
        // mutex group
        pddlISetIntersect2(&fset, &op->add_eff, mgroup);
        if (pddlISetSize(&fset) > 0){
            // Skip operators that can produce only unreachable states
            if (pddlISetSize(&fset) > 1)
                continue;
            //ASSERT(pddlISetSize(&fset) == 1);
            int fact = pddlISetGet(&fset, 0);
            int fact_idx = indexOfFactWithinMGroup(tss, mgroup_id, fact);
            pddlISetAdd(in_ops + fact_idx, op_id);
            // Add it also on the loop if the operator can be applied on
            // the state that contains the fact that is in the add effect
            if (!pddlMutexPairsIsMutexFactSet(mutex, fact, &op->pre))
                pddlISetAdd(loop + fact_idx, op_id);
        }
    }
    pddlISetFree(&fset);
    pddlISetFree(&eff);
}

static void createTransitionsInProjection(pddl_trans_system_t *ts,
                                          const pddl_mg_strips_t *mg_strips,
                                          const pddl_mutex_pairs_t *mutex,
                                          const pddl_iset_t *mgroup)
{
    pddl_trans_systems_t *tss = ts->trans_systems;
    int mgroup_size = pddlISetSize(mgroup);
    pddl_iset_t *in_ops = CALLOC_ARR(pddl_iset_t, mgroup_size);
    pddl_iset_t *out_ops = CALLOC_ARR(pddl_iset_t, mgroup_size);
    pddl_iset_t *loop = CALLOC_ARR(pddl_iset_t, mgroup_size);
    PDDL_ISET(ops);

    // Find outgoing, incoming and loop transitions separately
    _findInOutLoopOps(tss, ts, mg_strips, mutex, mgroup, in_ops, out_ops, loop);

    for (int s1 = 0; s1 < mgroup_size; ++s1){
        for (int s2 = 0; s2 < mgroup_size; ++s2){
            if (s1 == s2){
                if (pddlISetSize(loop + s1) == 0)
                    continue;
                addTrans(ts, loop + s1, s1, s1);

            }else{
                pddlISetIntersect2(&ops, out_ops + s1, in_ops + s2);
                if (pddlISetSize(&ops) == 0)
                    continue;

                addTrans(ts, &ops, s1, s2);
            }
        }
    }

    pddlISetFree(&ops);
    for (int i = 0; i < mgroup_size; ++i){
        pddlISetFree(in_ops + i);
        pddlISetFree(out_ops + i);
        pddlISetFree(loop + i);
    }
    FREE(in_ops);
    FREE(out_ops);
    FREE(loop);
}

static void setInitState(pddl_trans_system_t *ts,
                         const pddl_iset_t *mgroup,
                         const pddl_strips_t *strips)
{
    PDDL_ISET(init);
    pddlISetIntersect2(&init, mgroup, &strips->init);
    ASSERT_RUNTIME(pddlISetSize(&init) == 1);
    for (int idx = 0; idx < pddlISetSize(mgroup); ++idx){
        if (pddlISetGet(mgroup, idx) == pddlISetGet(&init, 0)){
            ts->init_state = idx;
            break;
        }
    }
    pddlISetFree(&init);
}

static void setGoalStates(pddl_trans_system_t *ts,
                          const pddl_iset_t *mgroup,
                          const pddl_strips_t *strips,
                          const pddl_mutex_pairs_t *mutex)
{
    int mg_size = pddlISetSize(mgroup);
    PDDL_ISET(goal);
    pddlISetIntersect2(&goal, mgroup, &strips->goal);
    if (pddlISetSize(&goal) == 0){
        for (int idx = 0; idx < mg_size; ++idx){
            int fact = pddlISetGet(mgroup, idx);
            if (!pddlMutexPairsIsMutexFactSet(mutex, fact, &strips->goal))
                pddlISetAdd(&ts->goal_states, idx);
        }

    }else{
        int goal_size = pddlISetSize(&goal);
        int goal_id = 0;
        for (int idx = 0; idx < mg_size && goal_id < goal_size; ++idx){
            if (pddlISetGet(mgroup, idx) == pddlISetGet(&goal, goal_id)){
                pddlISetAdd(&ts->goal_states, idx);
                ++goal_id;
            }
        }
    }
    pddlISetFree(&goal);
}

static void copyLabeledTransitions(pddl_trans_system_t *ts,
                                   pddl_labeled_transitions_set_t *dst,
                                   const pddl_labeled_transitions_set_t *src)
{
    pddl_trans_systems_t *tss = ts->trans_systems;
    for (int ltri = 0; ltri < src->trans_size; ++ltri){
        const pddl_labeled_transitions_t *ltr = src->trans + ltri;
        pddl_label_set_t *label = ltr->label;
        pddlLabelsSetIncRef(&tss->label, label);

        pddl_labeled_transitions_t *new_ltr;
        int added;
        new_ltr = pddlLabeledTransitionsSetAddLabel(dst, label, &added);
        ASSERT_RUNTIME(added);
        pddlTransitionsUnion(&new_ltr->trans, &ltr->trans);
    }
}

static void mergeTransitions(pddl_trans_system_t *t,
                             const pddl_labeled_transitions_t *tr1,
                             const pddl_labeled_transitions_t *tr2)
{
    PDDL_ISET(label);
    pddlISetIntersect2(&label, &tr1->label->label, &tr2->label->label);
    // Remove dead labels
    if (pddlISetSize(&label) > 0)
        pddlISetMinus(&label, &t->trans_systems->dead_labels);
    if (pddlISetSize(&label) == 0){
        pddlISetFree(&label);
        return;
    }

    pddl_labeled_transitions_t *ltr = NULL;
    for (int tr1i = 0; tr1i < tr1->trans.trans_size; ++tr1i){
        int f1 = tr1->trans.trans[tr1i].from;
        int t1 = tr1->trans.trans[tr1i].to;
        for (int tr2i = 0; tr2i < tr2->trans.trans_size; ++tr2i){
            int f2 = tr2->trans.trans[tr2i].from;
            int t2 = tr2->trans.trans[tr2i].to;
            int from = pddlCascadingTableMergeValue(t->repr, f1, f2);
            int to = pddlCascadingTableMergeValue(t->repr, t1, t2);
            ASSERT(from >= 0 && from < t->num_states);
            ASSERT(to >= 0 && to < t->num_states);

            if (ltr == NULL){
                pddl_trans_systems_t *tss = t->trans_systems;
                pddl_label_set_t *l = pddlLabelsAddSet(&tss->label, &label);
                int added;
                ltr = pddlLabeledTransitionsSetAddLabel(&t->trans, l, &added);
                if (!added){
                    ASSERT(l->ref > 1);
                    pddlLabelsSetDecRef(&tss->label, l);
                }
            }
            pddlTransitionsAdd(&ltr->trans, from, to);
        }
    }
    pddlISetFree(&label);
}

static void freeLabeledTransitions(pddl_trans_system_t *ts)
{
    pddl_trans_systems_t *tss = ts->trans_systems;
    for (int ti = 0; ti < ts->trans.trans_size; ++ti){
        pddl_labeled_transitions_t *t = ts->trans.trans + ti;
        pddlLabelsSetDecRef(&tss->label, t->label);
    }
    pddlLabeledTransitionsSetFree(&ts->trans);
}

static int transSystemsAddTS(pddl_trans_systems_t *tss,
                             pddl_trans_system_t *ts)
{
    if (tss->ts_size == tss->ts_alloc){
        if (tss->ts_alloc == 0)
            tss->ts_alloc = 4;
        tss->ts_alloc *= 2;
        tss->ts = REALLOC_ARR(tss->ts, pddl_trans_system_t *,
                              tss->ts_alloc);
    }

    int ts_id = tss->ts_size++;
    tss->ts[ts_id] = ts;
    return ts_id;
}
