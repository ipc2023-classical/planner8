/***
 * cpddl
 * -------
 * Copyright (c)2022 Daniel Fiser <danfis@danfis.cz>,
 * FAI Group, Saarland University
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

#include "pddl/unify.h"
#include "internal.h"

void pddlUnifyInit(pddl_unify_t *u,
                   const pddl_types_t *type,
                   const pddl_params_t *param1,
                   const pddl_params_t *param2)
{
    ASSERT_RUNTIME(param1 != param2);
    ASSERT_RUNTIME(param1 != NULL);
    ASSERT_RUNTIME(param2 != NULL);
    ZEROIZE(u);
    u->type = type;
    u->param[0] = param1;
    u->param[1] = param2;
    u->map[0] = ALLOC_ARR(pddl_unify_val_t, u->param[0]->param_size);
    u->map[1] = ALLOC_ARR(pddl_unify_val_t, u->param[1]->param_size);

    int var = 0;
    for (int i = 0; i < u->param[0]->param_size; ++i){
        u->map[0][i].obj = PDDL_OBJ_ID_UNDEF;
        u->map[0][i].var = var;
        u->map[0][i].var_type = u->param[0]->param[i].type;
        ++var;
    }
    for (int i = 0; i < u->param[1]->param_size; ++i){
        u->map[1][i].obj = PDDL_OBJ_ID_UNDEF;
        u->map[1][i].var = var;
        u->map[1][i].var_type = u->param[1]->param[i].type;
        ++var;
    }
}

void pddlUnifyInitCopy(pddl_unify_t *u, const pddl_unify_t *u2)
{
    *u = *u2;
    u->map[0] = ALLOC_ARR(pddl_unify_val_t, u->param[0]->param_size);
    u->map[1] = ALLOC_ARR(pddl_unify_val_t, u->param[1]->param_size);
    memcpy(u->map[0], u2->map[0],
           sizeof(pddl_unify_val_t) * u->param[0]->param_size);
    memcpy(u->map[1], u2->map[1],
           sizeof(pddl_unify_val_t) * u->param[1]->param_size);
}

void pddlUnifyFree(pddl_unify_t *u)
{
    if (u->map[0] != NULL)
        FREE(u->map[0]);
    if (u->map[1] != NULL)
        FREE(u->map[1]);
}

static void initVal(pddl_unify_val_t *v,
                    const pddl_unify_val_t *map,
                    const pddl_fm_atom_t *a,
                    int argi)
{
    v->obj = a->arg[argi].obj;
    v->var = a->arg[argi].param;
    v->var_type = -1;

    if (v->var >= 0)
        *v = map[v->var];
}

static void unifyVars(pddl_unify_t *u, int var1, int var2, int var_type)
{
    for (int i = 0; i < 2; ++i){
        for (int j = 0; j < u->param[i]->param_size; ++j){
            if (u->map[i][j].var == var1 || u->map[i][j].var == var2){
                u->map[i][j].var = var1;
                u->map[i][j].var_type = var_type;
            }
        }
    }
}

static void unifyVarObj(pddl_unify_t *u, int var, pddl_obj_id_t obj)
{
    for (int i = 0; i < 2; ++i){
        for (int j = 0; j < u->param[i]->param_size; ++j){
            if (u->map[i][j].var == var){
                u->map[i][j].obj = obj;
                u->map[i][j].var = -1;
                u->map[i][j].var_type = -1;
            }
        }
    }
}

static int unifyVals(pddl_unify_t *u,
                     const pddl_unify_val_t *v0,
                     const pddl_unify_val_t *v1)
{
    if (v0->var >= 0 && v1->var >= 0){
        int to_type = -1;
        if (pddlTypesIsSubset(u->type, v1->var_type, v0->var_type)){
            to_type = v1->var_type;
        }else if (pddlTypesIsSubset(u->type, v0->var_type, v1->var_type)){
            to_type = v0->var_type;
        }else{
            return -1;
        }
        // Variables with empty types are not unifiable
        if (pddlTypeNumObjs(u->type, to_type) == 0)
            return -1;
        unifyVars(u, v0->var, v1->var, to_type);

    }else if (v0->var >= 0){
        if (!pddlTypesObjHasType(u->type, v0->var_type, v1->obj))
            return -1;
        unifyVarObj(u, v0->var, v1->obj);

    }else if (v1->var >= 0){
        if (!pddlTypesObjHasType(u->type, v1->var_type, v0->obj))
            return -1;
        unifyVarObj(u, v1->var, v0->obj);

    }else{
        if (v0->obj != v1->obj)
            return -1;
    }
    return 0;
}

int pddlUnify(pddl_unify_t *u,
              const pddl_fm_atom_t *a1,
              const pddl_fm_atom_t *a2)
{
    if (a1->pred != a2->pred)
        return -1;

    for (int argi = 0; argi < a1->arg_size; ++argi){
        pddl_unify_val_t v0, v1;
        initVal(&v0, u->map[0], a1, argi);
        initVal(&v1, u->map[1], a2, argi);
        if (unifyVals(u, &v0, &v1) != 0)
            return -1;
    }

    return 0;
}

static int applyEquality(pddl_unify_t *u,
                         int idx,
                         int eq_pred,
                         const pddl_fm_t *cond)
{
    if (cond == NULL)
        return 0;

    const pddl_unify_val_t *map = u->map[idx];
    pddl_fm_const_it_atom_t it;
    const pddl_fm_atom_t *eq;
    PDDL_FM_FOR_EACH_ATOM(cond, &it, eq){
        if (!eq->neg && eq->pred == eq_pred){
            pddl_unify_val_t v0,v1;
            initVal(&v0, map, eq, 0);
            initVal(&v1, map, eq, 1);
            if (unifyVals(u, &v0, &v1) != 0)
                return -1;
        }
    }
    return 0;
}

int pddlUnifyApplyEquality(pddl_unify_t *u,
                           const pddl_params_t *param,
                           int eq_pred,
                           const pddl_fm_t *cond)
{
    if (param == u->param[0]){
        return applyEquality(u, 0, eq_pred, cond);
    }else if (param == u->param[1]){
        return applyEquality(u, 1, eq_pred, cond);
    }
    ASSERT_RUNTIME_M(0, "Invalid set of parameters");
    return -1;
}

static int checkInequality(const pddl_unify_val_t *map,
                           int eq_pred,
                           const pddl_fm_t *cond)
{
    if (cond == NULL)
        return 1;

    pddl_fm_const_it_atom_t it;
    const pddl_fm_atom_t *ineq;
    PDDL_FM_FOR_EACH_ATOM(cond, &it, ineq){
        if (ineq->neg && ineq->pred == eq_pred){
            pddl_unify_val_t v0,v1;
            initVal(&v0, map, ineq, 0);
            initVal(&v1, map, ineq, 1);
            if (memcmp(&v0, &v1, sizeof(v0)) == 0)
                return 0;
        }
    }
    return 1;
}

int pddlUnifyCheckInequality(const pddl_unify_t *u,
                             const pddl_params_t *param,
                             int eq_pred,
                             const pddl_fm_t *cond)
{
    if (param == u->param[0]){
        return checkInequality(u->map[0], eq_pred, cond);
    }else if (param == u->param[1]){
        return checkInequality(u->map[1], eq_pred, cond);
    }
    ASSERT_RUNTIME_M(0, "Invalid set of parameters");
    return -1;
}

int pddlUnifyAtomsDiffer(const pddl_unify_t *u,
                         const pddl_params_t *param1,
                         const pddl_fm_atom_t *a1,
                         const pddl_params_t *param2,
                         const pddl_fm_atom_t *a2)
{
    ASSERT_RUNTIME(param1 == u->param[0] || param1 == u->param[1]);
    ASSERT_RUNTIME(param2 == u->param[0] || param2 == u->param[1]);
    if (a1->pred != a2->pred)
        return 1;

    int idx1 = 0;
    if (param1 == u->param[1])
        idx1 = 1;
    int idx2 = 0;
    if (param2 == u->param[1])
        idx2 = 1;

    for (int i = 0; i < a1->arg_size; ++i){
        pddl_unify_val_t v1,v2;
        initVal(&v1, u->map[idx1], a1, i);
        initVal(&v2, u->map[idx2], a2, i);
        if (v1.obj != v2.obj || v1.var != v2.var || v1.var_type != v2.var_type)
            return 1;
    }
    return 0;
}

int pddlUnifyEq(const pddl_unify_t *u, const pddl_unify_t *u2)
{
    return u->param[0] == u2->param[0]
            && u->param[1] == u2->param[1]
            && memcmp(u->map[0], u2->map[0],
                      sizeof(pddl_unify_val_t) * u->param[0]->param_size) == 0
            && memcmp(u->map[1], u2->map[1],
                      sizeof(pddl_unify_val_t) * u->param[1]->param_size) == 0;
}

static pddl_fm_t *_pddlUnifyToCond(const pddl_unify_t *u, int eq_pred, int idx)
{
    pddl_fm_t *and = pddlFmNewEmptyAnd();
    pddl_fm_atom_t *eq;

    for (int v1 = 0; v1 < u->param[idx]->param_size; ++v1){
        for (int v2 = v1 + 1; v2 < u->param[idx]->param_size; ++v2){
            if (u->map[idx][v1].var == u->map[idx][v2].var
                    && u->map[idx][v1].var >= 0){
                eq = pddlFmNewEmptyAtom(2);
                eq->pred = eq_pred;
                eq->arg[0].param = v1;
                eq->arg[1].param = v2;
                pddlFmJuncAdd(pddlFmToJunc(and), &eq->fm);
            }
        }
    }

    for (int v = 0; v < u->param[idx]->param_size; ++v){
        if (u->map[idx][v].obj >= 0){
            eq = pddlFmNewEmptyAtom(2);
            eq->pred = eq_pred;
            eq->arg[0].param = v;
            eq->arg[1].obj = u->map[idx][v].obj;
            pddlFmJuncAdd(pddlFmToJunc(and), &eq->fm);
        }
    }

    for (int v = 0; v < u->param[idx]->param_size; ++v){
        if (u->map[idx][v].var >= 0
                && u->map[idx][v].var_type != u->param[idx]->param[v].type){
            pddl_fm_t *or = pddlFmNewEmptyOr();
            int type = u->map[idx][v].var_type;
            const pddl_obj_id_t *objs;
            int obj_size;
            objs = pddlTypesObjsByType(u->type, type, &obj_size);
            for (int i = 0; i < obj_size; ++i){
                eq = pddlFmNewEmptyAtom(2);
                eq->pred = eq_pred;
                eq->arg[0].param = v;
                eq->arg[1].obj = objs[i];
                pddlFmJuncAdd(pddlFmToJunc(or), &eq->fm);
            }
            pddlFmJuncAdd(pddlFmToJunc(and), or);
        }
    }

    if (pddlFmJuncIsEmpty(pddlFmToJunc(and))){
        pddlFmDel(and);
        return &pddlFmNewBool(1)->fm;
    }
    return and;
}

pddl_fm_t *pddlUnifyToCond(const pddl_unify_t *u,
                           int eq_pred,
                           const pddl_params_t *param)
{
    if (param == u->param[0])
        return _pddlUnifyToCond(u, eq_pred, 0);
    if (param == u->param[1])
        return _pddlUnifyToCond(u, eq_pred, 1);
    ASSERT_RUNTIME_M(0, "Invalid argument param");
    return NULL;
}

void pddlUnifyResetCountedVars(const pddl_unify_t *u)
{
    int var = 0;
    for (int v = 0; v < 2; ++v){
        for (int i = 0; i < u->param[v]->param_size; ++i){
            if (u->param[v]->param[i].is_counted_var){
                u->map[v][i].obj = PDDL_OBJ_ID_UNDEF;
                u->map[v][i].var = var;
                u->map[v][i].var_type = u->param[v]->param[i].type;
            }
            ++var;
        }
    }
}
