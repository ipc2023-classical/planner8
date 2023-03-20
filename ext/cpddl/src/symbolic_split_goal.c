/***
 * cpddl
 * -------
 * Copyright (c)2022 Daniel Fiser <danfis@danfis.cz>,
 * FAI Group at Saarland University, and
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

#include "pddl/rbtree.h"
#include "pddl/disambiguation.h"
#include "pddl/symbolic_split_goal.h"
#include "internal.h"

#define EPS 1E-5

struct states {
    double key;
    pddl_bdd_t *states;
    pddl_rbtree_node_t rbtree;
};
typedef struct states states_t;

static states_t *statesNew(double key, pddl_bdd_t *bdd)
{
    states_t *s = ALLOC(states_t);
    s->key = key;
    s->states = bdd;
    return s;
}

static void statesDel(states_t *states, pddl_bdd_manager_t *mgr)
{
    pddlBDDDel(mgr, states->states);
    FREE(states);
}


static int statesRBTreeCmp(const pddl_rbtree_node_t *n1,
                           const pddl_rbtree_node_t *n2,
                           void *data)
{
    const states_t *s1 = pddl_container_of(n1, states_t, rbtree);
    const states_t *s2 = pddl_container_of(n2, states_t, rbtree);
    if (s1->key == s2->key)
        return 0;
    //if (fabs(s1->key - s2->key) < EPS)
    //    return 0;
    if (s1->key < s2->key)
        return -1;
    return 1;
}

static pddl_rbtree_t *statesMapNew(void)
{
    pddl_rbtree_t *map = pddlRBTreeNew(statesRBTreeCmp, NULL);
    return map;
}

static void statesMapDel(pddl_rbtree_t *map,
                         pddl_bdd_manager_t *mgr)
{
    pddl_rbtree_node_t *n;
    while ((n = pddlRBTreeExtractMin(map)) != NULL){
        states_t *s = pddl_container_of(n, states_t, rbtree);
        statesDel(s, mgr);
    }
    pddlRBTreeDel(map);
}

static int statesMapInsert(pddl_rbtree_t *map,
                           pddl_bdd_manager_t *mgr,
                           states_t *s)
{
    pddl_rbtree_node_t *find;
    if ((find = pddlRBTreeFind(map, &s->rbtree)) != NULL){
        states_t *sfound = pddl_container_of(find, states_t, rbtree);
        int ret = pddlBDDOrUpdate(mgr, &sfound->states, s->states);
        ASSERT_RUNTIME(ret == 0);
        return 0;

    }else{
        pddl_rbtree_node_t *n = pddlRBTreeInsert(map, &s->rbtree);
        ASSERT_RUNTIME(n == NULL);
        return 1;
    }
}

static pddl_rbtree_t *statesMapNewMGroup(const pddl_iset_t *mg,
                                        pddl_symbolic_vars_t *symb_vars,
                                        pddl_bdd_manager_t *mgr,
                                        const double *pot)
{
    pddl_rbtree_t *map = statesMapNew();
    int fact;
    PDDL_ISET_FOR_EACH(mg, fact){
        double key = pot[fact];
        PDDL_ISET(ps);
        pddlISetAdd(&ps, fact);
        pddl_bdd_t *bdd = pddlSymbolicVarsCreatePartialState(symb_vars, &ps);
        pddlISetFree(&ps);

        states_t *s = statesNew(key, bdd);
        if (statesMapInsert(map, mgr, s) == 0)
            statesDel(s, mgr);
    }
    return map;
}

static pddl_rbtree_t *statesMapMerge(pddl_rbtree_t *map1,
                                     pddl_rbtree_t *map2,
                                     pddl_bdd_manager_t *mgr)
{
    pddl_rbtree_t *map = statesMapNew();
    pddl_rbtree_node_t *n1;
    PDDL_RBTREE_FOR_EACH(map1, n1){
        states_t *s1 = pddl_container_of(n1, states_t, rbtree);
        pddl_rbtree_node_t *n2;
        PDDL_RBTREE_FOR_EACH(map2, n2){
            states_t *s2 = pddl_container_of(n2, states_t, rbtree);
            double key = s1->key + s2->key;
            states_t *s;
            s = statesNew(key, pddlBDDAnd(mgr, s1->states, s2->states));
            if (statesMapInsert(map, mgr, s) == 0)
                statesDel(s, mgr);
        }
    }
    return map;
}

void pddlSymbolicStatesSplitByPotDel(pddl_symbolic_states_split_by_pot_t *s,
                                     pddl_bdd_manager_t *mgr)
{
    for (int i = 0; i < s->state_size; ++i){
        pddlBDDDel(mgr, s->state[i].state);
    }
    if (s->state != NULL)
        FREE(s->state);
    FREE(s);
}

pddl_symbolic_states_split_by_pot_t *
pddlSymbolicStatesSplitByPot(const pddl_iset_t *state,
                             const pddl_mgroups_t *mgroups,
                             const pddl_mutex_pairs_t *mutex,
                             const double *pot,
                             pddl_symbolic_vars_t *symb_vars,
                             pddl_bdd_manager_t *mgr,
                             pddl_err_t *err)
{
    CTX(err, "symba_split_state_by_pot", "Split-state-by-pot");
    pddl_disambiguate_t disamb;
    if (pddlDisambiguateInit(&disamb, symb_vars->fact_size,
                             mutex, mgroups) != 0){
        PANIC("Disambiguation failed because there are"
               " no exactly-1 mutex groups");
    }
    PDDL_INFO(err, "Disambiguation created.");

    pddl_mgroups_t mgs;
    pddlMGroupsInitEmpty(&mgs);
    for (int i = 0; i < mgroups->mgroup_size; ++i){
        const pddl_mgroup_t *mgin = mgroups->mgroup + i;
        if (!mgin->is_exactly_one)
            continue;

        PDDL_ISET(mg_fact);
        if (!pddlISetIsDisjoint(state, &mgin->mgroup)){
            pddlISetIntersect2(&mg_fact, state, &mgin->mgroup);
            ASSERT(pddlISetSize(&mg_fact) == 1);
            pddlMGroupsAdd(&mgs, &mg_fact);

        }else{
            int dret = pddlDisambiguate(&disamb, state, &mgin->mgroup,
                                        1, 0, NULL, &mg_fact);
            if (dret < 0){
                pddlMGroupsFree(&mgs);
                pddlDisambiguateFree(&disamb);
                ASSERT_RUNTIME(0);
                // TODO: Unsolvable task
            }else if (dret == 0){
                pddlMGroupsAdd(&mgs, &mgin->mgroup);
            }else{
                pddlISetIntersect(&mg_fact, &mgin->mgroup);
                pddlMGroupsAdd(&mgs, &mg_fact);
            }
        }
        pddlISetFree(&mg_fact);
    }

    int maps_alloc = 2 * mgs.mgroup_size;
    int maps_size = 0;
    pddl_rbtree_t **maps = ALLOC_ARR(pddl_rbtree_t *, maps_alloc);
    for (int i = 0; i < mgs.mgroup_size; ++i){
        maps[maps_size++] = statesMapNewMGroup(&mgs.mgroup[i].mgroup,
                                               symb_vars, mgr, pot);
    }
    pddlMGroupsFree(&mgs);
    pddlDisambiguateFree(&disamb);

    for (int mi = 0; mi < maps_size - 1; mi = mi + 2){
        pddl_rbtree_t *map = statesMapMerge(maps[mi], maps[mi + 1], mgr);
        ASSERT(maps_size < maps_alloc);
        maps[maps_size++] = map;
        statesMapDel(maps[mi], mgr);
        statesMapDel(maps[mi + 1], mgr);
        maps[mi] = maps[mi + 1] = NULL;
    }

    pddl_symbolic_states_split_by_pot_t *ret;
    ret = ZALLOC(pddl_symbolic_states_split_by_pot_t);

    pddl_rbtree_t *map = maps[maps_size - 1];
    maps[maps_size - 1] = NULL;
#ifdef PDDL_DEBUG
    for (int i = 0; i < maps_size; ++i)
        ASSERT(maps[i] == NULL);
#endif /* PDDL_DEBUG */
    FREE(maps);

    pddl_rbtree_node_t *node;
    PDDL_RBTREE_FOR_EACH(map, node){
        states_t *states = pddl_container_of(node, states_t, rbtree);
        if (ret->state_size == ret->state_alloc){
            if (ret->state_alloc == 0)
                ret->state_alloc = 2;
            ret->state_alloc *= 2;
            ret->state = REALLOC_ARR(ret->state,
                                     pddl_symbolic_states_split_by_pot_bdd_t,
                                     ret->state_alloc);
        }
        pddl_symbolic_states_split_by_pot_bdd_t *s;
        s = ret->state + ret->state_size++;
        s->h = states->key;
        s->h_int = (int)ceil(states->key - EPS);
        s->state = pddlBDDClone(mgr, states->states);

        LOG_IN_CTX(err, "found_states", "Found states",
                   "h-value %{h}.2f (%{h_int}d),"
                   " bdd size: %{bdd_size}d",
                   s->h, s->h_int, pddlBDDSize(s->state));
    }

    statesMapDel(map, mgr);
    CTXEND(err);

    return ret;
}
