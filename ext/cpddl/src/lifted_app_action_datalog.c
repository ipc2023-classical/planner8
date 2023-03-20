/***
 * Copyright (c)2022 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

#include "internal.h"
#include "_lifted_app_action.h"
#include "pddl/datalog.h"
#include "datalog_pddl.h"

struct action {
    int id;
    unsigned app_dlpred;
};
typedef struct action action_t;

struct app_action {
    pddl_lifted_app_action_t app_action;
    pddl_err_t *err;

    pddl_datalog_t *dl;
    unsigned *type_to_dlpred;
    unsigned *pred_to_dlpred;
    unsigned *obj_to_dlconst;
    action_t *action;
    int action_size;
    unsigned *dlvar;
    int dlvar_size;
};
typedef struct app_action app_action_t;

#define AA(S,_S) CONTAINER_OF(S, (_S), app_action_t, app_action)

static void aaDel(pddl_lifted_app_action_t *_aa)
{
    AA(aa, _aa);
    pddlDatalogDel(aa->dl);
    FREE(aa->type_to_dlpred);
    FREE(aa->pred_to_dlpred);
    FREE(aa->obj_to_dlconst);
    FREE(aa->action);
    FREE(aa->dlvar);
}

static void aaClearState(pddl_lifted_app_action_t *_aa)
{
    AA(aa, _aa);
    pddlDatalogRollbackDB(aa->dl);
}

static int aaSetStateAtom(pddl_lifted_app_action_t *_aa,
                          const pddl_ground_atom_t *atom)
{
    AA(aa, _aa);
    unsigned pred = aa->pred_to_dlpred[atom->pred];
    unsigned args[atom->arg_size];
    for (int i = 0; i < atom->arg_size; ++i)
        args[i] = aa->obj_to_dlconst[atom->arg[i]];
    pddlDatalogAddFactToDB(aa->dl, pred, args);
    return 0;
}

static void addAction(int aid, int arity, const pddl_obj_id_t *args, void *ud)
{
    app_action_t *aa = ud;
    pddlLiftedAppActionAdd(&aa->app_action, aid, args, arity);
}

static int aaFindAppActions(pddl_lifted_app_action_t *_aa)
{
    AA(aa, _aa);
    pddlDatalogCanonicalModelCont(aa->dl, NULL);

    for (int a = 0; a < aa->action_size; ++a){
        pddlDatalogFactsFromCanonicalModel(aa->dl, aa->action[a].app_dlpred,
                                           addAction, aa);
    }
    return 0;
}

static void actionToDLAtom(app_action_t *aa,
                           unsigned dlpred,
                           int action_arity,
                           pddl_datalog_atom_t *dlatom)
{
    pddlDatalogAtomInit(aa->dl, dlatom, dlpred);
    for (int i = 0; i < action_arity; ++i)
        pddlDatalogAtomSetArg(aa->dl, dlatom, i, aa->dlvar[i]);
}

static unsigned addActionRule(app_action_t *aa,
                              const pddl_t *pddl,
                              int action_id)
{
    pddl_datalog_atom_t atom;
    pddl_datalog_rule_t rule;
    const pddl_action_t *action = pddl->action.action + action_id;
    int action_arity = action->param.param_size;

    char name[128];
    snprintf(name, 128, "app-%s", action->name);
    unsigned app_dlpred = pddlDatalogAddPred(aa->dl, action_arity, name);
    pddlDatalogSetUserId(aa->dl, app_dlpred, action_id);

    pddlDatalogRuleInit(aa->dl, &rule);
    actionToDLAtom(aa, app_dlpred, action_arity, &atom);
    pddlDatalogRuleSetHead(aa->dl, &rule, &atom);
    pddlDatalogAtomFree(aa->dl, &atom);

    const pddl_fm_atom_t *catom;
    pddl_fm_const_it_atom_t it;
    PDDL_FM_FOR_EACH_ATOM(action->pre, &it, catom){
        pddlDatalogPddlAtomToDLAtom(aa->dl, &atom, catom, aa->pred_to_dlpred,
                                    aa->obj_to_dlconst, aa->dlvar);
        if (catom->neg){
            pddlDatalogRuleAddNegStaticBody(aa->dl, &rule, &atom);
        }else{
            pddlDatalogRuleAddBody(aa->dl, &rule, &atom);
        }
        pddlDatalogAtomFree(aa->dl, &atom);
    }
    pddlDatalogPddlSetActionTypeBody(aa->dl, &rule, pddl, &action->param,
                                     action->pre, NULL, aa->type_to_dlpred,
                                     aa->dlvar);

    pddlDatalogAddRule(aa->dl, &rule);
    pddlDatalogRuleFree(aa->dl, &rule);

    return app_dlpred;
}

static void addActionRules(app_action_t *aa, const pddl_t *pddl, int action_id)
{
    action_t *a = aa->action + action_id;
    a->id = action_id;
    a->app_dlpred = addActionRule(aa, pddl, action_id);
}

static void addActionsRules(app_action_t *aa, const pddl_t *pddl)
{
    for (int i = 0; i < pddl->action.action_size; ++i)
        addActionRules(aa, pddl, i);
}

static void addInitStaticFacts(app_action_t *aa, const pddl_t *pddl)
{
    const pddl_fm_atom_t *a;
    pddl_fm_const_it_atom_t it;
    PDDL_FM_FOR_EACH_ATOM(&pddl->init->fm, &it, a){
        if (!pddlPredIsStatic(&pddl->pred.pred[a->pred]))
            continue;

        pddl_datalog_atom_t atom;
        pddl_datalog_rule_t rule;
        pddlDatalogRuleInit(aa->dl, &rule);
        pddlDatalogAtomInit(aa->dl, &atom, aa->pred_to_dlpred[a->pred]);
        for (int i = 0; i < a->arg_size; ++i){
            int obj = a->arg[i].obj;
            ASSERT(obj >= 0);
            pddlDatalogAtomSetArg(aa->dl, &atom, i, aa->obj_to_dlconst[obj]);
        }
        pddlDatalogRuleSetHead(aa->dl, &rule, &atom);
        pddlDatalogAtomFree(aa->dl, &atom);
        pddlDatalogAddRule(aa->dl, &rule);
        pddlDatalogRuleFree(aa->dl, &rule);
    }
}

pddl_lifted_app_action_t *pddlLiftedAppActionNewDatalog(const pddl_t *pddl,
                                                        pddl_err_t *err)
{
    app_action_t *aa = ZALLOC(app_action_t);
    _pddlLiftedAppActionInit(&aa->app_action, pddl, aaDel, aaClearState,
                             aaSetStateAtom, aaFindAppActions, err);

    aa->err = err;
    aa->dl = pddlDatalogNew();
    // TODO: Refactor with strips_ground_datalog
    aa->type_to_dlpred = ALLOC_ARR(unsigned, pddl->type.type_size);
    aa->pred_to_dlpred = ALLOC_ARR(unsigned, pddl->pred.pred_size);
    aa->obj_to_dlconst = ALLOC_ARR(unsigned, pddl->obj.obj_size);
    aa->action = CALLOC_ARR(action_t, pddl->action.action_size);
    aa->action_size = pddl->action.action_size;

    aa->dlvar_size = pddlDatalogPddlMaxVarSize(pddl);
    aa->dlvar = ALLOC_ARR(unsigned, aa->dlvar_size);
    for (int i = 0; i < aa->dlvar_size; ++i)
        aa->dlvar[i] = pddlDatalogAddVar(aa->dl, NULL);

    for (int i = 0; i < pddl->type.type_size; ++i){
        aa->type_to_dlpred[i] = UINT_MAX;
    }

    for (int i = 0; i < pddl->pred.pred_size; ++i){
        const pddl_pred_t *pred = pddl->pred.pred + i;
        aa->pred_to_dlpred[i]
            = pddlDatalogAddPred(aa->dl, pred->param_size, pred->name);
        pddlDatalogSetUserId(aa->dl, aa->pred_to_dlpred[i], i);
    }

    for (int i = 0; i < pddl->obj.obj_size; ++i){
        const pddl_obj_t *obj = pddl->obj.obj + i;
        aa->obj_to_dlconst[i] = pddlDatalogAddConst(aa->dl, obj->name);
        pddlDatalogSetUserId(aa->dl, aa->obj_to_dlconst[i], i);
    }

    pddlDatalogPddlAddEqRules(aa->dl, pddl, aa->pred_to_dlpred,
                              aa->obj_to_dlconst);
    addActionsRules(aa, pddl);
    addInitStaticFacts(aa, pddl);
    pddlDatalogPddlAddTypeRules(aa->dl, pddl, aa->type_to_dlpred,
                                aa->obj_to_dlconst);

    pddlDatalogToNormalForm(aa->dl, err);
    pddlDatalogCanonicalModel(aa->dl, err);
    pddlDatalogSaveStateOfDB(aa->dl);

    return &aa->app_action;
}

