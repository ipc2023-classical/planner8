/***
 * cpddl
 * -------
 * Copyright (c)2016 Daniel Fiser <danfis@danfis.cz>,
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


#include "pddl/pddl_struct.h"
#include "internal.h"

void pddlConfigLog(const pddl_config_t *cfg, pddl_err_t *err)
{
    CTX_NO_TIME(err, "cfg", "Cfg");
    LOG_CONFIG_BOOL(cfg, force_adl, err);
    LOG_CONFIG_BOOL(cfg, normalize, err);
    LOG_CONFIG_BOOL(cfg, remove_empty_types, err);
    LOG_CONFIG_BOOL(cfg, compile_away_cond_eff, err);
    LOG_CONFIG_BOOL(cfg, enforce_unit_cost, err);
    CTXEND(err);
}

static int checkDerivedPredicates(const pddl_t *pddl, pddl_err_t *err)
{
    const pddl_lisp_node_t *root = &pddl->domain_lisp->root;
    for (int i = 0; i < root->child_size; ++i){
        const pddl_lisp_node_t *n = root->child + i;
        if (pddlLispNodeHeadKw(n) == PDDL_KW_DERIVED){
            ERR_RET(err, -1, "Derived predicates are not supported"
                    " (line %d).", n->lineno);
        }
    }
    return 0;
}

static int checkConfig(const pddl_config_t *cfg)
{
    return 1;
}

static const char *parseName(pddl_lisp_t *lisp, int kw,
                             const char *err_name, pddl_err_t *err)
{
    const pddl_lisp_node_t *n;

    n = pddlLispFindNode(&lisp->root, kw);
    if (n == NULL){
        // TODO: Configure warn/err
        ERR_RET(err, NULL, "Could not find %s name definition in %s.",
                err_name, lisp->filename);
    }

    if (n->child_size != 2 || n->child[1].value == NULL){
        ERR_RET(err, NULL, "Invalid %s name definition in %s.",
                err_name, lisp->filename);
    }

    return n->child[1].value;
}

static char *parseDomainName(pddl_lisp_t *lisp, pddl_err_t *err)
{
    const char *name = parseName(lisp, PDDL_KW_DOMAIN, "domain", err);
    if (name != NULL)
        return STRDUP(name);
    return NULL;
}

static char *parseProblemName(pddl_lisp_t *lisp, pddl_err_t *err)
{
    if (lisp == NULL)
        return STRDUP("no-problem");

    const char *name = parseName(lisp, PDDL_KW_PROBLEM, "problem", err);
    if (name != NULL)
        return STRDUP(name);
    return NULL;
}

static int checkDomainName(pddl_t *pddl, pddl_err_t *err)
{
    if (pddl->problem_lisp == NULL)
        return 0;

    const char *problem_domain_name;

    // TODO: Configure err/warn/nothing
    problem_domain_name = parseName(pddl->problem_lisp,
                                    PDDL_KW_DOMAIN2, ":domain", err);
    if (problem_domain_name == NULL)
        TRACE_RET(err, -1);

    if (strcmp(problem_domain_name, pddl->domain_name) != 0){
        WARN(err, "Domain names does not match: `%s' x `%s'",
             pddl->domain_name, problem_domain_name);
        return 0;
    }
    return 0;
}

static int parseMetric(pddl_t *pddl, const pddl_lisp_t *lisp, pddl_err_t *err)
{
    if (lisp == NULL)
        return 0;

    const pddl_lisp_node_t *n;

    n = pddlLispFindNode(&lisp->root, PDDL_KW_METRIC);
    if (n == NULL)
        return 0;

    if (n->child_size != 3
            || n->child[1].value == NULL
            || n->child[1].kw != PDDL_KW_MINIMIZE
            || n->child[2].value != NULL
            || n->child[2].child_size != 1
            || strcmp(n->child[2].child[0].value, "total-cost") != 0){
        ERR_RET(err, -1, "Only (:metric minimize (total-cost)) is supported"
                " (line %d in %s).", n->lineno, lisp->filename);
    }

    pddl->metric = 1;
    return 0;
}

static int parseInit(pddl_t *pddl, pddl_err_t *err)
{
    if (pddl->problem_lisp == NULL){
        pddl->init = pddlFmToAnd(pddlFmNewEmptyAnd());
        return 0;
    }

    const pddl_lisp_node_t *ninit;

    ninit = pddlLispFindNode(&pddl->problem_lisp->root, PDDL_KW_INIT);
    if (ninit == NULL){
        ERR_RET(err, -1, "Missing :init in %s.", pddl->problem_lisp->filename);
    }

    pddl->init = pddlFmParseInit(ninit, pddl, err);
    if (pddl->init == NULL){
        TRACE_PREPEND_RET(err, -1, "While parsing :init specification"
                          " in %s: ", pddl->problem_lisp->filename);
    }

    pddl_fm_const_it_atom_t it;
    const pddl_fm_atom_t *atom;
    PDDL_FM_FOR_EACH_ATOM(&pddl->init->fm, &it, atom)
        pddl->pred.pred[atom->pred].in_init = 1;

    return 0;
}

static int parseGoal(pddl_t *pddl, pddl_err_t *err)
{
    if (pddl->problem_lisp == NULL){
        pddl->goal = &pddlFmNewBool(0)->fm;
        return 0;
    }

    const pddl_lisp_node_t *ngoal;

    ngoal = pddlLispFindNode(&pddl->problem_lisp->root, PDDL_KW_GOAL);
    if (ngoal == NULL)
        ERR_RET(err, -1, "Missing :goal in %s.", pddl->problem_lisp->filename);

    if (ngoal->child_size != 2 || ngoal->child[1].value != NULL){
        ERR_RET(err, -1, "Invalid definition of :goal in %s (line %d).",
                pddl->problem_lisp->filename, ngoal->lineno);
    }

    pddl->goal = pddlFmParse(ngoal->child + 1, pddl, NULL, "", err);
    if (pddl->goal == NULL){
        TRACE_PREPEND_RET(err, -1, "While parsing :goal specification"
                          " in %s: ", pddl->problem_lisp->filename);
    }
    return 0;
}

int pddlInit(pddl_t *pddl, const char *domain_fn, const char *problem_fn,
             const pddl_config_t *cfg, pddl_err_t *err)
{
    CTX(err, "pddl", "PDDL");
    pddlConfigLog(cfg, err);

    ZEROIZE(pddl);
    pddl->cfg = *cfg;

    if (problem_fn == NULL)
        pddl->only_domain = 1;

    LOG(err, "Processing %{domain_fn}s and %{problem_fn}s.",
        domain_fn, (problem_fn != NULL ? problem_fn : "null"));

    if (!checkConfig(cfg)){
        CTXEND(err);
        TRACE_RET(err, -1);
    }

    LOG(err, "Parsing domain lisp file...");
    pddl->domain_lisp = pddlLispParse(domain_fn, err);
    if (pddl->domain_lisp == NULL){
        CTXEND(err);
        TRACE_RET(err, -1);
    }

    if (problem_fn != NULL){
        LOG(err, "Parsing problem lisp file...");
        pddl->problem_lisp = pddlLispParse(problem_fn, err);
        if (pddl->problem_lisp == NULL){
            CTXEND(err);
            if (pddl->domain_lisp)
                pddlLispDel(pddl->domain_lisp);
            TRACE_RET(err, -1);
        }
    }

    LOG(err, "Parsing entire contents of domain/problem PDDL...");
    pddl->domain_name = parseDomainName(pddl->domain_lisp, err);
    if (pddl->domain_name == NULL)
        goto pddl_fail;

    pddl->problem_name = parseProblemName(pddl->problem_lisp, err);
    if (pddl->domain_name == NULL)
        goto pddl_fail;

    if (checkDerivedPredicates(pddl, err) != 0
            || checkDomainName(pddl, err) != 0
            || pddlRequireFlagsParse(pddl, err) != 0
            || pddlTypesParse(pddl, err) != 0
            || pddlObjsParse(pddl, err) != 0
            || pddlPredsParse(pddl, err) != 0
            || pddlFuncsParse(pddl, err) != 0
            || parseInit(pddl, err) != 0
            || parseGoal(pddl, err) != 0
            || pddlActionsParse(pddl, err) != 0
            || parseMetric(pddl, pddl->problem_lisp, err) != 0){
        goto pddl_fail;
    }
    pddlTypesBuildObjTypeMap(&pddl->type, pddl->obj.obj_size);
    LOG(err, "PDDL files processed.");

    if (cfg->normalize){
        pddlNormalize(pddl);
        LOG(err, "PDDL task normalized."
            " types: %{normalized_types}d,"
            " objects: %{normalized_objects}d,"
            " predicates: %{normalized_predicates}d,"
            " functions: %{normalized_functions}d,"
            " actions: %{normalized_actions}d",
            pddl->type.type_size,
            pddl->obj.obj_size,
            pddl->pred.pred_size,
            pddl->func.pred_size,
            pddl->action.action_size);
    }

    if (cfg->remove_empty_types){
        pddlRemoveEmptyTypes(pddl, err);
        if (cfg->normalize){
            pddlNormalize(pddl);
            LOG(err, "PDDL task normalized again."
                " types: %{normalized_types}d,"
                " objects: %{normalized_objects}d,"
                " predicates: %{normalized_predicates}d,"
                " functions: %{normalized_functions}d,"
                " actions: %{normalized_actions}d",
                pddl->type.type_size,
                pddl->obj.obj_size,
                pddl->pred.pred_size,
                pddl->func.pred_size,
                pddl->action.action_size);
        }
    }

    if (cfg->enforce_unit_cost){
        LOG(err, "Enforcing unit-cost...");
        pddlEnforceUnitCost(pddl, err);
        LOG(err, "Enforcing unit-cost DONE.");
    }

    if (cfg->compile_away_cond_eff){
        LOG(err, "Compiling away conditional effects...");
        pddlCompileAwayCondEff(pddl);
        LOG(err, "Conditional effects compiled away.");
    }

    pddlCheckSizeTypes(pddl);
    LOG(err, "Number of PDDL Types: %{num_types}d", pddl->type.type_size);
    LOG(err, "Number of PDDL Objects: %{num_objs}d", pddl->obj.obj_size);
    LOG(err, "Number of PDDL Predicates: %{num_preds}d", pddl->pred.pred_size);
    LOG(err, "Number of PDDL Functions: %{num_funcs}d", pddl->func.pred_size);
    LOG(err, "Number of PDDL Actions: %{num_actions}d", pddl->action.action_size);
    LOG(err, "PDDL Metric: %{has_metric}d", pddl->metric);

    CTXEND(err);
    return 0;

pddl_fail:
    CTXEND(err);
    if (pddl != NULL)
        pddlFree(pddl);
    TRACE_RET(err, -1);
}

void pddlInitCopy(pddl_t *dst, const pddl_t *src)
{
    ZEROIZE(dst);
    dst->cfg = src->cfg;
    dst->domain_lisp = pddlLispClone(src->domain_lisp);
    if (src->problem_lisp != NULL)
        dst->problem_lisp = pddlLispClone(src->problem_lisp);
    if (src->domain_name != NULL)
        dst->domain_name = STRDUP(src->domain_name);
    if (src->problem_name != NULL)
        dst->problem_name = STRDUP(src->problem_name);
    dst->require = src->require;
    pddlTypesInitCopy(&dst->type, &src->type);
    pddlObjsInitCopy(&dst->obj, &src->obj);
    pddlPredsInitCopy(&dst->pred, &src->pred);
    pddlPredsInitCopy(&dst->func, &src->func);
    if (src->init != NULL)
        dst->init = pddlFmToAnd(pddlFmClone(&src->init->fm));
    if (src->goal != NULL)
        dst->goal = pddlFmClone(src->goal);
    pddlActionsInitCopy(&dst->action, &src->action);
    dst->metric = src->metric;
    dst->normalized = src->normalized;
}

pddl_t *pddlNew(const char *domain_fn, const char *problem_fn,
                const pddl_config_t *cfg, pddl_err_t *err)
{
    pddl_t *pddl = ALLOC(pddl_t);

    if (pddlInit(pddl, domain_fn, problem_fn, cfg, err) != 0){
        FREE(pddl);
        return NULL;
    }

    return pddl;
}

void pddlDel(pddl_t *pddl)
{
    pddlFree(pddl);
    FREE(pddl);
}

void pddlFree(pddl_t *pddl)
{
    if (pddl->domain_lisp)
        pddlLispDel(pddl->domain_lisp);
    if (pddl->problem_lisp)
        pddlLispDel(pddl->problem_lisp);
    if (pddl->domain_name != NULL)
        FREE(pddl->domain_name);
    if (pddl->problem_name != NULL)
        FREE(pddl->problem_name);
    pddlTypesFree(&pddl->type);
    pddlObjsFree(&pddl->obj);
    pddlPredsFree(&pddl->pred);
    pddlPredsFree(&pddl->func);
    if (pddl->init)
        pddlFmDel(&pddl->init->fm);
    if (pddl->goal)
        pddlFmDel(pddl->goal);
    pddlActionsFree(&pddl->action);
}

static int markNegPre(pddl_fm_t *c, void *_m)
{
    pddl_fm_atom_t *atom;
    int *m = _m;

    if (c->type == PDDL_FM_ATOM){
        atom = PDDL_FM_CAST(c, atom);
        if (atom->neg)
            m[atom->pred] = 1;
    }

    return 0;
}

static int markNegPreWhen(pddl_fm_t *c, void *_m)
{
    pddl_fm_when_t *when;

    if (c->type == PDDL_FM_WHEN){
        when = PDDL_FM_CAST(c, when);
        pddlFmTraverse(when->pre, markNegPre, NULL, _m);
    }

    return 0;
}

/** Sets to 1 indexes in {np} of those predicates that are not static and
 *  appear as negative preconditions */
static void findNonStaticPredInNegPre(pddl_t *pddl, int *np)
{
    int i;

    ZEROIZE_ARR(np, pddl->pred.pred_size);
    for (i = 0; i < pddl->action.action_size; ++i){
        pddlFmTraverse(pddl->action.action[i].pre, markNegPre, NULL, np);
        pddlFmTraverse(pddl->action.action[i].eff, markNegPreWhen, NULL, np);
    }
    // Also, check the goal
    if (pddl->goal)
        pddlFmTraverse(pddl->goal, markNegPre, NULL, np);

    for (i = 0; i < pddl->pred.pred_size; ++i){
        if (pddlPredIsStatic(pddl->pred.pred + i))
            np[i] = 0;
    }
}

/** Create a new NOT-... predicate and returns its ID */
static int createNewNotPred(pddl_t *pddl, int pred_id)
{
    pddl_pred_t *pos = pddl->pred.pred + pred_id;
    pddl_pred_t *neg;
    int name_size;
    char *name;

    name_size = strlen(pos->name) + 4;
    name = ALLOC_ARR(char, name_size + 1);
    strcpy(name, "NOT-");
    strcpy(name + 4, pos->name);

    neg = pddlPredsAddCopy(&pddl->pred, pred_id);
    if (neg->name != NULL)
        FREE(neg->name);
    neg->name = name;
    neg->neg_of = pred_id;
    pddl->pred.pred[pred_id].neg_of = neg->id;

    return neg->id;
}

static int replaceNegPre(pddl_fm_t **c, void *_ids)
{
    int *ids = _ids;
    int pos = ids[0];
    int neg = ids[1];
    pddl_fm_atom_t *atom;

    if ((*c)->type == PDDL_FM_ATOM){
        atom = PDDL_FM_CAST(*c, atom);
        if (atom->pred == pos && atom->neg){
            atom->pred = neg;
            atom->neg = 0;
        }
    }

    return 0;
}

static int replaceNegEff(pddl_fm_t **c, void *_ids)
{
    int *ids = _ids;
    int pos = ids[0];
    int neg = ids[1];
    pddl_fm_t *c2;
    pddl_fm_atom_t *atom, *not_atom;
    pddl_fm_when_t *when;
    pddl_fm_junc_t *and;

    if ((*c)->type == PDDL_FM_WHEN){
        when = PDDL_FM_CAST(*c, when);
        pddlFmRebuild(&when->pre, NULL, replaceNegPre, _ids);
        pddlFmRebuild(&when->eff, replaceNegEff, NULL, _ids);
        return -1;

    }else if ((*c)->type == PDDL_FM_ATOM){
        atom = PDDL_FM_CAST(*c, atom);
        if (atom->pred == pos){
            // Create new NOT atom and flip negation
            c2 = pddlFmClone(*c);
            not_atom = PDDL_FM_CAST(c2, atom);
            not_atom->pred = neg;
            not_atom->neg = !atom->neg;

            // Transorm atom to (and atom)
            *c = pddlFmAtomToAnd(*c);
            and = pddlFmToJunc(*c);
            pddlFmJuncAdd(and, c2);

            // Prevent recursion
            return -1;
        }
    }

    return 0;
}

static void compileOutNegPreInAction(pddl_t *pddl, int pos, int neg,
                                     pddl_action_t *a)
{
    int ids[2] = { pos, neg };
    pddlFmRebuild(&a->pre, NULL, replaceNegPre, ids);
    pddlFmRebuild(&a->eff, replaceNegEff, NULL, ids);
    pddlActionNormalize(a, pddl);
}

static void compileOutNegPre(pddl_t *pddl, int pos, int neg)
{
    int i;

    for (i = 0; i < pddl->action.action_size; ++i)
        compileOutNegPreInAction(pddl, pos, neg, pddl->action.action + i);

    if (pddl->goal){
        int ids[2] = { pos, neg };
        pddlFmRebuild(&pddl->goal, NULL, replaceNegPre, ids);
    }
}

static int initHasFact(const pddl_t *pddl, int pred,
                       int arg_size, const pddl_obj_id_t *arg)
{
    pddl_list_t *item;
    const pddl_fm_t *c;
    const pddl_fm_atom_t *a;
    int i;

    PDDL_LIST_FOR_EACH(&pddl->init->part, item){
        c = PDDL_LIST_ENTRY(item, const pddl_fm_t, conn);
        if (c->type != PDDL_FM_ATOM)
            continue;
        a = PDDL_FM_CAST(c, atom);
        if (a->pred != pred || a->arg_size != arg_size)
            continue;
        for (i = 0; i < arg_size; ++i){
            if (a->arg[i].obj != arg[i])
                break;
        }
        if (i == arg_size)
            return 1;
    }

    return 0;
}

static void addNotPredsToInitRec(pddl_t *pddl, int pos, int neg,
                                 int arg_size, pddl_obj_id_t *arg,
                                 const pddl_pred_t *pred, int argi)
{
    pddl_fm_atom_t *a;
    const pddl_obj_id_t *obj;
    int obj_size;

    if (argi == arg_size){
        if (!initHasFact(pddl, pos, arg_size, arg)){
            a = pddlFmCreateFactAtom(neg, arg_size, arg);
            pddlFmJuncAdd(pddl->init, &a->fm);
            pddl->pred.pred[a->pred].in_init = 1;
        }

        return;
    }

    obj = pddlTypesObjsByType(&pddl->type, pred->param[argi], &obj_size);
    for (int i = 0; i < obj_size; ++i){
        arg[argi] = obj[i];
        addNotPredsToInitRec(pddl, pos, neg, arg_size, arg, pred, argi + 1);
    }
}

static void addNotPredsToInit(pddl_t *pddl, int pos, int neg)
{
    const pddl_pred_t *pos_pred = pddl->pred.pred + pos;
    pddl_obj_id_t arg[pos_pred->param_size];

    // Recursivelly try all possible objects for each argument
    addNotPredsToInitRec(pddl, pos, neg,
                         pos_pred->param_size, arg, pos_pred, 0);
}

/** Compile out negative preconditions if they are not static */
static void compileOutNonStaticNegPre(pddl_t *pddl)
{
    int size, *negpred;

    size = pddl->pred.pred_size;
    negpred = ALLOC_ARR(int, size);
    findNonStaticPredInNegPre(pddl, negpred);

    for (int i = 0; i < size; ++i){
        if (negpred[i]){
            int not = createNewNotPred(pddl, i);
            compileOutNegPre(pddl, i, not);
            addNotPredsToInit(pddl, i, not);
        }
    }
    FREE(negpred);
}

static int isFalsePre(const pddl_fm_t *c)
{
    if (c->type == PDDL_FM_BOOL){
        const pddl_fm_bool_t *b = PDDL_FM_CAST(c, bool);
        return !b->val;
    }
    return 0;
}

static pddl_fm_t *simplifyPre(pddl_fm_t *pre,
                                const pddl_t *pddl,
                                const pddl_params_t *param)
{
    pddl_fm_t *c = pddlFmSimplify(pre, pddl, param);
    if (pddlFmIsTrue(c)){
        pddlFmDel(c);
        c = pddlFmNewEmptyAnd();
    }else if (pddlFmIsAtom(c)){
        pddl_fm_t *a = pddlFmNewEmptyAnd();
        pddlFmJuncAdd(pddlFmToJunc(a), c);
        c = a;
    }
    return c;
}

static void removeIrrelevantActions(pddl_t *pddl)
{
    PDDL_ISET(rm);
    for (int ai = 0; ai < pddl->action.action_size; ++ai){
        pddl_action_t *a = pddl->action.action + ai;
        a->pre = simplifyPre(a->pre, pddl, &a->param);
        a->eff = pddlFmDeconflictEff(a->eff, pddl, &a->param);

        if (isFalsePre(a->pre) || !pddlFmHasAtom(a->eff))
            pddlISetAdd(&rm, ai);
    }

    pddlActionsRemoveSet(&pddl->action, &rm);
    pddlISetFree(&rm);
}

static int removeActionsWithUnsatisfiableArgs(pddl_t *pddl)
{
    PDDL_ISET(rm);
    int ret = 0;
    for (int ai = 0; ai < pddl->action.action_size; ++ai){
        pddl_action_t *a = pddl->action.action + ai;
        int remove = 0;
        for (int pi = 0; pi < a->param.param_size; ++pi){
            if (pddlTypeNumObjs(&pddl->type, a->param.param[pi].type) == 0){
                remove = 1;
                break;
            }
        }

        if (remove){
            pddlISetAdd(&rm, ai);
            ret = 1;
        }
    }

    pddlActionsRemoveSet(&pddl->action, &rm);
    pddlISetFree(&rm);

    return ret;
}

static int isStaticPreUnreachable(const pddl_t *pddl, const pddl_fm_t *c)
{
    pddl_fm_const_it_atom_t it;
    const pddl_fm_atom_t *atom;
    PDDL_FM_FOR_EACH_ATOM(c, &it, atom){
        const pddl_pred_t *pred = pddl->pred.pred + atom->pred;
        if (pred->id != pddl->pred.eq_pred
                && pddlPredIsStatic(pred)
                && !pred->in_init){
            return 1;
        }
    }
    return 0;
}

static int isInequalityUnsatisfiable(const pddl_t *pddl,
                                     const pddl_action_t *action)
{
    pddl_fm_const_it_atom_t it;
    const pddl_fm_atom_t *atom;
    PDDL_FM_FOR_EACH_ATOM(action->pre, &it, atom){
        if (atom->neg && atom->pred == pddl->pred.eq_pred){
            int param1 = atom->arg[0].param;
            int obj1 = atom->arg[0].obj;
            int param2 = atom->arg[1].param;
            int obj2 = atom->arg[1].obj;
            if (param1 >= 0){
                int type = action->param.param[param1].type;
                if (pddlTypeNumObjs(&pddl->type, type) == 1)
                    obj1 = pddlTypeGetObj(&pddl->type, type, 0);
            }
            if (param2 >= 0){
                int type = action->param.param[param2].type;
                if (pddlTypeNumObjs(&pddl->type, type) == 1)
                    obj2 = pddlTypeGetObj(&pddl->type, type, 0);
            }

            if (obj1 >= 0 && obj2 >= 0 && obj1 == obj2)
                return 1;
        }
    }
    return 0;
}

static int removeUnreachableActions(pddl_t *pddl)
{
    PDDL_ISET(rm);
    int ret = 0;
    for (int ai = 0; ai < pddl->action.action_size; ++ai){
        pddl_action_t *a = pddl->action.action + ai;
        a->pre = simplifyPre(a->pre, pddl, &a->param);
        a->eff = pddlFmDeconflictEff(a->eff, pddl, &a->param);

        if (isStaticPreUnreachable(pddl, a->pre)
                || isInequalityUnsatisfiable(pddl, a)){
            pddlISetAdd(&rm, ai);
            ret = 1;
        }
    }

    pddlActionsRemoveSet(&pddl->action, &rm);
    pddlISetFree(&rm);

    return ret;
}

static void pddlResetPredReadWrite(pddl_t *pddl)
{
    for (int i = 0; i < pddl->pred.pred_size; ++i)
        pddl->pred.pred[i].read = pddl->pred.pred[i].write = 0;
    for (int i = 0; i < pddl->action.action_size; ++i){
        const pddl_action_t *a = pddl->action.action + i;
        pddlFmSetPredRead(a->pre, &pddl->pred);
        pddlFmSetPredReadWriteEff(a->eff, &pddl->pred);
    }
}

void pddlNormalize(pddl_t *pddl)
{
    pddl_fm_t *c = pddlFmDeduplicateAtoms(&pddl->init->fm, pddl);
    ASSERT_RUNTIME(c->type == PDDL_FM_AND);
    pddl->init = pddlFmToAnd(c);

    if (!pddl->only_domain && !pddl->cfg.keep_all_actions)
        removeActionsWithUnsatisfiableArgs(pddl);

    for (int i = 0; i < pddl->action.action_size; ++i)
        pddlActionNormalize(pddl->action.action + i, pddl);

    for (int i = 0; i < pddl->action.action_size; ++i)
        pddlActionSplit(pddl->action.action + i, pddl);

    if (!pddl->only_domain && !pddl->cfg.keep_all_actions)
        removeIrrelevantActions(pddl);

#ifdef PDDL_DEBUG
    for (int i = 0; i < pddl->action.action_size; ++i){
        pddlActionAssertPreConjuction(pddl->action.action + i);
    }
#endif

    if (pddl->goal)
        pddl->goal = pddlFmNormalize(pddl->goal, pddl, NULL);

    compileOutNonStaticNegPre(pddl);
    if (!pddl->only_domain && !pddl->cfg.keep_all_actions){
        removeIrrelevantActions(pddl);
        do {
            pddlResetPredReadWrite(pddl);
        } while (removeUnreachableActions(pddl));
    }
    pddl->normalized = 1;
}

static void compileAwayCondEff(pddl_t *pddl, int only_non_static)
{
    pddl_action_t *a, *new_a;
    pddl_fm_when_t *w;
    pddl_fm_t *neg_pre;
    int asize;
    int change;

    do {
        change = 0;
        pddlNormalize(pddl);

        asize = pddl->action.action_size;
        for (int ai = 0; ai < asize; ++ai){
            a = pddl->action.action + ai;
            if (only_non_static){
                w = pddlFmRemoveFirstNonStaticWhen(a->eff, pddl);
            }else{
                w = pddlFmRemoveFirstWhen(a->eff, pddl);
            }
            if (w != NULL){
                // Create a new action
                new_a = pddlActionsAddCopy(&pddl->action, ai);

                // Get the original action again, because pddlActionsAdd()
                // could realloc the array.
                a = pddl->action.action + ai;

                // The original takes additional precondition which is the
                // negation of w->pre
                if ((neg_pre = pddlFmNegate(w->pre, pddl)) == NULL){
                    // This shoud never fail, because we force
                    // normalization before this.
                    PANIC("Fatal Error: Encountered problem in"
                                " the normalization.");
                }
                a->pre = pddlFmNewAnd2(a->pre, neg_pre);

                // The new action extends both pre and eff by w->pre and
                // w->eff.
                new_a->pre = pddlFmNewAnd2(new_a->pre, pddlFmClone(w->pre));
                new_a->eff = pddlFmNewAnd2(new_a->eff, pddlFmClone(w->eff));

                pddlFmDel(&w->fm);
                change = 1;
            }
        }
    } while (change);
    pddlResetPredReadWrite(pddl);
}

void pddlCompileAwayCondEff(pddl_t *pddl)
{
    compileAwayCondEff(pddl, 0);
}

void pddlCompileAwayNonStaticCondEff(pddl_t *pddl)
{
    compileAwayCondEff(pddl, 1);
}

int pddlPredFuncMaxParamSize(const pddl_t *pddl)
{
    int max = 0;

    for (int i = 0; i < pddl->pred.pred_size; ++i)
        max = PDDL_MAX(max, pddl->pred.pred[i].param_size);
    for (int i = 0; i < pddl->func.pred_size; ++i)
        max = PDDL_MAX(max, pddl->func.pred[i].param_size);

    return max;
}

void pddlCheckSizeTypes(const pddl_t *pddl)
{
    unsigned long max_size;

    max_size = (1ul << (sizeof(pddl_obj_size_t) * 8)) - 1;
    if (pddl->obj.obj_size > max_size){
        PANIC("The problem has %d objects, but pddl_obj_size_t can"
                   " hold only %lu.",
                   pddl->obj.obj_size,
                   sizeof(pddl_obj_size_t) * 8 - 1);
    }

    max_size = (1ul << (sizeof(pddl_action_param_size_t) * 8)) - 1;
    for (int ai = 0; ai < pddl->action.action_size; ++ai){
        int param_size = pddl->action.action[ai].param.param_size;
        if (param_size > max_size){
            PANIC("The action %s has %d parameters, but"
                       "pddl_action_param_size_t can hold only %lu.",
                       pddl->action.action[ai].name,
                       param_size,
                       sizeof(pddl_action_param_size_t) * 8 - 1);
        }
    }
}

void pddlAddObjectTypes(pddl_t *pddl)
{
    for (pddl_obj_id_t obj_id = 0; obj_id < pddl->obj.obj_size; ++obj_id){
        pddl_obj_t *obj = pddl->obj.obj + obj_id;
        ASSERT(obj->type >= 0);
        if (pddlTypeNumObjs(&pddl->type, obj->type) <= 1)
            continue;

        char *name = ALLOC_ARR(char, strlen(obj->name) + 8 + 1);
        sprintf(name, "%s-OBJTYPE", obj->name);
        int type_id = pddlTypesAdd(&pddl->type, name, obj->type);
        ASSERT(type_id == pddl->type.type_size - 1);
        pddlTypesAddObj(&pddl->type, obj_id, type_id);
        obj->type = type_id;
        FREE(name);
    }
    pddlTypesBuildObjTypeMap(&pddl->type, pddl->obj.obj_size);
}


void pddlRemoveObjs(pddl_t *pddl, const pddl_iset_t *rm_obj, pddl_err_t *err)
{
    if (pddlISetSize(rm_obj) == 0)
        return;
    pddl_obj_id_t *remap = ALLOC_ARR(pddl_obj_id_t, pddl->obj.obj_size);
    pddlRemoveObjsGetRemap(pddl, rm_obj, remap, err);
    FREE(remap);
}

void pddlRemoveObjsGetRemap(pddl_t *pddl,
                            const pddl_iset_t *rm_obj,
                            pddl_obj_id_t *remap,
                            pddl_err_t *err)
{
    if (pddlISetSize(rm_obj) == 0)
        return;
    CTX(err, "pddl_rm_objs", "PDDL rm objs");
    LOG(err, "Removing %d objects", pddlISetSize(rm_obj));

    for (int i = 0, idx = 0, id = 0; i < pddl->obj.obj_size; ++i){
        if (idx < pddlISetSize(rm_obj) && pddlISetGet(rm_obj, idx) == i){
            remap[i] = PDDL_OBJ_ID_UNDEF;
            ++idx;
        }else{
            remap[i] = id++;
        }
    }

    pddlRemapObjs(pddl, remap);
    CTXEND(err);
}

void pddlRemapObjs(pddl_t *pddl, const pddl_obj_id_t *remap)
{
    pddlFmRemapObjs(&pddl->init->fm, remap);
    pddl_fm_t *c = pddlFmRemoveInvalidAtoms(&pddl->init->fm);
    ASSERT_RUNTIME(c->type == PDDL_FM_AND);
    pddl->init = pddlFmToAnd(c);

    pddlFmRemapObjs(pddl->goal, remap);
    pddl->goal = pddlFmRemoveInvalidAtoms(pddl->goal);
    if (pddl->goal == NULL)
        pddl->goal = &pddlFmNewBool(1)->fm;

    pddlActionsRemapObjs(&pddl->action, remap);
    pddlTypesRemapObjs(&pddl->type, remap);
    pddlObjsRemap(&pddl->obj, remap);
}

void pddlRemoveEmptyTypes(pddl_t *pddl, pddl_err_t *err)
{
    CTX(err, "pddl_rm_empty_types", "PDDL rm empty-types");
    int *type_remap = CALLOC_ARR(int, pddl->type.type_size);
    int *pred_remap = CALLOC_ARR(int, pddl->pred.pred_size);
    int *func_remap = CALLOC_ARR(int, pddl->func.pred_size);
    int type_size = pddl->type.type_size;
    int pred_size = pddl->pred.pred_size;
    int func_size = pddl->func.pred_size;
    int action_size = pddl->action.action_size;

    pddlTypesRemoveEmpty(&pddl->type, pddl->obj.obj_size, type_remap);
    LOG(err, "Removed %d empty types", type_size - pddl->type.type_size);
    if (type_size != pddl->type.type_size){
        pddlObjsRemapTypes(&pddl->obj, type_remap);
        pddlPredsRemapTypes(&pddl->pred, type_remap, pred_remap);
        LOG(err, "Removed %d predicates", pred_size - pddl->pred.pred_size);
        pddlPredsRemapTypes(&pddl->func, type_remap, func_remap);
        LOG(err, "Removed %d functions", func_size - pddl->func.pred_size);
        pddlActionsRemapTypesAndPreds(&pddl->action, type_remap,
                                      pred_remap, func_remap);
        LOG(err, "Removed %d actions",
                  action_size - pddl->action.action_size);

        if (pred_size != pddl->pred.pred_size
                || func_size != pddl->func.pred_size){

            if (pddlFmRemapPreds(&pddl->init->fm,
                                 pred_remap, func_remap) != 0){
                LOG(err, "The task is unsolvable, because the initial"
                           " state is false");
                pddlFmDel(&pddl->init->fm);
                pddl_fm_t *c = pddlFmNewEmptyAnd();
                pddl->init = pddlFmToAnd(c);
                pddl_fm_bool_t *b = pddlFmNewBool(0);
                pddlFmJuncAdd(pddl->init, &b->fm);
            }

            if (pddlFmRemapPreds(pddl->goal, pred_remap, func_remap) != 0){
                LOG(err, "The task is unsolvable, because the goal"
                           " is false");
                pddlFmDel(pddl->goal);
                pddl_fm_bool_t *b = pddlFmNewBool(0);
                pddl->goal = &b->fm;
            }
        }
    }


    FREE(type_remap);
    FREE(pred_remap);
    FREE(func_remap);
    CTXEND(err);
}

static int _removeAssignIncrease(pddl_fm_t **c, void *_)
{
    if ((*c)->type == PDDL_FM_ASSIGN || (*c)->type == PDDL_FM_INCREASE){
        pddlFmDel(*c);
        *c = NULL;
    }
    return 0;
}

void pddlEnforceUnitCost(pddl_t *pddl, pddl_err_t *err)
{
    CTX(err, "pddl_enforce_unit_cost", "PDDL enforce unit-cost");
    // Remove (= ...) from the initial state
    pddl_fm_t *init = &pddl->init->fm;
    pddlFmRebuild(&init, NULL, _removeAssignIncrease, NULL);
    ASSERT_RUNTIME(init->type == PDDL_FM_AND);
    pddl->init = pddlFmToAnd(init);

    for (int ai = 0; ai < pddl->action.action_size; ++ai){
        pddl_action_t *a = pddl->action.action + ai;
        pddlFmRebuild(&a->eff, NULL, _removeAssignIncrease, NULL);
    }

    pddl->metric = 0;
    CTXEND(err);
}

void pddlPrintPDDLDomain(const pddl_t *pddl, FILE *fout)
{
    fprintf(fout, "(define (domain %s)\n", pddl->domain_name);
    pddlRequireFlagsPrintPDDL(&pddl->require, fout);
    pddlTypesPrintPDDL(&pddl->type, fout);
    pddlObjsPrintPDDLConstants(&pddl->obj, &pddl->type, fout);
    pddlPredsPrintPDDL(&pddl->pred, &pddl->type, fout);
    pddlFuncsPrintPDDL(&pddl->func, &pddl->type, fout);
    pddlActionsPrintPDDL(&pddl->action, pddl, fout);
    fprintf(fout, ")\n");
}

void pddlPrintPDDLProblem(const pddl_t *pddl, FILE *fout)
{
    pddl_list_t *item;
    pddl_fm_t *c;
    pddl_params_t params;

    fprintf(fout, "(define (problem %s) (:domain %s)\n",
            pddl->problem_name, pddl->domain_name);

    pddlParamsInit(&params);
    fprintf(fout, "(:init\n");
    PDDL_LIST_FOR_EACH(&pddl->init->part, item){
        c = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);
        fprintf(fout, "  ");
        pddlFmPrintPDDL(c, pddl, &params, fout);
        fprintf(fout, "\n");
    }
    fprintf(fout, ")\n");
    pddlParamsFree(&params);

    fprintf(fout, "(:goal ");
    pddlFmPrintPDDL(pddl->goal, pddl, NULL, fout);
    fprintf(fout, ")\n");

    if (pddl->metric)
        fprintf(fout, "(:metric minimize (total-cost))\n");

    fprintf(fout, ")\n");
}

static int initCondSize(const pddl_t *pddl, int type)
{
    pddl_list_t *item;
    const pddl_fm_t *c;
    int size = 0;

    PDDL_LIST_FOR_EACH(&pddl->init->part, item){
        c = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);
        if (c->type == type)
            ++size;
    }
    return size;
}

// TODO: Rename to pddlPrintDebug
void pddlPrintDebug(const pddl_t *pddl, FILE *fout)
{
    pddl_list_t *item;
    pddl_fm_t *c;
    pddl_fm_atom_t *a;
    pddl_params_t params;

    fprintf(fout, "Domain: %s\n", pddl->domain_name);
    fprintf(fout, "Problem: %s\n", pddl->problem_name);
    fprintf(fout, "Require: %x\n", pddlRequireFlagsToMask(&pddl->require));
    pddlTypesPrint(&pddl->type, fout);
    pddlObjsPrint(&pddl->obj, fout);
    pddlPredsPrint(&pddl->pred, "Predicate", fout);
    pddlPredsPrint(&pddl->func, "Function", fout);
    pddlActionsPrint(pddl, &pddl->action, fout);

    pddlParamsInit(&params);
    fprintf(fout, "Init[%d]:\n", initCondSize(pddl, PDDL_FM_ATOM));
    PDDL_LIST_FOR_EACH(&pddl->init->part, item){
        c = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);
        if (c->type != PDDL_FM_ATOM)
            continue;
        a = PDDL_FM_CAST(c, atom);
        fprintf(fout, "  ");
        if (pddlPredIsStatic(&pddl->pred.pred[a->pred]))
            fprintf(fout, "S:");
        pddlFmPrintPDDL(c, pddl, &params, fout);
        fprintf(fout, "\n");
    }

    fprintf(fout, "Init[%d]:\n", initCondSize(pddl, PDDL_FM_ASSIGN));
    PDDL_LIST_FOR_EACH(&pddl->init->part, item){
        c = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);
        if (c->type != PDDL_FM_ASSIGN)
            continue;
        fprintf(fout, "  ");
        pddlFmPrintPDDL(c, pddl, &params, fout);
        fprintf(fout, "\n");
    }
    pddlParamsFree(&params);

    fprintf(fout, "Goal: ");
    pddlFmPrint(pddl, pddl->goal, NULL, fout);
    fprintf(fout, "\n");

    fprintf(fout, "Metric: %d\n", pddl->metric);
}
