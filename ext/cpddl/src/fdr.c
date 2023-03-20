/***
 * cpddl
 * -------
 * Copyright (c)2019 Daniel Fiser <danfis@danfis.cz>,
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
#include "pddl/timer.h"
#include "pddl/fdr.h"
#include "pddl/disambiguation.h"
#include "pddl/cg.h"

static void stripsToFDRState(const pddl_fdr_vars_t *fdr_var,
                             const pddl_iset_t *state,
                             int *fdr_state);
static int stripsToFDRPartState(const pddl_fdr_vars_t *fdr_var,
                                const pddl_iset_t *part_state,
                                pddl_fdr_part_state_t *fdr_ps);
static void addOp(pddl_fdr_ops_t *fdr_ops,
                  const pddl_fdr_vars_t *fdr_var,
                  const pddl_strips_t *strips,
                  const pddl_mutex_pairs_t *mutex,
                  unsigned fdr_flags,
                  int op_id);

int pddlFDRInitFromStrips(pddl_fdr_t *fdr,
                          const pddl_strips_t *strips,
                          const pddl_mgroups_t *mg,
                          const pddl_mutex_pairs_t *mutex,
                          unsigned fdr_var_flags,
                          unsigned fdr_flags,
                          pddl_err_t *err)
{
    CTX(err, "fdr", "FDR");
    pddl_timer_t timer;
    pddlTimerStart(&timer);

    if (fdr_flags == PDDL_FDR_SET_NONE_OF_THOSE_IN_PRE){
        PDDL_INFO(err, "cfg.set_none_of_those_in_pre = 1");
    }else{
        PDDL_INFO(err, "cfg.set_none_of_those_in_pre = 0");
    }

    if ((fdr_var_flags & 0xfu) == PDDL_FDR_VARS_ESSENTIAL_FIRST){
        PDDL_INFO(err, "cfg.vars_selection_order = essential");
    }else if ((fdr_var_flags & 0xfu) == PDDL_FDR_VARS_LARGEST_FIRST){
        PDDL_INFO(err, "cfg.vars_selection_order = largest");
    }else if ((fdr_var_flags & 0xfu) == PDDL_FDR_VARS_LARGEST_FIRST_MULTI){
        PDDL_INFO(err, "cfg.vars_selection_order = largest-multi");
    }


    ZEROIZE(fdr);

    // variables
    if (pddlFDRVarsInitFromStrips(&fdr->var, strips, mg, mutex,
                                  fdr_var_flags) != 0){
        CTXEND(err);
        return -1;
    }
    LOG(err, "Created %{num_vars}d variables.", fdr->var.var_size);
    LOG(err, "Created %{num_facts}d facts.", fdr->var.global_id_size);
    int num_none_of_those = 0;
    for (int vi = 0; vi < fdr->var.var_size; ++vi){
        if (fdr->var.var[vi].val_none_of_those != -1)
            ++num_none_of_those;
    }
    LOG(err, "Created %{num_none_of_those}d none-of-those values.",
        num_none_of_those);

    fdr->goal_is_unreachable = strips->goal_is_unreachable;

    // Initial state
    fdr->init = ALLOC_ARR(int, fdr->var.var_size);
    stripsToFDRState(&fdr->var, &strips->init, fdr->init);
    PDDL_INFO(err, "Created initial state.");

    // Goal
    pddlFDRPartStateInit(&fdr->goal);
    stripsToFDRPartState(&fdr->var, &strips->goal, &fdr->goal);
    PDDL_INFO(err, "Created goal specification.");

    // Operators
    pddlFDROpsInit(&fdr->op);
    for (int op_id = 0; op_id < strips->op.op_size; ++op_id)
        addOp(&fdr->op, &fdr->var, strips, mutex, fdr_flags, op_id);
    LOG(err, "Created %{num_ops}d operators", fdr->op.op_size);

    pddlTimerStop(&timer);
    PDDL_LOG(err, "Translation took %{translation_time}.2f seconds",
             pddlTimerElapsedInSF(&timer));
    CTXEND(err);
    return 0;
}

void pddlFDRInitCopy(pddl_fdr_t *fdr, const pddl_fdr_t *fdr_in)
{
    ZEROIZE(fdr);
    pddlFDRVarsInitCopy(&fdr->var, &fdr_in->var);
    pddlFDROpsInitCopy(&fdr->op, &fdr_in->op);
    fdr->init = ALLOC_ARR(int, fdr->var.var_size);
    memcpy(fdr->init, fdr_in->init, sizeof(int) * fdr->var.var_size);
    pddlFDRPartStateInitCopy(&fdr->goal, &fdr_in->goal);
    fdr->goal_is_unreachable = fdr_in->goal_is_unreachable;
    fdr->has_cond_eff = fdr_in->has_cond_eff;
}

void pddlFDRFree(pddl_fdr_t *fdr)
{
    if (fdr->init != NULL)
        FREE(fdr->init);

    if (!fdr->is_shallow_copy){
        pddlFDRPartStateFree(&fdr->goal);
        pddlFDROpsFree(&fdr->op);
        pddlFDRVarsFree(&fdr->var);
    }
}

pddl_fdr_t *pddlFDRClone(const pddl_fdr_t *fdr_in)
{
    pddl_fdr_t *fdr = ALLOC(pddl_fdr_t);
    pddlFDRInitCopy(fdr, fdr_in);
    return fdr;
}

void pddlFDRDel(pddl_fdr_t *fdr)
{
    pddlFDRFree(fdr);
    FREE(fdr);
}

void pddlFDRInitShallowCopyWithDifferentInitState(pddl_fdr_t *fdr,
                                                  const pddl_fdr_t *fdr_in,
                                                  const int *state)
{
    *fdr = *fdr_in;
    fdr->init = ALLOC_ARR(int, fdr->var.var_size);
    memcpy(fdr->init, state, sizeof(int) * fdr->var.var_size);
    fdr->is_shallow_copy = 1;
}

void pddlFDRReorderVarsCG(pddl_fdr_t *fdr)
{
    int *ordering = CALLOC_ARR(int, fdr->var.var_size + 1);
    pddl_cg_t cg;
    pddlCGInit(&cg, &fdr->var, &fdr->op, 0);
    pddlCGVarOrdering(&cg, &fdr->goal, ordering);
    pddlCGFree(&cg);

    int *remap = CALLOC_ARR(int, fdr->var.var_size);
    for (int vi = 0; vi < fdr->var.var_size; ++vi){
        ASSERT(ordering[vi] >= 0);
        ASSERT(remap[ordering[vi]] == 0);
        remap[ordering[vi]] = vi;
    }

    pddlFDRVarsRemap(&fdr->var, remap);
    pddlFDROpsRemapVars(&fdr->op, remap);
    pddlFDRPartStateRemapVars(&fdr->goal, remap);

    int *init = ALLOC_ARR(int, fdr->var.var_size);
    memcpy(init, fdr->init, sizeof(int) * fdr->var.var_size);
    for (int vi = 0; vi < fdr->var.var_size; ++vi)
        fdr->init[remap[vi]] = init[vi];
    FREE(init);


    FREE(remap);
    FREE(ordering);
}

void pddlFDRReduce(pddl_fdr_t *fdr,
                   const pddl_iset_t *del_vars,
                   const pddl_iset_t *_del_facts,
                   const pddl_iset_t *del_ops)
{
    if (del_ops != NULL && pddlISetSize(del_ops) > 0)
        pddlFDROpsDelSet(&fdr->op, del_ops);

    PDDL_ISET(del_facts);
    if (_del_facts != NULL && pddlISetSize(_del_facts) > 0)
        pddlISetUnion(&del_facts, _del_facts);

    if (del_vars != NULL && pddlISetSize(del_vars) > 0){
        int var;
        PDDL_ISET_FOR_EACH(del_vars, var){
            for (int val = 0; val < fdr->var.var[var].val_size; ++val)
                pddlISetAdd(&del_facts, fdr->var.var[var].val[val].global_id);
        }
    }

    if (pddlISetSize(&del_facts) > 0){
        int old_var_size = fdr->var.var_size;

        pddl_fdr_vars_remap_t remap;
        // Delete facts
        pddlFDRVarsDelFacts(&fdr->var, &del_facts, &remap);
        // Remap facts in operators
        pddlFDROpsRemapFacts(&fdr->op, &remap);

        // Remap the initial state
        for (int v = 0; v < old_var_size; ++v){
            if (remap.remap[v][fdr->init[v]] != NULL){
                const pddl_fdr_val_t *val = remap.remap[v][fdr->init[v]];
                fdr->init[val->var_id] = val->val_id;
            }
        }

        // Remap goal
        pddlFDRPartStateRemapFacts(&fdr->goal, &remap);

        // Remove operators with empty effects
        PDDL_ISET(useless_ops);
        for (int op_id = 0; op_id < fdr->op.op_size; ++op_id){
            const pddl_fdr_op_t *op = fdr->op.op[op_id];
            if (op->eff.fact_size == 0 && op->cond_eff_size == 0)
                pddlISetAdd(&useless_ops, op_id);
        }
        if (pddlISetSize(&useless_ops) > 0)
            pddlFDROpsDelSet(&fdr->op, &useless_ops);
        pddlISetFree(&useless_ops);

        // Set cond-eff flag
        fdr->has_cond_eff = 0;
        for (int op_id = 0; op_id < fdr->op.op_size; ++op_id){
            if (fdr->op.op[op_id]->cond_eff_size > 0){
                fdr->has_cond_eff = 1;
                break;
            }
        }

        pddlFDRVarsRemapFree(&remap);
    }
    pddlISetFree(&del_facts);
}

static int relaxedPreHold(const pddl_fdr_t *fdr,
                          const int *reached,
                          const pddl_fdr_part_state_t *pre)
{
    for (int fi = 0; fi < pre->fact_size; ++fi){
        int var = pre->fact[fi].var;
        int val = pre->fact[fi].val;
        int fact = fdr->var.var[var].val[val].global_id;
        if (!reached[fact])
            return 0;
    }
    return 1;
}

static void relaxedReachFacts(const pddl_fdr_t *fdr,
                              int *reached,
                              const pddl_fdr_part_state_t *eff)
{
    for (int fi = 0; fi < eff->fact_size; ++fi){
        int var = eff->fact[fi].var;
        int val = eff->fact[fi].val;
        int fact = fdr->var.var[var].val[val].global_id;
        reached[fact] = 1;
    }
}

int pddlFDRIsRelaxedPlan(const pddl_fdr_t *fdr,
                         const int *fdr_state,
                         const pddl_iarr_t *plan,
                         pddl_err_t *err)
{
    int *reached = CALLOC_ARR(int, fdr->var.global_id_size);
    for (int var = 0; var < fdr->var.var_size; ++var)
        reached[fdr->var.var[var].val[fdr_state[var]].global_id] = 1;

    int op_id;
    PDDL_IARR_FOR_EACH(plan, op_id){
        const pddl_fdr_op_t *op = fdr->op.op[op_id];
        if (!relaxedPreHold(fdr, reached, &op->pre)){
            PDDL_INFO(err, "Relaxed plan failed: %d:(%s) pre unsatisfied",
                      op_id, op->name);
            FREE(reached);
            return 0;
        }
        relaxedReachFacts(fdr, reached, &op->eff);

        for (int cei = 0; cei < op->cond_eff_size; ++cei){
            const pddl_fdr_op_cond_eff_t *ce = op->cond_eff + cei;
            if (relaxedPreHold(fdr, reached, &ce->pre))
                relaxedReachFacts(fdr, reached, &ce->eff);
        }
    }

    int found_goal = 1;
    for (int fi = 0; fi < fdr->goal.fact_size; ++fi){
        int var = fdr->goal.fact[fi].var;
        int val = fdr->goal.fact[fi].val;
        int fact = fdr->var.var[var].val[val].global_id;
        if (!reached[fact]){
            found_goal = 0;
            break;
        }
    }

    FREE(reached);
    return found_goal;
}

static void stripsToFDRState(const pddl_fdr_vars_t *fdr_var,
                             const pddl_iset_t *state,
                             int *fdr_state)
{
    for (int vi = 0; vi < fdr_var->var_size; ++vi)
        fdr_state[vi] = -1;

    int fact_id;
    PDDL_ISET_FOR_EACH(state, fact_id){
        int val_id;
        PDDL_ISET_FOR_EACH(&fdr_var->strips_id_to_val[fact_id], val_id){
            const pddl_fdr_val_t *v = fdr_var->global_id_to_val[val_id];
            fdr_state[v->var_id] = v->val_id;
        }
    }

    for (int vi = 0; vi < fdr_var->var_size; ++vi){
        if (fdr_state[vi] == -1){
            ASSERT(fdr_var->var[vi].val_none_of_those >= 0);
            fdr_state[vi] = fdr_var->var[vi].val_none_of_those;
        }
    }
}

static void setDelEffFact(const pddl_mutex_pairs_t *mutex,
                          const pddl_fdr_vars_t *fdr_var,
                          const pddl_iset_t *pre,
                          const pddl_iset_t *ce_pre,
                          pddl_fdr_part_state_t *eff,
                          int fact_id,
                          const pddl_fdr_val_t *v)
{
    const pddl_fdr_var_t *var = fdr_var->var + v->var_id;
    if (!pddlMutexPairsIsMutexFactSet(mutex, fact_id, pre)
            && (ce_pre == NULL
                    || !pddlMutexPairsIsMutexFactSet(mutex, fact_id, ce_pre))
            && var->val_none_of_those >= 0){
        pddlFDRPartStateSet(eff, var->var_id, var->val_none_of_those);
    }
}

static void stripsToFDRDelEff(const pddl_fdr_vars_t *fdr_var,
                              const pddl_iset_t *eff,
                              pddl_fdr_part_state_t *fdr_eff,
                              const pddl_mutex_pairs_t *mutex,
                              const pddl_iset_t *pre,
                              const pddl_iset_t *ce_pre)
{
    int fact_id;
    PDDL_ISET_FOR_EACH(eff, fact_id){
        int val_id;
        PDDL_ISET_FOR_EACH(&fdr_var->strips_id_to_val[fact_id], val_id){
            const pddl_fdr_val_t *v = fdr_var->global_id_to_val[val_id];
            setDelEffFact(mutex, fdr_var, pre, ce_pre, fdr_eff, fact_id, v);
        }
    }
}

static int stripsToFDRPartState(const pddl_fdr_vars_t *fdr_var,
                                const pddl_iset_t *part_state,
                                pddl_fdr_part_state_t *fdr_ps)
{
    int ret = 0;
    int fact_id;
    PDDL_ISET_FOR_EACH(part_state, fact_id){
        int val_id;
        PDDL_ISET_FOR_EACH(&fdr_var->strips_id_to_val[fact_id], val_id){
            const pddl_fdr_val_t *v = fdr_var->global_id_to_val[val_id];
            if (pddlFDRPartStateIsSet(fdr_ps, v->var_id))
                ret = -1;
            pddlFDRPartStateSet(fdr_ps, v->var_id, v->val_id);
        }
    }

    return ret;
}

static int isAllButNoneOfThoseMutex(const pddl_fdr_vars_t *vars,
                                    const pddl_mutex_pairs_t *mutex,
                                    const pddl_fdr_part_state_t *ps,
                                    const pddl_fdr_var_t *var)
{
    if (var->val_none_of_those < 0)
        return 0;

    for (int psi = 0; psi < ps->fact_size; ++psi){
        const pddl_fdr_fact_t *fdr_fact = ps->fact + psi;
        int strips_fact = vars->var[fdr_fact->var].val[fdr_fact->val].strips_id;
        if (strips_fact < 0)
            continue;

        int is_mutex = 1;
        for (int val = 0; val < var->val_size && is_mutex; ++val){
            if (val == var->val_none_of_those)
                continue;
            int strips_fact2 = var->val[val].strips_id;
            if (strips_fact2 < 0)
                return 0;
            is_mutex |= pddlMutexPairsIsMutex(mutex, strips_fact, strips_fact2);
        }
        if (is_mutex)
            return 1;
    }
    return 0;
}
                                    
static int setNoneOfThoseInPre(const pddl_fdr_vars_t *fdr_var,
                               const pddl_mutex_pairs_t *mutex,
                               const pddl_fdr_part_state_t *fdr_eff,
                               pddl_fdr_part_state_t *fdr_pre)
{
    PDDL_ISET(extend_by);
    int prei = 0, effi = 0;
    for (; prei < fdr_pre->fact_size && effi < fdr_eff->fact_size;){
        const pddl_fdr_fact_t *pre_fact = fdr_pre->fact + prei;
        const pddl_fdr_fact_t *eff_fact = fdr_eff->fact + effi;
        if (pre_fact->var == eff_fact->var){
            ++prei;
            ++effi;
        }else if (pre_fact->var < eff_fact->var){
            ++prei;
        }else{ // eff_fact->var < pre_fact->var
            const pddl_fdr_var_t *var = fdr_var->var + eff_fact->var;
            if (isAllButNoneOfThoseMutex(fdr_var, mutex, fdr_pre, var))
                pddlISetAdd(&extend_by, eff_fact->var);
            ++effi;
        }
    }
    for (; effi < fdr_eff->fact_size; ++effi){
        const pddl_fdr_fact_t *eff_fact = fdr_eff->fact + effi;
        const pddl_fdr_var_t *var = fdr_var->var + eff_fact->var;
        if (isAllButNoneOfThoseMutex(fdr_var, mutex, fdr_pre, var))
            pddlISetAdd(&extend_by, eff_fact->var);
    }

    int var_id;
    PDDL_ISET_FOR_EACH(&extend_by, var_id){
        pddlFDRPartStateSet(fdr_pre, var_id,
                            fdr_var->var[var_id].val_none_of_those);
    }

    int ret = pddlISetSize(&extend_by);
    pddlISetFree(&extend_by);
    return ret;
}

static int cmpCondEff(const void *a, const void *b, void *_)
{
    const pddl_fdr_op_cond_eff_t *ce1 = a;
    const pddl_fdr_op_cond_eff_t *ce2 = b;
    return pddlFDRPartStateCmp(&ce1->pre, &ce2->pre);
}

static void addOp(pddl_fdr_ops_t *fdr_ops,
                  const pddl_fdr_vars_t *fdr_var,
                  const pddl_strips_t *strips,
                  const pddl_mutex_pairs_t *mutex,
                  unsigned fdr_flags,
                  int op_id)
{
    const pddl_strips_op_t *op = strips->op.op[op_id];
    if (pddlMutexPairsIsMutexSet(mutex, &op->pre))
        return;

    pddl_fdr_op_t *fdr_op = pddlFDROpNewEmpty();
    pddl_fdr_part_state_t pre;

    if (op->name != NULL)
        fdr_op->name = STRDUP(op->name);
    fdr_op->cost = op->cost;

    pddlFDRPartStateInit(&pre);
    if (stripsToFDRPartState(fdr_var, &op->pre, &pre) == 0){
        pddlFDRPartStateInitCopy(&fdr_op->pre, &pre);
    }else{
        pddlFDRPartStateFree(&pre);
        pddlFDROpDel(fdr_op);
        return;
    }
    pddlFDRPartStateFree(&pre);

    stripsToFDRDelEff(fdr_var, &op->del_eff, &fdr_op->eff,
                      mutex, &op->pre, NULL);
    stripsToFDRPartState(fdr_var, &op->add_eff, &fdr_op->eff);
    if (fdr_flags & PDDL_FDR_SET_NONE_OF_THOSE_IN_PRE)
        setNoneOfThoseInPre(fdr_var, mutex, &fdr_op->eff, &fdr_op->pre);

    for (int cei = 0; cei < op->cond_eff_size; ++cei){
        const pddl_strips_op_cond_eff_t *ce = op->cond_eff + cei;
        pddl_fdr_op_cond_eff_t *fdr_ce = pddlFDROpAddEmptyCondEff(fdr_op);
        pddlFDRPartStateInit(&pre);
        if (stripsToFDRPartState(fdr_var, &ce->pre, &pre) == 0){
            pddlFDRPartStateInitCopy(&fdr_ce->pre, &pre);
        }else{
            pddlFDRPartStateFree(&pre);
            pddlFDROpDel(fdr_op);
            return;
        }
        pddlFDRPartStateFree(&pre);
        stripsToFDRDelEff(fdr_var, &ce->del_eff, &fdr_ce->eff,
                          mutex, &op->pre, &ce->pre);
        stripsToFDRPartState(fdr_var, &ce->add_eff, &fdr_ce->eff);
        if (fdr_flags & PDDL_FDR_SET_NONE_OF_THOSE_IN_PRE)
            setNoneOfThoseInPre(fdr_var, mutex, &fdr_ce->eff, &fdr_ce->pre);
    }

    if (fdr_op->cond_eff_size > 1){
        pddlSort(fdr_op->cond_eff, fdr_op->cond_eff_size,
                 sizeof(pddl_fdr_op_cond_eff_t), cmpCondEff, NULL);
    }

    pddlFDROpsAddSteal(fdr_ops, fdr_op);
}

static void tnfPreToEff(const pddl_fdr_part_state_t *pre,
                        pddl_fdr_part_state_t *eff)
{
    for (int fi = 0; fi < pre->fact_size; ++fi){
        int var = pre->fact[fi].var;
        int val = pre->fact[fi].val;
        if (!pddlFDRPartStateIsSet(eff, var))
            pddlFDRPartStateSet(eff, var, val);
    }
}

static void tnfEffToPre(pddl_fdr_t *fdr,
                        pddl_fdr_val_t **u_vals,
                        const pddl_fdr_part_state_t *eff,
                        pddl_fdr_part_state_t *pre)
{
    for (int fi = 0; fi < eff->fact_size; ++fi){
        int var = eff->fact[fi].var;
        if (!pddlFDRPartStateIsSet(pre, var)){
            if (u_vals[var] == NULL)
                u_vals[var] = pddlFDRVarsAddVal(&fdr->var, var, "tnf-unkown");
            pddlFDRPartStateSet(pre, var, u_vals[var]->val_id);
        }
    }
}

static void tnfFull(pddl_fdr_t *fdr,
                    const pddl_fdr_t *fdr_in,
                    unsigned flags,
                    pddl_err_t *err)
{
    pddl_fdr_val_t **u_vals = CALLOC_ARR(pddl_fdr_val_t *,
                                         fdr->var.var_size);

    for (int opi = 0; opi < fdr->op.op_size; ++opi){
        pddl_fdr_op_t *op = fdr->op.op[opi];
        if (flags & PDDL_FDR_TNF_PREVAIL_TO_EFF)
            tnfPreToEff(&op->pre, &op->eff);
        tnfEffToPre(fdr, u_vals, &op->eff, &op->pre);
        for (int cei = 0; cei < op->cond_eff_size; ++cei){
            pddl_fdr_op_cond_eff_t *ce = op->cond_eff + cei;
            if (flags & PDDL_FDR_TNF_PREVAIL_TO_EFF)
                tnfPreToEff(&ce->pre, &ce->eff);
            tnfEffToPre(fdr, u_vals, &ce->eff, &ce->pre);
        }
    }

    for (int var_id = 0; var_id < fdr->var.var_size; ++var_id){
        if (!pddlFDRPartStateIsSet(&fdr_in->goal, var_id)){
            if (u_vals[var_id] == NULL)
                u_vals[var_id] = pddlFDRVarsAddVal(&fdr->var, var_id,
                                                   "tnf-unknown");
            pddlFDRPartStateSet(&fdr->goal, var_id, u_vals[var_id]->val_id);
        }

        if (u_vals[var_id] == NULL)
            continue;

        const pddl_fdr_var_t *var = fdr->var.var + var_id;
        for (int val_id = 0; val_id < var->val_size; ++val_id){
            if (val_id == u_vals[var_id]->val_id)
                continue;
            pddl_fdr_op_t *op = pddlFDROpNewEmpty();
            op->cost = 0;
            char name[128];
            sprintf(name, "tnf-forget-%d-%d", var_id, val_id);
            op->name = STRDUP(name);
            pddlFDRPartStateSet(&op->pre, var_id, val_id);
            pddlFDRPartStateSet(&op->eff, var_id, u_vals[var_id]->val_id);
            pddlFDROpsAddSteal(&fdr->op, op);
        }
    }

    FREE(u_vals);
}

static int tnfDisambiguate(pddl_fdr_t *fdr,
                           pddl_disambiguate_t *dis,
                           int dis_offset,
                           pddl_set_iset_t *dis_sets,
                           unsigned flags,
                           const pddl_iset_t *pre,
                           const pddl_iset_t *eff,
                           pddl_iset_t *extend)
{
    pddl_set_iset_t hset;
    pddlSetISetInit(&hset);
    int sf_flag = ((flags & PDDL_FDR_TNF_WEAK_DISAMBIGUATION) ? 1 : 0);
    int ret = pddlDisambiguate(dis, pre, eff, 1, sf_flag, &hset, extend);
    int size = pddlSetISetSize(&hset);
    for (int i = 0; i < size; ++i){
        const pddl_iset_t *set = pddlSetISetGet(&hset, i);
        // sets containing one fact are already included in {extend}
        if (pddlISetSize(set) <= 1)
            continue;

        int set_id = pddlSetISetAdd(dis_sets, set);
        ASSERT(set_id + dis_offset <= fdr->var.global_id_size);
        if (set_id + dis_offset == fdr->var.global_id_size){
            int fact_id = pddlISetGet(set, 0);
            const pddl_fdr_val_t *v = fdr->var.global_id_to_val[fact_id];
            char name[128];
            sprintf(name, "tnf-unknown-%d", set_id + dis_offset);
            pddlFDRVarsAddVal(&fdr->var, v->var_id, name);
        }
        pddlISetAdd(extend, set_id + dis_offset);
    }
    pddlSetISetFree(&hset);
    return ret;
}

static int tnfDisOp(pddl_fdr_t *fdr,
                    pddl_disambiguate_t *dis,
                    int dis_offset,
                    pddl_set_iset_t *dis_sets,
                    unsigned flags,
                    pddl_fdr_op_t *op,
                    pddl_err_t *err)
{
    if (flags & PDDL_FDR_TNF_PREVAIL_TO_EFF)
        tnfPreToEff(&op->pre, &op->eff);

    PDDL_ISET(pre);
    PDDL_ISET(eff);
    PDDL_ISET(ext);

    pddlFDRPartStateToGlobalIDs(&op->pre, &fdr->var, &pre);
    pddlFDRPartStateToGlobalIDs(&op->eff, &fdr->var, &eff);

    int ret = tnfDisambiguate(fdr, dis, dis_offset, dis_sets, flags,
                              &pre, &eff, &ext);
    if (ret < 0){
        pddlISetFree(&pre);
        pddlISetFree(&eff);
        pddlISetFree(&ext);
        return -1;
    }

    int fact_id;
    PDDL_ISET_FOR_EACH(&ext, fact_id){
        const pddl_fdr_val_t *val = fdr->var.global_id_to_val[fact_id];
        pddlFDRPartStateSet(&op->pre, val->var_id, val->val_id);
        if (!(flags & PDDL_FDR_TNF_PREVAIL_TO_EFF)
                && pddlFDRPartStateGet(&op->eff, val->var_id) == val->val_id){
            pddlFDRPartStateUnset(&op->eff, val->var_id);
        }
    }

    pddlISetFree(&pre);
    pddlISetFree(&eff);
    pddlISetFree(&ext);
    return 0;
}

static int tnfDisGoal(pddl_fdr_t *fdr,
                      pddl_disambiguate_t *dis,
                      int dis_offset,
                      pddl_set_iset_t *dis_sets,
                      unsigned flags,
                      pddl_err_t *err)
{
    PDDL_ISET(goal);
    PDDL_ISET(ext);

    pddlFDRPartStateToGlobalIDs(&fdr->goal, &fdr->var, &goal);

    int ret = tnfDisambiguate(fdr, dis, dis_offset, dis_sets, flags,
                              &goal, NULL, &ext);
    if (ret < 0){
        fdr->goal_is_unreachable = 1;
        // Set the undefined variables in the goal to anything since the
        // goal is unreachable anyway
        for (int var = 0; var < fdr->var.var_size; ++var){
            if (!pddlFDRPartStateIsSet(&fdr->goal, var))
                pddlFDRPartStateSet(&fdr->goal, var, 0);
        }
        return -1;
    }

    int fact_id;
    PDDL_ISET_FOR_EACH(&ext, fact_id){
        const pddl_fdr_val_t *val = fdr->var.global_id_to_val[fact_id];
        pddlFDRPartStateSet(&fdr->goal, val->var_id, val->val_id);
    }

    pddlISetFree(&goal);
    pddlISetFree(&ext);
    return 0;
}

static void tnfDisForgettingOps(pddl_fdr_t *fdr,
                                int dis_offset,
                                const pddl_set_iset_t *dis_sets,
                                pddl_err_t *err)
{
    int dis_size = pddlSetISetSize(dis_sets);
    for (int dis_id = 0; dis_id < dis_size; ++dis_id){
        int fact_id = dis_id + dis_offset;
        const pddl_fdr_val_t *val = fdr->var.global_id_to_val[fact_id];
        int undef_id = val->val_id;
        int var_id = val->var_id;

        const pddl_iset_t *set = pddlSetISetGet(dis_sets, dis_id);
        int fid;
        PDDL_ISET_FOR_EACH(set, fid){
            const pddl_fdr_val_t *val = fdr->var.global_id_to_val[fid];
            int pre_var_id = val->var_id;
            int pre_val_id = val->val_id;

            pddl_fdr_op_t *op = pddlFDROpNewEmpty();
            op->cost = 0;
            char name[128];
            sprintf(name, "tnf-forget-%d-%d-%d",
                    dis_id, pre_var_id, pre_val_id);
            op->name = STRDUP(name);
            pddlFDRPartStateSet(&op->pre, pre_var_id, pre_val_id);
            pddlFDRPartStateSet(&op->eff, var_id, undef_id);
            pddlFDROpsAddSteal(&fdr->op, op);
        }
    }
}

static void tnfDis(pddl_fdr_t *fdr,
                   pddl_disambiguate_t *dis,
                   unsigned flags,
                   pddl_err_t *err)
{
    PDDL_ISET(unreachable_ops);
    pddl_set_iset_t dis_sets;
    pddlSetISetInit(&dis_sets);

    int dis_offset = fdr->var.global_id_size;
    for (int opi = 0; opi < fdr->op.op_size; ++opi){
        pddl_fdr_op_t *op = fdr->op.op[opi];
        if (tnfDisOp(fdr, dis, dis_offset, &dis_sets, flags, op, err) < 0)
            pddlISetAdd(&unreachable_ops, opi);
    }

    tnfDisGoal(fdr, dis, dis_offset, &dis_sets, flags, err);
    tnfDisForgettingOps(fdr, dis_offset, &dis_sets, err);

    if (pddlISetSize(&unreachable_ops) > 0)
        pddlFDRReduce(fdr, NULL, NULL, &unreachable_ops);

    pddlSetISetFree(&dis_sets);
    pddlISetFree(&unreachable_ops);
}

static void tnfMultiplyOpSet(pddl_fdr_t *fdr,
                             pddl_set_iset_t *hset,
                             int set_id,
                             pddl_fdr_op_t *op,
                             pddl_err_t *err)
{
    if (set_id >= pddlSetISetSize(hset)){
        pddl_fdr_op_t *new_op = pddlFDROpClone(op);
        pddlFDRPartStateMinus(&new_op->eff, &new_op->pre);
        if (new_op->eff.fact_size > 0){
            pddlFDROpsAddSteal(&fdr->op, new_op);
        }else{
            pddlFDROpDel(new_op);
        }
        return;
    }

    const pddl_iset_t *set = pddlSetISetGet(hset, set_id);
    if (pddlISetSize(set) <= 1){
        tnfMultiplyOpSet(fdr, hset, set_id + 1, op, err);

    }else{
        int fact_id = pddlISetGet(set, 0);
        const pddl_fdr_val_t *v = fdr->var.global_id_to_val[fact_id];
        int var_id = v->var_id;

        PDDL_ISET_FOR_EACH(set, fact_id){
            const pddl_fdr_val_t *v = fdr->var.global_id_to_val[fact_id];
            ASSERT(v->var_id == var_id);
            pddl_fdr_op_t *new_op = pddlFDROpClone(op);
            ASSERT(!pddlFDRPartStateIsSet(&new_op->pre, var_id));
            pddlFDRPartStateSet(&new_op->pre, var_id, v->val_id);
            tnfMultiplyOpSet(fdr, hset, set_id + 1, new_op, err);
            pddlFDROpDel(new_op);
        }
    }
}

static int tnfMultiplyOp(pddl_fdr_t *fdr,
                         pddl_disambiguate_t *dis,
                         unsigned flags,
                         pddl_fdr_op_t *op,
                         pddl_err_t *err)
{
    pddl_set_iset_t hset;
    pddlSetISetInit(&hset);
    PDDL_ISET(pre);
    PDDL_ISET(eff);
    PDDL_ISET(extend);
    int ret = 0;
    int sf_flag = ((flags & PDDL_FDR_TNF_WEAK_DISAMBIGUATION) ? 1 : 0);

    pddlFDRPartStateToGlobalIDs(&op->pre, &fdr->var, &pre);
    pddlFDRPartStateToGlobalIDs(&op->eff, &fdr->var, &eff);

    ASSERT_RUNTIME(dis != NULL);
    int disret = pddlDisambiguate(dis, &pre, &eff, 1, sf_flag, &hset, &extend);
    if (disret < 0)
        ret = -1;

    int fact_id;
    PDDL_ISET_FOR_EACH(&extend, fact_id){
        const pddl_fdr_val_t *val = fdr->var.global_id_to_val[fact_id];
        ASSERT(!pddlFDRPartStateIsSet(&op->pre, val->var_id));
        pddlFDRPartStateSet(&op->pre, val->var_id, val->val_id);
        if (!(flags & PDDL_FDR_TNF_PREVAIL_TO_EFF)
                && pddlFDRPartStateGet(&op->eff, val->var_id) == val->val_id){
            pddlFDRPartStateUnset(&op->eff, val->var_id);
        }
    }

    // TODO: We can improve this by disambiguating for each partial
    //       assignment separately, i.e., to recurse on this function
    //       instead of tnfMultiplyOpSet()
    if (disret >= 0 && pddlSetISetSize(&hset) > 0){
        ret = -1;
        tnfMultiplyOpSet(fdr, &hset, 0, op, err);
    }

    pddlSetISetFree(&hset);
    pddlISetFree(&pre);
    pddlISetFree(&eff);
    pddlISetFree(&extend);
    return ret;
}

static void tnfMultiply(pddl_fdr_t *fdr,
                        pddl_disambiguate_t *dis,
                        unsigned flags,
                        pddl_err_t *err)
{
    PDDL_ISET(rm_ops);

    int op_size = fdr->op.op_size;
    for (int opi = 0; opi < op_size; ++opi){
        pddl_fdr_op_t *op = fdr->op.op[opi];
        if (tnfMultiplyOp(fdr, dis, flags, op, err) < 0)
            pddlISetAdd(&rm_ops, opi);
    }

    if (pddlISetSize(&rm_ops) > 0){
        pddlFDRReduce(fdr, NULL, NULL, &rm_ops);
        pddlFDROpsSortByName(&fdr->op);
    }
    pddlISetFree(&rm_ops);
}

static void removeUnreachableOps(pddl_fdr_t *fdr,
                                 const pddl_mutex_pairs_t *mutex,
                                 pddl_err_t *err)
{
    PDDL_ISET(rm_ops);

    for (int op_id = 0; op_id < fdr->op.op_size; ++op_id){
        const pddl_fdr_op_t *op = fdr->op.op[op_id];
        PDDL_ISET(pre);
        pddlFDRPartStateToGlobalIDs(&op->pre, &fdr->var, &pre);
        if (pddlMutexPairsIsMutexSet(mutex, &pre))
            pddlISetAdd(&rm_ops, op_id);
        pddlISetFree(&pre);
    }

    if (pddlISetSize(&rm_ops) > 0){
        pddlFDRReduce(fdr, NULL, NULL, &rm_ops);
        pddlFDROpsSortByName(&fdr->op);
        PDDL_LOG(err, "Removed %{rm_unreachable_ops}d unreachable operators",
                 pddlISetSize(&rm_ops));
    }
    pddlISetFree(&rm_ops);
}

int pddlFDRInitTransitionNormalForm(pddl_fdr_t *fdr,
                                    const pddl_fdr_t *fdr_in,
                                    const pddl_mutex_pairs_t *mutex,
                                    unsigned flags,
                                    pddl_err_t *err)
{
    if (fdr_in->has_cond_eff && mutex != NULL){
        PDDL_ERR_RET(err, -1, "Disambiguated Transition Normal Form is not"
                      " supported for conditional effects");
    }

    if ((flags & PDDL_FDR_TNF_PREVAIL_TO_EFF)
            && (flags & PDDL_FDR_TNF_MULTIPLY_OPS)){
        PDDL_ERR_RET(err, -1, "PDDL_FDR_TNF_PREVAIL_TO_EFF cannot be combined"
                      "with PDDL_FDR_TNF_MULTIPLY_OPS");
    }

    CTX(err, "tnf", "TNF");
    PDDL_LOG(err, "Creating a Transition Normal Form"
             " (vars: %{tnf_in_vars}d, facts: %{tnf_in_facts}d,"
             " ops: %{tnf_in_ops}d)",
             fdr_in->var.var_size,
             fdr_in->var.global_id_size,
             fdr_in->op.op_size);

    pddlFDRInitCopy(fdr, fdr_in);

    if (mutex == NULL){
        PDDL_INFO(err, "Constructing full TNF");
        tnfFull(fdr, fdr_in, flags, err);

    }else{
        pddl_mgroups_t mgs;
        pddlMGroupsInitEmpty(&mgs);
        pddlMGroupsAddFDRVars(&mgs, &fdr->var);

        pddl_disambiguate_t dis;
        pddlDisambiguateInit(&dis, fdr->var.global_id_size, mutex, &mgs);

        if (flags & PDDL_FDR_TNF_MULTIPLY_OPS){
            PDDL_INFO(err, "Multiply operators with disambiguation");
            tnfMultiply(fdr, &dis, flags, err);
            removeUnreachableOps(fdr, mutex, err);
        }else{
            PDDL_INFO(err, "Using disambiguation");
            tnfDis(fdr, &dis, flags, err);
        }

        pddlDisambiguateFree(&dis);
        pddlMGroupsFree(&mgs);
    }

    PDDL_LOG(err, "Transition Normal Form created."
             " (vars: %{tnf_vars}d, facts: %{tnf_facts}d, ops: %{tnf_ops}d)",
             fdr->var.var_size,
             fdr->var.global_id_size,
             fdr->op.op_size);
    CTXEND(err);
    return 0;
}

static void printFDOp(const pddl_fdr_op_t *op,
                      const pddl_fdr_write_config_t *cfg,
                      FILE *fout)
{
    pddl_fdr_part_state_t prevail;

    fprintf(fout, "begin_operator\n");
    if (cfg->encode_op_ids)
        fprintf(fout, "id-%d-", op->id);
    fprintf(fout, "%s\n", op->name);

    PDDL_ISET(eff_var);
    for (int i = 0; i < op->eff.fact_size; ++i)
        pddlISetAdd(&eff_var, op->eff.fact[i].var);
    for (int cei = 0; cei < op->cond_eff_size; ++cei){
        const pddl_fdr_op_cond_eff_t *ce = op->cond_eff + cei;
        for (int i = 0; i < ce->eff.fact_size; ++i)
            pddlISetAdd(&eff_var, ce->eff.fact[i].var);
    }

    pddlFDRPartStateInit(&prevail);
    for (int pi = 0; pi < op->pre.fact_size; ++pi){
        const pddl_fdr_fact_t *f = op->pre.fact + pi;
        if (!pddlISetIn(f->var, &eff_var))
            pddlFDRPartStateSet(&prevail, f->var, f->val);
    }

    fprintf(fout, "%d\n", prevail.fact_size);
    for (int i = 0; i < prevail.fact_size; ++i)
        fprintf(fout, "%d %d\n", prevail.fact[i].var, prevail.fact[i].val);
    pddlFDRPartStateFree(&prevail);
    pddlISetFree(&eff_var);

    int num_effs = op->eff.fact_size;
    for (int cei = 0; cei < op->cond_eff_size; ++cei)
        num_effs += op->cond_eff[cei].eff.fact_size;

    fprintf(fout, "%d\n", num_effs);
    for (int i = 0; i < op->eff.fact_size; ++i){
        const pddl_fdr_fact_t *f = op->eff.fact + i;
        int pre = pddlFDRPartStateGet(&op->pre, f->var);
        fprintf(fout, "0 %d %d %d\n", f->var, pre, f->val);
    }

    for (int cei = 0; cei < op->cond_eff_size; ++cei){
        const pddl_fdr_op_cond_eff_t *ce = op->cond_eff + cei;
        for (int i = 0; i < ce->eff.fact_size; ++i){
            const pddl_fdr_fact_t *f = ce->eff.fact + i;
            int num_prevails = ce->pre.fact_size;
            fprintf(fout, "%d", num_prevails);
            for (int pi = 0; pi < ce->pre.fact_size; ++pi){
                const pddl_fdr_fact_t *p = ce->pre.fact + pi;
                fprintf(fout, " %d %d", p->var, p->val);
            }
            int pre = pddlFDRPartStateGet(&op->pre, f->var);
            fprintf(fout, " %d %d %d\n", f->var, pre, f->val);
        }
    }

    fprintf(fout, "%d\n", op->cost);
    fprintf(fout, "end_operator\n");
}

static void printFDFactName(const pddl_fdr_var_t *var, int val, FILE *fout)
{
    if (val == var->val_none_of_those){
        fprintf(fout, "<none of those>\n");
    }else{
        fprintf(fout, "Atom ");
        int pred_found = 0;
        for (const char *nc = var->val[val].name; *nc != 0; ++nc){
            if (*nc == ' '){
                if (!pred_found){
                    fprintf(fout, "(");
                    pred_found = 1;
                }else{
                    fprintf(fout, ", ");
                }
            }else{
                fprintf(fout, "%c", *nc);
            }
        }
        if (!pred_found)
            fprintf(fout, "(");
        fprintf(fout, ")\n");
    }
}

void pddlFDRPrintFD(const pddl_fdr_t *fdr,
                    const pddl_mgroups_t *mg,
                    int use_fd_fact_names,
                    FILE *fout)
{
    pddl_fdr_write_config_t cfg = PDDL_FDR_WRITE_CONFIG_INIT;
    cfg.fd = 1;
    cfg.fout = fout;
    cfg.mgroups = mg;
    cfg.use_fd_fact_names = use_fd_fact_names;
    pddlFDRWrite(fdr, &cfg);
}

static void pddlFDRWriteFD(const pddl_fdr_t *fdr,
                           const pddl_fdr_write_config_t *cfg,
                           FILE *fout)
{
    fprintf(fout, "begin_version\n3\nend_version\n");
    fprintf(fout, "begin_metric\n1\nend_metric\n");

    // variables
    fprintf(fout, "%d\n", fdr->var.var_size);
    for (int vi = 0; vi < fdr->var.var_size; ++vi){
        const pddl_fdr_var_t *var = fdr->var.var + vi;
        fprintf(fout, "begin_variable\n");
        if (var->is_black){
            fprintf(fout, "black-var%d\n", vi);
        }else{
            fprintf(fout, "var%d\n", vi);
        }
        fprintf(fout, "-1\n");
        fprintf(fout, "%d\n", var->val_size);
        for (int vali = 0; vali < var->val_size; ++vali){
            if (cfg->use_fd_fact_names){
                printFDFactName(var, vali, fout);
            }else{
                fprintf(fout, "%s\n", var->val[vali].name);
            }
        }
        fprintf(fout, "end_variable\n");
    }

    // mutex groups
    if (cfg->mgroups == NULL){
        fprintf(fout, "0\n");
    }else{
        fprintf(fout, "%d\n", cfg->mgroups->mgroup_size);
        for (int mi = 0; mi < cfg->mgroups->mgroup_size; ++mi){
            const pddl_mgroup_t *m = cfg->mgroups->mgroup + mi;
            fprintf(fout, "begin_mutex_group\n");
            fprintf(fout, "%d\n", pddlISetSize(&m->mgroup));
            int fact_id;
            PDDL_ISET_FOR_EACH(&m->mgroup, fact_id){
                // TODO
                int val_id = pddlISetGet(&fdr->var.strips_id_to_val[fact_id], 0);
                const pddl_fdr_val_t *v = fdr->var.global_id_to_val[val_id];
                fprintf(fout, "%d %d\n", v->var_id, v->val_id);
            }
            fprintf(fout, "end_mutex_group\n");
        }
    }

    // initial state
    fprintf(fout, "begin_state\n");
    for (int vi = 0; vi < fdr->var.var_size; ++vi)
        fprintf(fout, "%d\n", fdr->init[vi]);
    fprintf(fout, "end_state\n");

    // goal
    fprintf(fout, "begin_goal\n");
    fprintf(fout, "%d\n", fdr->goal.fact_size);
    for (int i = 0; i < fdr->goal.fact_size; ++i){
        const pddl_fdr_fact_t *f = fdr->goal.fact + i;
        fprintf(fout, "%d %d\n", f->var, f->val);
    }
    fprintf(fout, "end_goal\n");

    // operators
    fprintf(fout, "%d\n", fdr->op.op_size);
    for (int op_id = 0; op_id < fdr->op.op_size; ++op_id)
        printFDOp(fdr->op.op[op_id], cfg, fout);

    // axioms
    fprintf(fout, "0\n");
}

void pddlFDRWrite(const pddl_fdr_t *fdr, const pddl_fdr_write_config_t *cfg)
{
    PANIC_IF(cfg->filename == NULL && cfg->fout == NULL,
              "No output specified.");
    PANIC_IF(!cfg->fd, "pddlFDRWrite() supports cfg->fd = 1 only at this moment.");

    FILE *fout = cfg->fout;
    if (fout == NULL){
        fout = fopen(cfg->filename, "w");
        PANIC_IF(fout == NULL, "Could not open file %s", cfg->filename);
    }

    if (cfg->fd)
        pddlFDRWriteFD(fdr, cfg, fout);

    if (cfg->fout == NULL)
        fclose(fout);
}
