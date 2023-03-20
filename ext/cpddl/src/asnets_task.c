/***
 * Copyright (c)2022 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

#include "internal.h"
#include "pddl/asnets_task.h"
#include "pddl/lifted_mgroup_infer.h"
#include "pddl/strips_ground_datalog.h"
#include "pddl/critical_path.h"
#include "pddl/sha256.h"

static void addRelatedAction(pddl_asnets_pred_t *pred, int action_id, int pos)
{
    if (pred->related_action_size == pred->related_action_alloc){
        if (pred->related_action_alloc == 0)
            pred->related_action_alloc = 2;
        pred->related_action_alloc *= 2;
        pred->related_action = REALLOC_ARR(pred->related_action,
                                           pddl_asnets_action_pos_t,
                                           pred->related_action_alloc);
    }

    pred->related_action[pred->related_action_size].action_id = action_id;
    pred->related_action[pred->related_action_size].pos = pos;
    ++pred->related_action_size;
}

static int addRelatedAtom(pddl_asnets_action_t *action,
                          const pddl_fm_atom_t *atom)
{
    if (action->related_atom_size == action->related_atom_alloc){
        if (action->related_atom_alloc == 0)
            action->related_atom_alloc = 2;
        action->related_atom_alloc *= 2;
        action->related_atom = REALLOC_ARR(action->related_atom,
                                           const pddl_fm_atom_t *,
                                           action->related_atom_alloc);
    }

    int pos = action->related_atom_size;
    action->related_atom[action->related_atom_size++] = atom;
    return pos;
}

static int addUniqueRelatedAtom(pddl_asnets_action_t *action,
                                const pddl_fm_atom_t *atom)
{
    for (int i = 0; i < action->related_atom_size; ++i){
        const pddl_fm_atom_t *a = action->related_atom[i];
        if (a->pred == atom->pred){
            int eq = 1;
            for (int ai = 0; ai < atom->arg_size; ++ai){
                if (a->arg[ai].obj != atom->arg[ai].obj
                        || a->arg[ai].param != atom->arg[ai].param){
                    eq = 0;
                    break;
                }
            }
            if (eq)
                return -1;
        }
    }
    return addRelatedAtom(action, atom);
}

int pddlASNetsLiftedTaskInit(pddl_asnets_lifted_task_t *lt,
                             const char *domain_fn,
                             pddl_err_t *err)
{
    CTX(err, "asnets_lifted_task", "ASNets-LiftedTask");
    ZEROIZE(lt);
    pddl_config_t pddl_cfg = PDDL_CONFIG_INIT;
    pddl_cfg.force_adl = 1;
    pddl_cfg.normalize = 1;
    pddl_cfg.enforce_unit_cost = 1;
    pddl_cfg.remove_empty_types = 0;
    pddl_cfg.compile_away_cond_eff = 0;
    pddl_cfg.keep_all_actions = 1;
    if (pddlInit(&lt->pddl, domain_fn, NULL, &pddl_cfg, err) != 0){
        CTXEND(err);
        TRACE_RET(err, -1);
    }

    lt->action_size = lt->pddl.action.action_size;
    lt->action = CALLOC_ARR(pddl_asnets_action_t, lt->action_size);

    lt->pred_size = lt->pddl.pred.pred_size;
    lt->pred = CALLOC_ARR(pddl_asnets_pred_t, lt->pred_size);
    for (int pi = 0; pi < lt->pddl.pred.pred_size; ++pi)
        lt->pred[pi].pred_id = pi;

    for (int ai = 0; ai < lt->pddl.action.action_size; ++ai){
        const pddl_action_t *a = lt->pddl.action.action + ai;
        lt->action[ai].action_id = ai;

        const pddl_fm_atom_t *at;
        pddl_fm_const_it_atom_t it;
        PDDL_FM_FOR_EACH_ATOM(a->pre, &it, at){
            if (at->pred == lt->pddl.pred.eq_pred)
                continue;
            int pos;
            if ((pos = addUniqueRelatedAtom(lt->action + ai, at)) >= 0)
                addRelatedAction(lt->pred + at->pred, ai, pos);
        }
        PDDL_FM_FOR_EACH_ATOM(a->eff, &it, at){
            ASSERT_RUNTIME(at->pred != lt->pddl.pred.eq_pred);
            int pos;
            if ((pos = addUniqueRelatedAtom(lt->action + ai, at)) >= 0)
                addRelatedAction(lt->pred + at->pred, ai, pos);
        }
    }
    CTXEND(err);
    return 0;
}

void pddlASNetsLiftedTaskFree(pddl_asnets_lifted_task_t *lt)
{
    for (int ai = 0; ai < lt->action_size; ++ai){
        if (lt->action[ai].related_atom != NULL)
            FREE(lt->action[ai].related_atom);
    }
    if (lt->action != NULL)
        FREE(lt->action);

    for (int pi = 0; pi < lt->pred_size; ++pi){
        if (lt->pred[pi].related_action != NULL)
            FREE(lt->pred[pi].related_action);
    }
    if (lt->pred != NULL)
        FREE(lt->pred);
    pddlFree(&lt->pddl);
}

void pddlASNetsLiftedTaskToSHA256(const pddl_asnets_lifted_task_t *lt,
                                  char *hash_str)
{
    pddl_sha256_t sha;
    char hash[PDDL_SHA256_HASH_SIZE];
    pddlSHA256Init(&sha);
    for (int i = 0; i < lt->pddl.type.type_size; ++i){
        const pddl_type_t *type = lt->pddl.type.type + i;
        if (type->name != NULL)
            pddlSHA256Update(&sha, type->name, strlen(type->name));
    }
    for (int i = 0; i < lt->pddl.pred.pred_size; ++i){
        const pddl_pred_t *pred = lt->pddl.pred.pred + i;
        if (pred->name != NULL)
            pddlSHA256Update(&sha, pred->name, strlen(pred->name));
    }
    for (int i = 0; i < lt->pddl.action.action_size; ++i){
        const pddl_action_t *action = lt->pddl.action.action + i;
        if (action->name != NULL)
            pddlSHA256Update(&sha, action->name, strlen(action->name));
    }
    pddlSHA256Finalize(&sha, hash);
    pddlSHA256ToStr(hash, hash_str);
}

static int atomEq(const pddl_ground_atom_t *a1,
                  const pddl_fm_atom_t *a2,
                  const pddl_obj_id_t *args)
{
    if (a1->pred != a2->pred)
        return 0;
    for (int argi = 0; argi < a1->arg_size; ++argi){
        pddl_obj_id_t obj2 = a2->arg[argi].obj;
        if (a2->arg[argi].param >= 0)
            obj2 = args[a2->arg[argi].param];
        if (a1->arg[argi] != obj2)
            return 0;
    }
    return 1;
}

static void addRelatedOp(pddl_asnets_ground_task_t *gt,
                         int fact_id, int op_id, int pos)
{
    const pddl_asnets_action_t *action = gt->op[op_id].action;
    const pddl_asnets_pred_t *pred = gt->fact[fact_id].pred;
    for (size_t i = 0; i < pred->related_action_size; ++i){
        if (pred->related_action[i].action_id == action->action_id
                && pred->related_action[i].pos == pos){
            pddlIArrAdd(&gt->fact[fact_id].related_op[i], op_id);
            return;
        }
    }
    PANIC("Error: Cannot find the right action/pos related to a fact");
}

static void computeGroundRelatedness(pddl_asnets_ground_task_t *gt,
                                     pddl_err_t *err)
{
    CTX(err, "relatedness", "Relatedness");
    gt->op_size = gt->strips.op.op_size;
    gt->op = CALLOC_ARR(pddl_asnets_op_t, gt->op_size);
    for (int i = 0; i < gt->op_size; ++i){
        gt->op[i].op_id = i;
        ASSERT(gt->strips.op.op[i]->pddl_action_id >= 0);
        gt->op[i].action = gt->lifted_task->action + gt->strips.op.op[i]->pddl_action_id;
        gt->op[i].related_fact_size = gt->op[i].action->related_atom_size;
        gt->op[i].related_fact = ALLOC_ARR(int, gt->op[i].related_fact_size);
        for (int j = 0; j < gt->op[i].related_fact_size; ++j)
            gt->op[i].related_fact[j] = -1;
    }

    gt->fact_size = gt->strips.fact.fact_size;
    gt->fact = CALLOC_ARR(pddl_asnets_fact_t, gt->fact_size);
    for (int i = 0; i < gt->fact_size; ++i){
        gt->fact[i].fact_id = i;
        ASSERT(gt->strips.fact.fact[i]->ground_atom != NULL);
        gt->fact[i].pred = gt->lifted_task->pred + gt->strips.fact.fact[i]->ground_atom->pred;
        gt->fact[i].related_op_size = gt->fact[i].pred->related_action_size;
        gt->fact[i].related_op = CALLOC_ARR(pddl_iarr_t, gt->fact[i].related_op_size);
    }

    for (int op_id = 0; op_id < gt->op_size; ++op_id){
        const pddl_strips_op_t *so = gt->strips.op.op[op_id];
        pddl_asnets_op_t *op = gt->op + op_id;
        const pddl_obj_id_t *oargs = so->action_args;

        // TODO: Conditional effects not supported yet
        ASSERT(so->cond_eff_size == 0);
        ASSERT(so->action_args != NULL);
        ASSERT(so->pddl_action_id >= 0);

        PDDL_ISET(facts);
        pddlISetUnion(&facts, &so->pre);
        pddlISetUnion(&facts, &so->add_eff);
        pddlISetUnion(&facts, &so->del_eff);
        int fact_id;
        PDDL_ISET_FOR_EACH(&facts, fact_id){
            const pddl_ground_atom_t *fatom = gt->strips.fact.fact[fact_id]->ground_atom;
            for (size_t pos = 0; pos < op->action->related_atom_size; ++pos){
                const pddl_fm_atom_t *atom = op->action->related_atom[pos];
                if (atomEq(fatom, atom, oargs)){
                    ASSERT_RUNTIME(op->related_fact[pos] < 0);
                    op->related_fact[pos] = fact_id;
                    addRelatedOp(gt, fact_id, op_id, pos);
                }
            }
        }
        pddlISetFree(&facts);
    }
    CTXEND(err);
}

static int checkGroundRelatedness(const pddl_asnets_ground_task_t *gt,
                                  pddl_err_t *err)
{
    // TODO: Replace asserts with reporting what exactly is wrong
    LOG(err, "Checking everything is properly set up...");
    ASSERT_RUNTIME(gt->fdr.op.op_size == gt->strips.op.op_size);
    ASSERT_RUNTIME(gt->strips.op.op_size == gt->op_size);
    for (int op_id = 0; op_id < gt->op_size; ++op_id){
        ASSERT_RUNTIME(gt->op[op_id].related_fact_size
                            == gt->op[op_id].action->related_atom_size);
        for (int i = 0; i < gt->op[op_id].related_fact_size; ++i){
            ASSERT_RUNTIME(gt->op[op_id].related_fact[i] >= 0);
        }
    }

    ASSERT_RUNTIME(gt->strips.fact.fact_size == gt->fact_size);
    for (int fact_id = 0; fact_id < gt->fact_size; ++fact_id){
        ASSERT_RUNTIME(gt->fact[fact_id].related_op_size
                            == gt->fact[fact_id].pred->related_action_size);
        for (int i = 0; i < gt->fact[fact_id].related_op_size; ++i){
            const pddl_iarr_t *rop = gt->fact[fact_id].related_op + i;
            if (pddlIArrSize(rop) == 0){
                LOG(err, "Missing related operator."
                    " fact (%s), action/pos: %s/%d",
                    gt->strips.fact.fact[fact_id]->name,
                    gt->pddl.action.action[gt->fact[fact_id].pred->related_action[i].action_id].name,
                    gt->fact[fact_id].pred->related_action[i].pos);
            }
        }
    }
    LOG(err, "Check DONE.");
    return 1;
}

int pddlASNetsGroundTaskInit(pddl_asnets_ground_task_t *gt,
                             const pddl_asnets_lifted_task_t *lt,
                             const char *domain_fn,
                             const char *problem_fn,
                             pddl_err_t *err)
{
    CTX(err, "asnets_ground_task", "ASNets-GroundTask");
    ZEROIZE(gt);
    gt->lifted_task = lt;

    pddl_config_t pddl_cfg = PDDL_CONFIG_INIT;
    pddl_cfg.force_adl = 1;
    pddl_cfg.normalize = 1;
    pddl_cfg.enforce_unit_cost = 1;
    pddl_cfg.remove_empty_types = 0;
    pddl_cfg.compile_away_cond_eff = 0;
    pddl_cfg.keep_all_actions = 1;
    if (pddlInit(&gt->pddl, domain_fn, problem_fn, &pddl_cfg, err) != 0){
        CTXEND(err);
        TRACE_RET(err, -1);
    }

    ASSERT_RUNTIME(gt->pddl.action.action_size == lt->action_size);
    ASSERT_RUNTIME(gt->pddl.pred.pred_size == lt->pred_size);

    pddl_lifted_mgroups_infer_limits_t lifted_mgroups_limits
        = PDDL_LIFTED_MGROUPS_INFER_LIMITS_INIT;
    pddl_lifted_mgroups_t lmg;
    pddlLiftedMGroupsInit(&lmg);
    pddlLiftedMGroupsInferFAMGroups(&gt->pddl, &lifted_mgroups_limits, &lmg, err);

    pddl_ground_config_t ground_cfg = PDDL_GROUND_CONFIG_INIT;
    ground_cfg.prune_op_pre_mutex = 0;
    ground_cfg.prune_op_dead_end = 0;
    ground_cfg.remove_static_facts = 0;
    ground_cfg.keep_action_args = 1;
    ground_cfg.keep_all_static_facts = 1;
    if (pddlStripsGroundDatalog(&gt->strips, &gt->pddl, &ground_cfg, err) != 0){
        pddlFree(&gt->pddl);
        CTXEND(err);
        TRACE_RET(err, -1);
    }

    pddl_mutex_pairs_t mutex;
    PDDL_ISET(unreachable_op);
    PDDL_ISET(unreachable_fact);
    pddlMutexPairsInitStrips(&mutex, &gt->strips);
    pddlH2(&gt->strips, &mutex, &unreachable_fact, &unreachable_op, -1., err);
    pddlStripsReduce(&gt->strips, &unreachable_fact, &unreachable_op);
    pddlMutexPairsReduce(&mutex, &unreachable_fact);
    pddlISetFree(&unreachable_op);
    pddlISetFree(&unreachable_fact);

    pddl_mgroups_t mgroups;
    pddlMGroupsInitEmpty(&mgroups);
    pddlMGroupsGround(&mgroups, &gt->pddl, &lmg, &gt->strips);

    pddlFDRInitFromStrips(&gt->fdr, &gt->strips, &mgroups, &mutex,
                          PDDL_FDR_VARS_LARGEST_FIRST, 0, err);
    ASSERT_RUNTIME(gt->strips.op.op_size == gt->fdr.op.op_size);

    pddlFDRAppOpInit(&gt->fdr_app_op, &gt->fdr.var, &gt->fdr.op, &gt->fdr.goal);

    int *fact_in_fdr = CALLOC_ARR(int, gt->strips.fact.fact_size);
    for (int vari = 0; vari < gt->fdr.var.var_size; ++vari){
        for (int vali = 0; vali < gt->fdr.var.var[vari].val_size; ++vali){
            int strips_id = gt->fdr.var.var[vari].val[vali].strips_id;
            if (strips_id >= 0)
                fact_in_fdr[strips_id] = 1;
        }
    }
    for (int fact_id = 0; fact_id < gt->strips.fact.fact_size; ++fact_id){
        if (!fact_in_fdr[fact_id])
            pddlISetAdd(&gt->static_fact, fact_id);
    }
    FREE(fact_in_fdr);

    pddlMGroupsFree(&mgroups);
    pddlMutexPairsFree(&mutex);
    pddlLiftedMGroupsFree(&lmg);

    computeGroundRelatedness(gt, err);
    if (!checkGroundRelatedness(gt, err)){
        pddlASNetsGroundTaskFree(gt);
        CTXEND(err);
        TRACE_RET(err, -1);
    }

    CTXEND(err);
    return 0;
}

void pddlASNetsGroundTaskFree(pddl_asnets_ground_task_t *gt)
{
    for (int i = 0; i < gt->op_size; ++i){
        if (gt->op[i].related_fact != NULL)
            FREE(gt->op[i].related_fact);
    }
    if (gt->op != NULL)
        FREE(gt->op);

    for (int i = 0; i < gt->fact_size; ++i){
        for (int j = 0; j < gt->fact[i].related_op_size; ++j)
            pddlIArrFree(gt->fact[i].related_op + j);
        if (gt->fact[i].related_op != NULL)
            FREE(gt->fact[i].related_op);
    }
    if (gt->fact != NULL)
        FREE(gt->fact);

    pddlISetFree(&gt->static_fact);
    pddlFDRAppOpFree(&gt->fdr_app_op);
    pddlFDRFree(&gt->fdr);
    pddlStripsFree(&gt->strips);
    pddlFree(&gt->pddl);
}

void pddlASNetsGroundTaskFDRStateToStrips(const pddl_asnets_ground_task_t *gt,
                                          const int *fdr_state,
                                          pddl_iset_t *strips_state)
{
    pddlISetSet(strips_state, &gt->static_fact);
    for (int vari = 0; vari < gt->fdr.var.var_size; ++vari){
        int strips_id = gt->fdr.var.var[vari].val[fdr_state[vari]].strips_id;
        if (strips_id >= 0)
            pddlISetAdd(strips_state, strips_id);
    }
}

void pddlASNetsGroundTaskFDRApplicableOps(const pddl_asnets_ground_task_t *gt,
                                          const int *fdr_state,
                                          pddl_iset_t *ops)
{
    pddlISetEmpty(ops);
    pddlFDRAppOpFind(&gt->fdr_app_op, fdr_state, ops);
}

void pddlASNetsGroundTaskFDRGoal(const pddl_asnets_ground_task_t *gt,
                                 pddl_iset_t *strips_goal)
{
    pddlISetEmpty(strips_goal);
    for (int i = 0; i < gt->fdr.goal.fact_size; ++i){
        int var = gt->fdr.goal.fact[i].var;
        int val = gt->fdr.goal.fact[i].val;
        if (gt->fdr.var.var[var].val[val].strips_id >= 0)
            pddlISetAdd(strips_goal, gt->fdr.var.var[var].val[val].strips_id);
    }
}

void pddlASNetsGroundTaskFDRApplyOp(const pddl_asnets_ground_task_t *gt,
                                    const int *state,
                                    int op_id,
                                    int *out_state)
{
    pddlFDROpApplyOnState(gt->fdr.op.op[op_id], gt->fdr.var.var_size,
                          state, out_state);
}
