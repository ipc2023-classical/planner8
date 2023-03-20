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

#include "internal.h"
#include "pddl/endomorphism.h"
#include "pddl/sort.h"
#include "pddl/cp.h"

struct lifted_endomorphism {
    int obj_size;
    int *obj_is_fixed;
};
typedef struct lifted_endomorphism lifted_endomorphism_t;

static void setAtomTypeFixed(lifted_endomorphism_t *end,
                             const pddl_t *pddl,
                             const pddl_params_t *params,
                             const pddl_fm_atom_t *atom,
                             int parami)
{
    if (atom->arg[parami].param >= 0){
        int param = atom->arg[parami].param;
        int type_id = params->param[param].type;
        int objs_size;
        const pddl_obj_id_t *objs;
        objs = pddlTypesObjsByType(&pddl->type, type_id, &objs_size);
        for (int i = 0; i < objs_size; ++i)
            end->obj_is_fixed[objs[i]] = 1;

    }else{
        end->obj_is_fixed[atom->arg[parami].obj] = 1;
    }
}

static void setAtomTypesFixed(lifted_endomorphism_t *end,
                              const pddl_t *pddl,
                              const pddl_params_t *params,
                              const pddl_fm_atom_t *atom)
{
    for (int i = 0; i < atom->arg_size; ++i)
        setAtomTypeFixed(end, pddl, params, atom, i);
}

static int hasAtom(const pddl_fm_t *cond,
                   const pddl_fm_atom_t *atom)
{
    if (cond->type == PDDL_FM_ATOM){
        return pddlFmEq(cond, &atom->fm);
    }else{
        ASSERT_RUNTIME(cond->type == PDDL_FM_AND);
        const pddl_fm_junc_t *cand = pddlFmToJuncConst(cond);
        pddl_list_t *item;
        PDDL_LIST_FOR_EACH(&cand->part, item){
            const pddl_fm_t *c = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);
            ASSERT_RUNTIME(c->type == PDDL_FM_ATOM);
            if (pddlFmEq(c, &atom->fm))
                return 1;
        }
    }
    return 0;
}

static int isCovered(const pddl_t *pddl,
                     const pddl_params_t *atom_param,
                     const pddl_fm_atom_t *atom,
                     const pddl_params_t *ma_param,
                     const pddl_fm_atom_t *ma)
{
    for (int argi = 0; argi < atom->arg_size; ++argi){
        if (ma->arg[argi].param >= 0 && atom->arg[argi].param >= 0){
            int a_parami = atom->arg[argi].param;
            int a_type = atom_param->param[a_parami].type;
            int m_parami = ma->arg[argi].param;
            int m_type = ma_param->param[m_parami].type;
            //if (pddlTypesAreDisjunct(&pddl->type, m_type, a_type))
            if (!pddlTypesIsParent(&pddl->type, a_type, m_type))
                return 0;

        }else if (ma->arg[argi].param >= 0){
            int a_obj = atom->arg[argi].obj;
            int m_parami = ma->arg[argi].param;
            int m_type = ma_param->param[m_parami].type;
            if (!pddlTypesObjHasType(&pddl->type, m_type, a_obj))
                return 0;

        }else if (atom->arg[argi].param >= 0){
            int m_obj = ma->arg[argi].obj;
            int a_parami = atom->arg[argi].param;
            int a_type = atom_param->param[a_parami].type;
            if (!pddlTypesObjHasType(&pddl->type, a_type, m_obj)
                    || pddlTypeNumObjs(&pddl->type, a_type) > 1){
                return 0;
            }

        }else{
            if (atom->arg[argi].obj != ma->arg[argi].obj)
                return 0;
        }
    }

    return 1;
}

static int coverAtomWithMGroup(int *counted,
                               const pddl_t *pddl,
                               const pddl_params_t *act_param,
                               const pddl_fm_atom_t *atom,
                               const pddl_lifted_mgroup_t *mgroup)
{
    int covered = 0;

    for (int ci = 0; ci < mgroup->cond.size; ++ci){
        const pddl_fm_t *mc = mgroup->cond.fm[ci];
        const pddl_fm_atom_t *ma = PDDL_FM_CAST(mc, atom);
        if (ma->pred != atom->pred)
            continue;

        if (!isCovered(pddl, act_param, atom, &mgroup->param, ma))
            continue;

        covered = 1;
        for (int argi = 0; argi < atom->arg_size; ++argi){
            if (ma->arg[argi].param >= 0 && atom->arg[argi].param >= 0){
                int a_parami = atom->arg[argi].param;
                int a_type = act_param->param[a_parami].type;
                const pddl_obj_id_t *a_obj;
                int a_obj_size;
                a_obj = pddlTypesObjsByType(&pddl->type, a_type, &a_obj_size);

                int m_parami = ma->arg[argi].param;
#ifdef PDDL_DEBUG
                int m_type = mgroup->param.param[m_parami].type;
#endif /* PDDL_DEBUG */
                int is_counted = mgroup->param.param[m_parami].is_counted_var;

                for (int i = 0; i < a_obj_size; ++i){
                    ASSERT(pddlTypesObjHasType(&pddl->type, m_type, a_obj[i]));
                    if (!is_counted){
                        counted[a_obj[i]] = -1;
                    }else if (counted[a_obj[i]] == 0){
                        counted[a_obj[i]] = 1;
                    }
                }

            }else if (ma->arg[argi].param >= 0){
                counted[atom->arg[argi].obj] = -1;

            }else if (atom->arg[argi].param >= 0){
                counted[ma->arg[argi].obj] = -1;

            }else{
                counted[atom->arg[argi].obj] = -1;
            }
        }
    }

    return covered;
}

static void coverAtomWithMGroups(lifted_endomorphism_t *end,
                                 const pddl_t *pddl,
                                 const pddl_params_t *act_param,
                                 const pddl_fm_atom_t *atom,
                                 const pddl_lifted_mgroups_t *mgroups)
{
    int *counted = CALLOC_ARR(int, pddl->obj.obj_size);
    int covered = 0;
    for (int mgi = 0; mgi < mgroups->mgroup_size; ++mgi){
        covered |= coverAtomWithMGroup(counted, pddl, act_param, atom,
                                       mgroups->mgroup + mgi);
    }

    if (!covered){
        setAtomTypesFixed(end, pddl, act_param, atom);
        FREE(counted);
        return;
    }

    for (int argi = 0; argi < atom->arg_size; ++argi){
        if (atom->arg[argi].param >= 0){
            int a_parami = atom->arg[argi].param;
            int a_type = act_param->param[a_parami].type;
            const pddl_obj_id_t *a_obj;
            int a_obj_size;
            a_obj = pddlTypesObjsByType(&pddl->type, a_type, &a_obj_size);
            for (int i = 0; i < a_obj_size; ++i){
                if (counted[a_obj[i]] < 0)
                    end->obj_is_fixed[a_obj[i]] = 1;
            }

        }else{
            if (counted[atom->arg[argi].obj] < 0)
                end->obj_is_fixed[atom->arg[argi].obj] = 1;
        }
    }

    FREE(counted);
}

static void analyzeActionAtom(
                lifted_endomorphism_t *end,
                const pddl_t *pddl,
                const pddl_params_t *act_param,
                const pddl_fm_t *act_pre,
                const pddl_fm_atom_t *eff_atom,
                const pddl_lifted_mgroups_t *lifted_mgroups,
                const pddl_endomorphism_config_t *cfg,
                pddl_err_t *err)
{
    if (!eff_atom->neg)
        return;

    pddl_fm_t *pos_c = pddlFmClone(&eff_atom->fm);
    pddl_fm_atom_t *pos_a = PDDL_FM_CAST(pos_c, atom);
    pos_a->neg = 0;
    if (hasAtom(act_pre, pos_a)){
        coverAtomWithMGroups(end, pddl, act_param, eff_atom, lifted_mgroups);

    }else{
        setAtomTypesFixed(end, pddl, act_param, eff_atom);
    }
    pddlFmDel(pos_c);
}

static void fixAtomConstants(pddl_fm_atom_t *a, lifted_endomorphism_t *end)
{
    for (int i = 0; i < a->arg_size; ++i){
        if (a->arg[i].obj >= 0)
            end->obj_is_fixed[a->arg[i].obj] = 1;
    }
}

static int fixConstants(pddl_fm_t *c, void *_end)
{
    lifted_endomorphism_t *end = (lifted_endomorphism_t *)_end;
    if (c->type == PDDL_FM_ATOM){
        pddl_fm_atom_t *a = PDDL_FM_CAST(c, atom);
        fixAtomConstants(a, end);

    }else if (c->type == PDDL_FM_ASSIGN){
        pddl_fm_func_op_t *a = PDDL_FM_CAST(c, func_op);
        if (a->lvalue != NULL)
            fixAtomConstants(a->lvalue, end);
        if (a->fvalue != NULL)
            fixAtomConstants(a->fvalue, end);
    }

    return 0;
}

static void liftedEndomorphismAnalyzeAction(
                lifted_endomorphism_t *end,
                const pddl_t *pddl,
                const pddl_params_t *act_param,
                const pddl_fm_t *act_pre,
                const pddl_fm_t *act_eff,
                const pddl_lifted_mgroups_t *lifted_mgroups,
                const pddl_endomorphism_config_t *cfg,
                pddl_err_t *err)
{
    pddlFmTraverse((pddl_fm_t *)act_pre, NULL, fixConstants, end);
    pddlFmTraverse((pddl_fm_t *)act_eff, NULL, fixConstants, end);
    if (act_eff->type == PDDL_FM_ATOM){
        const pddl_fm_atom_t *a = PDDL_FM_CAST(act_eff, atom);
        analyzeActionAtom(end, pddl, act_param, act_pre, a,
                          lifted_mgroups, cfg, err);
        return;
    }

    ASSERT_RUNTIME(act_eff->type == PDDL_FM_AND);
    const pddl_fm_junc_t *cand = pddlFmToJuncConst(act_eff);
    pddl_list_t *item;
    PDDL_LIST_FOR_EACH(&cand->part, item){
        const pddl_fm_t *c = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);
        if (c->type == PDDL_FM_ATOM){
            const pddl_fm_atom_t *a = PDDL_FM_CAST(c, atom);
            analyzeActionAtom(end, pddl, act_param, act_pre, a,
                              lifted_mgroups, cfg, err);

        }else if (c->type == PDDL_FM_INCREASE){
            // We can ignore this, because it is already handled in
            // the initial state

        }else if (c->type == PDDL_FM_WHEN){
            const pddl_fm_when_t *w = PDDL_FM_CAST(c, when);
            liftedEndomorphismAnalyzeAction(end, pddl, act_param,
                                            w->pre, w->eff, lifted_mgroups,
                                            cfg, err);

        }else{
            PANIC("Unexpected atom of type %d:%s\n",
                       c->type, pddlFmTypeName(c->type));
        }
    }
}

static int _liftedEndomorphismFixGoal(pddl_fm_t *c, void *u)
{
    lifted_endomorphism_t *end = (lifted_endomorphism_t *)u;
    if (c->type == PDDL_FM_ATOM){
        const pddl_fm_atom_t *atom = PDDL_FM_CAST(c, atom);
        for (int i = 0; i < atom->arg_size; ++i){
            ASSERT(atom->arg[i].obj >= 0);
            end->obj_is_fixed[atom->arg[i].obj] = 1;
        }
    }
    return 0;
}

static void liftedEndomorphismInit(lifted_endomorphism_t *end,
                                   const pddl_t *pddl,
                                   const pddl_lifted_mgroups_t *lifted_mgroups,
                                   const pddl_endomorphism_config_t *cfg,
                                   pddl_err_t *err)
{
    ZEROIZE(end);
    end->obj_size = pddl->obj.obj_size;
    end->obj_is_fixed = CALLOC_ARR(int, end->obj_size);

    // Set objects in the goal as fixed
    pddlFmTraverse((pddl_fm_t *)pddl->goal, NULL,
                     _liftedEndomorphismFixGoal, end);

    if (lifted_mgroups != NULL){
        for (int ai = 0; ai < pddl->action.action_size; ++ai){
            const pddl_action_t *action = pddl->action.action + ai;
            LOG(err, "Analyzing action (%s) ...", action->name);
            liftedEndomorphismAnalyzeAction(end, pddl, &action->param,
                                            action->pre, action->eff,
                                            lifted_mgroups, cfg, err);
        }
    }

#ifdef PDDL_DEBUG
    for (int i = 0; i < end->obj_size; ++i){
        LOG(err, "Obj-fixed %d:(%s): %d",
            i, pddl->obj.obj[i].name, end->obj_is_fixed[i]);
    }
#endif /* PDDL_DEBUG */
}

static void liftedEndomorphismFree(lifted_endomorphism_t *end)
{
    if (end->obj_is_fixed != NULL)
        FREE(end->obj_is_fixed);
}

static int liftedEndomorphismNumUnfixed(const lifted_endomorphism_t *end)
{
    int num = 0;
    for (int i = 0; i < end->obj_size; ++i){
        if (!end->obj_is_fixed[i])
            ++num;
    }
    return num;
}

struct obj_tuple {
    int *tuple;
    int value;
};
typedef struct obj_tuple obj_tuple_t;

struct pred_obj_tuple {
    int pred;
    int size;
    int tuple_size;
    int tuple_alloc;
    obj_tuple_t *tuple;
};
typedef struct pred_obj_tuple pred_obj_tuple_t;

struct pred_obj_tuples {
    int pred_size;
    int func_offset;
    pred_obj_tuple_t *tuple;
};
typedef struct pred_obj_tuples pred_obj_tuples_t;

static void predObjTupleFree(pred_obj_tuple_t *tup)
{
    for (int i = 0; i < tup->tuple_size; ++i)
        FREE(tup->tuple[i].tuple);
    if (tup->tuple != NULL)
        FREE(tup->tuple);
}

static obj_tuple_t *predObjTupleAdd(pred_obj_tuple_t *tup)
{
    if (tup->tuple_size == tup->tuple_alloc){
        if (tup->tuple_alloc == 0)
            tup->tuple_alloc = 1;
        tup->tuple_alloc *= 2;
        tup->tuple = REALLOC_ARR(tup->tuple, obj_tuple_t,
                                 tup->tuple_alloc);
    }
    tup->tuple[tup->tuple_size].tuple = ALLOC_ARR(int, tup->size);
    tup->tuple[tup->tuple_size].value = 0;
    return tup->tuple + tup->tuple_size++;
}

static int cmpTuple(const void *a, const void *b, void *u)
{
    const obj_tuple_t *o1 = (const obj_tuple_t *)a;
    const obj_tuple_t *o2 = (const obj_tuple_t *)b;
    return o2->value - o1->value;
}

static void predObjTupleSort(pred_obj_tuple_t *tup)
{
    pddlSort(tup->tuple, tup->tuple_size, sizeof(obj_tuple_t),
             cmpTuple, NULL);
}

static void predObjTuplesInit(pred_obj_tuples_t *tup, const pddl_t *pddl)
{
    tup->pred_size = pddl->pred.pred_size + pddl->func.pred_size;
    tup->func_offset = pddl->pred.pred_size;
    tup->tuple = CALLOC_ARR(pred_obj_tuple_t, tup->pred_size);
    int i;
    for (i = 0; i < pddl->pred.pred_size; ++i){
        tup->tuple[i].pred = i;
        tup->tuple[i].size = pddl->pred.pred[i].param_size;
    }
    for (; i < tup->pred_size; ++i){
        int fi = i - pddl->pred.pred_size;
        tup->tuple[i].pred = i;
        tup->tuple[i].size = pddl->func.pred[fi].param_size;
    }
}

static void predObjTuplesFree(pred_obj_tuples_t *tup)
{
    for (int i = 0; i < tup->pred_size; ++i)
        predObjTupleFree(tup->tuple + i);
    if (tup->tuple != NULL)
        FREE(tup->tuple);
}

static int _predObjTuplesInitFromCond(pddl_fm_t *c, void *u)
{
    pred_obj_tuples_t *tup = (pred_obj_tuples_t *)u;
    if (c->type == PDDL_FM_ATOM){
        const pddl_fm_atom_t *atom = PDDL_FM_CAST(c, atom);
        int pred_id = atom->pred;
        pred_obj_tuple_t *tuple = tup->tuple + pred_id;
        ASSERT(atom->arg_size == tuple->size);
        obj_tuple_t *t = predObjTupleAdd(tuple);
        for (int i = 0; i < atom->arg_size; ++i){
            ASSERT(atom->arg[i].obj >= 0);
            t->tuple[i] = atom->arg[i].obj;
        }

    }else if (c->type != PDDL_FM_AND){
        const pddl_fm_func_op_t *ass = PDDL_FM_CAST(c, func_op);
        ASSERT(ass->fvalue == NULL);
        ASSERT(ass->lvalue != NULL);
        ASSERT(pddlFmAtomIsGrounded(ass->lvalue));
        int pred_id = ass->lvalue->pred + tup->func_offset;
        pred_obj_tuple_t *tuple = tup->tuple + pred_id;
        ASSERT(ass->lvalue->arg_size == tuple->size);
        obj_tuple_t *t = predObjTupleAdd(tuple);
        t->value = ass->value;
        for (int i = 0; i < ass->lvalue->arg_size; ++i){
            ASSERT(ass->lvalue->arg[i].obj >= 0);
            t->tuple[i] = ass->lvalue->arg[i].obj;
        }

    }else if (c->type != PDDL_FM_AND){
        PANIC("Unexpected atom of type %d:%s\n",
                   c->type, pddlFmTypeName(c->type));
    }
    return 0;
}

static void predObjTuplesInitFromCond(pred_obj_tuples_t *tup,
                                      const pddl_t *pddl,
                                      const pddl_fm_t *cond)
{
    predObjTuplesInit(tup, pddl);
    pddlFmTraverse((pddl_fm_t *)cond, NULL,
                     _predObjTuplesInitFromCond, tup);
}

static void liftedAddDomains(const pddl_t *pddl,
                             const lifted_endomorphism_t *end,
                             pddl_cp_t *cp)
{
    ASSERT(pddl->type.type[0].parent < 0);

    // First deal with fixed objects
    for (int obj = 0; obj < pddl->obj.obj_size; ++obj){
        if (!end->obj_is_fixed[obj]){
            pddlCPAddIVar(cp, 0, pddl->obj.obj_size - 1, pddl->obj.obj[obj].name);
            continue;
        }else{
            pddlCPAddIVar(cp, obj, obj, pddl->obj.obj[obj].name);
        }
    }

    // And then with unfixed ones
    for (int type = 0; type < pddl->type.type_size; ++type){
        int num_objs = pddlTypeNumObjs(&pddl->type, type);
        int obj_values[num_objs];
        for (int i = 0; i < num_objs; ++i)
            obj_values[i] = pddlTypeGetObj(&pddl->type, type, i);

        for (int i = 0; i < num_objs; ++i){
            int obj = pddlTypeGetObj(&pddl->type, type, i);
            if (!end->obj_is_fixed[obj])
                pddlCPAddConstrIVarDomainArr(cp, obj, num_objs, obj_values);
        }
    }
}

static void liftedAddTupleConstr(const pred_obj_tuple_t *tup,
                                 int from,
                                 int to,
                                 pddl_cp_t *cp)
{
    int *vals = ALLOC_ARR(int, tup->size * tup->tuple_size);
    for (int ti = from, ins = 0; ti < tup->tuple_size; ++ti){
        for(int i = 0; i < tup->size; ++i)
            vals[ins++] = tup->tuple[ti].tuple[i];
    }

    for (int ti = from; ti < to; ++ti){
        pddlCPAddConstrIVarAllowed(cp, tup->size, tup->tuple[ti].tuple,
                                   tup->tuple_size, vals);
    }
    FREE(vals);
}

static void liftedAddInitConstr(const pddl_t *pddl,
                                const lifted_endomorphism_t *end,
                                const pddl_endomorphism_config_t *cfg,
                                pddl_cp_t *cp)
{
    pred_obj_tuples_t tuples;
    predObjTuplesInitFromCond(&tuples, pddl, &pddl->init->fm);

    if (cfg->ignore_costs){
        for (int pred = 0; pred < tuples.pred_size; ++pred){
            pred_obj_tuple_t *tup = tuples.tuple + pred;
            for (int i = 0; i < tup->tuple_size; ++i)
                tup->tuple[i].value = 0;
        }
    }

    for (int pred = 0; pred < tuples.pred_size; ++pred){
        pred_obj_tuple_t *tup = tuples.tuple + pred;
        if (tup->tuple_size == 0 || tup->size == 0)
            continue;
        predObjTupleSort(tup);

        int from = 0;
        int to = 1;
        for (; to < tup->tuple_size; ++to){
            if (tup->tuple[from].value != tup->tuple[to].value){
                liftedAddTupleConstr(tup, from, to, cp);
                from = to;
            }
        }
        if (from != to)
            liftedAddTupleConstr(tup, from, to, cp);
    }
    predObjTuplesFree(&tuples);
}

/*
static void liftedAddGoalConstr(IloEnv &env,
                                IloModel &model,
                                const pddl_t *pddl,
                                const lifted_endomorphism_t *end,
                                IloIntVarArray &csp_var)
{
    pred_obj_tuples_t tuples;
    predObjTuplesInitFromCond(&tuples, pddl, pddl->goal);
    for (int pred = 0; pred < pddl->pred.pred_size; ++pred){
        if (tuples.tuple[pred].tuple_size == 0)
            continue;
        const pred_obj_tuple_t *tup = tuples.tuple + pred;

        for (int ti = 0; ti < tup->tuple_size; ++ti){
            IloIntTupleSet obj_values(env, tup->size);
            IloIntArray vals(env, tup->size);
            IloIntVarArray vars(env, tup->size);
            for(int i = 0; i < tup->size; ++i){
                vals[i] = tup->tuple[ti].tuple[i];
                vars[i] = csp_var[tup->tuple[ti].tuple[i]];
            }
            obj_values.add(vals);
            model.add(IloAllowedAssignments(env, vars, obj_values));
        }
    }
    predObjTuplesFree(&tuples);
}
*/


static int extractSol(int obj_size,
                      const int *sol,
                      pddl_iset_t *redundant_objs,
                      pddl_obj_id_t *map)
{
    int *mapped_to = CALLOC_ARR(int, obj_size);

    PDDL_ISET(redundant);
    for (int i = 0; i < obj_size; ++i)
        mapped_to[sol[i]] = 1;

    for (int i = 0; i < obj_size; ++i){
        if (!mapped_to[i]){
            if (map != NULL)
                map[i] = sol[i];
            pddlISetAdd(&redundant, i);

        }else if (map != NULL){
            // Set identity everywhere else to get rid of symmetries
            map[i] = i;
        }
    }
    int num_redundant = pddlISetSize(&redundant);

    if (redundant_objs != NULL){
        pddlISetEmpty(redundant_objs);
        pddlISetUnion(redundant_objs, &redundant);
    }
    pddlISetFree(&redundant);

    FREE(mapped_to);
    return num_redundant;
}

static int liftedSolve(const pddl_t *pddl,
                       const lifted_endomorphism_t *end,
                       const pddl_endomorphism_config_t *cfg,
                       pddl_iset_t *redundant_objs,
                       pddl_obj_id_t *map,
                       pddl_err_t *err)
{
    int ret = 0;
    int obj_size = pddl->obj.obj_size;

    pddl_cp_t cp;
    pddlCPInit(&cp);

    // Create variables and set domains
    liftedAddDomains(pddl, end, &cp);

    // Add constraints on the initial state and the goal
    liftedAddInitConstr(pddl, end, cfg, &cp);

    pddlCPSetObjectiveMinCountDiffAllIVars(&cp);
    LOG(err, "Added objective function min(count-diff())");

    pddlCPSimplify(&cp);
    //pddlCPWriteMinizinc(&cp, stderr);

    pddl_cp_solve_config_t sol_cfg = PDDL_CP_SOLVE_CONFIG_INIT;
    if (cfg->max_search_time > 0)
        sol_cfg.max_search_time = cfg->max_search_time;
    pddl_cp_sol_t sol;
    int sret = pddlCPSolve(&cp, &sol_cfg, &sol, err);
    // There must exist a solution -- at least identity
    ASSERT_RUNTIME(sret == PDDL_CP_FOUND
                    || sret == PDDL_CP_FOUND_SUBOPTIMAL
                    || sret == PDDL_CP_ABORTED);
    int num_redundant = -1;
    if (sret == PDDL_CP_FOUND || sret == PDDL_CP_FOUND_SUBOPTIMAL){
        ASSERT_RUNTIME(sol.num_solutions == 1);
        num_redundant = extractSol(obj_size, sol.isol[0], redundant_objs, map);
        LOG(err, "Found a solution with %d redundant objects", num_redundant);

    }else if (sret == PDDL_CP_ABORTED){
        LOG(err, "Solver was aborted.");
    }
    pddlCPSolFree(&sol);

    if (num_redundant >= 0){
        LOG(err, "Found %{num_redundant}d redundant objects", num_redundant);
        ret = 0;
    }else{
        LOG(err, "Solution not found");
        ret = -1;
    }

    pddlCPFree(&cp);
    return ret;
}

struct select_mgroups {
    int mgroup_size;
    int *mgroup_used;
    int obj_size;
    int *obj_st;
    pddl_lifted_mgroups_t lifted_mgroups;
    int tried_all;
};
typedef struct select_mgroups select_mgroups_t;

static void selectMGroupsInit(select_mgroups_t *select,
                              const pddl_t *pddl,
                              const pddl_lifted_mgroups_t *lifted_mgroups)
{
    select->mgroup_size = lifted_mgroups->mgroup_size;
    select->mgroup_used = CALLOC_ARR(int, select->mgroup_size);
    select->obj_size = pddl->obj.obj_size;
    select->obj_st = CALLOC_ARR(int, select->obj_size);
    pddlLiftedMGroupsInit(&select->lifted_mgroups);
    select->tried_all = 0;
}

static void selectMGroupsFree(select_mgroups_t *select)
{
    FREE(select->mgroup_used);
    FREE(select->obj_st);
    pddlLiftedMGroupsFree(&select->lifted_mgroups);
}

static int selectMGroupsAdd(select_mgroups_t *select,
                            const pddl_t *pddl,
                            int mgi,
                            const pddl_lifted_mgroup_t *mgroup)
{
    int *obj_st = ALLOC_ARR(int, select->obj_size);
    memcpy(obj_st, select->obj_st, sizeof(int) * select->obj_size);

    for (int condi = 0; condi < mgroup->cond.size; ++condi){
        const pddl_fm_t *c = mgroup->cond.fm[condi];
        const pddl_fm_atom_t *a = PDDL_FM_CAST(c, atom);
        for (int argi = 0; argi < a->arg_size; ++argi){
            if (a->arg[argi].obj >= 0){
                if (obj_st[a->arg[argi].obj] > 0){
                    FREE(obj_st);
                    return -1;
                }
                obj_st[a->arg[argi].obj] = -1;
            }
        }
    }

    for (int parami = 0; parami < mgroup->param.param_size; ++parami){
        const pddl_param_t *param = mgroup->param.param + parami;
        int type_id = param->type;
        int obj_size;
        const pddl_obj_id_t *objs;
        objs = pddlTypesObjsByType(&pddl->type, type_id, &obj_size);
        for (int obji = 0; obji < obj_size; ++obji){
            int obj = objs[obji];
            if (param->is_counted_var){
                if (obj_st[obj] < 0){
                    FREE(obj_st);
                    return -1;
                }
                obj_st[obj] = 1;
            }else{
                if (obj_st[obj] > 0){
                    FREE(obj_st);
                    return -1;
                }
                obj_st[obj] = -1;
            }
        }
    }
    pddlLiftedMGroupsAdd(&select->lifted_mgroups, mgroup);
    memcpy(select->obj_st, obj_st, sizeof(int) * select->obj_size);
    FREE(obj_st);
    return 0;
}

static int selectMGroups(select_mgroups_t *select,
                         const pddl_t *pddl,
                         const pddl_lifted_mgroups_t *lifted_mgroups,
                         const pddl_endomorphism_config_t *cfg)
{
    ZEROIZE_ARR(select->obj_st, select->obj_size);

    if (!select->tried_all){
        pddlLiftedMGroupsFree(&select->lifted_mgroups);
        pddlLiftedMGroupsInitCopy(&select->lifted_mgroups, lifted_mgroups);
        select->tried_all = 1;
        return 0;
    }

    if (!cfg->lifted_use_combinations)
        return -1;

    int num_used = 0;
    int start_i;
    do {
        for (start_i = 0;
                start_i < select->mgroup_size && select->mgroup_used[start_i];
                ++start_i);
        if (start_i == select->mgroup_size)
            return -1;

        select->mgroup_used[start_i] = 1;
        num_used = 1;
        pddlLiftedMGroupsFree(&select->lifted_mgroups);
        pddlLiftedMGroupsInit(&select->lifted_mgroups);
        if (selectMGroupsAdd(select, pddl, start_i,
                             &lifted_mgroups->mgroup[start_i]) != 0){
            start_i = -1;
        }
    } while (start_i < 0);

    for (int i = (start_i + 1) % select->mgroup_size; i != start_i;
            i = (i + 1) % select->mgroup_size){
        if (selectMGroupsAdd(select, pddl, i,
                             &lifted_mgroups->mgroup[i]) == 0){
            select->mgroup_used[i] = 1;
            num_used += 1;
        }
    }

    // We skip this because we always make sure that the variant with all
    // selected mutex groups is returned as a first choice
    if (num_used == select->mgroup_size)
        return -1;
    return 0;
}


int pddlEndomorphismLifted(const pddl_t *pddl,
                           const pddl_lifted_mgroups_t *lifted_mgroups_in,
                           const pddl_endomorphism_config_t *cfg,
                           pddl_iset_t *redundant_objects,
                           pddl_obj_id_t *omap,
                           pddl_err_t *err)
{
    if (!pddl->normalized)
        PDDL_ERR_RET(err, -1, "PDDL needs to be normalized!");

    CTX(err, "lendo", "Lifted endomorphism");
    // TODO
    if (cfg->run_in_subprocess)
        LOG(err, "run_in_subprocess is ignored");

    if (cfg->ignore_costs)
        LOG(err, "Ignoring operator costs");

    for (int i = 0; omap != NULL && i < pddl->obj.obj_size; ++i)
        omap[i] = i;

    if (!pddlTypesHasStrictPartitioning(&pddl->type, &pddl->obj)){
        LOG(err, "Non-strict type partitioning"
             " -- abstaining from the inference");
        CTXEND(err);
        return 0;
    }

    // Filter out mutex groups without counted variables
    pddl_lifted_mgroups_t lifted_mgroups;
    pddlLiftedMGroupsInit(&lifted_mgroups);
    for (int mgi = 0; mgi < lifted_mgroups_in->mgroup_size; ++mgi){
        const pddl_lifted_mgroup_t *mg = lifted_mgroups_in->mgroup + mgi;
        if (pddlLiftedMGroupNumCountedVars(mg) > 0)
            pddlLiftedMGroupsAdd(&lifted_mgroups, mg);
    }

    // Remove atoms without counted variables
    for (int mgi = 0; mgi < lifted_mgroups.mgroup_size; ++mgi){
        pddl_lifted_mgroup_t *mg = lifted_mgroups.mgroup + mgi;
        pddlLiftedMGroupRemoveFixedAtoms(mg);
    }

    if (lifted_mgroups.mgroup_size == 0){
        LOG(err, "No mutex groups so lifted endomorphisms cannot be inferred");
        CTXEND(err);
        pddlLiftedMGroupsFree(&lifted_mgroups);
        return 0;
    }


    select_mgroups_t select;
    selectMGroupsInit(&select, pddl, &lifted_mgroups);
    while (selectMGroups(&select, pddl, &lifted_mgroups, cfg) == 0){
        CTX(err, "select_mgroup", "Select mgroup");
        for (int i = 0; i < select.lifted_mgroups.mgroup_size; ++i)
            pddlLiftedMGroupLog(pddl, &select.lifted_mgroups.mgroup[i], err);
        CTXEND(err);

        lifted_endomorphism_t end;
        liftedEndomorphismInit(&end, pddl, &select.lifted_mgroups, cfg, err);
        if (liftedEndomorphismNumUnfixed(&end) > 1){
            liftedSolve(pddl, &end, cfg, redundant_objects, omap, err);
        }else{
            LOG(err, "Not enough unfixed objects to try to find endomorphisms");
        }
        liftedEndomorphismFree(&end);
    }
    selectMGroupsFree(&select);
    pddlLiftedMGroupsFree(&lifted_mgroups);
    CTXEND(err);
    return 0;
}

static int relaxedLifted(const pddl_t *pddl,
                         const pddl_endomorphism_config_t *cfg,
                         pddl_iset_t *redundant_objects,
                         pddl_obj_id_t *omap,
                         pddl_err_t *err)
{
    lifted_endomorphism_t end;
    liftedEndomorphismInit(&end, pddl, NULL, cfg, err);
    if (liftedEndomorphismNumUnfixed(&end) > 1){
        int *map = NULL;
        if (omap != NULL)
            map = ALLOC_ARR(int, pddl->obj.obj_size);
        liftedSolve(pddl, &end, cfg, redundant_objects, map, err);
        if (map != NULL){
            for (int i = 0; i < pddl->obj.obj_size; ++i)
                omap[i] = map[i];
            FREE(map);
        }
    }else{
        LOG(err, "Not enough unfixed objects to try to find endomorphisms");
    }
    liftedEndomorphismFree(&end);
    return 0;
}

static int relaxedLiftedInSubprocess(const pddl_t *pddl,
                                     const pddl_endomorphism_config_t *cfg,
                                     pddl_iset_t *redundant_objects,
                                     pddl_obj_id_t *omap,
                                     pddl_err_t *err)
{
    LOG(err, "Lifted Relaxed Endomorphism in a subprocess ...");
    fflush(stdout);
    fflush(stderr);
    fflush(err->warn_out);
    fflush(err->info_out);

    int obj_size = pddl->obj.obj_size;
    size_t shared_size = sizeof(int) + (sizeof(pddl_obj_id_t) * obj_size);
    void *shared = mmap(NULL, shared_size, PROT_WRITE | PROT_READ,
                        MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shared == MAP_FAILED){
        //perror("mmap() failed");
        LOG(err, "Could not allocate shared memory of size %ld using mmap: %s",
            (long)shared_size, strerror(errno));
        return -1;
    }
    ZEROIZE_RAW(shared, shared_size);
    int *shared_ret = (int *)shared;
    pddl_obj_id_t *shared_map = (pddl_obj_id_t *)(shared_ret + 1);
    *shared_ret = -1;
    LOG(err, "  Allocated %ld bytes of shared memory", (long)shared_size);

    int pid = fork();
    if (pid == -1){
        perror("fork() failed");
        return -1;

    }else if (pid == 0){
        *shared_ret = -1;
        for (int i = 0; i < pddl->obj.obj_size; ++i)
            shared_map[i] = i;

        PDDL_ISET(red);
        int ret = relaxedLifted(pddl, cfg, &red, shared_map, err);
        pddlISetFree(&red);
        *shared_ret = ret;
        exit(ret);

    }else{
        waitpid(pid, NULL, 0);
        int ret = *shared_ret;
        if (ret == 0){
            if (omap != NULL)
                memcpy(omap, shared_map, sizeof(pddl_obj_id_t) * obj_size);

            if (redundant_objects != NULL){
                for (int i = 0; i < pddl->obj.obj_size; ++i){
                    if (shared_map[i] != i)
                        pddlISetAdd(redundant_objects, i);
                }
            }
        }
        munmap(shared, shared_size);
        if (redundant_objects != NULL){
            LOG(err, "Relaxed Lifted Endomorphism in a subprocess: ret: %d,"
                " redundant ops: %d",
                ret, pddlISetSize(redundant_objects));
        }else{
            LOG(err, "Relaxed Lifted Endomorphism in a subprocess: ret: %d",
                ret);
        }
        LOG(err, "Relaxed Lifted Endomorphism in a subprocess DONE");
        return ret;
    }
}

int pddlEndomorphismRelaxedLifted(const pddl_t *pddl,
                                  const pddl_endomorphism_config_t *cfg,
                                  pddl_iset_t *redundant_objects,
                                  pddl_obj_id_t *omap,
                                  pddl_err_t *err)
{
    if (!pddl->normalized)
        ERR_RET(err, -1, "PDDL needs to be normalized!");

    CTX(err, "relax_lendo", "Relaxed lifted endomorphism");
    if (cfg->ignore_costs)
        LOG(err, "Ignoring operator costs");

    for (int i = 0; omap != NULL && i < pddl->obj.obj_size; ++i)
        omap[i] = i;

    if (!pddlTypesHasStrictPartitioning(&pddl->type, &pddl->obj)){
        LOG(err, "Non-strict type partitioning"
             " -- abstaining from the inference");
        CTXEND(err);
        return 0;
    }

    int ret = 0;
    if (cfg->run_in_subprocess){
        ret = relaxedLiftedInSubprocess(pddl, cfg, redundant_objects, omap, err);
    }else{
        ret = relaxedLifted(pddl, cfg, redundant_objects, omap, err);
    }
    CTXEND(err);
    return ret;
}
