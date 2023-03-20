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

#include "internal.h"
#include "lisp_err.h"
#include "pddl/config.h"
#include "pddl/pddl.h"
#include "pddl/action.h"


#define PDDL_ACTIONS_ALLOC_INIT 4

#define ERR_PREFIX_MAXSIZE 128


static int parseAction(pddl_t *pddl, const pddl_lisp_node_t *root,
                       pddl_err_t *err)
{
    char err_prefix[ERR_PREFIX_MAXSIZE];
    const pddl_lisp_node_t *n;
    pddl_action_t *a;
    int i, ret;

    if (root->child_size < 4
            || root->child_size / 2 == 1
            || root->child[1].value == NULL){
        PDDL_ERR_RET(err, -1, "Invalid definition.");
    }

    a = pddlActionsAddEmpty(&pddl->action);
    a->name = STRDUP(root->child[1].value);
    for (i = 2; i < root->child_size; i += 2){
        n = root->child + i + 1;
        if (root->child[i].kw == PDDL_KW_AGENT){
            if (!pddl->require.multi_agent){
                // TODO: err/warn
                ERR_LISP_RET2(err, -1, root->child + i,
                              ":agent is allowed only with :multi-agent"
                              " requirement");
            }

            ret = pddlParamsParseAgent(&a->param, root, i, &pddl->type, err);
            if (ret < 0)
                PDDL_TRACE_RET(err, -1);
            i = ret - 2;

        }else if (root->child[i].kw == PDDL_KW_PARAMETERS){
            if (pddlParamsParse(&a->param, n, &pddl->type, err) != 0)
                PDDL_TRACE_RET(err, -1);

        }else if (root->child[i].kw == PDDL_KW_PRE){
            // Skip empty preconditions, i.e., () or (and)
            //      -- it will be set to the empty conjunction later anyway.
            if (pddlLispNodeIsEmptyAnd(n))
                continue;

            snprintf(err_prefix, ERR_PREFIX_MAXSIZE,
                     "Precondition of the action `%s': ", a->name);
            a->pre = pddlFmParse(n, pddl, &a->param, err_prefix, err);
            if (a->pre == NULL)
                PDDL_TRACE_RET(err, -1);
            if (pddlFmCheckPre(a->pre, &pddl->require, err) != 0)
                PDDL_TRACE_RET(err, -1);
            pddlFmSetPredRead(a->pre, &pddl->pred);

        }else if (root->child[i].kw == PDDL_KW_EFF){
            if (pddlLispNodeIsEmptyAnd(n))
                continue;

            snprintf(err_prefix, ERR_PREFIX_MAXSIZE,
                     "Effect of the action `%s': ", a->name);
            a->eff = pddlFmParse(n, pddl, &a->param, err_prefix, err);
            if (a->eff == NULL)
                PDDL_TRACE_RET(err, -1);
            if (pddlFmCheckEff(a->eff, &pddl->require, err) != 0)
                PDDL_TRACE_RET(err, -1);
            pddlFmSetPredReadWriteEff(a->eff, &pddl->pred);

        }else{
            ERR_LISP_RET(err, -1, root->child + i, "Unexpected token: %s",
                         root->child[i].value);
        }
    }

    // Empty precondition is allowed meaning the action can be applied in
    // any state
    if (a->pre == NULL)
        a->pre = pddlFmNewEmptyAnd();

    // Empty effect is also allowed because of some domains that contain
    // these actions. This action can be later removed by pddlNormalize().
    if (a->eff == NULL)
        a->eff = pddlFmNewEmptyAnd();

    // TODO: Check compatibility of types of parameters and types of
    //       arguments of all predicates.
    //       --> Restrict types instead of disallowing such an action?

    return 0;
}

int pddlActionsParse(pddl_t *pddl, pddl_err_t *err)
{
    const pddl_lisp_node_t *root = &pddl->domain_lisp->root;
    const pddl_lisp_node_t *n;

    for (int i = 0; i < root->child_size; ++i){
        n = root->child + i;
        if (pddlLispNodeHeadKw(n) == PDDL_KW_ACTION){
            if (parseAction(pddl, n, err) != 0){
                PDDL_TRACE_PREPEND_RET(err, -1, "While parsing :action in %s"
                                       " on line %d: ",
                                       pddl->domain_lisp->filename, n->lineno);
            }
        }
    }
    LOG(err, "Actions parsed, num actions: %{parsed_actions}d",
        pddl->action.action_size);
    return 0;
}

void pddlActionsInitCopy(pddl_actions_t *dst, const pddl_actions_t *src)
{
    ZEROIZE(dst);
    dst->action_size = dst->action_alloc = src->action_size;
    dst->action = CALLOC_ARR(pddl_action_t, src->action_size);
    for (int i = 0; i < src->action_size; ++i)
        pddlActionInitCopy(dst->action + i, src->action + i);
}

void pddlActionInit(pddl_action_t *a)
{
    ZEROIZE(a);
    pddlParamsInit(&a->param);
    a->id = -1;
}

void pddlActionFree(pddl_action_t *a)
{
    if (a->name != NULL)
        FREE(a->name);
    pddlParamsFree(&a->param);
    if (a->pre != NULL)
        pddlFmDel(a->pre);
    if (a->eff != NULL)
        pddlFmDel(a->eff);
}

void pddlActionInitCopy(pddl_action_t *dst, const pddl_action_t *src)
{
    pddlActionInit(dst);
    if (src->name != NULL)
        dst->name = STRDUP(src->name);
    pddlParamsInitCopy(&dst->param, &src->param);
    if (src->pre != NULL)
        dst->pre = pddlFmClone(src->pre);
    if (src->eff != NULL)
        dst->eff = pddlFmClone(src->eff);
    dst->id = src->id;
}

struct propagate_eq {
    pddl_action_t *a;
    int eq_pred;

    const pddl_fm_atom_t *eq_atom;
    int param;
    pddl_obj_id_t obj;
};

static int setParamToObj(pddl_fm_t *cond, void *ud)
{
    struct propagate_eq *ctx = ud;

    if (cond->type == PDDL_FM_ATOM){
        pddl_fm_atom_t *atom = PDDL_FM_CAST(cond, atom);
        if (atom == ctx->eq_atom)
            return 0;

        for (int i = 0; i < atom->arg_size; ++i){
            if (atom->arg[i].param == ctx->param){
                atom->arg[i].param = -1;
                atom->arg[i].obj = ctx->obj;
            }
        }
    }

    return 0;
}

static int _propagateEquality(pddl_fm_t *c, void *ud)
{
    struct propagate_eq *ctx = ud;

    if (c->type == PDDL_FM_ATOM){
        const pddl_fm_atom_t *atom = PDDL_FM_CAST(c, atom);
        if (atom->pred == ctx->eq_pred && !atom->neg){
            if (atom->arg[0].param >= 0 && atom->arg[1].obj >= 0){
                ctx->eq_atom = atom;
                ctx->param = atom->arg[0].param;
                ctx->obj = atom->arg[1].obj;
                pddlFmTraverse(ctx->a->pre, NULL, setParamToObj, ctx);
                pddlFmTraverse(ctx->a->eff, NULL, setParamToObj, ctx);
            }else if (atom->arg[1].param >= 0 && atom->arg[0].obj >= 0){
                ctx->eq_atom = atom;
                ctx->param = atom->arg[1].param;
                ctx->obj = atom->arg[0].obj;
                pddlFmTraverse(ctx->a->pre, NULL, setParamToObj, ctx);
                pddlFmTraverse(ctx->a->eff, NULL, setParamToObj, ctx);
            }
        }
    }
    return 0;
}

static void propagateEquality(pddl_action_t *a, const pddl_t *pddl)
{
    if (a->pre == NULL || pddl->pred.eq_pred < 0)
        return;

    struct propagate_eq ctx = { a, pddl->pred.eq_pred, NULL, -1, -1 };
    if (a->pre->type != PDDL_FM_AND
            && a->pre->type != PDDL_FM_ATOM)
        return;
    pddlFmTraverse(a->pre, _propagateEquality, NULL, &ctx);
}

void pddlActionNormalize(pddl_action_t *a, const pddl_t *pddl)
{
    a->pre = pddlFmNormalize(a->pre, pddl, &a->param);
    a->eff = pddlFmNormalize(a->eff, pddl, &a->param);

    if (a->pre->type == PDDL_FM_BOOL && PDDL_FM_CAST(a->pre, bool)->val){
        pddlFmDel(a->pre);
        a->pre = pddlFmNewEmptyAnd();
    }
    if (a->pre->type == PDDL_FM_ATOM)
        a->pre = pddlFmAtomToAnd(a->pre);
    if (a->eff->type == PDDL_FM_ATOM
            || a->eff->type == PDDL_FM_ASSIGN
            || a->eff->type == PDDL_FM_INCREASE
            || a->eff->type == PDDL_FM_WHEN){
        a->eff = pddlFmAtomToAnd(a->eff);
    }

    propagateEquality(a, pddl);
}

pddl_action_t *pddlActionsAddEmpty(pddl_actions_t *as)
{
    return pddlActionsAddCopy(as, -1);
}

pddl_action_t *pddlActionsAddCopy(pddl_actions_t *as, int copy_id)
{
    pddl_action_t *a;

    if (as->action_size == as->action_alloc){
        if (as->action_alloc == 0)
            as->action_alloc = PDDL_ACTIONS_ALLOC_INIT;
        as->action_alloc *= 2;
        as->action = REALLOC_ARR(as->action, pddl_action_t,
                                     as->action_alloc);
    }

    a = as->action + as->action_size;
    ++as->action_size;
    if (copy_id >= 0){
        pddlActionInitCopy(a, as->action + copy_id);
    }else{
        pddlActionInit(a);
    }
    a->id = as->action_size - 1;
    return a;
}

void pddlActionsFree(pddl_actions_t *actions)
{
    pddl_action_t *a;
    int i;

    for (i = 0; i < actions->action_size; ++i){
        a = actions->action + i;
        pddlActionFree(a);
    }
    if (actions->action != NULL)
        FREE(actions->action);
}

void pddlActionSplit(pddl_action_t *a, pddl_t *pddl)
{
    pddl_actions_t *as = &pddl->action;
    pddl_action_t *newa;
    pddl_fm_junc_t *pre;
    pddl_fm_t *first_cond, *cond;
    pddl_list_t *item;
    int aidx;

    if (a->pre->type != PDDL_FM_OR)
        return;

    pre = pddlFmToJunc(a->pre);
    if (pddlListEmpty(&pre->part))
        return;

    item = pddlListNext(&pre->part);
    pddlListDel(item);
    first_cond = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);
    a->pre = NULL;
    aidx = a - as->action;
    while (!pddlListEmpty(&pre->part)){
        item = pddlListNext(&pre->part);
        pddlListDel(item);
        cond = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);
        newa = pddlActionsAddCopy(as, aidx);
        newa->pre = cond;
        pddlActionNormalize(newa, pddl);
    }
    as->action[aidx].pre = first_cond;
    pddlActionNormalize(as->action + aidx, pddl);

    pddlFmDel(&pre->fm);
}

void pddlActionAssertPreConjuction(pddl_action_t *a)
{
    pddl_list_t *item;
    pddl_fm_junc_t *pre;
    pddl_fm_t *c;

    if (a->pre->type != PDDL_FM_AND){
        PANIC("Precondition of the action `%s' is" " not a conjuction.", a->name);
    }

    pre = pddlFmToJunc(a->pre);
    PDDL_LIST_FOR_EACH(&pre->part, item){
        c = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);
        if (c->type != PDDL_FM_ATOM){
            PANIC("Precondition of the action `%s' is"
                       " not a flatten conjuction (conjuction contains"
                       " something else besides atoms).", a->name);
        }
    }
}

void pddlActionRemapObjs(pddl_action_t *a, const pddl_obj_id_t *remap)
{
    pddlFmRemapObjs(a->pre, remap);
    pddlFmRemapObjs(a->eff, remap);
}

void pddlActionsRemapObjs(pddl_actions_t *as, const pddl_obj_id_t *remap)
{
    for (int i = 0; i < as->action_size; ++i)
        pddlActionRemapObjs(as->action + i, remap);
}

int pddlActionRemapTypesAndPreds(pddl_action_t *a,
                                 const int *type_remap,
                                 const int *pred_remap,
                                 const int *func_remap)
{
    for (int i = 0; i < a->param.param_size; ++i){
        if (type_remap[a->param.param[i].type] < 0)
            return -1;
        a->param.param[i].type = type_remap[a->param.param[i].type];
    }
    if (pddlFmRemapPreds(a->pre, pred_remap, func_remap) != 0)
        return -1;
    if (pddlFmRemapPreds(a->eff, pred_remap, func_remap) != 0)
        return -1;

    return 0;
}

void pddlActionsRemapTypesAndPreds(pddl_actions_t *as,
                                   const int *type_remap,
                                   const int *pred_remap,
                                   const int *func_remap)
{
    int ins = 0;
    for (int i = 0; i < as->action_size; ++i){
        if (pddlActionRemapTypesAndPreds(as->action + i, type_remap,
                    pred_remap, func_remap) == 0){
            as->action[ins] = as->action[i];
            as->action[ins].id = ins;
            ++ins;
        }else{
            pddlActionFree(as->action + i);
        }
    }
    as->action_size = ins;
}

void pddlActionsRemoveSet(pddl_actions_t *as, const pddl_iset_t *ids)
{
    int size = pddlISetSize(ids);
    if (size == 0)
        return;

    int cur = 0;
    int ins = 0;
    for (int ai = 0; ai < as->action_size; ++ai){
        if (cur < pddlISetSize(ids) && ai == pddlISetGet(ids, cur)){
            pddlActionFree(as->action + ai);
            ++cur;

        }else{
            if (ai != ins){
                as->action[ins] = as->action[ai];
                as->action[ins].id = ins;
            }
            ++ins;
        }
    }
    as->action_size = ins;
}

void pddlActionPrint(const pddl_t *pddl, const pddl_action_t *a, FILE *fout)
{
    fprintf(fout, "    %s: ", a->name);
    pddlParamsPrint(&a->param, fout);
    fprintf(fout, "\n");

    fprintf(fout, "        pre: ");
    pddlFmPrint(pddl, a->pre, &a->param, fout);
    fprintf(fout, "\n");

    fprintf(fout, "        eff: ");
    pddlFmPrint(pddl, a->eff, &a->param, fout);
    fprintf(fout, "\n");
}

void pddlActionsPrint(const pddl_t *pddl,
                      const pddl_actions_t *actions,
                      FILE *fout)
{
    int i;

    fprintf(fout, "Action[%d]:\n", actions->action_size);
    for (i = 0; i < actions->action_size; ++i){
        ASSERT(actions->action[i].id == i);
        pddlActionPrint(pddl, actions->action + i, fout);
    }
}

static void pddlActionPrintPDDL(const pddl_action_t *a,
                                const pddl_t *pddl,
                                FILE *fout)
{
    fprintf(fout, "(:action %s\n", a->name);
    if (a->param.param_size > 0){
        fprintf(fout, "    :parameters (");
        pddlParamsPrintPDDL(&a->param, &pddl->type, fout);
        fprintf(fout, ")\n");
    }

    if (a->pre != NULL){
        fprintf(fout, "    :precondition ");
        pddlFmPrintPDDL(a->pre, pddl, &a->param, fout);
        fprintf(fout, "\n");
    }

    if (a->eff != NULL){
        fprintf(fout, "    :effect ");
        pddlFmPrintPDDL(a->eff, pddl, &a->param, fout);
        fprintf(fout, "\n");
    }

    fprintf(fout, ")\n");

}

void pddlActionsPrintPDDL(const pddl_actions_t *actions,
                          const pddl_t *pddl,
                          FILE *fout)
{
    for (int i = 0; i < actions->action_size; ++i){
        pddlActionPrintPDDL(&actions->action[i], pddl, fout);
        fprintf(fout, "\n");
    }
}
