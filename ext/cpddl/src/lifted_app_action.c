/***
 * Copyright (c)2022 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

#include "internal.h"
#include "_lifted_app_action.h"
#include "pddl/sort.h"


void pddlLiftedAppActionAdd(pddl_lifted_app_action_t *a,
                            int action_id,
                            const pddl_obj_id_t *args,
                            int args_size)
{
    if (a->bytesize + a->struct_size > a->bytealloc){
        if (a->bytealloc == 0)
            a->bytealloc = a->struct_size * 4;
        a->bytealloc *= 2;
        a->data = REALLOC_ARR(a->data, char, a->bytealloc);
    }

    _pddl_lifted_app_action_t *d;
    d = (_pddl_lifted_app_action_t *)(a->data + a->bytesize);
    d->action_id = action_id;
    memcpy((pddl_obj_id_t *)d->args, args, sizeof(pddl_obj_id_t) * args_size);

    ++a->size;
    a->bytesize += a->struct_size;
}


void _pddlLiftedAppActionInit(pddl_lifted_app_action_t *aa,
                              const pddl_t *pddl,
                              void (*fn_del)(pddl_lifted_app_action_t *),
                              void (*fn_clear_state)(pddl_lifted_app_action_t *),
                              int (*fn_set_state_atom)(pddl_lifted_app_action_t *,
                                                       const pddl_ground_atom_t *),
                              int (*fn_find_app_actions)(pddl_lifted_app_action_t *),
                              pddl_err_t *err)
{
    ZEROIZE(aa);
    aa->fn_del = fn_del;
    aa->fn_clear_state = fn_clear_state;
    aa->fn_set_state_atom = fn_set_state_atom;
    aa->fn_find_app_actions = fn_find_app_actions;

    int max_arity = 0;
    for (int ai = 0; ai < pddl->action.action_size; ++ai)
        max_arity = PDDL_MAX(max_arity, pddl->action.action[ai].param.param_size);

    aa->data = NULL;
    aa->struct_size = sizeof(_pddl_lifted_app_action_t);
    aa->struct_size += sizeof(pddl_obj_id_t) * max_arity;
    aa->size = 0;
    aa->bytesize = 0;
    aa->bytealloc = 0;
}

pddl_lifted_app_action_t *pddlLiftedAppActionNew(const pddl_t *pddl,
                                             pddl_lifted_app_action_backend_t backend,
                                             pddl_err_t *err)
{
    ASSERT_RUNTIME(pddl->normalized);

    CTX(err, "lifted_app_action", "Lifted-App-Action");
    pddl_lifted_app_action_t *aa = NULL;
    switch (backend){
        case PDDL_LIFTED_APP_ACTION_SQL:
            LOG(err, "backend: %{backend}s", "sql");
            aa = pddlLiftedAppActionNewSql(pddl, err);
            break;

        case PDDL_LIFTED_APP_ACTION_DL:
            LOG(err, "backend: %{backend}s", "datalog");
            aa = pddlLiftedAppActionNewDatalog(pddl, err);
            break;
    }
    CTXEND(err);
    if (aa == NULL)
        ERR_RET(err, NULL, "Unkown backend '%d'", (int)backend);
    return aa;
}

void pddlLiftedAppActionDel(pddl_lifted_app_action_t *aa)
{
    if (aa->data != NULL)
        FREE(aa->data);
    aa->fn_del(aa);
}

void pddlLiftedAppActionClearState(pddl_lifted_app_action_t *aa)
{
    aa->size = 0;
    aa->bytesize = 0;
    aa->fn_clear_state(aa);
}

int pddlLiftedAppActionSetStateAtom(pddl_lifted_app_action_t *aa,
                                  const pddl_ground_atom_t *atom)
{
    return aa->fn_set_state_atom(aa, atom);
}

int pddlLiftedAppActionSetStripsState(pddl_lifted_app_action_t *aa,
                                    const pddl_strips_maker_t *maker,
                                    const pddl_iset_t *state)
{
    pddlLiftedAppActionClearState(aa);
    int fact;
    PDDL_ISET_FOR_EACH(state, fact){
        const pddl_ground_atom_t *ga;
        ga = pddlStripsMakerGroundAtomConst(maker, fact);
        pddlLiftedAppActionSetStateAtom(aa, ga);
    }
    return 0;
}

int pddlLiftedAppActionFindAppActions(pddl_lifted_app_action_t *aa)
{
    return aa->fn_find_app_actions(aa);
}

int pddlLiftedAppActionSize(const pddl_lifted_app_action_t *a)
{
    return a->size;
}

const _pddl_lifted_app_action_t *get(const pddl_lifted_app_action_t *a, int idx)
{
    return (const _pddl_lifted_app_action_t *)(a->data + (a->struct_size * idx));
}

int pddlLiftedAppActionId(const pddl_lifted_app_action_t *a, int idx)
{
    return get(a, idx)->action_id;
}

const pddl_obj_id_t *pddlLiftedAppActionArgs(const pddl_lifted_app_action_t *a, int idx)
{
    return get(a, idx)->args;
}

static int cmp(const void *a, const void *b, void *ud)
{
    const pddl_t *pddl = ud;
    const _pddl_lifted_app_action_t *a1 = (const _pddl_lifted_app_action_t *)a;
    const _pddl_lifted_app_action_t *a2 = (const _pddl_lifted_app_action_t *)b;
    int cmp = a1->action_id - a2->action_id;
    if (cmp == 0){
        int arg_size = pddl->action.action[a1->action_id].param.param_size;
        return memcmp(a1->args, a2->args, sizeof(pddl_obj_id_t) * arg_size);
    }
    return cmp;
}

void pddlLiftedAppActionSort(pddl_lifted_app_action_t *a, const pddl_t *pddl)
{
    pddlSort(a->data, a->size, a->struct_size, cmp, (void *)pddl);
}
