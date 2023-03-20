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
#include "pddl/sort.h"
#include "pddl/pddl.h"
#include "pddl/strips_ground.h"

struct pddl_strips_ground_atree {
    const pddl_prep_action_t *action;
    pddl_strips_ground_tree_t *tree;
    int tree_size;
};

static void atreeInit(pddl_strips_ground_atree_t *ga,
                      const pddl_t *pddl,
                      const pddl_prep_action_t *a);
static void atreeFree(pddl_strips_ground_atree_t *ga);
static void atreeBlockStatic(pddl_strips_ground_atree_t *atr);
static void atreeUnifyFact(pddl_strips_ground_t *g,
                           pddl_strips_ground_atree_t *ga,
                           const pddl_ground_atom_t *fact,
                           int static_fact);


struct pddl_strips_ground_args {
    pddl_obj_id_t *arg;
    int action_id;
    const pddl_prep_action_t *action;
    int op_id;
};

static void groundArgsFree(pddl_strips_ground_args_arr_t *ga);
static void groundArgsAdd(pddl_strips_ground_args_arr_t *ga, int action_id,
                          const pddl_prep_action_t *action,
                          const pddl_obj_id_t *arg);
static void groundArgsSortAndUniq(pddl_strips_ground_args_arr_t *ga,
                                  pddl_err_t *err);

static int unifyStaticFacts(pddl_strips_ground_t *g);
static int unifyFacts(pddl_strips_ground_t *g);

static int groundActions(pddl_strips_ground_t *g, pddl_strips_t *strips);
static void groundActionAddEff(pddl_strips_ground_t *g,
                               const pddl_prep_action_t *a,
                               const pddl_obj_id_t *oarg);
static void groundActionAddEffEmptyPre(pddl_strips_ground_t *g,
                                       const pddl_prep_action_t *a);
static char *groundOpName(const pddl_t *pddl,
                          const pddl_action_t *action,
                          const pddl_obj_id_t *args);


/*** atree_t ***/
static int atomHasParam(const pddl_fm_atom_t *a, const pddl_iset_t *param)
{
    for (int i = 0; i < a->arg_size; ++i){
        if (a->arg[i].param >= 0 && pddlISetIn(a->arg[i].param, param))
            return 1;
    }
    return 0;
}

static int preHasParam(const pddl_prep_action_t *a, const pddl_iset_t *param)
{
    for (int i = 0; i < a->pre.size; ++i){
        const pddl_fm_atom_t *atom = PDDL_FM_CAST(a->pre.fm[i], atom);
        if (atomHasParam(atom, param))
            return 1;
    }
    return 0;
}

static void atomAddParam(const pddl_fm_atom_t *a, pddl_iset_t *param)
{
    for (int i = 0; i < a->arg_size; ++i){
        if (a->arg[i].param >= 0)
           pddlISetAdd(param, a->arg[i].param);
    }
}

static void atreeFindConnectedPreParams(const pddl_prep_action_t *a,
                                        pddl_iset_t *param,
                                        const pddl_iset_t *used_param)
{
    pddlISetEmpty(param);
    int first_param;
    for (first_param = 0;
            first_param < a->param_size && pddlISetIn(first_param, used_param);
            ++first_param);
    if (first_param == a->param_size)
        return;

    pddlISetAdd(param, first_param);

    int used_cond[a->pre.size];
    ZEROIZE_ARR(used_cond, a->pre.size);

    int changed = 1;
    while (changed){
        changed = 0;
        for (int i = 0; i < a->pre.size; ++i){
            if (used_cond[i])
                continue;

            const pddl_fm_atom_t *atom = PDDL_FM_CAST(a->pre.fm[i], atom);
            if (atomHasParam(atom, param)){
                used_cond[i] = 1;
                changed = 1;
                atomAddParam(atom, param);
            }
        }
    }
}

static void atreeInit(pddl_strips_ground_atree_t *atr,
                      const pddl_t *pddl,
                      const pddl_prep_action_t *a)
{
    ZEROIZE(atr);
    atr->action = a;

    PDDL_ISET(param_used);
    pddl_iset_t params[a->param_size];
    while (pddlISetSize(&param_used) < a->param_size){
        pddlISetInit(params + atr->tree_size);
        atreeFindConnectedPreParams(a, params + atr->tree_size, &param_used);
        pddlISetUnion(&param_used, params + atr->tree_size);

        if (preHasParam(a, params + atr->tree_size)){
            ++atr->tree_size;
        }else{
            pddlISetFree(params + atr->tree_size);
        }
    }
    pddlISetFree(&param_used);

    if (atr->tree_size > 0){
        atr->tree = CALLOC_ARR(pddl_strips_ground_tree_t, atr->tree_size);
        for (int i = 0; i < atr->tree_size; ++i)
            pddlStripsGroundTreeInit(atr->tree + i, pddl, a, params + i);
        for (int i = 0; i < atr->tree_size; ++i)
            pddlISetFree(params + i);

    }else{
        atr->tree_size = 1;
        atr->tree = ALLOC(pddl_strips_ground_tree_t);

        PDDL_ISET(params);
        pddlStripsGroundTreeInit(atr->tree, pddl, a, &params);
        pddlISetFree(&params);
    }
}

static void atreeFree(pddl_strips_ground_atree_t *ga)
{
    for (int i = 0; i < ga->tree_size; ++i)
        pddlStripsGroundTreeFree(ga->tree + i);
    if (ga->tree != NULL)
        FREE(ga->tree);
}

static void atreeBlockStatic(pddl_strips_ground_atree_t *atr)
{
    for (int ti = 0; ti < atr->tree_size; ++ti)
        pddlStripsGroundTreeBlockStatic(atr->tree + ti);
}

static int atreeAllTreesNonEmpty(const pddl_strips_ground_atree_t *atr)
{
    for (int ti = 0; ti < atr->tree_size; ++ti){
        const pddl_strips_ground_tree_t *tr = atr->tree + ti;
        if (pddlActionArgsSize(&tr->args) == 0)
            return 0;
    }
    return 1;
}

static void _atreeActionAddEff(pddl_strips_ground_t *g,
                               const pddl_strips_ground_atree_t *atr,
                               int skip_tree_id,
                               const pddl_obj_id_t *args_in,
                               int tree_id)
{
    if (tree_id == skip_tree_id)
        ++tree_id;
    if (tree_id >= atr->tree_size){
        groundActionAddEff(g, atr->action, args_in);
        return;
    }

    const pddl_strips_ground_tree_t *tr = atr->tree + tree_id;
    int size = pddlActionArgsSize(&tr->args);
    for (int argi = 0; argi < size; ++argi){
        const pddl_obj_id_t *tr_args = pddlActionArgsGet(&tr->args, argi);

        pddl_obj_id_t args[atr->action->param_size];
        memcpy(args, args_in, sizeof(pddl_obj_id_t) * atr->action->param_size);

        for (int i = 0; i < atr->action->param_size; ++i){
            if (tr_args[i] != PDDL_OBJ_ID_UNDEF){
                ASSERT(args[i] == PDDL_OBJ_ID_UNDEF);
                args[i] = tr_args[i];
            }
        }
        _atreeActionAddEff(g, atr, skip_tree_id, args, tree_id + 1);
    }
}

static void atreeActionAddEff(pddl_strips_ground_t *g,
                              const pddl_strips_ground_atree_t *atr,
                              int tree_id,
                              int start_arg)
{
    const pddl_strips_ground_tree_t *tr = atr->tree + tree_id;

    int size = pddlActionArgsSize(&tr->args);
    for (int argi = start_arg; argi < size; ++argi){
        const pddl_obj_id_t *args = pddlActionArgsGet(&tr->args, argi);
        _atreeActionAddEff(g, atr, tree_id, args, 0);
    }
}

static void atreeUnifyFact(pddl_strips_ground_t *g,
                           pddl_strips_ground_atree_t *atr,
                           const pddl_ground_atom_t *fact,
                           int static_fact)
{
    for (int ti = 0; ti < atr->tree_size; ++ti){
        pddl_strips_ground_tree_t *tr = atr->tree + ti;

        int start = pddlActionArgsSize(&tr->args);
        pddlStripsGroundTreeUnifyFact(tr, fact, static_fact);
        if (start < pddlActionArgsSize(&tr->args)
                && atreeAllTreesNonEmpty(atr)){
            atreeActionAddEff(g, atr, ti, start);
        }
    }
}
/*** atree_t END ***/

/*** ground_args_t ***/
static void groundArgsFree(pddl_strips_ground_args_arr_t *ga)
{
    for (int i = 0; i < ga->size; ++i){
        if (ga->arg[i].arg != NULL)
            FREE(ga->arg[i].arg);
    }
    if (ga->arg != NULL)
        FREE(ga->arg);
}

static void groundArgsAdd(pddl_strips_ground_args_arr_t *ga, int action_id,
                          const pddl_prep_action_t *action,
                          const pddl_obj_id_t *arg)
{
    pddl_strips_ground_args_t *garg;

    if (ga->size >= ga->alloc){
        if (ga->alloc == 0)
            ga->alloc = 4;
        ga->alloc *= 2;
        ga->arg = REALLOC_ARR(ga->arg, pddl_strips_ground_args_t,
                                  ga->alloc);
    }

    garg = ga->arg + ga->size++;
    garg->arg = ALLOC_ARR(pddl_obj_id_t, action->param_size);
    memcpy(garg->arg, arg, sizeof(pddl_obj_id_t) * action->param_size);
    garg->action_id = action_id;
    garg->action = action;
    garg->op_id = -1;
}

static int groundArgsCmp(const void *a, const void *b, void *_)
{
    const pddl_strips_ground_args_t *g1 = a;
    const pddl_strips_ground_args_t *g2 = b;
    int g1_action_id = g1->action_id;
    int g2_action_id = g2->action_id;
    int cmp;

    if (g1->action->parent_action >= 0)
        g1_action_id = g1->action->parent_action;
    if (g2->action->parent_action >= 0)
        g2_action_id = g2->action->parent_action;

    if (g1_action_id == g2_action_id){
        cmp = memcmp(g1->arg, g2->arg,
                     sizeof(pddl_obj_id_t) * g1->action->param_size);
        if (cmp != 0)
            return cmp;
        if (g1->action->parent_action < 0)
            return -1;
        if (g2->action->parent_action < 0)
            return 1;
        return g1->action_id - g2->action_id;
    }
    return g1_action_id - g2_action_id;
}

static void groundArgsSortAndUniq(pddl_strips_ground_args_arr_t *ga,
                                  pddl_err_t *err)
{
    int ins;

    if (ga->arg == 0)
        return;

    pddlSort(ga->arg, ga->size, sizeof(pddl_strips_ground_args_t),
            groundArgsCmp, NULL);

    // Remove duplicates -- it shoud not happen, but just in case...
    // Report warning.
    ins = 0;
    for (int i = 1; i < ga->size; ++i){
        if (groundArgsCmp(ga->arg + i, ga->arg + ins, NULL) == 0){
            PDDL_WARN(err, "Duplicate grounded action"
                           " -- this should not happen!");
            if (ga->arg[i].arg != NULL)
                FREE(ga->arg[i].arg);
        }else{
            ga->arg[++ins] = ga->arg[i];
        }
    }
    ga->size = ins + 1;
}
/*** pddl_strips_ground_args_t END ***/


/*** unify ***/
static void _unifyFacts(pddl_strips_ground_t *g, pddl_ground_atoms_t *ga,
                        int start_idx, int static_fact)
{
    int next_batch = ga->atom_size;
    for (int i = start_idx; i < ga->atom_size; ++i){
        const pddl_ground_atom_t *fact = ga->atom[i];
        for (int j = 0; j < g->action.action_size; ++j)
            atreeUnifyFact(g, g->atree + j, fact, static_fact);

        if (!static_fact && i == next_batch - 1){
            PDDL_INFO(g->err, "  Next batch unified. (unified facts: %d,"
                             " facts: %d, funcs: %d, add effs: %d)",
                     i + 1,
                     g->facts.atom_size,
                     g->funcs.atom_size,
                     g->ground_args.size);
            next_batch = ga->atom_size;
        }
    }
}

static int unifyStaticFacts(pddl_strips_ground_t *g)
{
    // First ground actions without preconditions
    for (int i = 0; i < g->action.action_size; ++i){
        if (g->action.action[i].pre.size == 0)
            groundActionAddEffEmptyPre(g, g->action.action + i);
    }

    _unifyFacts(g, &g->static_facts, 0, 1);
    for (int i = 0; i < g->action.action_size; ++i)
        atreeBlockStatic(g->atree + i);
    g->static_facts_unified = 1;

    PDDL_INFO(g->err, "  Static facts unified."
                     " (static facts: %d, facts: %d, funcs: %d, add effs: %d)",
             g->static_facts.atom_size,
             g->facts.atom_size,
             g->funcs.atom_size,
             g->ground_args.size);

    return 0;
}

static int unifyFacts(pddl_strips_ground_t *g)
{
    _unifyFacts(g, &g->facts, g->unify_start_idx, 0);
    g->unify_start_idx = g->facts.atom_size;
    return 0;
}
/*** unify END ***/

static void groundAtomsAddFact(pddl_strips_ground_t *g,
                               const pddl_fm_atom_t *c,
                               const pddl_obj_id_t *arg)
{
    if (g->unify_new_atom_fn == NULL){
        pddlGroundAtomsAddAtom(&g->facts, c, arg);
    }else{
        int size = g->facts.atom_size;
        pddl_ground_atom_t *atom = pddlGroundAtomsAddAtom(&g->facts, c, arg);
        if (g->facts.atom_size > size)
            g->unify_new_atom_fn(atom, g->unify_new_atom_data);

    }
}

static void _groundActionAddEff(pddl_strips_ground_t *g,
                                const pddl_prep_action_t *a,
                                pddl_obj_id_t *arg, int argi)
{
    const pddl_obj_id_t *obj;
    int size;

    // Skip bound arguments
    for (; argi < a->param_size && arg[argi] >= 0; ++argi);
    // and try every possible object for the unbound arguments
    if (argi < a->param_size){
        obj = pddlTypesObjsByType(a->type, a->param_type[argi], &size);
        for (int i = 0; i < size; ++i){
            arg[argi] = obj[i];
            _groundActionAddEff(g, a, arg, argi + 1);
            arg[argi] = -1;
        }
        return;
    }

    if (!pddlPrepActionCheck(a, &g->static_facts, arg))
        return;

    if (g->cfg.lifted_mgroups != NULL){
        if (g->cfg.prune_op_pre_mutex
                && pddlLiftedMGroupsIsGroundedConjTooHeavy(
                            g->cfg.lifted_mgroups, g->pddl, &a->pre, arg)){
            return;
        }

        if (g->cfg.prune_op_dead_end
                && a->parent_action < 0
                && pddlLiftedMGroupsAnyIsDeleted(&g->goal_mgroup, g->pddl,
                                                 &a->pre, &a->add_eff,
                                                 &a->del_eff, arg)){
            return;
        }
    }

    const pddl_fm_atom_t *atom;
    for (int i = 0; i < a->add_eff.size; ++i){
        atom = PDDL_FM_CAST(a->add_eff.fm[i], atom);
        groundAtomsAddFact(g, atom, arg);
    }

    groundArgsAdd(&g->ground_args, a - g->action.action, a, arg);
}

static void groundActionAddEff(pddl_strips_ground_t *g,
                               const pddl_prep_action_t *a,
                               const pddl_obj_id_t *oarg)
{
    pddl_obj_id_t arg[a->param_size];
    for (int i = 0; i < a->param_size; ++i)
        arg[i] = (oarg[i] == PDDL_OBJ_ID_UNDEF ? -1 : oarg[i]);
    _groundActionAddEff(g, a, arg, 0);
}

static void groundActionAddEffEmptyPre(pddl_strips_ground_t *g,
                                       const pddl_prep_action_t *a)
{
    ASSERT(a->pre.size == 0);
    pddl_obj_id_t arg[a->param_size];
    for (int i = 0; i < a->param_size; ++i)
        arg[i] = PDDL_OBJ_ID_UNDEF;
    _groundActionAddEff(g, a, arg, 0);
}

static char *groundOpName(const pddl_t *pddl,
                          const pddl_action_t *action,
                          const pddl_obj_id_t *args)
{
    int i, slen;
    char *name, *cur;

    slen = strlen(action->name) + 2 + 1;
    for (i = 0; i < action->param.param_size; ++i)
        slen += 1 + strlen(pddl->obj.obj[args[i]].name);

    cur = name = ALLOC_ARR(char, slen);
    cur += sprintf(cur, "%s", action->name);
    for (i = 0; i < action->param.param_size; ++i)
        cur += sprintf(cur, " %s", pddl->obj.obj[args[i]].name);

    return name;
}

static int groundIncrease(pddl_strips_ground_t *g,
                          const pddl_obj_id_t *arg,
                          const pddl_fm_arr_t *atoms,
                          const pddl_action_t *action)
{
    const pddl_fm_func_op_t *inc;
    const pddl_ground_atom_t *ga;
    int cost = 0;

    // Only (increase (total-cost) ...) is allowed.
    for (int i = 0; i < atoms->size; ++i){
        inc = PDDL_FM_CAST(atoms->fm[i], func_op);
        if (inc->fvalue != NULL){
            ga = pddlGroundAtomsFindAtom(&g->funcs, inc->fvalue, arg);
            if (ga != NULL){
                cost += ga->func_val;
            }else{
                char *name = groundOpName(g->pddl, action, arg);
                PDDL_WARN(g->err, "Undefined cost for action (%s).", name);
                FREE(name);
            }
        }else{
            cost += inc->value;
        }
    }

    return cost;
}

static void groundAtoms(pddl_strips_ground_t *g,
                        int atom_max_arg_size,
                        const pddl_obj_id_t *arg,
                        const pddl_fm_arr_t *atoms,
                        pddl_iset_t *out)
{
    const pddl_fm_atom_t *atom;
    const pddl_ground_atom_t *ga;

    for (int i = 0; i < atoms->size; ++i){
        atom = PDDL_FM_CAST(atoms->fm[i], atom);
        ga = pddlGroundAtomsFindAtom(&g->facts, atom, arg);
        if (ga != NULL)
            pddlISetAdd(out, g->ground_atom_to_fact_id[ga->id]);
    }
}

static int setUpOp(pddl_strips_ground_t *g, pddl_strips_op_t *op,
                    const pddl_strips_ground_args_t *ga)
{
    const pddl_prep_action_t *a = ga->action;
    char *name;

    // Different operator cost for the conditional effects is not allowed
    if (a->parent_action >= 0 && a->increase.size > 0){
        PDDL_ERR_RET(g->err, -1,
                     "Costs in conditional effects are not supported.");
    }

    // Ground precontions, add and delete effects and set cost
    groundAtoms(g, a->max_arg_size, ga->arg, &a->pre, &op->pre);
    groundAtoms(g, a->max_arg_size, ga->arg, &a->add_eff, &op->add_eff);
    groundAtoms(g, a->max_arg_size, ga->arg, &a->del_eff, &op->del_eff);
    op->cost = 1;
    if (g->pddl->metric)
        op->cost = groundIncrease(g, ga->arg, &a->increase, a->action);
    name = groundOpName(g->pddl, a->action, ga->arg);

    // Make the operator well-formed
    pddlStripsOpFinalize(op, name);

    return 0;
}

static void groundCondEff(pddl_strips_ground_t *g, pddl_strips_t *strips,
                          pddl_strips_op_t *op,
                          pddl_strips_ground_args_t *ga,
                          pddl_strips_ground_args_t *parent_ga)
{
    pddl_strips_op_t *parent;

    // If the operator corresponds to a conditional effect the
    // parent must be known already, because this is the way we
    // sorted pddl_strips_ground_args_t structures.
    ASSERT_RUNTIME(parent_ga != NULL);

    // If parent action is not created then it had to have empty
    // effects. Therefore, we need to create the parent first.
    if (parent_ga->op_id == -1){
        pddl_strips_op_t op2;
        pddlStripsOpInit(&op2);
        setUpOp(g, &op2, parent_ga);
        parent_ga->op_id = pddlStripsOpsAdd(&strips->op, &op2);
        pddlStripsOpFree(&op2);
    }
    parent = strips->op.op[parent_ga->op_id];

    // Find out preconditions that belong only to the conditional
    // effect.
    pddlISetMinus(&op->pre, &parent->pre);
    if (op->pre.size > 0){
        // Create conditional effect if necessary
        pddlStripsOpAddCondEff(parent, op);
        strips->has_cond_eff = 1;

    }else{
        // If precondition of the conditional effect is empty, then
        // we can merge conditional effect directly to the parent
        // operator.
        // The operators are hashed only using its name so we can
        // merge effects without re-inserting operator.
        pddlStripsOpAddEffFromOp(parent, op);

        // If operator was well-formed before it must remain
        // well-formed.
        ASSERT(parent->add_eff.size > 0 || parent->del_eff.size > 0);
    }
}

static int groundActions(pddl_strips_ground_t *g, pddl_strips_t *strips)
{
    pddl_strips_ground_args_t *ga, *parent_ga;
    const pddl_prep_action_t *a;
    pddl_strips_op_t op;

    // Sorts unified arguments for actions so that conditional effects are
    // placed right after their respective non-cond-eff parent action.
    groundArgsSortAndUniq(&g->ground_args, g->err);

    parent_ga = NULL;
    for (int i = 0; i < g->ground_args.size; ++i){
        ga = g->ground_args.arg + i;
        a = ga->action;
        ASSERT(pddlPrepActionCheck(a, &g->static_facts, ga->arg));

        pddlStripsOpInit(&op);
        if (setUpOp(g, &op, ga) != 0){
            pddlStripsOpFree(&op);
            PDDL_TRACE_RET(g->err, -1);
        }

        // Remember this action as a parent for conditional effects
        if (a->parent_action < 0)
            parent_ga = ga;

        // Use only operators with effects
        if (op.add_eff.size > 0 || op.del_eff.size > 0){
            if (a->parent_action >= 0){
                groundCondEff(g, strips, &op, ga, parent_ga);
            }else{
                ga->op_id = pddlStripsOpsAdd(&strips->op, &op);
            }
        }

        pddlStripsOpFree(&op);
    }

    pddlStripsOpsSort(&strips->op);

    return 0;
}

static int createStripsFacts(pddl_strips_ground_t *g, pddl_strips_t *strips)
{
    const pddl_ground_atom_t *ga;
    int fact_id;

    for (int i = 0; i < g->facts.atom_size; ++i){
        ga = g->facts.atom[i];
        ASSERT(ga->id == i);
        fact_id = pddlFactsAddGroundAtom(&strips->fact, ga, g->pddl);
        if (fact_id != ga->id){
            PANIC("The fact and the corresponding grounded atom have"
                       " different IDs. This is definitelly a bug!");
        }
    }

    g->ground_atom_to_fact_id = ALLOC_ARR(int, strips->fact.fact_size);
    pddlFactsSort(&strips->fact, g->ground_atom_to_fact_id);
#ifdef DEBUG
    for (int i = 0; i < g->facts.atom_size; ++i){
        ga = g->facts.atom[i];
        ASSERT(ga->id == i);
        fact_id = pddlFactsAddGroundAtom(&strips->fact, ga, g->pddl);
        ASSERT(fact_id == g->ground_atom_to_fact_id[ga->id]);
    }
#endif
    return 0;
}

static int groundInitState(pddl_strips_ground_t *g, pddl_strips_t *strips)
{
    pddl_list_t *item;
    const pddl_fm_t *c;
    const pddl_fm_atom_t *a;
    const pddl_ground_atom_t *ga;

    PDDL_LIST_FOR_EACH(&g->pddl->init->part, item){
        c = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);
        if (c->type == PDDL_FM_ATOM){
            a = PDDL_FM_CAST(c, atom);
            ga = pddlGroundAtomsFindAtom(&g->facts, a, NULL);
            if (ga != NULL)
                pddlISetAdd(&strips->init, g->ground_atom_to_fact_id[ga->id]);
        }
    }
    return 0;
}

struct ground_goal {
    pddl_strips_ground_t *g;
    pddl_strips_t *strips;
    int fail;
};

static int _groundGoal(pddl_fm_t *c, void *_g)
{
    struct ground_goal *ggoal = _g;
    const pddl_ground_atom_t *ga;
    pddl_strips_ground_t *g = ggoal->g;
    pddl_strips_t *strips = ggoal->strips;

    if (c->type == PDDL_FM_ATOM){
        const pddl_fm_atom_t *atom = PDDL_FM_CAST(c, atom);
        if (!pddlFmAtomIsGrounded(atom))
            PDDL_ERR_RET(g->err, -1, "Goal specification cannot contain"
                         " parametrized atoms.");

        // Find fact in the set of reachable facts
        ga = pddlGroundAtomsFindAtom(&g->facts, atom, NULL);
        if (ga != NULL){
            // Add the fact to the goal specification
            pddlISetAdd(&strips->goal, g->ground_atom_to_fact_id[ga->id]);
        }else{
            // The goal can be static fact in which case we simply skip
            // this fact
            ga = pddlGroundAtomsFindAtom(&g->static_facts, atom, NULL);
            if (ga == NULL){
                // The problem is unsolvable, because a goal fact is not
                // reachable.
                strips->goal_is_unreachable = 1;
            }
        }
        return 0;

    }else if (c->type == PDDL_FM_AND){
        return 0;

    }else if (c->type == PDDL_FM_BOOL){
        const pddl_fm_bool_t *b = PDDL_FM_CAST(c, bool);
        if (!b->val)
            strips->goal_is_unreachable = 1;
        return 0;

    }else{
        PDDL_ERR(g->err, "Only conjuctive goal specifications are supported."
                " (Goal contains %s.)", pddlFmTypeName(c->type));
        ggoal->fail = 1;
        return -2;
    }
}

static int groundGoal(pddl_strips_ground_t *g, pddl_strips_t *strips)
{
    struct ground_goal ggoal = { g, strips, 0 };
    if (g->pddl->goal->type == PDDL_FM_OR){
        PDDL_ERR_RET(g->err, -1, "Only conjuctive goal specifications"
                     " are supported. This goal is a disjunction.");
    }

    pddlFmTraverse(g->pddl->goal, _groundGoal, NULL, &ggoal);
    if (ggoal.fail)
        PDDL_TRACE_RET(g->err, -1);
    return 0;
}

static void groundInitFact(pddl_strips_ground_t *g, const pddl_t *pddl)
{
    pddl_list_t *item;
    const pddl_fm_t *c;
    const pddl_fm_atom_t *a;
    const pddl_fm_func_op_t *ass;
    pddl_ground_atom_t *ga;

    PDDL_LIST_FOR_EACH(&pddl->init->part, item){
        c = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);
        if (c->type == PDDL_FM_ATOM){
            a = PDDL_FM_CAST(c, atom);
            if (pddlPredIsStatic(&pddl->pred.pred[a->pred])){
                ASSERT(pddlFmAtomIsGrounded(a));
                pddlGroundAtomsAddAtom(&g->static_facts, a, NULL);
            }else{
                ASSERT(pddlFmAtomIsGrounded(a));
                groundAtomsAddFact(g, a, NULL);
            }
        }else if (c->type == PDDL_FM_ASSIGN){
            ass = PDDL_FM_CAST(c, func_op);
            ASSERT(ass->fvalue == NULL);
            ASSERT(ass->lvalue != NULL);
            ASSERT(pddlFmAtomIsGrounded(ass->lvalue));
            ga = pddlGroundAtomsAddAtom(&g->funcs, ass->lvalue, NULL);
            ga->func_val = ass->value;
        }
    }
}

static int groundInit(pddl_strips_ground_t *g, const pddl_t *pddl,
                      const pddl_ground_config_t *cfg,
                      pddl_err_t *err,
                      pddl_strips_ground_unify_new_atom_fn new_atom,
                      void *new_atom_data)
{
    ZEROIZE(g);
    g->pddl = pddl;
    g->cfg = *cfg;
    if (g->cfg.lifted_mgroups == NULL){
        g->cfg.prune_op_pre_mutex = 0;
        g->cfg.prune_op_dead_end = 0;
    }

    g->err = err;
    g->unify_new_atom_fn = new_atom;
    g->unify_new_atom_data = new_atom_data;

    if (pddlPrepActionsInit(pddl, &g->action, g->err) != 0)
        PDDL_TRACE_RET(g->err, -1);

    if (g->cfg.lifted_mgroups != NULL){
        pddlLiftedMGroupsExtractGoalAware(&g->goal_mgroup,
                                          g->cfg.lifted_mgroups, pddl);
    }

    pddlGroundAtomsInit(&g->static_facts);
    g->static_facts_unified = 0;
    pddlGroundAtomsInit(&g->facts);
    g->ground_atom_to_fact_id = NULL;
    pddlGroundAtomsInit(&g->funcs);

    groundInitFact(g, pddl);
    g->unify_start_idx = 0;

    g->atree = ALLOC_ARR(pddl_strips_ground_atree_t,
                             g->action.action_size);
    for (int i = 0; i < g->action.action_size; ++i){
        const pddl_prep_action_t *a = g->action.action + i;
        atreeInit(g->atree + i, pddl, a);
    }

    return 0;
}

static void groundFree(pddl_strips_ground_t *g)
{
    for (int i = 0; i < g->action.action_size; ++i)
        atreeFree(g->atree + i);
    if (g->atree != NULL)
        FREE(g->atree);
    pddlGroundAtomsFree(&g->static_facts);
    pddlGroundAtomsFree(&g->facts);
    if (g->ground_atom_to_fact_id != NULL)
        FREE(g->ground_atom_to_fact_id);
    pddlGroundAtomsFree(&g->funcs);
    pddlPrepActionsFree(&g->action);
    pddlLiftedMGroupsFree(&g->goal_mgroup);
    groundArgsFree(&g->ground_args);
}

int pddlStripsGroundStart(pddl_strips_ground_t *g,
                          const pddl_t *pddl,
                          const pddl_ground_config_t *cfg,
                          pddl_err_t *err,
                          pddl_strips_ground_unify_new_atom_fn new_atom,
                          void *new_atom_data)
{
    PDDL_INFO(err, "PDDL to STRIPS (domain: %s, problem: %s) ...",
             pddl->domain_lisp->filename,
             pddl->problem_lisp->filename);

    if (groundInit(g, pddl, cfg, err, new_atom, new_atom_data) != 0){
        groundFree(g);
        PDDL_TRACE_RET(err, -1);
    }

    PDDL_INFO(err, "  lifted mutex groups: %d",
             (g->cfg.lifted_mgroups != NULL
                ?  g->cfg.lifted_mgroups->mgroup_size : -1));
    PDDL_INFO(err, "  goal-aware lifted mutex groups: %d",
             (g->cfg.lifted_mgroups != NULL
                ?  g->goal_mgroup.mgroup_size : -1));
    PDDL_INFO(err, "  prune-op-pre-mutex: %d", g->cfg.prune_op_pre_mutex);
    PDDL_INFO(err, "  prune-op-dead-end: %d", g->cfg.prune_op_dead_end);
    PDDL_INFO(err, "  prep-actions: %d", g->action.action_size);

    return 0;
}

int pddlStripsGroundUnifyStep(pddl_strips_ground_t *g)
{
    if (!g->static_facts_unified && unifyStaticFacts(g) != 0){
        groundFree(g);
        PDDL_TRACE_RET(g->err, -1);
    }
    if (unifyFacts(g) != 0){
        groundFree(g);
        PDDL_TRACE_RET(g->err, -1);
    }

    PDDL_INFO(g->err, "  Unification finished."
                     " (facts: %d, funcs: %d, add effs: %d)",
             g->facts.atom_size,
             g->funcs.atom_size,
             g->ground_args.size);
    return 0;
}

int pddlStripsGroundAddGroundAtom(pddl_strips_ground_t *g, int pred,
                                  const pddl_obj_id_t *arg, int arg_size)
{
    int size = g->facts.atom_size;
    pddlGroundAtomsAddPred(&g->facts, pred, arg, arg_size);
    if (g->facts.atom_size > size)
        return 1;
    return 0;
}

int pddlStripsGroundFinalize(pddl_strips_ground_t *g, pddl_strips_t *strips)
{
    pddlStripsInit(strips);
    strips->cfg = g->cfg;

    if (g->pddl->domain_name)
        strips->domain_name = STRDUP(g->pddl->domain_name);
    if (g->pddl->problem_name)
        strips->problem_name = STRDUP(g->pddl->problem_name);
    if (g->pddl->domain_lisp->filename)
        strips->domain_file = STRDUP(g->pddl->domain_lisp->filename);
    if (g->pddl->problem_lisp->filename)
        strips->problem_file = STRDUP(g->pddl->problem_lisp->filename);

    if (createStripsFacts(g, strips) != 0
            || groundActions(g, strips) != 0
            || groundInitState(g, strips) != 0
            || groundGoal(g, strips) != 0){
        groundFree(g);
        PDDL_TRACE_RET(g->err, -1);
    }

    groundFree(g);

    if (g->cfg.remove_static_facts)
        pddlStripsRemoveStaticFacts(strips, g->err);

    pddlStripsMergeCondEffIfPossible(strips);

    // TODO: Parametrize
    pddlStripsOpsDeduplicate(&strips->op);

    if (strips->goal_is_unreachable)
        pddlStripsMakeUnsolvable(strips);

    PDDL_INFO(g->err, "PDDL grounded to STRIPS.");

    return 0;
}

int pddlStripsGround(pddl_strips_t *strips,
                     const pddl_t *pddl,
                     const pddl_ground_config_t *cfg,
                     pddl_err_t *err)
{
    CTX(err, "ground", "Ground");
    CTX_NO_TIME(err, "cfg", "Cfg");
    pddlGroundConfigLog(cfg, err);
    CTXEND(err);
    pddl_strips_ground_t g;

    if (pddlStripsGroundStart(&g, pddl, cfg, err, NULL, NULL) != 0
            || pddlStripsGroundUnifyStep(&g) != 0
            || pddlStripsGroundFinalize(&g, strips) != 0){
        PDDL_INFO(err, "Grounding failed.");
        CTXEND(err);
        PDDL_TRACE_RET(err, -1);
    }

    pddlStripsLogInfo(strips, err);
    CTXEND(err);
    return 0;
}
