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

#include "pddl/param.h"
#include "lisp_err.h"
#include "internal.h"

void pddlParamInit(pddl_param_t *param)
{
    ZEROIZE(param);
    param->inherit = -1;
}

void pddlParamInitCopy(pddl_param_t *dst, const pddl_param_t *src)
{
    *dst = *src;
    if (dst->name != NULL)
        dst->name = STRDUP(src->name);
}

void pddlParamsInit(pddl_params_t *params)
{
    ZEROIZE(params);
}

void pddlParamsFree(pddl_params_t *params)
{
    for (int i = 0; i < params->param_size; ++i){
        if (params->param[i].name != NULL)
            FREE(params->param[i].name);
    }
    if (params->param != NULL)
        FREE(params->param);
}

pddl_param_t *pddlParamsAdd(pddl_params_t *params)
{
    pddl_param_t *param;

    if (params->param_size >= params->param_alloc){
        if (params->param_alloc == 0)
            params->param_alloc = 1;
        params->param_alloc *= 2;
        params->param = REALLOC_ARR(params->param, pddl_param_t,
                                    params->param_alloc);
    }

    param = params->param + params->param_size++;
    pddlParamInit(param);
    return param;
}

void pddlParamsInitCopy(pddl_params_t *dst, const pddl_params_t *src)
{
    dst->param_size = dst->param_alloc = src->param_size;
    dst->param = ALLOC_ARR(pddl_param_t, dst->param_alloc);
    for (int i = 0; i < dst->param_size; ++i)
        pddlParamInitCopy(dst->param + i, src->param + i);
}

int pddlParamsGetId(const pddl_params_t *param, const char *name)
{
    for (int i = 0; i < param->param_size; ++i){
        if (strcmp(name, param->param[i].name) == 0)
            return i;
    }

    return -1;
}

struct _set_param_t {
    pddl_params_t *param;
    pddl_types_t *types;
};
typedef struct _set_param_t set_param_t;

static int setParams(const pddl_lisp_node_t *root,
                     int child_from, int child_to, int child_type, void *ud,
                     pddl_err_t *err)
{
    pddl_params_t *params = ((set_param_t *)ud)->param;
    pddl_types_t *types = ((set_param_t *)ud)->types;
    pddl_param_t *param;
    int tid;

    tid = 0;
    if (child_type >= 0){
        const pddl_lisp_node_t *node = root->child + child_type;
        if ((tid = pddlTypeFromLispNode(types, node, err)) < 0)
            return -1;
    }

    for (int i = child_from; i < child_to; ++i){
        ASSERT(root->child[i].value != NULL);
        if (root->child[i].value == NULL)
            ERR_LISP_RET2(err, -1, root->child + i, "Unexpected expression");

        if (root->child[i].value[0] != '?'){
            ERR_LISP_RET(err, -1, root->child + i,
                         "Expected variable, got `%s'.", root->child[i].value);
        }

        param = pddlParamsAdd(params);
        param->name = STRDUP(root->child[i].value);
        param->type = tid;
        param->is_agent = 0;
    }

    return 0;
}

int pddlParamsParse(pddl_params_t *params,
                    const pddl_lisp_node_t *root,
                    pddl_types_t *types,
                    pddl_err_t *err)
{
    set_param_t set_param;
    set_param.param = params;
    set_param.types = types;
    if (pddlLispParseTypedList(root, 0, root->child_size,
                                setParams, &set_param, err) != 0)
        PDDL_TRACE_RET(err, -1);
    return 0;
}

int pddlParamsParseAgent(pddl_params_t *params,
                         const pddl_lisp_node_t *n,
                         int nid,
                         pddl_types_t *types,
                         pddl_err_t *err)
{
    set_param_t set_param;
    int to;

    if (nid + 2 < n->child_size
            && n->child[nid + 2].value != NULL
            && n->child[nid + 2].value[0] == '-'){
        to = nid + 4;
    }else{
        to = nid + 2;
    }

    set_param.param = params;
    set_param.types = types;
    if (pddlLispParseTypedList(n, nid + 1, to, setParams, &set_param, err) != 0)
        PDDL_TRACE_RET(err, -1);

    params->param[params->param_size - 1].is_agent = 1;
    return to;
}

void pddlParamsRemap(pddl_params_t *params, const int *remap)
{
    for (int i = 0; i < params->param_size; ++i){
        if (remap[i] == -1){
            if (params->param[i].name != NULL)
                FREE(params->param[i].name);
            params->param[i].name = NULL;
        }
    }

    int max_id = -1;
    for (int i = 0; i < params->param_size; ++i){
        if (remap[i] != -1){
            params->param[remap[i]] = params->param[i];
            max_id = PDDL_MAX(max_id, remap[i]);
        }
    }
    params->param_size = max_id + 1;
}

void pddlParamsPrint(const pddl_params_t *params, FILE *fout)
{
    int i;

    for (i = 0; i < params->param_size; ++i){
        if (i > 0)
            fprintf(fout, " ");
        if (params->param[i].is_agent)
            fprintf(fout, "A:");
        if (params->param[i].inherit >= 0)
            fprintf(fout, "I[%d]:", params->param[i].inherit);
        fprintf(fout, "%s:%d", params->param[i].name, params->param[i].type);
    }
}

void pddlParamsPrintPDDL(const pddl_params_t *params,
                         const pddl_types_t *ts,
                         FILE *fout)
{
    int printed = 0;
    for (int i = 0; i < params->param_size; ++i){
        const pddl_param_t *p = params->param + i;
        if (p->inherit == -1){
            if (printed)
                fprintf(fout, " ");
            fprintf(fout, "%s - %s", p->name, ts->type[p->type].name);
            printed = 1;
        }
    }
}
