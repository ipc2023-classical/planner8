/***
 * cpddl
 * -------
 * Copyright (c)2018 Daniel Fiser <danfis@danfis.cz>,
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

#include "pddl/pddl.h"
#include "pddl/strips_ground_tree.h"
#include "internal.h"

#define TNODE_FOR_EACH_CHILD(TN, CH) \
    for (int __i = 0; __i < (TN)->child_size \
            && ((CH) = (TN)->child[__i]); ++__i)

static pddl_strips_ground_tnode_t *tnodeNew(pddl_strips_ground_tree_t *t,
                                            pddl_strips_ground_tnode_t *parent,
                                            int param,
                                            pddl_obj_id_t obj_id)
{
    pddl_strips_ground_tnode_t *n;

    n = ZALLOC(pddl_strips_ground_tnode_t);
    n->param = param;
    n->obj_id = obj_id;
    if (parent != NULL)
        n->pre_unified = parent->pre_unified;
    return n;
}

static void tnodeDel(pddl_strips_ground_tnode_t *t)
{
    pddl_strips_ground_tnode_t *ch;
    TNODE_FOR_EACH_CHILD(t, ch)
        tnodeDel(ch);
    if (t->child != NULL)
        FREE(t->child);
    FREE(t);
}

static void tnodeReserveChild(pddl_strips_ground_tree_t *tr,
                              pddl_strips_ground_tnode_t *n)
{
    if (n->child_size == n->child_alloc){
        if (n->child_alloc == 0)
            n->child_alloc = 1;
        n->child_alloc *= 2;
        n->child = REALLOC_ARR(n->child, pddl_strips_ground_tnode_t *,
                               n->child_alloc);
    }
}

static pddl_strips_ground_tnode_t *tnodeAddChild(pddl_strips_ground_tree_t *t,
                                                 pddl_strips_ground_tnode_t *p,
                                                 int param,
                                                 pddl_obj_id_t obj_id)
{
    pddl_strips_ground_tnode_t *n = tnodeNew(t, p, param, obj_id);
    tnodeReserveChild(t, p);
    p->child[p->child_size++] = n;
    return n;
}

static void propagatePre(pddl_strips_ground_tree_t *tr,
                         pddl_strips_ground_tnode_t *tn,
                         pddl_obj_id_t *arg)
{
    // If all preconditions are unified, we can ground the action using
    // assigned arguments. Note that we don't actually need to be in a
    // leaf.
    if (tn->pre_unified == tr->pre_size){
        // TODO: If grounding fails then it means that this argument
        //       assignement cannot be grounded -- can we utilize this
        //       somehow?
        //       Also, the reason of the failure cannot be types of
        //       arguments, or equality of arguments, because these things
        //       are checked at the beggining. Therefore the only reason
        //       can be negative preconditions on static predicates.
        // TODO: If grounding is successful, we can probably safe some
        //       memory removing part of tree. The question is whether is
        //       it useful.
        pddlActionArgsAdd(&tr->args, arg);
        tn->flag_blocked = 1;
        return;
    }

    pddl_strips_ground_tnode_t *ch;
    TNODE_FOR_EACH_CHILD(tn, ch){
        ++ch->pre_unified;
        if (arg[ch->param] == PDDL_OBJ_ID_UNDEF){
            arg[ch->param] = ch->obj_id;
            propagatePre(tr, ch, arg);
            arg[ch->param] = PDDL_OBJ_ID_UNDEF;
        }else{
            propagatePre(tr, ch, arg);
        }
    }
}
static void unifyPre(pddl_strips_ground_tree_t *tr,
                     pddl_strips_ground_tnode_t *tn,
                     pddl_obj_id_t *arg,
                     int pre_i)
{
    ++tn->pre_unified;
    tn->flag_pre_unified = 1;
    propagatePre(tr, tn, arg);
}

static void unifyNew(pddl_strips_ground_tree_t *tr,
                     pddl_strips_ground_tnode_t *tn,
                     pddl_obj_id_t *arg,
                     int remain,
                     const pddl_obj_id_t *arg_pre,
                     int pre_i,
                     int static_fact);
static void unifyNewArg(pddl_strips_ground_tree_t *tr,
                        pddl_strips_ground_tnode_t *tn,
                        pddl_obj_id_t *arg,
                        int param,
                        int remain,
                        const pddl_obj_id_t *arg_pre,
                        int pre_i,
                        int static_fact)
{
    pddl_strips_ground_tnode_t *new;

    arg[param] = arg_pre[param];
    new = tnodeAddChild(tr, tn, param, arg[param]);
    if (static_fact)
        new->flag_static_arg = 1;
    if (remain - 1 > 0){
        unifyNew(tr, new, arg, remain - 1, arg_pre, pre_i, static_fact);
    }else{
        unifyPre(tr, new, arg, pre_i);
    }
    arg[param] = PDDL_OBJ_ID_UNDEF;
}

static void unifyNew(pddl_strips_ground_tree_t *tr,
                     pddl_strips_ground_tnode_t *tn,
                     pddl_obj_id_t *arg,
                     int remain,
                     const pddl_obj_id_t *arg_pre,
                     int pre_i,
                     int static_fact)
{
    pddl_strips_ground_tnode_t *ch;

    // To reduce branching, first try to create a new
    // node using an argument that has some assignements on this level.
    TNODE_FOR_EACH_CHILD(tn, ch){
        if (arg[ch->param] == PDDL_OBJ_ID_UNDEF
                && arg_pre[ch->param] != PDDL_OBJ_ID_UNDEF){
            unifyNewArg(tr, tn, arg, ch->param, remain, arg_pre, pre_i,
                        static_fact);
            return;
        }
    }

    int param;
    PDDL_ISET_FOR_EACH(&tr->param, param){
        if (arg[param] == PDDL_OBJ_ID_UNDEF
                && arg_pre[param] != PDDL_OBJ_ID_UNDEF){
            unifyNewArg(tr, tn, arg, param, remain,
                        arg_pre, pre_i, static_fact);
            return;
        }
    }
}

static void unify(pddl_strips_ground_tree_t *tr,
                  pddl_strips_ground_tnode_t *tn,
                  pddl_obj_id_t *arg,
                  int remain,
                  const pddl_obj_id_t *arg_pre,
                  int pre_i,
                  int parent_match,
                  int static_fact)
{
    pddl_strips_ground_tnode_t *ch;
    int match = 0;

    if (!pddlPrepActionCheckEqDef(tr->action, arg))
        return;
    if (remain == 0){
        unifyPre(tr, tn, arg, pre_i);
        return;
    }

    TNODE_FOR_EACH_CHILD(tn, ch){
        ASSERT(ch->obj_id != PDDL_OBJ_ID_UNDEF);
        arg[ch->param] = arg_pre[ch->param];
        if (ch->obj_id == arg[ch->param]){
            if (static_fact)
                ch->flag_static_arg = 1;
            // Found exact match on the argument
            unify(tr, ch, arg, remain - 1, arg_pre, pre_i, 1, static_fact);
            match = 1;

        }else if (arg[ch->param] == PDDL_OBJ_ID_UNDEF){
            // Argument is not set therefore we need to unify with all set
            // arguments
            arg[ch->param] = ch->obj_id;
            unify(tr, ch, arg, remain, arg_pre, pre_i, 0, static_fact);
            arg[ch->param] = PDDL_OBJ_ID_UNDEF;
        }
        arg[ch->param] = PDDL_OBJ_ID_UNDEF;
    }

    // Create a new branch only if all of the following holds
    // 1) no argument could be matched in the current node
    // 2) the current node is allowed to have more children (it could be
    //    blocked due to static facts)
    // 3) there was match in the parent node
    //    or the current node corresponds to the end of some previously
    //    unified fact
    if (!match
            && !tn->flag_blocked
            && (parent_match || tn->flag_pre_unified)){
        unifyNew(tr, tn, arg, remain, arg_pre, pre_i, static_fact);
    }
}

static void unifyTree(pddl_strips_ground_tree_t *tr,
                      const pddl_ground_atom_t *fact,
                      int pre_i,
                      int static_fact)
{
    pddl_obj_id_t arg[tr->action->param_size];
    pddl_obj_id_t arg_pre[tr->action->param_size];

    // Check whether the fact can be unified -- this test is not enough but
    // it can filter out some facts.
    if (!pddlPrepActionCheckFact(tr->action, pre_i, fact->arg))
        return;

    // Initialize arg[] to undef -- this array will be filled with unified
    // arguments in unify() recursive call.
    for (int i = 0; i < tr->action->param_size; ++i)
        arg_pre[i] = arg[i] = PDDL_OBJ_ID_UNDEF;

    // Set arg_pre[] according to the fact's arguments and count the number
    // of set arguments.
    const pddl_fm_atom_t *atom;
    int num_args_set = 0;
    atom = PDDL_FM_CAST(tr->action->pre.fm[pre_i], atom);
    for (int i = 0; i < atom->arg_size; ++i){
        int param = atom->arg[i].param;
        if (param >= 0 && arg_pre[param] == PDDL_OBJ_ID_UNDEF){
            arg_pre[param] = fact->arg[i];
            ++num_args_set;

        }else if (param >= 0 && arg_pre[param] != fact->arg[i]){
            return;
        }
    }

    // Recursivelly unify arguments
    unify(tr, tr->root, arg, num_args_set, arg_pre, pre_i, 1, static_fact);
}

static int removeIncompleteStatic(pddl_strips_ground_tree_t *tr,
                                  pddl_strips_ground_tnode_t *tn)
{
    pddl_strips_ground_tnode_t *ch;

    for (int i = 0; i < tn->child_size; ++i){
        ch = tn->child[i];
        if (removeIncompleteStatic(tr, ch)){
            tnodeDel(ch);
            tn->child[i] = tn->child[--tn->child_size];
            --i;
        }
    }

    if (tn->child_size == 0
            && tn->pre_unified != tr->pre_static_size
            && tn->flag_static_arg){
        return 1;
    }
    return 0;
}

static void _blockStatic(pddl_strips_ground_tree_t *tr,
                         pddl_strips_ground_tnode_t *tn)
{
    pddl_strips_ground_tnode_t *ch;
    int prune[tr->action->param_size];

    for (int i = 0; i < tr->action->param_size; ++i)
        prune[i] = 0;
    TNODE_FOR_EACH_CHILD(tn, ch){
        if (ch->flag_static_arg)
            prune[ch->param] = 1;
    }

    for (int i = 0; i < tn->child_size; ++i){
        ch = tn->child[i];
        ASSERT(ch != NULL);
        if (!prune[ch->param])
            continue;
        if (!ch->flag_static_arg){
            tnodeDel(ch);
            tn->child[i] = tn->child[--tn->child_size];
            --i;
        }
    }

    TNODE_FOR_EACH_CHILD(tn, ch)
        _blockStatic(tr, ch);

    if (tn->child_size > 0)
        tn->flag_blocked = 1;
}

static int instantiateArgs(pddl_strips_ground_tree_t *tr,
                           pddl_strips_ground_tnode_t *tn,
                           int param_start,
                           int arg_size,
                           int arg_size_max)
{
    int param;
    PDDL_ISET_FOR_EACH(&tr->param, param){
        if (param < param_start)
            continue;
        const pddl_obj_id_t *obj;
        int size;
        obj = pddlTypesObjsByType(tr->action->type,
                                  tr->action->param_type[param], &size);
        if (size != arg_size)
            continue;
        for (int i = 0; i < size; ++i){
            pddl_strips_ground_tnode_t *ch;
            ch = tnodeAddChild(tr, tn, param, obj[i]);
            instantiateArgs(tr, ch, param + 1, arg_size, arg_size_max);
        }
        if (size > 0){
            tn->flag_blocked = 1;
            return 1;
        }
        return 0;
    }

    if (arg_size < arg_size_max)
        return instantiateArgs(tr, tn, 0, arg_size + 1, arg_size_max);
    tn->flag_pre_unified = 1;

    return 0;
}

static int isPreRelevant(const pddl_fm_atom_t *atom,
                         const pddl_iset_t *params)
{
    for (int ai = 0; ai < atom->arg_size; ++ai){
        int param = atom->arg[ai].param;
        if (param >= 0 && !pddlISetIn(param, params))
            return 0;
    }
    return 1;
}

void pddlStripsGroundTreeInit(pddl_strips_ground_tree_t *tr,
                              const pddl_t *pddl,
                              const pddl_prep_action_t *a,
                              const pddl_iset_t *params)
{
    ZEROIZE(tr);
    tr->pddl = pddl;
    tr->action = a;
    pddlISetUnion(&tr->param, params);

    tr->pred_to_pre = CALLOC_ARR(pddl_iset_t, pddl->pred.pred_size);
    for (int i = 0; i < a->pre.size; ++i){
        const pddl_fm_atom_t *atom;
        atom = PDDL_FM_CAST(tr->action->pre.fm[i], atom);
        if (!isPreRelevant(atom, params))
            continue;

        ++tr->pre_size;
        pddlISetAdd(tr->pred_to_pre + atom->pred, i);

        const pddl_pred_t *pred = pddl->pred.pred + atom->pred;
        if (pddlPredIsStatic(pred))
            ++tr->pre_static_size;
    }

    pddlActionArgsInit(&tr->args, a->param_size);

    tr->root = tnodeNew(tr, NULL, -1, PDDL_OBJ_ID_UNDEF);
    // TODO: move constans 1 and 3 into either parameter of grounding or
    //       define constants. Consider also instantiation also a small
    //       number (1 or 2) of bigger arguments.
    instantiateArgs(tr, tr->root, 0, 1, 3);
}

void pddlStripsGroundTreeFree(pddl_strips_ground_tree_t *tr)
{
    for (int i = 0; i < tr->pddl->pred.pred_size; ++i)
        pddlISetFree(tr->pred_to_pre + i);
    pddlISetFree(&tr->param);
    if (tr->root != NULL)
        tnodeDel(tr->root);
    pddlActionArgsFree(&tr->args);
    if (tr->pred_to_pre != NULL)
        FREE(tr->pred_to_pre);
}

void pddlStripsGroundTreeUnifyFact(pddl_strips_ground_tree_t *tr,
                                   const pddl_ground_atom_t *fact,
                                   int static_fact)
{
    int pre_i;
    PDDL_ISET_FOR_EACH(tr->pred_to_pre + fact->pred, pre_i){
        unifyTree(tr, fact, pre_i, static_fact);
    }
}

void pddlStripsGroundTreeBlockStatic(pddl_strips_ground_tree_t *tr)
{
    _blockStatic(tr, tr->root);
    removeIncompleteStatic(tr, tr->root);

    // If the action has any static preconditions, they must be already in
    // place therefore we can block the root node. This fixes the problem
    // with actions that cannot be grounded because there are no
    // corresponding static facts.
    if (tr->pre_static_size > 0)
        tr->root->flag_blocked = 1;
}
