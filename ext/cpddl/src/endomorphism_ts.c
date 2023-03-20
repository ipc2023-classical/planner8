/***
 * Copyright (c)2022 Daniel Fiser <danfis@danfis.cz>
 *
 *  This file is part of cpddl.
 *
 *  Distributed under the OSI-approved BSD License (the "License");
 *  see accompanying file BDS-LICENSE for details or see
 *  <http://www.opensource.org/licenses/bsd-license.php>.
 *
 *  This software is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the License for more information.
 */

#include "internal.h"
#include "pddl/endomorphism.h"
#include "pddl/cp.h"
#include "pddl/time_limit.h"
#include "pddl/hfunc.h"
#include "pddl/sort.h"


struct label_groups {
    pddl_iset_t init_loop;
    pddl_iset_t loop;
    pddl_iset_t init_to;
    pddl_iset_t init_from;
    pddl_iset_t goal_to;
    pddl_iset_t goal_from;
    pddl_iset_t goal;
};
typedef struct label_groups label_groups_t;

struct ts_presolve {
    int **op_allow;
    int *op_identity;
    int ***state_allow;
};
typedef struct ts_presolve ts_presolve_t;

struct iset_ptr_arr {
    pddl_iset_t **set;
    int set_size;
    int set_alloc;
};
typedef struct iset_ptr_arr iset_ptr_arr_t;

static void isetPtrArrAdd(iset_ptr_arr_t *arr, pddl_iset_t *set)
{
    if (arr->set_size == arr->set_alloc){
        if (arr->set_alloc == 0)
            arr->set_alloc = 2;
        arr->set_alloc *= 2;
        arr->set = REALLOC_ARR(arr->set, pddl_iset_t *, arr->set_alloc);
    }
    arr->set[arr->set_size++] = set;
}

static void isetPtrArrFree(iset_ptr_arr_t *arr)
{
    if (arr->set != NULL)
        FREE(arr->set);
}

static void presolveFree(ts_presolve_t *presolve,
                         const pddl_trans_systems_t *tss)
{
    if (presolve->op_allow != NULL){
        for (int i = 0; i < tss->label.label_size; ++i)
            FREE(presolve->op_allow[i]);
        FREE(presolve->op_allow);
    }
    if (presolve->op_identity != NULL)
        FREE(presolve->op_identity);
    if (presolve->state_allow != NULL){
        for (int tsi = 0; tsi < tss->ts_size; ++tsi){
            if (presolve->state_allow[tsi] == NULL)
                continue;
            for (int i = 0; i < tss->ts[tsi]->num_states; ++i){
                if (presolve->state_allow[tsi][i] != NULL)
                    FREE(presolve->state_allow[tsi][i]);
            }
            FREE(presolve->state_allow[tsi]);
        }
        FREE(presolve->state_allow);
    }
}

static int cmpISetSize(const void *a, const void *b, void *_)
{
    const pddl_iset_t *s1 = *(pddl_iset_t **)a;
    const pddl_iset_t *s2 = *(pddl_iset_t **)b;
    return pddlISetSize(s1) - pddlISetSize(s2);
}

static void presolveTRStateAllow(pddl_iset_t *state_allow,
                                 const pddl_endomorphism_config_t *cfg,
                                 const ts_presolve_t *presolve,
                                 const pddl_trans_systems_t *tss,
                                 int tsi,
                                 int from,
                                 int label,
                                 int to)
{
    const pddl_trans_system_t *ts = tss->ts[tsi];
    int olabel, ofrom, oto;
    int from_is_goal = pddlISetIn(from, &ts->goal_states);
    int to_is_goal = pddlISetIn(to, &ts->goal_states);
    int label_cost = tss->label.label[label].cost;
    const int *op_allow = presolve->op_allow[label];
    PDDL_ISET(from_allow);
    PDDL_ISET(to_allow);
    PDDL_LABELED_TRANSITIONS_SET_FOR_EACH(&ts->trans, ofrom, olabel, oto){
        if (!op_allow[olabel])
            continue;
        if (!cfg->ignore_costs && tss->label.label[olabel].cost > label_cost)
            continue;
        if (from == ts->init_state && ofrom != from)
            continue;
        if (to == ts->init_state && oto != to)
            continue;
        if (from == to && ofrom != oto)
            continue;
        if (from_is_goal && !pddlISetIn(ofrom, &ts->goal_states))
            continue;
        if (to_is_goal && !pddlISetIn(oto, &ts->goal_states))
            continue;
        if (!pddlISetIn(ofrom, state_allow + from))
            continue;
        if (!pddlISetIn(oto, state_allow + to))
            continue;
        pddlISetAdd(&from_allow, ofrom);
        pddlISetAdd(&to_allow, oto);
    }
    pddlISetIntersect(state_allow + from, &from_allow);
    pddlISetIntersect(state_allow + to, &to_allow);
    pddlISetFree(&from_allow);
    pddlISetFree(&to_allow);
}

static void stateAllowFree(pddl_iset_t *state_allow, int num_states)
{
    for (int si = 0; si < num_states; ++si)
        pddlISetFree(state_allow + si);
    FREE(state_allow);
}

static int presolveStateAllow(ts_presolve_t *presolve,
                              const pddl_endomorphism_config_t *cfg,
                              const pddl_trans_systems_t *tss,
                              int tsi,
                              pddl_time_limit_t *time_limit)
{
    const pddl_trans_system_t *ts = tss->ts[tsi];

    pddl_iset_t *state_allow = CALLOC_ARR(pddl_iset_t, ts->num_states);
    for (int si = 0; si < ts->num_states; ++si){
        if (pddlTimeLimitCheck(time_limit) != 0){
            stateAllowFree(state_allow, ts->num_states);
            return -1;
        }

        if (si == ts->init_state){
            pddlISetAdd(state_allow + si, si);

        }else if (pddlISetIn(si, &ts->goal_states)){
            int si2;
            PDDL_ISET_FOR_EACH(&ts->goal_states, si2)
                pddlISetAdd(state_allow + si, si2);

        }else{
            for (int si2 = 0; si2 < ts->num_states; ++si2)
                pddlISetAdd(state_allow + si, si2);
        }
    }

    int label_id, from, to;
    PDDL_LABELED_TRANSITIONS_SET_FOR_EACH(&ts->trans, from, label_id, to){
        presolveTRStateAllow(state_allow, cfg, presolve,
                             tss, tsi, from, label_id, to);
    }
    for (int si = 0; si < ts->num_states; ++si){
        int state;
        PDDL_ISET_FOR_EACH(state_allow + si, state)
            presolve->state_allow[tsi][si][state] = 1;
    }
    stateAllowFree(state_allow, ts->num_states);
    return 0;
}

static void labelGroupsFree(const pddl_trans_systems_t *tss,
                            label_groups_t *label_group)
{
    for (int tsi = 0; tsi < tss->ts_size; ++tsi){
        pddlISetFree(&label_group[tsi].init_loop);
        pddlISetFree(&label_group[tsi].loop);
        pddlISetFree(&label_group[tsi].init_to);
        pddlISetFree(&label_group[tsi].init_from);
        pddlISetFree(&label_group[tsi].goal_to);
        pddlISetFree(&label_group[tsi].goal_from);
        pddlISetFree(&label_group[tsi].goal);
    }
    FREE(label_group);
}

static int tsPresolve(ts_presolve_t *presolve,
                      const pddl_endomorphism_config_t *cfg,
                      const pddl_trans_systems_t *tss,
                      pddl_time_limit_t *time_limit,
                      pddl_err_t *err)
{
    presolve->op_identity = CALLOC_ARR(int, tss->label.label_size);
    presolve->op_allow = CALLOC_ARR(int *, tss->label.label_size);
    presolve->state_allow = CALLOC_ARR(int **, tss->ts_size);
    for (int tsi = 0; tsi < tss->ts_size; ++tsi){
        if (pddlTimeLimitCheck(time_limit) != 0)
            return -1;
        int num_states = tss->ts[tsi]->num_states;
        presolve->state_allow[tsi] = CALLOC_ARR(int *, num_states);
        for (int si = 0; si < tss->ts[tsi]->num_states; ++si)
            presolve->state_allow[tsi][si] = CALLOC_ARR(int, num_states);
    }

    label_groups_t *label_group;
    label_group = CALLOC_ARR(label_groups_t, tss->ts_size);

    iset_ptr_arr_t *relevant = CALLOC_ARR(iset_ptr_arr_t, tss->label.label_size);
    for (int tsi = 0; tsi < tss->ts_size; ++tsi){
        if (pddlTimeLimitCheck(time_limit) != 0){
            labelGroupsFree(tss, label_group);
            return -1;
        }
        const pddl_trans_system_t *ts = tss->ts[tsi];
        int consider_goal = (pddlISetSize(&ts->goal_states) != ts->num_states);
        const pddl_label_set_t *labels;
        int from, to;
        PDDL_LABELED_TRANSITIONS_SET_FOR_EACH_LABEL_SET(&ts->trans,
                                                        from, labels, to){
            if (from == to){
                if (from == ts->init_state)
                    pddlISetUnion(&label_group[tsi].init_loop, &labels->label);
                pddlISetUnion(&label_group[tsi].loop, &labels->label);
            }

            int from_init = (from == ts->init_state);
            int to_init = (to == ts->init_state);
            if (from_init)
                pddlISetUnion(&label_group[tsi].init_from, &labels->label);
            if (to_init)
                pddlISetUnion(&label_group[tsi].init_to, &labels->label);

            if (consider_goal){
                int from_goal = pddlISetIn(from, &ts->goal_states);
                int to_goal = pddlISetIn(to, &ts->goal_states);
                if (from_goal)
                    pddlISetUnion(&label_group[tsi].goal_from, &labels->label);
                if (to_goal)
                    pddlISetUnion(&label_group[tsi].goal_to, &labels->label);
                if (from_goal && to_goal)
                    pddlISetUnion(&label_group[tsi].goal, &labels->label);
            }
        }
        int op;
        PDDL_ISET_FOR_EACH(&label_group[tsi].init_loop, op)
            isetPtrArrAdd(relevant + op, &label_group[tsi].init_loop);
        PDDL_ISET_FOR_EACH(&label_group[tsi].loop, op)
            isetPtrArrAdd(relevant + op, &label_group[tsi].loop);
        PDDL_ISET_FOR_EACH(&label_group[tsi].init_to, op)
            isetPtrArrAdd(relevant + op, &label_group[tsi].init_to);
        PDDL_ISET_FOR_EACH(&label_group[tsi].init_from, op)
            isetPtrArrAdd(relevant + op, &label_group[tsi].init_from);
        PDDL_ISET_FOR_EACH(&label_group[tsi].goal_to, op)
            isetPtrArrAdd(relevant + op, &label_group[tsi].goal_to);
        PDDL_ISET_FOR_EACH(&label_group[tsi].goal_from, op)
            isetPtrArrAdd(relevant + op, &label_group[tsi].goal_from);
        PDDL_ISET_FOR_EACH(&label_group[tsi].goal, op)
            isetPtrArrAdd(relevant + op, &label_group[tsi].goal);
    }

    for (int op_id = 0; op_id < tss->label.label_size; ++op_id){
        if (pddlTimeLimitCheck(time_limit) != 0){
            labelGroupsFree(tss, label_group);
            return -1;
        }

        if (relevant[op_id].set_size == 0){
            presolve->op_allow[op_id] = ALLOC_ARR(int, tss->label.label_size);
            for (int i = 0; i < tss->label.label_size; ++i)
                presolve->op_allow[op_id][i] = 1;
            continue;
        }

        pddlSort(relevant[op_id].set, relevant[op_id].set_size,
                 sizeof(pddl_iset_t *), cmpISetSize, NULL);

        PDDL_ISET(allowed);
        pddlISetUnion(&allowed, relevant[op_id].set[0]);
        for (size_t i = 1; i < relevant[op_id].set_size; ++i)
            pddlISetIntersect(&allowed, relevant[op_id].set[i]);

        PDDL_ISET(allowed2);
        int op_cost = tss->label.label[op_id].cost;
        int other_op;
        PDDL_ISET_FOR_EACH(&allowed, other_op){
            if (cfg->ignore_costs || tss->label.label[other_op].cost <= op_cost)
                pddlISetAdd(&allowed2, other_op);
        }
        pddlISetFree(&allowed);

        if (pddlISetSize(&allowed2) <= 1){
            presolve->op_identity[op_id] = 1;
            presolve->op_allow[op_id] = CALLOC_ARR(int, tss->label.label_size);
            presolve->op_allow[op_id][op_id] = 1;
        }else{
            presolve->op_allow[op_id] = CALLOC_ARR(int, tss->label.label_size);
            PDDL_ISET_FOR_EACH(&allowed2, other_op)
                presolve->op_allow[op_id][other_op] = 1;
        }
        pddlISetFree(&allowed2);
    }

    labelGroupsFree(tss, label_group);
    for (int op_id = 0; op_id < tss->label.label_size; ++op_id)
        isetPtrArrFree(relevant + op_id);
    LOG(err, "presolve restriction of operator domains done");

    for (int tsi = 0; tsi < tss->ts_size; ++tsi){
        if (presolveStateAllow(presolve, cfg, tss, tsi, time_limit) != 0)
            return -1;
    }
    LOG(err, "presolve restriction of state domains done");

    return 0;
}

static int transConstraints(const pddl_trans_systems_t *tss,
                            int tsi,
                            const ts_presolve_t *presolve,
                            int from,
                            int label,
                            int to,
                            pddl_cp_t *cp,
                            int var_state_offset,
                            int var_label_offset,
                            pddl_err_t *err)
{
    const pddl_trans_system_t *ts = tss->ts[tsi];

    int var[3];
    var[0] = label + var_label_offset;
    var[1] = from + var_state_offset;
    var[2] = to + var_state_offset;

    int val_alloc = 2;
    int val_size = 0;
    int *val = ALLOC_ARR(int, 3 * val_alloc);
    int olabel, ofrom, oto;
    const int *op_allow = presolve->op_allow[label];
    PDDL_LABELED_TRANSITIONS_SET_FOR_EACH(&ts->trans, ofrom, olabel, oto){
        if (!op_allow[olabel]
                || !presolve->state_allow[tsi][from][ofrom]
                || !presolve->state_allow[tsi][to][oto]){
            continue;
        }
        if (from == to && ofrom != oto)
            continue;
        ASSERT(from != ts->init_state || ofrom == from);
        ASSERT(to != ts->init_state || oto == to);
        if (val_size == val_alloc){
            val_alloc *= 2;
            val = REALLOC_ARR(val, int, 3 * val_alloc);
        }
        val[3 * val_size] = olabel;
        val[3 * val_size + 1] = ofrom;
        val[3 * val_size + 2] = oto;
        ++val_size;
    }
    if (val_size == 0){
        LOG(err, "Could not find mapping for (%d)->%d->(%d)",
            from, label, to);
        return 0;
    }

    pddlCPAddConstrIVarAllowed(cp, 3, var, val_size, val);
    FREE(val);
    return 1;
}

static int tsConstraints(const pddl_trans_systems_t *tss,
                         const ts_presolve_t *presolve,
                         int tsi,
                         pddl_cp_t *cp,
                         int var_state_offset,
                         int var_label_offset,
                         pddl_time_limit_t *time_limit,
                         pddl_err_t *err)
{
    int num_constrs = 0;
    const pddl_trans_system_t *ts = tss->ts[tsi];

    // Add init constraint
    ASSERT(ts->init_state >= 0);
    pddlCPAddConstrIVarEq(cp, var_state_offset + ts->init_state, ts->init_state);
    num_constrs += 1;

    // Goal constraints
    int goal_state;
    PDDL_ISET_FOR_EACH(&ts->goal_states, goal_state){
        pddlCPAddConstrIVarDomainArr(cp, goal_state + var_state_offset,
                                     ts->goal_states.size,
                                     ts->goal_states.s);
        num_constrs += 1;
    }

    // Transition constraints
    int label_id, from, to;
    PDDL_LABELED_TRANSITIONS_SET_FOR_EACH(&ts->trans, from, label_id, to){
        if (pddlTimeLimitCheck(time_limit) != 0)
            return -1;
        num_constrs += transConstraints(tss, tsi, presolve, from, label_id, to,
                                        cp, var_state_offset, var_label_offset,
                                        err);
    }
    return num_constrs;
}

static int tsSetModel(const pddl_trans_systems_t *tss,
                      const pddl_endomorphism_config_t *cfg,
                      const ts_presolve_t *presolve,
                      pddl_cp_t *cp,
                      int *var_label_offset_out,
                      pddl_time_limit_t *time_limit,
                      pddl_err_t *err)
{
    // Create state variables
    int var_label_offset = 0;
    int *var_state_offset = ALLOC_ARR(int, tss->ts_size);
    for (int tsi = 0, sid = 0; tsi < tss->ts_size; ++tsi){
        int ts_num_states = tss->ts[tsi]->num_states;
        var_state_offset[tsi] = sid;
        for (int i = 0; i < ts_num_states; ++i){
            char name[128];
            snprintf(name, 128, "%d:%d:%d", tsi, i, sid);
            pddlCPAddIVar(cp, 0, ts_num_states - 1, name);
            sid++;
        }
        var_label_offset = sid;
    }

    // Create operator variables
    for (int li = 0; li < tss->label.label_size; ++li){
        char name[128];
        snprintf(name, 128, "O%d", li);
        pddlCPAddIVar(cp, 0, tss->label.label_size - 1, name);
    }

    if (pddlTimeLimitCheck(time_limit) != 0){
        FREE(var_state_offset);
        return -1;
    }

    // Operator identity constraints
    int num_ident = 0;
    for (int op_id = 0; op_id < tss->label.label_size; ++op_id){
        if (presolve->op_identity[op_id]){
            pddlCPAddConstrIVarEq(cp, op_id + var_label_offset, op_id);
            ++num_ident;
        }
    }
    LOG(err, "Set %d operator-identity bounds", num_ident);

    if (pddlTimeLimitCheck(time_limit) != 0){
        FREE(var_state_offset);
        return -1;
    }

    int num_constrs = 0;
    for (int tsi = 0; tsi < tss->ts_size; ++tsi){
        if (pddlTimeLimitCheck(time_limit) != 0){
            FREE(var_state_offset);
            return -1;
        }

        int num = tsConstraints(tss, presolve, tsi, cp,
                                var_state_offset[tsi], var_label_offset,
                                time_limit, err);
        if (num < 0)
            return -1;
        LOG(err, "Added %d constraints for TS %d with %d states",
            num, tsi, tss->ts[tsi]->num_states);
        num_constrs += num;
    }
    LOG(err, "Added %d constraints overall", num_constrs);

    PDDL_ISET(labels);
    for (int op_id = 0; op_id < tss->label.label_size; ++op_id)
        pddlISetAdd(&labels, op_id + var_label_offset);
    pddlCPSetObjectiveMinCountDiff(cp, &labels);
    pddlISetFree(&labels);
    LOG(err, "Added objective function");

    if (pddlTimeLimitCheck(time_limit) != 0){
        FREE(var_state_offset);
        return -1;
    }

    *var_label_offset_out = var_label_offset;
    FREE(var_state_offset);
    return 0;
}

static void setSolIdentity(pddl_endomorphism_sol_t *sol, int label_size)
{
    sol->op_size = label_size;
    sol->op_map = ALLOC_ARR(int, label_size);
    for (int oi = 0; oi < label_size; ++oi)
        sol->op_map[oi] = oi;
}

static void extractSol(const int *cpsol,
                       int label_size,
                       int var_label_offset,
                       pddl_endomorphism_sol_t *sol)
{
    sol->op_size = label_size;
    sol->op_map = ALLOC_ARR(int, label_size);
    memcpy(sol->op_map, cpsol + var_label_offset, sizeof(int) * label_size);

    int *mapped_to = CALLOC_ARR(int, label_size);
    for (int oi = 0; oi < label_size; ++oi)
        mapped_to[sol->op_map[oi]] = 1;
    for (int oi = 0; oi < label_size; ++oi){
        if (mapped_to[oi]){
            // get rid of symmetries
            sol->op_map[oi] = oi;
        }else{
            pddlISetAdd(&sol->redundant_ops, oi);
        }
    }

    if (mapped_to != NULL)
        FREE(mapped_to);
}


int pddlEndomorphismTransSystem(const pddl_trans_systems_t *tss,
                                const pddl_endomorphism_config_t *cfg,
                                pddl_endomorphism_sol_t *sol,
                                pddl_err_t *err)
{
    CTX(err, "endo_ts", "Endo-TS");
    ZEROIZE(sol);

    pddl_time_limit_t time_limit;
    pddlTimeLimitInit(&time_limit);
    if (cfg->max_time > 0.)
        pddlTimeLimitSet(&time_limit, cfg->max_time);

    int ret = 0;
    LOG(err, "Endomorphism on factored TS (num-ts: %d, num-labels: %d) ...",
        tss->ts_size, tss->label.label_size);
    ts_presolve_t presolve;
    LOG(err, "Running presolve...");
    if (tsPresolve(&presolve, cfg, tss, &time_limit, err) != 0){
        LOG(err, "Time limit reached");
        LOG(err, "Terminating presolve phase");
        LOG(err, "Terminating inference of endomorphism");
        LOG(err, "Terminated by a time limit");
        return -1;
    }

    int num_identity = 0;
    for (size_t i = 0; i < tss->label.label_size; ++i)
        num_identity += presolve.op_identity[i];
    LOG(err, "Presolve found %d identity operators", num_identity);

    if (num_identity == tss->label.label_size){
        LOG(err, "All operators are identity");
        LOG(err, "Found 0 redundant operators");
        setSolIdentity(sol, tss->label.label_size);
        CTXEND(err);
        return 0;
    }

    pddl_cp_t cp;
    pddlCPInit(&cp);
    int var_label_offset;
    if (tsSetModel(tss, cfg, &presolve, &cp, &var_label_offset,
                   &time_limit, err) != 0){
        presolveFree(&presolve, tss);
        pddlCPFree(&cp);
        CTXEND(err);
        PDDL_TRACE_RET(err, -2);
    }
    LOG(err, "Created model.");

    pddlCPSimplify(&cp);
    LOG(err, "Model simplified.");

    /*
    FILE *fout = fopen("model.mzn", "w");
    pddlCPWriteMinizinc(&cp, fout);
    fclose(fout);
    */

    pddl_cp_solve_config_t sol_cfg = PDDL_CP_SOLVE_CONFIG_INIT;
    if (cfg->max_search_time > 0)
        sol_cfg.max_search_time = pddlTimeLimitRemain(&time_limit);
    sol_cfg.run_in_subprocess = cfg->run_in_subprocess;

    pddl_cp_sol_t cpsol;
    int sret = pddlCPSolve(&cp, &sol_cfg, &cpsol, err);
    // There must exist a solution -- at least identity
    ASSERT_RUNTIME(sret == PDDL_CP_FOUND
                    || sret == PDDL_CP_FOUND_SUBOPTIMAL
                    || sret == PDDL_CP_ABORTED);
    if (sret == PDDL_CP_FOUND || sret == PDDL_CP_FOUND_SUBOPTIMAL){
        ASSERT_RUNTIME(cpsol.num_solutions == 1);
        extractSol(cpsol.isol[0], tss->label.label_size, var_label_offset, sol);
        LOG(err, "Found a solution with %d redundant ops",
            pddlISetSize(&sol->redundant_ops));
        if (sret == PDDL_CP_FOUND)
            sol->is_optimal = 1;

    }else if (sret == PDDL_CP_ABORTED){
        LOG(err, "Solver was aborted.");
        ret = -1;
    }
    pddlCPSolFree(&cpsol);

    pddlCPFree(&cp);
    presolveFree(&presolve, tss);
    CTXEND(err);
    return ret;
}

int pddlEndomorphismTransSystemRedundantOps(const pddl_trans_systems_t *tss,
                                            const pddl_endomorphism_config_t *c,
                                            pddl_iset_t *redundant_ops,
                                            pddl_err_t *err)
{
    pddl_endomorphism_sol_t sol;
    int ret = pddlEndomorphismTransSystem(tss, c, &sol, err);
    if (ret == 0)
        pddlISetUnion(redundant_ops, &sol.redundant_ops);
    pddlEndomorphismSolFree(&sol);
    return ret;
}
