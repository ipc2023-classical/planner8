/***
 * Copyright (c)2022 Daniel Fiser <danfis@danfis.cz>,
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

#include "pddl/lifted_heur_relaxed.h"
#include "datalog_pddl.h"
#include "internal.h"


static void addPreToBody(pddl_lifted_heur_relaxed_t *h,
                         pddl_datalog_rule_t *rule,
                         const pddl_fm_t *pre)
{
    const pddl_fm_atom_t *catom;
    pddl_fm_const_it_atom_t it;
    PDDL_FM_FOR_EACH_ATOM(pre, &it, catom){
        pddl_datalog_atom_t atom;
        pddlDatalogPddlAtomToDLAtom(h->dl, &atom, catom, h->pred_to_dlpred,
                                    h->obj_to_dlconst, h->dlvar);
        if (catom->neg){
            pddlDatalogRuleAddNegStaticBody(h->dl, rule, &atom);
        }else{
            pddlDatalogRuleAddBody(h->dl, rule, &atom);
        }
        pddlDatalogAtomFree(h->dl, &atom);
    }
}

static void addActionRule(pddl_lifted_heur_relaxed_t *h,
                          int action_id,
                          const pddl_fm_t *pre,
                          const pddl_fm_t *eff,
                          const pddl_fm_t *pre2)
{
    const pddl_action_t *action = h->pddl->action.action + action_id;


    pddl_datalog_rule_t rule;
    pddlDatalogRuleInit(h->dl, &rule);

    addPreToBody(h, &rule, pre);
    if (pre2 != NULL)
        addPreToBody(h, &rule, pre2);
    pddlDatalogPddlSetActionTypeBody(h->dl, &rule, h->pddl, &action->param,
                                     pre, pre2, h->type_to_dlpred, h->dlvar);

    // Set cost of the operator
    pddl_cost_t w;
    if (!h->pddl->metric){
        pddlCostSetOp(&w, 1);
    }else{
        pddlCostSetZero(&w);
        const pddl_fm_t *fm;
        pddl_fm_const_it_t it;
        PDDL_FM_FOR_EACH(eff, &it, fm){
            if (pddlFmIsIncrease(fm)){
                const pddl_fm_increase_t *inc = pddlFmToIncreaseConst(fm);
                if (inc->fvalue != NULL){
                    // TODO
                    PANIC("Lifted relaxed heuristics do not support"
                           " non-static action costs yet.");
                }else{
                    w.cost += inc->value;
                }
            }
        }
    }

    const pddl_fm_atom_t *catom;
    pddl_fm_const_it_atom_t it;
    PDDL_FM_FOR_EACH_ATOM(eff, &it, catom){
        if (catom->neg)
            continue;

        pddl_datalog_atom_t atom;
        pddlDatalogPddlAtomToDLAtom(h->dl, &atom, catom, h->pred_to_dlpred,
                                    h->obj_to_dlconst, h->dlvar);
        pddlDatalogRuleSetHead(h->dl, &rule, &atom);
        pddlDatalogAtomFree(h->dl, &atom);

        pddlDatalogRuleSetWeight(h->dl, &rule, &w);
        pddlDatalogAddRule(h->dl, &rule);
    }

    pddlDatalogRuleFree(h->dl, &rule);
}

static void addActionRules(pddl_lifted_heur_relaxed_t *h, int action_id)
{
    const pddl_action_t *action = h->pddl->action.action + action_id;

    addActionRule(h, action_id, action->pre, action->eff, NULL);

    // Conditional effects
    pddl_fm_const_it_when_t wit;
    const pddl_fm_when_t *when;
    PDDL_FM_FOR_EACH_WHEN(action->eff, &wit, when)
        addActionRule(h, action_id, action->pre, when->eff, when->pre);
}

static void addActionsRules(pddl_lifted_heur_relaxed_t *h)
{
    for (int i = 0; i < h->pddl->action.action_size; ++i)
        addActionRules(h, i);
}

static void addGoal(pddl_lifted_heur_relaxed_t *h)
{
    pddl_datalog_rule_t rule;
    pddlDatalogRuleInit(h->dl, &rule);

    h->goal_dlpred = pddlDatalogAddGoalPred(h->dl, "GOAL");
    pddl_datalog_atom_t atom;
    pddlDatalogAtomInit(h->dl, &atom, h->goal_dlpred);
    pddlDatalogRuleSetHead(h->dl, &rule, &atom);
    pddlDatalogAtomFree(h->dl, &atom);

    const pddl_fm_atom_t *a;
    pddl_fm_const_it_atom_t it;
    PDDL_FM_FOR_EACH_ATOM(h->pddl->goal, &it, a){
        pddl_datalog_atom_t atom;
        pddlDatalogAtomInit(h->dl, &atom, h->pred_to_dlpred[a->pred]);
        for (int i = 0; i < a->arg_size; ++i){
            int obj = a->arg[i].obj;
            ASSERT(obj >= 0);
            pddlDatalogAtomSetArg(h->dl, &atom, i, h->obj_to_dlconst[obj]);
        }
        pddlDatalogRuleAddBody(h->dl, &rule, &atom);
        pddlDatalogAtomFree(h->dl, &atom);
    }
    pddlDatalogAddRule(h->dl, &rule);
    pddlDatalogRuleFree(h->dl, &rule);
}

static int addFacts(pddl_lifted_heur_relaxed_t *h,
                    const pddl_iset_t *facts,
                    const pddl_ground_atoms_t *gatoms)
{
    int num_rules = 0;
    int fact;
    PDDL_ISET_FOR_EACH(facts, fact){
        const pddl_ground_atom_t *ga = gatoms->atom[fact];
        if (pddlPredIsStatic(h->pddl->pred.pred + ga->pred))
            continue;

        pddl_datalog_rule_t rule;
        pddlDatalogRuleInit(h->dl, &rule);

        pddl_datalog_atom_t atom;
        pddlDatalogAtomInit(h->dl, &atom, h->pred_to_dlpred[ga->pred]);
        for (int i = 0; i < ga->arg_size; ++i){
            int obj = ga->arg[i];
            ASSERT(obj >= 0);
            pddlDatalogAtomSetArg(h->dl, &atom, i, h->obj_to_dlconst[obj]);
        }
        pddlDatalogRuleSetHead(h->dl, &rule, &atom);
        pddlDatalogAtomFree(h->dl, &atom);
        pddlDatalogAddRule(h->dl, &rule);
        pddlDatalogRuleFree(h->dl, &rule);
        ++num_rules;
    }

    return num_rules;
}

static void addInitStaticFacts(pddl_lifted_heur_relaxed_t *h)
{
    const pddl_fm_atom_t *a;
    pddl_fm_const_it_atom_t it;
    PDDL_FM_FOR_EACH_ATOM(&h->pddl->init->fm, &it, a){
        if (!pddlPredIsStatic(h->pddl->pred.pred + a->pred))
            continue;

        pddl_datalog_atom_t atom;
        pddl_datalog_rule_t rule;
        pddlDatalogRuleInit(h->dl, &rule);
        pddlDatalogAtomInit(h->dl, &atom, h->pred_to_dlpred[a->pred]);
        for (int i = 0; i < a->arg_size; ++i){
            int obj = a->arg[i].obj;
            ASSERT(obj >= 0);
            pddlDatalogAtomSetArg(h->dl, &atom, i, h->obj_to_dlconst[obj]);
        }
        pddlDatalogRuleSetHead(h->dl, &rule, &atom);
        pddlDatalogAtomFree(h->dl, &atom);
        pddlDatalogAddRule(h->dl, &rule);
        pddlDatalogRuleFree(h->dl, &rule);
    }
}

static void pddlLiftedHeurRelaxedInit(pddl_lifted_heur_relaxed_t *h,
                                      const pddl_t *pddl,
                                      int collect_best_achiever_facts,
                                      pddl_err_t *err)
{
    CTX(err, "lifted_relax_heur", "lifted-relax-heur");
    ZEROIZE(h);
    h->pddl = pddl;
    h->collect_best_achiever_facts = collect_best_achiever_facts;
    h->dl = pddlDatalogNew();
    h->type_to_dlpred = ALLOC_ARR(unsigned, h->pddl->type.type_size);
    h->pred_to_dlpred = ALLOC_ARR(unsigned, h->pddl->pred.pred_size);
    h->obj_to_dlconst = ALLOC_ARR(unsigned, h->pddl->obj.obj_size);

    h->dlvar_size = pddlDatalogPddlMaxVarSize(pddl);
    h->dlvar = ALLOC_ARR(unsigned, h->dlvar_size);
    for (int i = 0; i < h->dlvar_size; ++i)
        h->dlvar[i] = pddlDatalogAddVar(h->dl, NULL);

    for (int i = 0; i < h->pddl->type.type_size; ++i)
        h->type_to_dlpred[i] = UINT_MAX;

    for (int i = 0; i < h->pddl->pred.pred_size; ++i){
        const pddl_pred_t *pred = h->pddl->pred.pred + i;
        h->pred_to_dlpred[i]
            = pddlDatalogAddPred(h->dl, pred->param_size, pred->name);
        pddlDatalogSetUserId(h->dl, h->pred_to_dlpred[i], i);
    }

    for (int i = 0; i < h->pddl->obj.obj_size; ++i){
        const pddl_obj_t *obj = h->pddl->obj.obj + i;
        h->obj_to_dlconst[i] = pddlDatalogAddConst(h->dl, obj->name);
        pddlDatalogSetUserId(h->dl, h->obj_to_dlconst[i], i);
    }

    pddlDatalogPddlAddEqRules(h->dl, h->pddl, h->pred_to_dlpred,
                              h->obj_to_dlconst);
    addActionsRules(h);
    addInitStaticFacts(h);
    pddlDatalogPddlAddTypeRules(h->dl, h->pddl, h->type_to_dlpred,
                                h->obj_to_dlconst);
    addGoal(h);

    pddlDatalogToNormalForm(h->dl, err);
    CTXEND(err);
}

static void pddlLiftedHeurRelaxedFree(pddl_lifted_heur_relaxed_t *h)
{
    pddlDatalogDel(h->dl);
    FREE(h->type_to_dlpred);
    FREE(h->pred_to_dlpred);
    FREE(h->obj_to_dlconst);
    FREE(h->dlvar);
}

pddl_cost_t pddlLiftedHeurRelaxed(pddl_lifted_heur_relaxed_t *h,
                                  const pddl_iset_t *state,
                                  const pddl_ground_atoms_t *gatoms,
                                  int (*eval)(pddl_datalog_t *,
                                              pddl_cost_t *,
                                              int collect_fact_achievers,
                                              pddl_err_t *))
{
    pddlDatalogClear(h->dl);
    int new_rules = addFacts(h, state, gatoms);

    pddl_cost_t w = pddl_cost_zero;
    if (eval(h->dl, &w, h->collect_best_achiever_facts, NULL) != 0)
        w = pddl_cost_dead_end;

    pddlDatalogRmLastRules(h->dl, new_rules);
    return w;
}

struct best_achievers {
    const pddl_ground_atoms_t *gatoms;
    pddl_iset_t *achievers;
};
static void bestAchieverFacts(int pred,
                              int arity,
                              const pddl_obj_id_t *arg,
                              const pddl_cost_t *weight,
                              void *_d)
{
    struct best_achievers *d = _d;
    pddl_ground_atom_t *ga;
    ga = pddlGroundAtomsFindPred(d->gatoms, pred, arg, arity);
    if (ga != NULL)
        pddlISetAdd(d->achievers, ga->id);
}

void pddlLiftedHeurRelaxedBestAchieverFacts(pddl_lifted_heur_relaxed_t *h,
                                            const pddl_ground_atoms_t *gatoms,
                                            pddl_iset_t *achievers)
{
    struct best_achievers d;
    d.gatoms = gatoms;
    d.achievers = achievers;
    pddlDatalogFactsFromWeightedCanonicalModel(h->dl, h->goal_dlpred,
                                               bestAchieverFacts, &d);
}


void pddlLiftedHMaxInit(pddl_lifted_hmax_t *h,
                        const pddl_t *pddl,
                        int collect_best_achiever_facts,
                        pddl_err_t *err)
{
    pddlLiftedHeurRelaxedInit(h, pddl, collect_best_achiever_facts, err);
}

void pddlLiftedHMaxFree(pddl_lifted_hmax_t *h)
{
    pddlLiftedHeurRelaxedFree(h);
}

pddl_cost_t pddlLiftedHMax(pddl_lifted_hmax_t *h,
                           const pddl_iset_t *state,
                           const pddl_ground_atoms_t *gatoms)
{
    return pddlLiftedHeurRelaxed(h, state, gatoms,
                                 pddlDatalogWeightedCanonicalModelMax);
}

void pddlLiftedHMaxBestAchieverFacts(pddl_lifted_hmax_t *h,
                                     const pddl_ground_atoms_t *gatoms,
                                     pddl_iset_t *achievers)
{
    pddlLiftedHeurRelaxedBestAchieverFacts(h, gatoms, achievers);
}

void pddlLiftedHAddInit(pddl_lifted_hadd_t *h,
                        const pddl_t *pddl,
                        int collect_best_achiever_facts,
                        pddl_err_t *err)
{
    pddlLiftedHeurRelaxedInit(h, pddl, collect_best_achiever_facts, err);
}

void pddlLiftedHAddFree(pddl_lifted_hadd_t *h)
{
    pddlLiftedHeurRelaxedFree(h);
}

pddl_cost_t pddlLiftedHAdd(pddl_lifted_hadd_t *h,
                           const pddl_iset_t *state,
                           const pddl_ground_atoms_t *gatoms)
{
    return pddlLiftedHeurRelaxed(h, state, gatoms,
                                 pddlDatalogWeightedCanonicalModelAdd);
}

void pddlLiftedHAddBestAchieverFacts(pddl_lifted_hadd_t *h,
                                     const pddl_ground_atoms_t *gatoms,
                                     pddl_iset_t *achievers)
{
    pddlLiftedHeurRelaxedBestAchieverFacts(h, gatoms, achievers);
}
