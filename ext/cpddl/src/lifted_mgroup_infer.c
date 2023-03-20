/***
 * cpddl
 * -------
 * Copyright (c)2019 Daniel Fiser <danfis@danfis.cz>,
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

// TODO: Merge candidates with proved mgroups if possible (e.g., when types
//       of variables are subtypes)

#include "internal.h"
#include "pddl/timer.h"
#include "pddl/fifo.h"
#include "pddl/pddl.h"
#include "pddl/lifted_mgroup_htable.h"
#include "pddl/lifted_mgroup_infer.h"

void pddlLiftedMGroupsInferConfigLog(const pddl_lifted_mgroups_infer_config_t *cfg,
                                     pddl_err_t *err)
{
    LOG_CONFIG_INT(cfg, max_candidates, err);
    LOG_CONFIG_INT(cfg, max_mgroups, err);
    LOG_CONFIG_BOOL(cfg, fd, err);
    LOG(err, "fd_monotonicity = %{fd_monotonicity}b", cfg->fd_monotonicity != NULL);
}

int pddlLiftedMGroupsInfer(const pddl_t *pddl,
                           const pddl_lifted_mgroups_infer_config_t *cfg,
                           pddl_lifted_mgroups_t *lmg,
                           pddl_err_t *err)
{
    CTX(err, "lmg", "LMG");
    CTX_NO_TIME(err, "cfg", "Cfg");
    pddlLiftedMGroupsInferConfigLog(cfg, err);
    CTXEND(err);

    pddl_lifted_mgroups_infer_limits_t limits
            = PDDL_LIFTED_MGROUPS_INFER_LIMITS_INIT;
    limits.max_candidates = cfg->max_candidates;
    limits.max_mgroups = cfg->max_mgroups;

    if (cfg->fd){
        pddlLiftedMGroupsInferMonotonicity(pddl, &limits,
                                           cfg->fd_monotonicity, lmg, err);
    }else{
        pddlLiftedMGroupsInferFAMGroups(pddl, &limits, lmg, err);
    }
    pddlLiftedMGroupsSetExactlyOne(pddl, lmg, err);
    pddlLiftedMGroupsSetStatic(pddl, lmg, err);

    CTXEND(err);
    return 0;
}

struct cand {
    int id;
    const pddl_lifted_mgroup_t *mgroup;
    int each_pred_only_once; /*!< True if each predicate is there only once */
    int refined_from; /*!< ID of the candidate this was refined from */
    int refined_var; /*!< True if fefined by changing variables */
    int refined_type; /*!< True if refined by changing types */
    int refined_by_extend; /*!< True if refined by adding predicates */
    int refined_by_extend_pred;
};
typedef struct cand cand_t;

#define CAND_LOCAL(NAME, MGROUP) \
    cand_t NAME = { -1, (MGROUP), -1, 0, 0, 0, -1 }

struct cfg {
    int max_counted_vars;
    int refine_type_too_heavy_init;
    int refine_type_too_heavy_action;
    int refine_type_unbalanced_action;
    int refine_var_too_heavy_init;
    int refine_var_too_heavy_action;
    int refine_var_proved;
    int refine_var_proved_only_goal_aware;
    int refine_extend_proved;
};
typedef struct cfg cfg_t;

struct refine {
    const pddl_t *pddl;
    pddl_lifted_mgroups_infer_limits_t limit;
    cfg_t cfg;
    pddl_err_t *err;

    pddl_lifted_mgroup_htable_t mgroup;
    pddl_extarr_t *cand;
    int cand_size;

    pddl_fifo_t queue1;
    pddl_fifo_t queue2;
};
typedef struct refine refine_t;


struct ce_atom {
    const pddl_fm_t *pre;
    const pddl_fm_atom_t *atom;
};
typedef struct ce_atom ce_atom_t;

#define CE_ATOM(NAME, PRE, ATOM) \
    ce_atom_t NAME = { (PRE), (ATOM) }

struct unify_action_ctx {
    const pddl_t *pddl;
    const pddl_params_t *action_param;
    const pddl_params_t *cand_param;
    int *action_arg;
    int *cand_arg;
    int next_name;
};
typedef struct unify_action_ctx unify_action_ctx_t;

static int ctxArgInit(const pddl_t *pddl, const pddl_param_t *param)
{
    if (pddlTypeNumObjs(&pddl->type, param->type) == 1)
        return pddlTypeGetObj(&pddl->type, param->type, 0);
    return -1;
}

#define UNIFY_ACTION_CTX(NAME, PDDL, ACTION_PARAMS, CAND_PARAMS) \
    int __action_arg_##NAME[(ACTION_PARAMS)->param_size]; \
    for (int __i = 0; __i < (ACTION_PARAMS)->param_size; ++__i) \
        __action_arg_##NAME[__i] \
            = ctxArgInit((PDDL), (ACTION_PARAMS)->param + __i); \
    int __cand_arg_##NAME[(CAND_PARAMS)->param_size]; \
    for (int __i = 0; __i < (CAND_PARAMS)->param_size; ++__i) \
        __cand_arg_##NAME[__i] \
            = ctxArgInit((PDDL), (CAND_PARAMS)->param + __i); \
    unify_action_ctx_t NAME = { (PDDL), (ACTION_PARAMS), (CAND_PARAMS), \
        __action_arg_##NAME, __cand_arg_##NAME, (PDDL)->obj.obj_size }

#define UNIFY_ACTION_CTX_PUSH(NAME, SRC) \
    int __action_arg_##NAME[(SRC)->action_param->param_size]; \
    memcpy(__action_arg_##NAME, (SRC)->action_arg, \
            sizeof(int) * (SRC)->action_param->param_size); \
    int __cand_arg_##NAME[(SRC)->cand_param->param_size]; \
    memcpy(__cand_arg_##NAME, (SRC)->cand_arg, \
            sizeof(int) * (SRC)->cand_param->param_size); \
    unify_action_ctx_t NAME = { \
        (SRC)->pddl, (SRC)->action_param, (SRC)->cand_param, \
        __action_arg_##NAME, __cand_arg_##NAME, (SRC)->next_name }


#define FOR_EACH_ATOM(MG, C) \
    for (int ___i = 0; \
            ___i < (MG)->cond.size \
                && ((C) = PDDL_FM_CAST((MG)->cond.fm[___i], atom)); \
                ++___i)





static void refineTooHeavyInit(refine_t *refine,
                               const pddl_params_t *params,
                               const pddl_fm_atom_t *a1,
                               const pddl_fm_atom_t *a2,
                               const cand_t *cand,
                               const pddl_fm_atom_t *cand_atom1,
                               const pddl_fm_atom_t *cand_atom2);

static void refineTooHeavyAction(refine_t *refine,
                                 const pddl_params_t *params,
                                 const pddl_fm_atom_t *a1,
                                 const pddl_fm_atom_t *a2,
                                 const cand_t *cand,
                                 const pddl_fm_atom_t *cand_atom1,
                                 const pddl_fm_atom_t *cand_atom2);

static void refineUnbalancedAction(refine_t *refine,
                                   const unify_action_ctx_t *ctx,
                                   const pddl_action_t *action,
                                   const pddl_fm_atom_t *add_eff,
                                   const cand_t *cand,
                                   const pddl_fm_atom_t *cand_add_eff);




static void replaceSingleObjectTypes(const pddl_t *pddl,
                                     pddl_lifted_mgroup_t *mg)
{
    int ins = 0;
    int remap[mg->param.param_size];
    for (int pid = 0; pid < mg->param.param_size; ++pid){
        int type = mg->param.param[pid].type;
        if (pddlTypeNumObjs(&pddl->type, type) == 1){
            int obj = pddlTypeGetObj(&pddl->type, type, 0);

            for (int ati = 0; ati < mg->cond.size; ++ati){
                pddl_fm_atom_t *a = PDDL_FM_CAST(mg->cond.fm[ati], atom);
                for (int ai = 0; ai < a->arg_size; ++ai){
                    if (a->arg[ai].param == pid){
                        a->arg[ai].param = -1;
                        a->arg[ai].obj = obj;
                    }
                }
            }
        }else{
            remap[pid] = ins;
            mg->param.param[ins++] = mg->param.param[pid];
        }
    }

    if (mg->param.param_size != ins){
        for (int ati = 0; ati < mg->cond.size; ++ati){
            pddl_fm_atom_t *a = PDDL_FM_CAST(mg->cond.fm[ati], atom);
            for (int ai = 0; ai < a->arg_size; ++ai){
                if (a->arg[ai].param >= 0)
                    a->arg[ai].param = remap[a->arg[ai].param];
            }
        }
    }
    mg->param.param_size = ins;
}

static void mgroupFinalize(const pddl_t *pddl, pddl_lifted_mgroup_t *mg)
{
    pddlLiftedMGroupSort(mg);
}

static void addProvedLiftedMGroup(const pddl_t *pddl,
                                  const pddl_lifted_mgroup_t *mg,
                                  pddl_lifted_mgroups_t *mgs)
{
    pddl_lifted_mgroup_t m;
    pddlLiftedMGroupInitCopy(&m, mg);
    replaceSingleObjectTypes(pddl, &m);
    pddlLiftedMGroupSort(&m);
    pddlLiftedMGroupsAdd(mgs, &m);
    pddlLiftedMGroupFree(&m);
}

/** Returns true if the candidate contains atom of the specified predicate */
static int candHasPred(const pddl_lifted_mgroup_t *cand, int pred)
{
    for (int i = 0; i < cand->cond.size; ++i){
        const pddl_fm_atom_t *a = PDDL_FM_CAST(cand->cond.fm[i], atom);
        if (a->pred == pred)
            return 1;
    }
    return 0;
}

/** Returns true if there is an effect matching one of the predicats from
 *  cand */
static int candHasAddEff(const cand_t *cand, const pddl_fm_t *eff)
{
    pddl_fm_const_it_eff_t it;
    const pddl_fm_atom_t *a, *c;
    const pddl_fm_t *pre;

    PDDL_FM_FOR_EACH_ADD_EFF(eff, &it, a, pre){
        FOR_EACH_ATOM(cand->mgroup, c){
            if (a->pred == c->pred)
                return 1;
        }
    }

    return 0;
}

/** Returns true if cand has at least one counted variable */
static int candHasCountedVar(const pddl_lifted_mgroup_t *cand)
{
    for (int i = 0; i < cand->param.param_size; ++i){
        if (cand->param.param[i].is_counted_var)
            return 1;
    }
    return 0;
}

/** Returns true if atom has counted variable as one of its arguments */
static int candAtomHasCountedVar(const pddl_lifted_mgroup_t *cand,
                                 const pddl_fm_atom_t *atom)
{
    for (int i = 0; i < atom->arg_size; ++i){
        if (atom->arg[i].param >= 0
                && cand->param.param[atom->arg[i].param].is_counted_var){
            return 1;
        }
    }
    return 0;
}

/** Returns false if there are two action arguments that have assigned the
 *  same value, but their types are disjunct. */
static int actionArgTypesAreValid(const pddl_t *pddl,
                                  const pddl_params_t *params,
                                  const int *args)
{
    const pddl_types_t *ts = &pddl->type;

    for (int i = 0; i < params->param_size; ++i){
        if (args[i] < 0)
            continue;
        int type1 = params->param[i].type;

        for (int j = i + 1; j < params->param_size; ++j){
            int type2 = params->param[j].type;
            if (args[i] == args[j] && pddlTypesAreDisjunct(ts, type1, type2))
                return 0;
        }
    }

    return 1;
}

/** Returns value corresponding to the specified argument */
static int atomArg(const pddl_fm_atom_t *atom, int argi, const int *args)
{
    int param = atom->arg[argi].param;
    if (param >= 0)
        return args[param];
    return atom->arg[argi].obj;
}

/** Returns true if the atoms are equal under the given variable assignment */
static int atomsEqual(const pddl_fm_atom_t *atom1,
                      const pddl_fm_atom_t *atom2,
                      const int *args)
{
    if (atom1->pred != atom2->pred)
        return 0;

    for (int ai = 0; ai < atom1->arg_size; ++ai){
        if (atomArg(atom1, ai, args) != atomArg(atom2, ai, args))
            return 0;
    }
    return 1;
}

/** Returns true if the exactly same atom can be found in conj */
static int equalAtomIn(const pddl_fm_atom_t *atom,
                       const pddl_fm_t *conj,
                       const int *args)
{
    pddl_fm_const_it_atom_t it;
    const pddl_fm_atom_t *a2;

    if (conj == NULL)
        return 0;

    PDDL_FM_FOR_EACH_ATOM(conj, &it, a2){
        if (!a2->neg && atomsEqual(a2, atom, args))
            return 1;
    }
    return 0;
}

static pddl_obj_id_t atomArgObj(const pddl_fm_atom_t *atom, int argi,
                                const pddl_obj_id_t *args)
{
    int param = atom->arg[argi].param;
    if (param >= 0)
        return args[param];
    return atom->arg[argi].obj;
}

static int atomsEqualObj(const pddl_fm_atom_t *atom1,
                         const pddl_fm_atom_t *atom2,
                         const pddl_obj_id_t *args)
{
    if (atom1->pred != atom2->pred)
        return 0;

    for (int ai = 0; ai < atom1->arg_size; ++ai){
        if (atomArgObj(atom1, ai, args) != atomArgObj(atom2, ai, args))
            return 0;
    }
    return 1;
}

static int equalAtomInArrObj(const pddl_fm_atom_t *atom,
                             const pddl_fm_arr_t *arr,
                             const pddl_obj_id_t *args)
{
    if (arr == NULL)
        return 0;

    for (int i = 0; i < arr->size; ++i){
        const pddl_fm_atom_t *a2 = PDDL_FM_CAST(arr->fm[i], atom);
        if (!a2->neg && atomsEqualObj(a2, atom, args))
            return 1;
    }
    return 0;
}

/** Returns true the inequalities in the conjuction hold given the
 *  bound arguments */
static int inequalitiesHold(const pddl_t *pddl,
                            const pddl_fm_t *pre,
                            const int *args)
{
    pddl_fm_const_it_atom_t it;
    const pddl_fm_atom_t *a;

    if (pre == NULL)
        return 1;

    PDDL_FM_FOR_EACH_ATOM(pre, &it, a){
        if (a->neg && a->pred == pddl->pred.eq_pred){
            int v0 = atomArg(a, 0, args);
            int v1 = atomArg(a, 1, args);
            if (v0 == v1 && v0 >= 0)
                return 0;
        }
    }
    return 1;
}

static int staticAtomHasEqArgs(const pddl_t *pddl, int pred_id,
                               int arg0, int arg1)
{
    pddl_fm_const_it_atom_t it;
    const pddl_fm_atom_t *a;

    PDDL_FM_FOR_EACH_ATOM(&pddl->init->fm, &it, a){
        if (!a->neg && a->pred == pred_id){
            if (a->arg[arg0].obj == a->arg[arg1].obj)
                return 1;
        }
    }
    return 0;
}

/** Returns true if static preconditions are not violated with the given
 *  arguments */
static int staticPreHold(const pddl_t *pddl,
                         const pddl_fm_t *pre,
                         const int *args)
{
    pddl_fm_const_it_atom_t it;
    const pddl_fm_atom_t *a;

    if (pre == NULL)
        return 1;

    PDDL_FM_FOR_EACH_ATOM(pre, &it, a){
        if (!a->neg && pddlPredIsStatic(pddl->pred.pred + a->pred)){
            for (int i = 0; i < a->arg_size; ++i){
                int arg0 = atomArg(a, i, args);
                for (int j = i + 1; arg0 >= 0 && j < a->arg_size; ++j){
                    if (arg0 == atomArg(a, j, args)){
                        if (!staticAtomHasEqArgs(pddl, a->pred, i, j))
                            return 0;
                    }
                }
            }
        }
    }

    return 1;
}



/*** UNIFICATION ***/
static int _unifyFact(const pddl_t *pddl,
                      const pddl_fm_atom_t *fact,
                      const pddl_obj_id_t *fact_arg,
                      const pddl_params_t *cand_params,
                      const pddl_fm_atom_t *cand_atom,
                      pddl_obj_id_t *cand_arg)
{
    if (fact->pred != cand_atom->pred)
        return 0;

    ASSERT(fact->arg_size == cand_atom->arg_size);
    for (int i = 0; i < fact->arg_size; ++i){
        pddl_obj_id_t fact_obj = fact->arg[i].obj;
        if (fact_arg != NULL)
            fact_obj = atomArgObj(fact, i, fact_arg);
        ASSERT(fact_obj >= 0);

        int param = cand_atom->arg[i].param;
        pddl_obj_id_t obj = cand_atom->arg[i].obj;
        if (param >= 0){
            if (!pddlTypesObjHasType(&pddl->type,
                                     cand_params->param[param].type,
                                     fact_obj)){
                return 0;
            }

            if (!cand_params->param[param].is_counted_var){
                if (cand_arg[param] == PDDL_OBJ_ID_UNDEF){
                    cand_arg[param] = fact_obj;
                }else if (cand_arg[param] != fact_obj){
                    return 0;
                }
            }

        }else{
            ASSERT(obj != PDDL_OBJ_ID_UNDEF);
            if (obj != fact_obj)
                return 0;
        }
    }

    return 1;
}

/** Unify fact (grounded with fact_arg) with the candidate atom */
static int unifyFact(const pddl_t *pddl,
                     const pddl_fm_atom_t *fact,
                     const pddl_obj_id_t *fact_arg,
                     const pddl_params_t *cand_params,
                     const pddl_fm_atom_t *cand_atom,
                     pddl_obj_id_t *cand_arg)
{
    for (int i = 0; i < cand_params->param_size; ++i)
        cand_arg[i] = PDDL_OBJ_ID_UNDEF;
    return _unifyFact(pddl, fact, fact_arg, cand_params, cand_atom, cand_arg);
}

/** Returns true if fact (grounded with fact_arg) can be unified with the
 *  given candidate atom and arguments. */
static int canUnifyFact(const pddl_t *pddl,
                        const pddl_fm_atom_t *fact,
                        const pddl_obj_id_t *fact_arg,
                        const pddl_params_t *cand_params,
                        const pddl_fm_atom_t *cand_atom,
                        const pddl_obj_id_t *cand_arg)
{
    pddl_obj_id_t args[cand_params->param_size];
    memcpy(args, cand_arg, sizeof(pddl_obj_id_t) * cand_params->param_size);
    return _unifyFact(pddl, fact, fact_arg, cand_params, cand_atom, args);
}

/** Returns true if the atoms are compatible, i.e., they are the same
 *  predicate and arguments have matching types/objects */
static int atomsAreCompatible(const pddl_t *pddl,
                              const pddl_fm_atom_t *a1,
                              const pddl_params_t *a1_params,
                              const pddl_fm_atom_t *a2,
                              const pddl_params_t *a2_params)
{
    if (a1->pred != a2->pred)
        return 0;

    for (int i = 0; i < a1->arg_size; ++i){
        if (a1->arg[i].param >= 0 && a2->arg[i].param >= 0){
            int a1type = a1_params->param[a1->arg[i].param].type;
            int a2type = a2_params->param[a2->arg[i].param].type;
            if (pddlTypesAreDisjunct(&pddl->type, a1type, a2type))
                return 0;

        }else if (a1->arg[i].param >= 0){ // && a2->arg[i].obj >= 0
            int a1type = a1_params->param[a1->arg[i].param].type;
            if (!pddlTypesObjHasType(&pddl->type, a1type, a2->arg[i].obj))
                return 0;

        }else if (a2->arg[i].param >= 0){ // && a1->arg[i].obj >= 0
            int a2type = a2_params->param[a2->arg[i].param].type;
            if (!pddlTypesObjHasType(&pddl->type, a2type, a1->arg[i].obj))
                return 0;

        }else{ // a1->arg[i].obj >= 0 && a2->arg[i].obj >= 0
            if (a1->arg[i].obj != a2->arg[i].obj)
                return 0;
        }
    }
    return 1;
}

static void renameArgs(unify_action_ctx_t *ctx, int from, int to)
{
    for (int i = 0; i < ctx->cand_param->param_size; ++i){
        if (ctx->cand_arg[i] == from)
            ctx->cand_arg[i] = to;
    }
    for (int i = 0; i < ctx->action_param->param_size; ++i){
        if (ctx->action_arg[i] == from)
            ctx->action_arg[i] = to;
    }
}

/** Unify action's atom with the candidate atom */
static int unifyActionAtom(unify_action_ctx_t *ctx,
                           const pddl_fm_atom_t *action_atom,
                           const pddl_fm_atom_t *cand_atom)
{
    if (!atomsAreCompatible(ctx->pddl,
                            cand_atom, ctx->cand_param,
                            action_atom, ctx->action_param)){
        return 0;
    }

    // Empty counted variables because they can be bound to something else
    // now
    for (int i = 0; i < cand_atom->arg_size; ++i){
        int param = cand_atom->arg[i].param;
        if (param >= 0 && ctx->cand_param->param[param].is_counted_var)
            ctx->cand_arg[param] = -1;
    }

    ASSERT(action_atom->arg_size == cand_atom->arg_size);
    for (int i = 0; i < cand_atom->arg_size; ++i){
        int aparam = action_atom->arg[i].param;
        int cparam = cand_atom->arg[i].param;

        if (aparam >= 0 && cparam >= 0){
            if (ctx->cand_arg[cparam] < 0 && ctx->action_arg[aparam] < 0){
                // Neither of cand and action parameters are bound. So bind
                // them to the same name
                ctx->cand_arg[cparam] = ctx->next_name;
                ctx->action_arg[aparam] = ctx->next_name;
                ++ctx->next_name;

            }else if (ctx->cand_arg[cparam] < 0){
                // Only cand param is not set, so copy the same name from
                // the action parameter
                ctx->cand_arg[cparam] = ctx->action_arg[aparam];

            }else if (ctx->action_arg[aparam] < 0){
                // Only action param is not set
                ctx->action_arg[aparam] = ctx->cand_arg[cparam];

            }else if (ctx->cand_arg[cparam] != ctx->action_arg[aparam]){
                // Both parameters are set, but they are different
                if (ctx->cand_arg[cparam] < ctx->pddl->obj.obj_size
                        && ctx->action_arg[aparam] < ctx->pddl->obj.obj_size){
                    // Both are set to an object that is different, which
                    // means that unification is not possible
                    return 0;

                }else if (ctx->cand_arg[cparam] < ctx->pddl->obj.obj_size){
                    // Candidate parameter is bound to an object, so
                    // propagate the same object to all parameters
                    renameArgs(ctx, ctx->action_arg[aparam],
                                    ctx->cand_arg[cparam]);

                }else{
                    renameArgs(ctx, ctx->cand_arg[cparam],
                                    ctx->action_arg[aparam]);
                }
            }

        }else if (cparam >= 0){
            int obj_id = action_atom->arg[i].obj;
            if (ctx->cand_arg[cparam] < 0){
                ctx->cand_arg[cparam] = obj_id;
            }else if (ctx->cand_arg[cparam] < ctx->pddl->obj.obj_size){
                if (ctx->cand_arg[cparam] != obj_id)
                    return 0;
            }else{
                renameArgs(ctx, ctx->cand_arg[cparam], obj_id);
            }

        }else if (aparam >= 0){
            int obj_id = cand_atom->arg[i].obj;
            if (ctx->action_arg[aparam] < 0){
                ctx->action_arg[aparam] = obj_id;
            }else if (ctx->action_arg[aparam] < ctx->pddl->obj.obj_size){
                if (ctx->action_arg[aparam] != obj_id)
                    return 0;
            }else{
                renameArgs(ctx, ctx->action_arg[aparam], obj_id);
            }

        }else{
            if (action_atom->arg[i].obj != cand_atom->arg[i].obj)
                return 0;
        }
    }

    return 1;
}



/*** HEAVINESS TEST ***/
/** Returns true if unified pair of effects can really be unified
 *  considering types and inequalities */
static int checkUnifiedEffPair(unify_action_ctx_t *ctx,
                               const pddl_t *pddl,
                               const pddl_action_t *action,
                               const ce_atom_t *eff1,
                               const ce_atom_t *eff2)
{
    // If two variables has the same name, but the corresponding types are
    // disjunct, then we cannot unify the atoms
    if (!actionArgTypesAreValid(pddl, ctx->action_param, ctx->action_arg))
        return 0;

    // Check inequality predicates: we cannot assign the same name to two
    // arguments that cannot be same
    if (!inequalitiesHold(pddl, action->pre, ctx->action_arg)
            || !inequalitiesHold(pddl, eff1->pre, ctx->action_arg)
            || !inequalitiesHold(pddl, eff2->pre, ctx->action_arg)){
        return 0;
    }

    // We unified two atoms, but we must check whether they differ
    if (atomsEqual(eff1->atom, eff2->atom, ctx->action_arg))
        return 0;

    // If exactly the same atoms are in the precondition, i.e.,
    // ((not a1) and (not a2)) is not satisfiable in the state where we
    // apply this action, then this action cannot increase the number of
    // facts in the resulting state.
    if (equalAtomIn(eff1->atom, action->pre, ctx->action_arg)
            || equalAtomIn(eff1->atom, eff1->pre, ctx->action_arg)
            || equalAtomIn(eff2->atom, action->pre, ctx->action_arg)
            || equalAtomIn(eff2->atom, eff2->pre, ctx->action_arg)){
        return 0;
    }

    // Check whether static preconditions are satisfiable
    if (!staticPreHold(pddl, action->pre, ctx->action_arg)
            || !staticPreHold(pddl, eff1->pre, ctx->action_arg)
            || !staticPreHold(pddl, eff2->pre, ctx->action_arg)){
        return 0;
    }

    return 1;
}

static int isGoalAware(const pddl_t *pddl, const pddl_lifted_mgroup_t *mg)
{
    pddl_obj_id_t arg[mg->param.param_size];

    pddl_fm_const_it_atom_t it;
    const pddl_fm_atom_t *goal;
    PDDL_FM_FOR_EACH_ATOM(pddl->goal, &it, goal){
        ASSERT(!goal->neg);
        const pddl_fm_atom_t *c;
        FOR_EACH_ATOM(mg, c){
            if (c->pred != goal->pred)
                continue;

            if (unifyFact(pddl, goal, NULL, &mg->param, c, arg))
                return 1;
        }
    }

    return 0;
}

static int initHeaviness(const pddl_t *pddl,
                         const cand_t *cand,
                         refine_t *refine)
{
    pddl_obj_id_t arg[cand->mgroup->param.param_size];
    const pddl_fm_atom_t *cand1, *cand2;
    pddl_fm_const_it_atom_t it1, it2;
    const pddl_fm_atom_t *a1, *a2;
    int unified = 0;

    PDDL_FM_FOR_EACH_ATOM(&pddl->init->fm, &it1, a1){
        if (a1->neg)
            continue;

        FOR_EACH_ATOM(cand->mgroup, cand1){
            if (cand1->pred != a1->pred)
                continue;
            if (!unifyFact(pddl, a1, NULL, &cand->mgroup->param, cand1, arg))
                continue;

            unified = 1;

            it2 = it1;
            PDDL_FM_FOR_EACH_ATOM_CONT(&it2, a2){
                if (a2->neg)
                    continue;
                FOR_EACH_ATOM(cand->mgroup, cand2){
                    if (cand2->pred != a2->pred)
                        continue;
                    if (canUnifyFact(pddl, a2, NULL,
                                     &cand->mgroup->param, cand2, arg)){
                        refineTooHeavyInit(refine, NULL, a1, a2,
                                           cand, cand1, cand2);
                        return 2;
                    }
                }
            }
        }
    }

    return unified;
}

static int isInitExactlyOne(const pddl_t *pddl,
                            const cand_t *cand,
                            refine_t *refine)
{
    return initHeaviness(pddl, cand, refine) == 1;
}

/** Returns the if the initial state is too heavy */
static int isInitTooHeavy(const pddl_t *pddl,
                          const cand_t *cand,
                          refine_t *refine)
{
    return initHeaviness(pddl, cand, refine) > 1;
}

/** Returns true if the conjuction of grounded atoms is too heavy for the
 *  candidate mutex group */
static int isGroundedCondArrTooHeavy(const cand_t *cand,
                                     const pddl_t *pddl,
                                     const pddl_fm_arr_t *arr,
                                     const pddl_obj_id_t *arr_args)
{
    pddl_obj_id_t arg[cand->mgroup->param.param_size];
    const pddl_fm_atom_t *cand1, *cand2;

    for (int i = 0; i < arr->size; ++i){
        const pddl_fm_atom_t *a1 = PDDL_FM_CAST(arr->fm[i], atom);
        if (a1->neg)
            continue;
        FOR_EACH_ATOM(cand->mgroup, cand1){
            if (cand1->pred != a1->pred)
                continue;
            if (!unifyFact(pddl, a1, arr_args,
                           &cand->mgroup->param, cand1, arg)){
                continue;
            }

            for (int j = i + 1; j < arr->size; ++j){
                const pddl_fm_atom_t *a2 = PDDL_FM_CAST(arr->fm[j], atom);
                if (a2->neg || atomsEqualObj(a1, a2, arr_args))
                    continue;
                FOR_EACH_ATOM(cand->mgroup, cand2){
                    if (cand2->pred != a2->pred)
                        continue;
                    if (canUnifyFact(pddl, a2, arr_args,
                                     &cand->mgroup->param, cand2, arg)){
                        return 1;
                    }
                }
            }
        }
    }

    return 0;
}

/** Returns true if the action is too heavy */
static int isActionTooHeavy(const cand_t *cand,
                            const pddl_t *pddl,
                            const pddl_action_t *action,
                            refine_t *refine)
{
    pddl_fm_const_it_eff_t it1, it2;
    const pddl_fm_atom_t *a1, *a2, *cand1, *cand2;
    const pddl_fm_t *pre1, *pre2;

    PDDL_FM_FOR_EACH_ADD_EFF(action->eff, &it1, a1, pre1){
        CE_ATOM(ce_a1, pre1, a1);

        FOR_EACH_ATOM(cand->mgroup, cand1){
            if (cand1->pred != a1->pred)
                continue;

            UNIFY_ACTION_CTX(ctx, pddl, &action->param, &cand->mgroup->param);
            if (!unifyActionAtom(&ctx, ce_a1.atom, cand1))
                continue;

            it2 = it1;
            PDDL_FM_FOR_EACH_ADD_EFF_CONT(&it2, a2, pre2){
                CE_ATOM(ce_a2, pre2, a2);

                if (cand->each_pred_only_once
                        && a2->pred == a1->pred
                        && !candAtomHasCountedVar(cand->mgroup, cand1))
                    continue;

                FOR_EACH_ATOM(cand->mgroup, cand2){
                    if (cand2->pred != a2->pred)
                        continue;

                    UNIFY_ACTION_CTX_PUSH(ctx2, &ctx);
                    if (!unifyActionAtom(&ctx2, ce_a2.atom, cand2))
                        continue;

                    if (checkUnifiedEffPair(&ctx2, pddl, action,
                                            &ce_a1, &ce_a2)){
                        refineTooHeavyAction(refine, &action->param, a1, a2,
                                             cand, cand1, cand2);
                        return 1;
                    }
                }
            }
        }
    }

    return 0;
}

static int isAnyActionTooHeavy(const pddl_t *pddl,
                               const cand_t *cand,
                               refine_t *refine)
{
    for (int ai = 0; ai < pddl->action.action_size; ++ai){
        const pddl_action_t *a = pddl->action.action + ai;
        if (isActionTooHeavy(cand, pddl, a, refine)){
            return 1;
        }
    }

    return 0;
}



/*** BALANCE TEST ***/
static int unifyActionEff(unify_action_ctx_t *ctx,
                          const pddl_action_t *action,
                          const ce_atom_t *eff,
                          const pddl_fm_atom_t *cand_atom)
{
    return unifyActionAtom(ctx, eff->atom, cand_atom)
                && actionArgTypesAreValid(ctx->pddl, &action->param,
                                          ctx->action_arg)
                && inequalitiesHold(ctx->pddl, action->pre, ctx->action_arg)
                && inequalitiesHold(ctx->pddl, eff->pre, ctx->action_arg);
}

static int canUnifyEff(const unify_action_ctx_t *ctx_in,
                       const pddl_action_t *action,
                       const ce_atom_t *eff,
                       const pddl_fm_atom_t *cand_atom,
                       int need_matching_pre)
{
    if (!atomsAreCompatible(ctx_in->pddl,
                            cand_atom, ctx_in->cand_param,
                            eff->atom, ctx_in->action_param)){
        return 0;
    }

    UNIFY_ACTION_CTX_PUSH(ctx, ctx_in);

    // Empty counted variables
    for (int i = 0; i < ctx.cand_param->param_size; ++i){
        if (ctx.cand_param->param[i].is_counted_var)
            ctx.cand_arg[i] = -1;
    }

    ASSERT(cand_atom->pred == eff->atom->pred);
    ASSERT(cand_atom->arg_size == eff->atom->arg_size);
    for (int ai = 0; ai < cand_atom->arg_size; ++ai){
        int cparam = cand_atom->arg[ai].param;
        int dparam = eff->atom->arg[ai].param;
        if (cparam >= 0 && dparam >= 0){
            if (ctx.cand_param->param[cparam].is_counted_var){
                // check that the type of the candidate's param is not too
                // narrow, i.e., if it may be possible to instantiate
                // the delete effect with something outside the candidate's
                // type
                int dtype = ctx.action_param->param[dparam].type;
                int ctype = ctx.cand_param->param[cparam].type;
                if (dtype != ctype
                        && pddlTypesIsParent(&ctx.pddl->type, ctype, dtype)){
                    return 0;
                }

                if (ctx.action_arg[dparam] < 0)
                    ctx.action_arg[dparam] = ctx.next_name++;
                ctx.cand_arg[cparam] = ctx.action_arg[dparam];

            }else{
                if (ctx.cand_arg[cparam] != ctx.action_arg[dparam]
                        || ctx.action_arg[dparam] < 0){
                    return 0;
                }
            }

        }else if (cparam >= 0){
            int dobj = eff->atom->arg[ai].obj;
            if (ctx.cand_param->param[cparam].is_counted_var){
                int ctype = ctx.cand_param->param[cparam].type;
                if (!pddlTypesObjHasType(&ctx.pddl->type, ctype, dobj))
                    return 0;
                ctx.cand_arg[cparam] = dobj;

            }else{
                if (ctx.cand_arg[cparam] != dobj)
                    return 0;
            }

        }else if (dparam >= 0){
            int cobj = cand_atom->arg[ai].obj;
            if (ctx.action_arg[dparam] != cobj)
                return 0;

        }else{
            int cobj = cand_atom->arg[ai].obj;
            int dobj = eff->atom->arg[ai].obj;
            if (cobj != dobj)
                return 0;
        }
    }

    if (need_matching_pre){
        // Now we have assigned names to action variables and we must check
        // that there is a precondition exactly matching the delete effect so
        // we can be sure that the delete effect is present in the state the
        // action is applied on, i.e., that the delete effect really balances
        // the add effect.
        if (equalAtomIn(eff->atom, action->pre, ctx.action_arg)
                || equalAtomIn(eff->atom, eff->pre, ctx.action_arg)){
            return 1;
        }

        // If we did not find a matching precondition, we report that the
        // delete effect cannot balance the add effect.
        return 0;
    }else{
        return 1;
    }
}

static int isAddEffBalanced(const unify_action_ctx_t *ctx,
                            const pddl_action_t *action,
                            const ce_atom_t *add_eff,
                            const cand_t *cand)
{
    pddl_fm_const_it_eff_t it_del;
    const pddl_fm_atom_t *del_eff_atom;
    const pddl_fm_t *pre;
    PDDL_FM_FOR_EACH_DEL_EFF(action->eff, &it_del, del_eff_atom, pre){
        ASSERT(del_eff_atom->neg);
        // Consider only delete effects that agree on the precondition with
        // the add effect it is suppose to cover.
        if (!pddlFmEq(add_eff->pre, pre))
            continue;
        CE_ATOM(del_eff, pre, del_eff_atom);

        const pddl_fm_atom_t *cand_atom;
        FOR_EACH_ATOM(cand->mgroup, cand_atom){
            if (cand_atom->pred != del_eff_atom->pred)
                continue;
            if (canUnifyEff(ctx, action, &del_eff, cand_atom, 1))
                return 1;
        }
    }

    return 0;
}
                            

/** Returns true if the action has balanced all add effects.
 *  If it has not, the candidate is refined and added to refine. */
static int isActionBalanced(const cand_t *cand,
                            const pddl_t *pddl,
                            const pddl_action_t *action,
                            refine_t *refine)
{
    pddl_fm_const_it_eff_t it;
    const pddl_fm_atom_t *a;
    const pddl_fm_t *pre;
    PDDL_FM_FOR_EACH_ADD_EFF(action->eff, &it, a, pre){
        ASSERT(!a->neg);
        CE_ATOM(add_eff, pre, a);

        const pddl_fm_atom_t *cand_atom;
        FOR_EACH_ATOM(cand->mgroup, cand_atom){
            if (cand_atom->pred != a->pred)
                continue;

            UNIFY_ACTION_CTX(ctx, pddl, &action->param, &cand->mgroup->param);
            if (unifyActionEff(&ctx, action, &add_eff, cand_atom)){
                if (!isAddEffBalanced(&ctx, action, &add_eff, cand)){
                    refineUnbalancedAction(refine, &ctx, action, a,
                                           cand, cand_atom);
                    return 0;
                }
            }
        }
    }

    return 1;
}

static int isAnyActionUnbalanced(const pddl_t *pddl,
                                 const cand_t *cand,
                                 refine_t *refine)
{
    for (int ai = 0; ai < pddl->action.action_size; ++ai){
        const pddl_action_t *a = pddl->action.action + ai;
        if (!isActionBalanced(cand, pddl, a, refine)){
            return 1;
        }
    }

    return 0;
}


/*** REFINEMENT ***/
static void refineInit(refine_t *r,
                       const pddl_t *pddl,
                       const pddl_lifted_mgroups_infer_limits_t *limit,
                       pddl_err_t *err)
{
    ZEROIZE(r);
    r->pddl = pddl;
    r->limit = *limit;
    r->err = err;

    ZEROIZE(&r->cfg);
    r->cfg.max_counted_vars = INT_MAX;
    r->cfg.refine_type_too_heavy_init = 1;
    r->cfg.refine_type_too_heavy_action = 1;
    r->cfg.refine_type_unbalanced_action = 1;
    r->cfg.refine_var_too_heavy_init = 1;
    r->cfg.refine_var_too_heavy_action = 1;
    r->cfg.refine_var_proved = 1;
    r->cfg.refine_var_proved_only_goal_aware = 1;
    r->cfg.refine_extend_proved = 1;

    pddlLiftedMGroupHTableInit(&r->mgroup);

    cand_t c;
    ZEROIZE(&c);
    r->cand = pddlExtArrNew(sizeof(c), NULL, &c);
    r->cand_size = 0;

    pddlFifoInit(&r->queue1, sizeof(int));
    pddlFifoInit(&r->queue2, sizeof(int));
}

static void refineInitMonotonicity(
                            refine_t *r,
                            const pddl_t *pddl,
                            const pddl_lifted_mgroups_infer_limits_t *limit,
                            pddl_err_t *err)
{
    refineInit(r, pddl, limit, err);
    ZEROIZE(&r->cfg);
    r->cfg.max_counted_vars = 1;
}

static void refineFree(refine_t *r)
{
    pddlLiftedMGroupHTableFree(&r->mgroup);
    pddlExtArrDel(r->cand);
    pddlFifoFree(&r->queue1);
    pddlFifoFree(&r->queue2);
}

static int refineCont(const refine_t *r)
{
    return !pddlFifoEmpty(&r->queue1) || !pddlFifoEmpty(&r->queue2);
}

static cand_t *refineNextCand(refine_t *r)
{
    int next = 0;
    if (!pddlFifoEmpty(&r->queue1)){
        next = *(int *)pddlFifoFront(&r->queue1);
        pddlFifoPop(&r->queue1);

    }else if (!pddlFifoEmpty(&r->queue2)){
        next = *(int *)pddlFifoFront(&r->queue2);
        pddlFifoPop(&r->queue2);

    }else{
        return NULL;
    }
    return pddlExtArrGet(r->cand, next);
}

static int eachPredOnlyOnce(const pddl_lifted_mgroup_t *m)
{
    for (int i = 0; i < m->cond.size; ++i){
        int p1 = PDDL_FM_CAST(m->cond.fm[i], atom)->pred;
        for (int j = i + 1; j < m->cond.size; ++j){
            int p2 = PDDL_FM_CAST(m->cond.fm[j], atom)->pred;
            if (p1 == p2)
                return 0;
        }
    }
    return 1;
}

static cand_t *_refineAddCand(refine_t *r,
                              const pddl_lifted_mgroup_t *m,
                              const cand_t *parent)
{
    if (r->cand_size >= r->limit.max_candidates)
        return NULL;

    int id = pddlLiftedMGroupHTableAdd(&r->mgroup, m);
    if (id >= r->cand_size){
        r->cand_size = id + 1;
        cand_t *cand = pddlExtArrGet(r->cand, id);
        ZEROIZE(cand);
        cand->id = id;
        cand->mgroup = pddlLiftedMGroupHTableGet(&r->mgroup, id);
        cand->refined_from = -1;
        if (parent != NULL)
            cand->refined_from = parent->id;
        ASSERT(!cand->refined_var);
        ASSERT(!cand->refined_type);
        ASSERT(!cand->refined_by_extend);
        cand->refined_by_extend_pred = -1;

        cand->each_pred_only_once = eachPredOnlyOnce(m);

        return cand;
    }else{
        ASSERT(((cand_t *)pddlExtArrGet(r->cand, id))->id == id);
        return NULL;
    }
}

static void refineAddCand(refine_t *r,
                          const pddl_lifted_mgroup_t *m,
                          const cand_t *parent)
{
    cand_t *c = _refineAddCand(r, m, parent);
    if (c != NULL)
        pddlFifoPush(&r->queue1, &c->id);
}

static void refineAddCandExtend(refine_t *r,
                                const pddl_lifted_mgroup_t *m,
                                const cand_t *parent,
                                int extend_pred)
{
    cand_t *c = _refineAddCand(r, m, parent);
    if (c != NULL){
        c->refined_by_extend = 1;
        c->refined_by_extend_pred = extend_pred;
        pddlFifoPush(&r->queue1, &c->id);
    }
}

static void refineAddCandType(refine_t *r,
                              const pddl_lifted_mgroup_t *m,
                              const cand_t *parent)
{
    cand_t *c = _refineAddCand(r, m, parent);
    if (c != NULL){
        c->refined_type = 1;
        pddlFifoPush(&r->queue2, &c->id);
    }
}

static void refineAddCandVar(refine_t *r,
                             const pddl_lifted_mgroup_t *m,
                             const cand_t *parent)
{
    cand_t *c = _refineAddCand(r, m, parent);
    if (c != NULL){
        c->refined_var = 1;
        pddlFifoPush(&r->queue2, &c->id);
    }
}

/** Restrict types of parameters so it is valid for all atoms. */
static void restrictParamTypes(const pddl_t *pddl, pddl_lifted_mgroup_t *mg)
{
    for (int ai = 0; ai < mg->cond.size; ++ai){
        const pddl_fm_atom_t *a = PDDL_FM_CAST(mg->cond.fm[ai], atom);
        const pddl_pred_t *pred = pddl->pred.pred + a->pred;

        for (int i = 0; i < a->arg_size; ++i){
            if (a->arg[i].param >= 0){
                int mg_type = mg->param.param[a->arg[i].param].type;
                int pred_type = pred->param[i];
                if (pred_type != mg_type
                        && pddlTypesIsParent(&pddl->type, pred_type, mg_type)){
                    mg->param.param[a->arg[i].param].type = pred_type;
                }

                ASSERT(!pddlTypesAreDisjunct(&pddl->type, pred_type, mg_type));
            }
        }
    }
}

/** Add a new candidate refined from the given candidate and atom with
 *  specified parameters (-1 means counted variable, >=0 is an ID of the
 *  parameter. */
static void addRefinedCandidate(refine_t *r,
                                const cand_t *cand_in,
                                const pddl_fm_atom_t *atom,
                                const int *atom_params)
{
    pddl_lifted_mgroup_t new_cand;
    pddl_fm_atom_t *new_atom;

    // Create a copy of the candidate
    pddlLiftedMGroupInitCopy(&new_cand, cand_in->mgroup);

    // Construct a new atom that will be added to the new candidate
    new_atom = pddlFmNewEmptyAtom(atom->arg_size);
    new_atom->pred = atom->pred;
    for (int i = 0; i < atom->arg_size; ++i){
        if (atom_params[i] < 0){
            new_atom->arg[i].param = new_cand.param.param_size;
            pddl_param_t *param = pddlParamsAdd(&new_cand.param);
            param->type = r->pddl->pred.pred[atom->pred].param[i];
            param->is_counted_var = 1;
        }else{
            int type_cand = new_cand.param.param[atom_params[i]].type;
            int type_atom = r->pddl->pred.pred[new_atom->pred].param[i];
            if (pddlTypesAreDisjunct(&r->pddl->type, type_cand, type_atom)){
                pddlLiftedMGroupFree(&new_cand);
                return;
            }
            new_cand.param.param[atom_params[i]].is_counted_var = 0;
            new_atom->arg[i].param = atom_params[i];
        }
    }
    pddlFmArrAdd(&new_cand.cond, &new_atom->fm);

    restrictParamTypes(r->pddl, &new_cand);
    mgroupFinalize(r->pddl, &new_cand);
    refineAddCandExtend(r, &new_cand, cand_in, new_atom->pred);
    pddlLiftedMGroupFree(&new_cand);
}

// Allow more than one counted variable
static void refineCandidateWithEff(refine_t *refine,
                                   const unify_action_ctx_t *ctx,
                                   const pddl_action_t *action,
                                   const cand_t *cand,
                                   const ce_atom_t *atom,
                                   int *atom_params,
                                   int atom_argi,
                                   int pre_test,
                                   int num_counted_vars,
                                   int max_counted_vars)
{
    if (atom_argi == atom->atom->arg_size){
        if (!pre_test
                || equalAtomIn(atom->atom, action->pre, ctx->action_arg)
                || equalAtomIn(atom->atom, atom->pre, ctx->action_arg)){
            addRefinedCandidate(refine, cand, atom->atom, atom_params);
        }
        return;
    }

    int atom_param = atom->atom->arg[atom_argi].param;
    int atom_obj = atom->atom->arg[atom_argi].obj;

    if (atom_param >= 0 && ctx->action_arg[atom_param] < 0){
        if (num_counted_vars < max_counted_vars){
            atom_params[atom_argi] = -1;
            UNIFY_ACTION_CTX_PUSH(ctx2, ctx);
            if (ctx->action_arg[atom_param] < 0)
                ctx2.action_arg[atom_param] = ctx2.next_name++;

            refineCandidateWithEff(refine, &ctx2, action, cand,
                                   atom, atom_params,
                                   atom_argi + 1,
                                   pre_test,
                                   num_counted_vars + 1,
                                   max_counted_vars);
        }

    }else{
        int arg = atom_obj;
        if (atom_param >= 0)
            arg = ctx->action_arg[atom_param];

        for (int ci = 0; ci < cand->mgroup->param.param_size; ++ci){
            if (ctx->cand_arg[ci] == arg){
                atom_params[atom_argi] = ci;
                refineCandidateWithEff(refine, ctx, action, cand,
                                       atom, atom_params,
                                       atom_argi + 1,
                                       pre_test,
                                       num_counted_vars,
                                       max_counted_vars);
            }
        }

        if (num_counted_vars < max_counted_vars){
            atom_params[atom_argi] = -1;
            refineCandidateWithEff(refine, ctx, action, cand,
                                   atom, atom_params,
                                   atom_argi + 1,
                                   pre_test,
                                   num_counted_vars + 1,
                                   max_counted_vars);
        }
    }
}

/** Refine candidate cand by extending it with more atoms so that add
 *  effects are balanced. */
static void refineExtend(refine_t *refine,
                         const unify_action_ctx_t *ctx,
                         const pddl_action_t *action,
                         const cand_t *cand)
{
    if (refine == NULL || refine->cand_size >= refine->limit.max_candidates)
        return;

    pddl_fm_const_it_eff_t it;
    //pddl_fm_const_it_atom_t it;
    const pddl_fm_atom_t *a;
    const pddl_fm_t *pre;

    PDDL_FM_FOR_EACH_DEL_EFF(action->eff, &it, a, pre){
        ASSERT(a->neg);
        if (!candHasPred(cand->mgroup, a->pred)){
            int del_eff_params[a->arg_size];
            CE_ATOM(ce_a, pre, a);
            refineCandidateWithEff(refine, ctx, action, cand, &ce_a,
                                   del_eff_params, 0, 1, 0,
                                   refine->cfg.max_counted_vars);
        }
    }
}

static void refineExtendProvedWithAddEff(refine_t *refine,
                                         const unify_action_ctx_t *ctx,
                                         const pddl_action_t *action,
                                         const cand_t *cand)
{
    pddl_fm_const_it_eff_t it;
    const pddl_fm_atom_t *a;
    const pddl_fm_t *pre;

    PDDL_FM_FOR_EACH_ADD_EFF(action->eff, &it, a, pre){
        ASSERT(!a->neg);
        CE_ATOM(ce_a, pre, a);
        int eff_params[a->arg_size];
        refineCandidateWithEff(refine, ctx, action, cand,
                               &ce_a, eff_params, 0, 0, 0,
                               refine->cfg.max_counted_vars);
    }
}

/** Refine proved candidate by extending it with an add effect */
static void refineExtendProved(refine_t *refine,
                               const pddl_action_t *action,
                               const cand_t *cand)
{
    if (refine == NULL || refine->cand_size >= refine->limit.max_candidates)
        return;

    pddl_fm_const_it_eff_t it;
    const pddl_fm_atom_t *a, *c;
    const pddl_fm_t *pre;

    PDDL_FM_FOR_EACH_DEL_EFF(action->eff, &it, a, pre){
        ASSERT(a->neg);
        CE_ATOM(ce_a, pre, a);

        FOR_EACH_ATOM(cand->mgroup, c){
            if (c->pred != a->pred)
                continue;
            UNIFY_ACTION_CTX(ctx, refine->pddl, &action->param,
                             &cand->mgroup->param);
            // If we are able to unify pre \cap del_eff, but add effect is
            // not covered by the mutex group, we can try to extend the
            // mutex group with add effects
            if (unifyActionEff(&ctx, action, &ce_a, c)
                    && (equalAtomIn(a, action->pre, ctx.action_arg)
                            || equalAtomIn(a, pre, ctx.action_arg))
                    && !candHasAddEff(cand, action->eff)){
                refineExtendProvedWithAddEff(refine, &ctx, action, cand);
            }
        }
    }
}

static void addCandidateWithChangedParamType(refine_t *refine,
                                             const cand_t *cand,
                                             int param,
                                             int type)
{
    pddl_lifted_mgroup_t new_cand;
    pddlLiftedMGroupInitCopy(&new_cand, cand->mgroup);
    new_cand.param.param[param].type = type;
    mgroupFinalize(refine->pddl, &new_cand);
    refineAddCandType(refine, &new_cand, cand);
    pddlLiftedMGroupFree(&new_cand);
}

static void refineParamTypesTree(refine_t *refine,
                                 const cand_t *cand,
                                 int param,
                                 int cand_type_id,
                                 int atom_type_id,
                                 int atom_parent_type_id)
{
    const pddl_types_t *ts = &refine->pddl->type;
    const pddl_type_t *atom_parent_type = ts->type + atom_parent_type_id;

    int tid;
    PDDL_ISET_FOR_EACH(&atom_parent_type->child, tid){
        if (tid == atom_type_id)
            continue;
        ASSERT(pddlTypesAreDisjunct(ts, tid, atom_type_id));
        addCandidateWithChangedParamType(refine, cand, param, tid);
    }

    if (atom_parent_type_id != cand_type_id){
        refineParamTypesTree(refine, cand, param, cand_type_id,
                             atom_parent_type_id, atom_parent_type->parent);
    }
}

static void refineParamTypes(refine_t *refine,
                             const cand_t *cand,
                             int param,
                             int cand_type_id,
                             int atom_type_id)
{
    const pddl_types_t *ts = &refine->pddl->type;
    const pddl_type_t *cand_type = ts->type + cand_type_id;
    const pddl_type_t *atom_type = ts->type + atom_type_id;

    if (cand_type_id == atom_type_id)
        return;

    if (pddlTypesIsEither(ts, cand_type_id)){
        int tid;
        PDDL_ISET_FOR_EACH(&cand_type->either, tid)
            refineParamTypes(refine, cand, param, tid, atom_type_id);
        return;
    }

    if (pddlTypesIsEither(ts, atom_type_id)){
        int tid;
        PDDL_ISET_FOR_EACH(&atom_type->either, tid)
            refineParamTypes(refine, cand, param, cand_type_id, tid);
        return;
    }

    if (!pddlTypesIsParent(ts, atom_type_id, cand_type_id)){
        addCandidateWithChangedParamType(refine, cand, param, cand_type_id);
        return;
    }

    refineParamTypesTree(refine, cand, param, cand_type_id,
                         atom_type_id, atom_type->parent);
}

/** Refine candidate cand by changing types of candidate variables so that
 *  atom and cand_atom cannot be unified. */
static void refineTypes(refine_t *refine,
                        const pddl_params_t *params,
                        const pddl_fm_atom_t *atom,
                        const cand_t *cand,
                        const pddl_fm_atom_t *cand_atom)
{
    if (refine == NULL || refine->cand_size >= refine->limit.max_candidates)
        return;

    const pddl_types_t *ts = &refine->pddl->type;
    for (int argi = 0; argi < atom->arg_size; ++argi){
        if (cand_atom->arg[argi].obj >= 0)
            continue;

        int pred_type = refine->pddl->pred.pred[atom->pred].param[argi];

        int cparam = cand_atom->arg[argi].param;
        int ctype = cand->mgroup->param.param[cparam].type;
        if (pred_type != ctype && pddlTypesIsParent(ts, pred_type, ctype))
            ctype = pred_type;

        int aparam = atom->arg[argi].param;
        pddl_obj_id_t aobj = atom->arg[argi].obj;
        int atype = -1;
        if (aparam >= 0){
            atype = params->param[aparam].type;
        }else{
            atype = refine->pddl->obj.obj[aobj].type;
        }

        if (atype != ctype)
            refineParamTypes(refine, cand, cparam, ctype, atype);

        if (aobj >= 0){
            int tid;
            PDDL_ISET_FOR_EACH(&ts->type[atype].child, tid){
                if (!pddlTypesObjHasType(ts, tid, aobj))
                    addCandidateWithChangedParamType(refine, cand, cparam, tid);
            }
        }
    }
}

static void countedVariables(const pddl_lifted_mgroup_t *cand,
                             const pddl_fm_atom_t *atom,
                             pddl_iset_t *vars)
{
    for (int i = 0; i < atom->arg_size; ++i){
        if (atom->arg[i].param >= 0
                && cand->param.param[atom->arg[i].param].is_counted_var){
            pddlISetAdd(vars, atom->arg[i].param);
        }
    }
}

/** Refine candidate cand by changing counted variables to non-counted
 *  variables so that a1 and a2 cannot be unified with cand_atom1 and
 *  cand_atom2. */
static void refineVariables(refine_t *refine,
                            const pddl_fm_atom_t *a1,
                            const pddl_fm_atom_t *a2,
                            const cand_t *cand,
                            const pddl_fm_atom_t *cand_atom1,
                            const pddl_fm_atom_t *cand_atom2)
{
    // TODO: Add variable -> object refinement
    if (refine == NULL || refine->cand_size >= refine->limit.max_candidates)
        return;

    PDDL_ISET(relevant_params);

    // Collect counted variables present in both cand_atom1 and cand_atom2
    PDDL_ISET(counted_vars2);
    countedVariables(cand->mgroup, cand_atom1, &relevant_params);
    countedVariables(cand->mgroup, cand_atom2, &counted_vars2);
    pddlISetIntersect(&relevant_params, &counted_vars2);
    pddlISetFree(&counted_vars2);

    // If a1 and a2 differ in a argument corresonding to counted variable,
    // then we can try to change this variable to non-counted variable
    int counted_var;
    PDDL_ISET_FOR_EACH(&relevant_params, counted_var){
        for (int ai1 = 0; ai1 < cand_atom1->arg_size; ++ai1){
            if (cand_atom1->arg[ai1].param != counted_var)
                continue;
            for (int ai2 = 0; ai2 < cand_atom2->arg_size; ++ai2){
                if (cand_atom2->arg[ai2].param != counted_var)
                    continue;

                if (a1->arg[ai1].obj != a2->arg[ai2].obj
                        || a1->arg[ai1].param != a2->arg[ai2].param){
                    pddl_lifted_mgroup_t new_cand;
                    pddlLiftedMGroupInitCopy(&new_cand, cand->mgroup);
                    ASSERT(new_cand.param.param[counted_var].is_counted_var);
                    new_cand.param.param[counted_var].is_counted_var = 0;
                    mgroupFinalize(refine->pddl, &new_cand);
                    refineAddCandVar(refine, &new_cand, cand);
                    pddlLiftedMGroupFree(&new_cand);
                }
            }
        }
    }

    pddlISetFree(&relevant_params);
}

static void _refineVariablesProved(refine_t *refine,
                                   const cand_t *cand,
                                   int var,
                                   pddl_lifted_mgroups_t *lm)
{
    for (; var < cand->mgroup->param.param_size
            && !cand->mgroup->param.param[var].is_counted_var; ++var);

    if (var == cand->mgroup->param.param_size){
        if (isInitExactlyOne(refine->pddl, cand, NULL)
                && !isAnyActionTooHeavy(refine->pddl, cand, NULL)
                && !isAnyActionUnbalanced(refine->pddl, cand, NULL)
                && (!refine->cfg.refine_var_proved_only_goal_aware
                        || isGoalAware(refine->pddl, cand->mgroup))){
            addProvedLiftedMGroup(refine->pddl, cand->mgroup, lm);
        }
    }else{
        pddl_lifted_mgroup_t mg;
        pddlLiftedMGroupInitCopy(&mg, cand->mgroup);
        ASSERT(mg.param.param[var].is_counted_var);
        mg.param.param[var].is_counted_var = 0;
        CAND_LOCAL(next_cand, &mg);
        _refineVariablesProved(refine, &next_cand, var + 1, lm);
        pddlLiftedMGroupFree(&mg);

        _refineVariablesProved(refine, cand, var + 1, lm);
    }
}

/** Refine variables for proved candidate, i.e., it tries to find valid
 *  subsets that are fam-groups. **/
static void refineVariablesProved(refine_t *refine,
                                  const cand_t *cand,
                                  pddl_lifted_mgroups_t *lm)
{
    if (cand->mgroup->param.param_size > 0
            && candHasCountedVar(cand->mgroup)){
        _refineVariablesProved(refine, cand, 0, lm);
    }
}

static void refineTooHeavyInit(refine_t *refine,
                               const pddl_params_t *params,
                               const pddl_fm_atom_t *a1,
                               const pddl_fm_atom_t *a2,
                               const cand_t *cand,
                               const pddl_fm_atom_t *cand_atom1,
                               const pddl_fm_atom_t *cand_atom2)
{
    if (refine == NULL || refine->cand_size >= refine->limit.max_candidates)
        return;

    if (refine->cfg.refine_type_too_heavy_init){
        refineTypes(refine, params, a1, cand, cand_atom1);
        refineTypes(refine, params, a2, cand, cand_atom2);
    }
    if (refine->cfg.refine_var_too_heavy_init)
        refineVariables(refine, a1, a2, cand, cand_atom1, cand_atom2);
}

static void refineTooHeavyAction(refine_t *refine,
                                 const pddl_params_t *params,
                                 const pddl_fm_atom_t *a1,
                                 const pddl_fm_atom_t *a2,
                                 const cand_t *cand,
                                 const pddl_fm_atom_t *cand_atom1,
                                 const pddl_fm_atom_t *cand_atom2)
{
    if (refine == NULL || refine->cand_size >= refine->limit.max_candidates)
        return;

    if (refine->cfg.refine_type_too_heavy_action){
        refineTypes(refine, params, a1, cand, cand_atom1);
        refineTypes(refine, params, a2, cand, cand_atom2);
    }
    if (refine->cfg.refine_var_too_heavy_action)
        refineVariables(refine, a1, a2, cand, cand_atom1, cand_atom2);
}

static void refineUnbalancedAction(refine_t *refine,
                                   const unify_action_ctx_t *ctx,
                                   const pddl_action_t *action,
                                   const pddl_fm_atom_t *add_eff,
                                   const cand_t *cand,
                                   const pddl_fm_atom_t *cand_add_eff)
{
    if (refine == NULL || refine->cand_size >= refine->limit.max_candidates)
        return;

    refineExtend(refine, ctx, action, cand);
    if (refine->cfg.refine_type_unbalanced_action)
        refineTypes(refine, &action->param, add_eff, cand, cand_add_eff);
}

static void refineProved(refine_t *refine,
                         const cand_t *cand,
                         pddl_lifted_mgroups_t *mgroups)
{
    if (refine->cfg.refine_var_proved){
        if (!refine->cfg.refine_var_proved_only_goal_aware
                || isGoalAware(refine->pddl, cand->mgroup)){
            refineVariablesProved(refine, cand, mgroups);
        }
    }
    if (refine->cfg.refine_extend_proved){
        for (int ai = 0; ai < refine->pddl->action.action_size; ++ai){
            const pddl_action_t *a = refine->pddl->action.action + ai;
            refineExtendProved(refine, a, cand);
        }
    }
}



/*** PARTIAL INSTANTIATION ***/
/** Set all parameters param to object obj and removes parameter param **/
static void candInstantiateParamWithObj(pddl_lifted_mgroup_t *dst,
                                        const pddl_lifted_mgroup_t *src,
                                        int param,
                                        pddl_obj_id_t obj)
{
    pddlLiftedMGroupInitCopy(dst, src);
    for (int ci = 0; ci < dst->cond.size; ++ci){
        pddl_fm_atom_t *a = PDDL_FM_CAST(dst->cond.fm[ci], atom);
        for (int i = 0; i < a->arg_size; ++i){
            if (a->arg[i].param == param){
                a->arg[i].param = -1;
                a->arg[i].obj = obj;
            }else if (a->arg[i].param > param){
                --a->arg[i].param;
            }
        }
    }

    // shift parameters
    for (int pi = param + 1; pi < dst->param.param_size; ++pi)
        dst->param.param[pi - 1] = dst->param.param[pi];
    --dst->param.param_size;
}

static void _removeHeavinessByInst(const pddl_t *pddl,
                                   const cand_t *cand,
                                   int param,
                                   pddl_lifted_mgroups_t *lm)
{
    ASSERT(!cand->mgroup->param.param[param].is_counted_var);
    ASSERT(cand->mgroup->param.param[param].type >= 0);
    const pddl_obj_id_t *obj;
    int obj_size;

    int type = cand->mgroup->param.param[param].type;
    obj = pddlTypesObjsByType(&pddl->type, type, &obj_size);
    for (int oi = 0; oi < obj_size; ++oi){
        pddl_lifted_mgroup_t new_mg;
        candInstantiateParamWithObj(&new_mg, cand->mgroup, param, obj[oi]);
        CAND_LOCAL(new_cand, &new_mg);


        if (isInitExactlyOne(pddl, &new_cand, NULL)){
            addProvedLiftedMGroup(pddl, &new_mg, lm);
        }else{
            for (int next = param; next < new_mg.param.param_size; ++next){
                if (!new_mg.param.param[next].is_counted_var)
                    _removeHeavinessByInst(pddl, &new_cand, next, lm);
            }
        }

        pddlLiftedMGroupFree(&new_mg);
    }
}

/** Try to instantiate some non-counted variables in cand to have at most
 *  one matching atom in conj. Successfuly instantiate mgroups are added to
 *  mgroup. */
static void removeHeavinessByInst(const pddl_t *pddl,
                                  const cand_t *cand,
                                  pddl_lifted_mgroups_t *mgroup)
{
    for (int i = 0; i < cand->mgroup->param.param_size; ++i){
        if (!cand->mgroup->param.param[i].is_counted_var)
            _removeHeavinessByInst(pddl, cand, i, mgroup);
    }
}

static void initialCandidatesAllVarsCounted(const pddl_t *pddl,
                                            refine_t *refine)
{
    for (int pred_id = 0; pred_id < pddl->pred.pred_size; ++pred_id){
        const pddl_pred_t *pred = pddl->pred.pred + pred_id;
        if (pddlPredIsStatic(pred) || pred_id == pddl->pred.eq_pred)
            continue;

        pddl_lifted_mgroup_t m;

        pddlLiftedMGroupInitCandFromPred(&m, pred, -1);
        for (int i = 0; i < m.param.param_size; ++i)
            m.param.param[i].is_counted_var = 1;
        mgroupFinalize(pddl, &m);
        refineAddCand(refine, &m, NULL);
        pddlLiftedMGroupFree(&m);
    }

    LOG(refine->err, "  %{num_initial_candidates}d initial candidates.",
        refine->cand_size);
}

static void initialCandidatesFD(const pddl_t *pddl, refine_t *refine)
{
    for (int pred_id = 0; pred_id < pddl->pred.pred_size; ++pred_id){
        const pddl_pred_t *pred = pddl->pred.pred + pred_id;
        if (pddlPredIsStatic(pred) || pred_id == pddl->pred.eq_pred)
            continue;

        pddl_lifted_mgroup_t m;

        pddlLiftedMGroupInitCandFromPred(&m, pred, -1);
        mgroupFinalize(pddl, &m);
        refineAddCand(refine, &m, NULL);
        pddlLiftedMGroupFree(&m);

        for (int i = 0; i < pred->param_size; ++i){
            pddlLiftedMGroupInitCandFromPred(&m, pred, i);
            mgroupFinalize(pddl, &m);
            refineAddCand(refine, &m, NULL);
            pddlLiftedMGroupFree(&m);
        }
    }

    LOG(refine->err, "  %{num_initial_candidates}d initial candidates.",
        refine->cand_size);
}







/*** PUBLIC API: ***/
int pddlLiftedMGroupIsInitTooHeavy(const pddl_lifted_mgroup_t *_cand,
                                   const pddl_t *pddl)
{
    CAND_LOCAL(cand, _cand);
    return isInitTooHeavy(pddl, &cand, NULL);
}

int pddlLiftedMGroupIsActionTooHeavy(const pddl_lifted_mgroup_t *_cand,
                                     const pddl_t *pddl,
                                     int action_id)
{
    CAND_LOCAL(cand, _cand);
    return isActionTooHeavy(&cand, pddl, pddl->action.action + action_id, NULL);
}

int pddlLiftedMGroupIsActionBalanced(const pddl_lifted_mgroup_t *_cand,
                                     const pddl_t *pddl,
                                     int action_id)
{
    CAND_LOCAL(cand, _cand);
    return isActionBalanced(&cand, pddl, pddl->action.action + action_id, NULL);
}


void pddlLiftedMGroupsExtractGoalAware(pddl_lifted_mgroups_t *dst,
                                       const pddl_lifted_mgroups_t *src,
                                       const pddl_t *pddl)
{

    for (int i = 0; i < src->mgroup_size; ++i){
        const pddl_lifted_mgroup_t *mg = src->mgroup + i;
        pddl_obj_id_t arg[mg->param.param_size];

        pddl_fm_const_it_atom_t it;
        const pddl_fm_atom_t *goal;
        PDDL_FM_FOR_EACH_ATOM(pddl->goal, &it, goal){
            ASSERT(!goal->neg);
            const pddl_fm_atom_t *c;
            FOR_EACH_ATOM(mg, c){
                if (c->pred != goal->pred)
                    continue;

                if (unifyFact(pddl, goal, NULL, &mg->param, c, arg))
                    pddlLiftedMGroupsAddInst(dst, mg, arg);
            }
        }
    }
    pddlLiftedMGroupsSortAndUniq(dst);
}

int pddlLiftedMGroupsIsGroundedConjTooHeavy(const pddl_lifted_mgroups_t *mgs,
                                            const pddl_t *pddl,
                                            const pddl_fm_arr_t *c,
                                            const pddl_obj_id_t *args)
{
    for (int i = 0; i < mgs->mgroup_size; ++i){
        CAND_LOCAL(cand, mgs->mgroup + i);
        if (isGroundedCondArrTooHeavy(&cand, pddl, c, args))
            return 1;
    }
    return 0;
}

static int mgroupIsDeleted(const pddl_lifted_mgroup_t *mg,
                           const pddl_t *pddl,
                           const pddl_fm_arr_t *pre,
                           const pddl_fm_arr_t *add_eff,
                           const pddl_fm_arr_t *del_eff,
                           const pddl_obj_id_t *args)
{
    pddl_obj_id_t mg_arg[mg->param.param_size];

    // First check whether there is a matching add effect. If there is one,
    // then mg cannot be deleted
    for (int addi = 0; addi < add_eff->size; ++addi){
        const pddl_fm_atom_t *a = PDDL_FM_CAST(add_eff->fm[addi], atom);
        const pddl_fm_atom_t *m;
        FOR_EACH_ATOM(mg, m){
            if (m->pred != a->pred)
                continue;
            if (unifyFact(pddl, a, args, &mg->param, m, mg_arg))
                return 0;
        }
    }

    // Then find out if there is a matching delete effect and precondition
    for (int di = 0; di < del_eff->size; ++di){
        const pddl_fm_atom_t *d = PDDL_FM_CAST(del_eff->fm[di], atom);
        const pddl_fm_atom_t *m;
        FOR_EACH_ATOM(mg, m){
            if (m->pred != d->pred)
                continue;
            if (unifyFact(pddl, d, args, &mg->param, m, mg_arg)
                    && equalAtomInArrObj(d, pre, args)){
                return 1;
            }
        }
    }
    return 0;
}

int pddlLiftedMGroupsAnyIsDeleted(const pddl_lifted_mgroups_t *mgs,
                                  const pddl_t *pddl,
                                  const pddl_fm_arr_t *pre,
                                  const pddl_fm_arr_t *add_eff,
                                  const pddl_fm_arr_t *del_eff,
                                  const pddl_obj_id_t *args)
{
    for (int i = 0; i < mgs->mgroup_size; ++i){
        if (mgroupIsDeleted(mgs->mgroup + i, pddl,
                            pre, add_eff, del_eff, args)){
            return 1;
        }
    }
    return 0;
}


void pddlLiftedMGroupsInferFAMGroups(
                            const pddl_t *pddl,
                            const pddl_lifted_mgroups_infer_limits_t *limit,
                            pddl_lifted_mgroups_t *mgroups,
                            pddl_err_t *err)
{
    CTX(err, "lmg_fam_group", "LMG-FAM-Group");
    int steps = 0;
    int tested_candidates = 0;
    refine_t refine;

    LOG(err, "limit.max_candidates = %{limit_max_candidates}d", limit->max_candidates);
    LOG(err, "limit.max_mgroups = %{limit_max_mgroups}d", limit->max_mgroups);
    LOG(err, "Inference of lifted fam-groups ...");

    refineInit(&refine, pddl, limit, err);

    initialCandidatesAllVarsCounted(pddl, &refine);
    while (refineCont(&refine) && mgroups->mgroup_size < limit->max_mgroups){
        cand_t *cand = refineNextCand(&refine);
        if (isInitExactlyOne(pddl, cand, &refine)
                && !isAnyActionTooHeavy(pddl, cand, &refine)
                && !isAnyActionUnbalanced(pddl, cand, &refine)){

            addProvedLiftedMGroup(pddl, cand->mgroup, mgroups);
            refineProved(&refine, cand, mgroups);
        }

        ++tested_candidates;
        if (++steps == limit->max_candidates / 10){
            LOG(err, "Tested candidates: %{progress_tested_candidates}d,"
                " Num candidates: %{progress_num_candidates}d,"
                " Proved: %{progress_proved_mgroups}d",
                tested_candidates,
                refine.cand_size,
                mgroups->mgroup_size);
            steps = 0;
        }
    }

    if (steps != 0){
        LOG(err, "Tested candidates: %{num_tested_candidates}d,"
            " Num candidates: %{num_candidates}d,"
            " Proved: %{num_mgroups}d",
            tested_candidates,
            refine.cand_size,
            mgroups->mgroup_size);
    }

    pddlLiftedMGroupsSortAndUniq(mgroups);
    refineFree(&refine);

    LOG(err, "Inference of lifted fam-groups done."
        " Found mutex groups: %{found_mgroups}d",
        mgroups->mgroup_size);
    CTXEND(err);
}


void pddlLiftedMGroupsInferMonotonicity(const pddl_t *pddl,
                                        const pddl_lifted_mgroups_infer_limits_t *limit,
                                        pddl_lifted_mgroups_t *inv,
                                        pddl_lifted_mgroups_t *mgroups,
                                        pddl_err_t *err)
{
    CTX(err, "lmg_monotonicity", "LMG-Monotonicity");
    int steps = 0;
    int tested_candidates = 0;
    refine_t refine;

    LOG(err, "limit.max_candidates = %{limit_max_candidates}d", limit->max_candidates);
    LOG(err, "limit.max_mgroups = %{limit_max_mgroups}d", limit->max_mgroups);
    LOG(err, "Inference of FD lifted mgroups ...");

    refineInitMonotonicity(&refine, pddl, limit, err);

    initialCandidatesFD(pddl, &refine);
    while (refineCont(&refine)
            && (mgroups == NULL || mgroups->mgroup_size < limit->max_mgroups)){
        cand_t *cand = refineNextCand(&refine);
        if (!isAnyActionTooHeavy(pddl, cand, NULL)
                && !isAnyActionUnbalanced(pddl, cand, &refine)){

            if (inv != NULL)
                pddlLiftedMGroupsAdd(inv, cand->mgroup);

            if (mgroups != NULL){
                if (isInitTooHeavy(pddl, cand, NULL)){
                    removeHeavinessByInst(pddl, cand, mgroups);
                }else if (isInitExactlyOne(pddl, cand, NULL)){
                    addProvedLiftedMGroup(pddl, cand->mgroup, mgroups);
                }
            }
        }

        ++tested_candidates;
        if (++steps == limit->max_candidates / 10){
            LOG(err, "Tested candidates: %{progress_tested_candidates}d,"
                " Num candidates: %{progress_num_candidates}d,"
                " Proved monotonicity invariants: %{progress_proved_monotonicity_invariants}d,"
                " mutex groups: %{progress_proved_mgroups}d",
                tested_candidates,
                refine.cand_size,
                (inv != NULL ? inv->mgroup_size : -1),
                (mgroups != NULL ? mgroups->mgroup_size : -1));
            steps = 0;
        }
    }

    if (steps != 0){
        LOG(err, "Tested candidates: %{num_tested_candidates}d,"
            " Num candidates: %{num_candidates}d,"
            " Proved monotonicity invariants: %{num_monotonicity_invariants}d,"
            " mutex groups: %{num_mgroups}d",
            tested_candidates,
            refine.cand_size,
            (inv != NULL ? inv->mgroup_size : -1),
            (mgroups != NULL ? mgroups->mgroup_size : -1));
    }

    if (inv != NULL)
        pddlLiftedMGroupsSortAndUniq(inv);
    if (mgroups != NULL)
        pddlLiftedMGroupsSortAndUniq(mgroups);
    refineFree(&refine);

    LOG(err, "Inference of FD lifted mgroups done."
        " Found monotonicity invariants: %{found_monotonicity_invariants}d,"
        " mutex groups: %{found_mgroups}d",
        (inv != NULL ? inv->mgroup_size : -1),
        (mgroups != NULL ? mgroups->mgroup_size : -1));
    CTXEND(err);
}


/** Returns true if the delete effect is always balanced by add effect */
static int isDelEffBalanced(const unify_action_ctx_t *ctx,
                            const pddl_action_t *action,
                            const ce_atom_t *del_eff,
                            const pddl_lifted_mgroup_t *mgroup)
{
    pddl_fm_const_it_eff_t it;
    const pddl_fm_atom_t *a;
    const pddl_fm_t *pre;
    PDDL_FM_FOR_EACH_ADD_EFF(action->eff, &it, a, pre){
        ASSERT(!a->neg);
        CE_ATOM(add_eff, pre, a);

        const pddl_fm_atom_t *cand_atom;
        FOR_EACH_ATOM(mgroup, cand_atom){
            if (cand_atom->pred != a->pred)
                continue;
            if (canUnifyEff(ctx, action, &add_eff, cand_atom, 0))
                return 1;
        }
    }

    return 0;
}

/** Returns true if the action can delete a fact from the mgroup without
 *  adding other. */
static int actionMayDeleteMGroup(const pddl_t *pddl,
                                 const pddl_action_t *action,
                                 const pddl_lifted_mgroup_t *mgroup,
                                 pddl_err_t *err)
{
    pddl_fm_const_it_eff_t it;
    const pddl_fm_atom_t *d;
    const pddl_fm_t *pre;
    PDDL_FM_FOR_EACH_DEL_EFF(action->eff, &it, d, pre){
        ASSERT(d->neg);
        CE_ATOM(del_eff, pre, d);

        const pddl_fm_atom_t *cand_atom;
        FOR_EACH_ATOM(mgroup, cand_atom){
            if (cand_atom->pred != d->pred)
                continue;
            UNIFY_ACTION_CTX(ctx, pddl, &action->param, &mgroup->param);
            if (unifyActionEff(&ctx, action, &del_eff, cand_atom)){
                if (!isDelEffBalanced(&ctx, action, &del_eff, mgroup)){
                    //fprintf(stderr, "XX %s\n", action->name);
                    return 1;
                }
            }
        }
    }

    return 0;
}

/** Returns true if lmg is exactly-one lifted mgroup. */
static int isMGroupSetExactlyOne(const pddl_t *pddl,
                                 const pddl_lifted_mgroup_t *lmg,
                                 pddl_err_t *err)
{
    //fprintf(stderr, "is-exactly-one? ");
    //pddlLiftedMGroupPrint(pddl, lmg, stderr);
    for (int ai = 0; ai < pddl->action.action_size; ++ai){
        const pddl_action_t *action = pddl->action.action + ai;
        if (actionMayDeleteMGroup(pddl, action, lmg, err))
            return 0;
    }

    return 1;
}

int pddlLiftedMGroupsSetExactlyOne(const pddl_t *pddl,
                                   pddl_lifted_mgroups_t *lm,
                                   pddl_err_t *err)
{
    CTX(err, "lmg_set_exactly_one", "LMG-Set-Exactly-One");
    LOG(err, "Looking for exactly-one lifted mgroups ...");
    int num = 0;
    for (int mi = 0; mi < lm->mgroup_size; ++mi){
        pddl_lifted_mgroup_t *lmg = lm->mgroup + mi;
        if (isMGroupSetExactlyOne(pddl, lmg, err)){
            lmg->is_exactly_one = 1;
            ++num;
        }
    }
    LOG(err, "Found %{num_exactly_one}d exactly-one lifted mgroups.", num);
    CTXEND(err);
    return 0;
}

/** If there is no unifiable delete effect, then the mutex group is static */
static int isMGroupStatic(const pddl_t *pddl,
                          const pddl_lifted_mgroup_t *mgroup,
                          pddl_err_t *err)
{
    //fprintf(stderr, "is-static? ");
    //pddlLiftedMGroupPrint(pddl, mgroup, stderr);
    for (int ai = 0; ai < pddl->action.action_size; ++ai){
        const pddl_action_t *action = pddl->action.action + ai;

        pddl_fm_const_it_eff_t it;
        const pddl_fm_atom_t *d;
        const pddl_fm_t *pre;
        PDDL_FM_FOR_EACH_DEL_EFF(action->eff, &it, d, pre){
            CE_ATOM(del_eff, pre, d);

            const pddl_fm_atom_t *cand_atom;
            FOR_EACH_ATOM(mgroup, cand_atom){
                if (cand_atom->pred != d->pred)
                    continue;
                UNIFY_ACTION_CTX(ctx, pddl, &action->param, &mgroup->param);
                if (unifyActionEff(&ctx, action, &del_eff, cand_atom))
                    return 0;
            }
        }
    }

    return 1;
}

int pddlLiftedMGroupsSetStatic(const pddl_t *pddl,
                               pddl_lifted_mgroups_t *lm,
                               pddl_err_t *err)
{
    CTX(err, "lmg_set_static", "LMG-Set-Static");
    LOG(err, "Looking for static lifted mgroups ...");
    int num = 0;
    for (int mi = 0; mi < lm->mgroup_size; ++mi){
        pddl_lifted_mgroup_t *lmg = lm->mgroup + mi;
        if (isMGroupStatic(pddl, lmg, err)){
            lmg->is_static = 1;
            ++num;
        }
    }
    LOG(err, "Found %{num_static}d static lifted mgroups.", num);
    CTXEND(err);
    return 0;
}
