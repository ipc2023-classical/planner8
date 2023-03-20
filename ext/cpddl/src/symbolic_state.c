/***
 * cpddl
 * -------
 * Copyright (c)2021 Daniel Fiser <danfis@danfis.cz>,
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

#include "pddl/symbolic_state.h"
#include "internal.h"

static void stateFree(pddl_symbolic_state_t *state,
                      pddl_bdd_manager_t *mgr)
{
    if (state->bdd != NULL)
        pddlBDDDel(mgr, state->bdd);
    pddlISetFree(&state->parent_ids);
}


static int openLT(const pddl_pairheap_node_t *n1,
                  const pddl_pairheap_node_t *n2,
                  void *data)
{
    const pddl_symbolic_state_t *o1, *o2;
    o1 = pddl_container_of(n1, pddl_symbolic_state_t, heap);
    o2 = pddl_container_of(n2, pddl_symbolic_state_t, heap);
    int cmp = pddlCostCmp(&o1->f_value, &o2->f_value);
    if (cmp == 0)
        cmp = pddlCostCmp(&o1->cost, &o2->cost);
    if (cmp == 0)
        cmp = pddlCostCmp(&o1->heur, &o2->heur);
    return cmp < 0;
}

static int openCostLT(const pddl_pairheap_node_t *n1,
                      const pddl_pairheap_node_t *n2,
                      void *data)
{
    const pddl_symbolic_state_t *o1, *o2;
    o1 = pddl_container_of(n1, pddl_symbolic_state_t, heap_cost);
    o2 = pddl_container_of(n2, pddl_symbolic_state_t, heap_cost);
    int cmp = pddlCostCmp(&o1->cost, &o2->cost);
    if (cmp == 0)
        cmp = pddlCostCmp(&o1->f_value, &o2->f_value);
    if (cmp == 0)
        cmp = pddlCostCmp(&o1->heur, &o2->heur);
    return cmp < 0;
}

static int rbtreeCostCmp(const pddl_rbtree_node_t *n1,
                         const pddl_rbtree_node_t *n2,
                         void *data)
{
    const pddl_symbolic_state_t *s1, *s2;
    s1 = pddl_container_of(n1, pddl_symbolic_state_t, rbtree);
    s2 = pddl_container_of(n2, pddl_symbolic_state_t, rbtree);
    int cmp = pddlCostCmp(&s1->cost, &s2->cost);
    if (cmp == 0)
        cmp = pddlCostCmp(&s1->heur, &s2->heur);
    if (cmp == 0)
        cmp = s1->id - s2->id;
    return cmp;
}

static int rbtreeAllClosedCmp(const pddl_rbtree_node_t *n1,
                              const pddl_rbtree_node_t *n2,
                              void *data)
{
    const pddl_symbolic_all_closed_t *c1, *c2;
    c1 = pddl_container_of(n1, pddl_symbolic_all_closed_t, rbtree);
    c2 = pddl_container_of(n2, pddl_symbolic_all_closed_t, rbtree);
    return pddlCostCmp(&c1->g_value, &c2->g_value);
}

void pddlSymbolicStatesInit(pddl_symbolic_states_t *states,
                            pddl_bdd_manager_t *mgr,
                            int use_heur_inconsistent,
                            pddl_err_t *err)
{
    ZEROIZE(states);
    size_t el_size = sizeof(pddl_symbolic_state_t);
    pddl_symbolic_state_t el_init;
    ZEROIZE(&el_init);
    el_init.id = -1;

    states->pool = pddlExtArrNew(el_size, NULL, &el_init);
    states->num_states = 0;

    states->open = pddlPairHeapNew(openLT, states);
    states->open_cost = pddlPairHeapNew(openCostLT, states);

    states->closed = pddlRBTreeNew(rbtreeCostCmp, NULL);
    states->num_closed = 0;

    states->all_closed = pddlBDDZero(mgr);
    if (use_heur_inconsistent){
        states->all_closed_g = pddlRBTreeNew(rbtreeAllClosedCmp, NULL);
        PDDL_INFO(err, "Created mapping from g-value to close-states BDDs");
    }

    pddlCostSetMax(&states->bound);
}

void pddlSymbolicStatesFree(pddl_symbolic_states_t *states,
                            pddl_bdd_manager_t *mgr)
{
    pddlPairHeapDel(states->open_cost);
    pddlPairHeapDel(states->open);

    pddlBDDDel(mgr, states->all_closed);

    if (states->all_closed_g != NULL){
        pddl_rbtree_node_t *tn;
        while ((tn = pddlRBTreeExtractMin(states->all_closed_g)) != NULL){
            pddl_symbolic_all_closed_t *c;
            c = pddl_container_of(tn, pddl_symbolic_all_closed_t, rbtree);
            pddlBDDDel(mgr, c->closed);
            FREE(c);
        }
        pddlRBTreeDel(states->all_closed_g);
    }

    for (int si = 0; si < states->num_states; ++si)
        stateFree(pddlExtArrGet(states->pool, si), mgr);
    pddlExtArrDel(states->pool);

    pddlRBTreeDel(states->closed);
}

pddl_symbolic_state_t *pddlSymbolicStatesGet(pddl_symbolic_states_t *states,
                                             int id)
{
    return pddlExtArrGet(states->pool, id);
}

void pddlSymbolicStatesRemoveClosedStates(pddl_symbolic_states_t *states,
                                          pddl_bdd_manager_t *mgr,
                                          pddl_bdd_t **bdd,
                                          const pddl_cost_t *cost)
{
    if (states->all_closed_g != NULL){
        pddl_rbtree_node_t *node;
        PDDL_RBTREE_FOR_EACH(states->all_closed_g, node){
            pddl_symbolic_all_closed_t *c;
            c = pddl_container_of(node, pddl_symbolic_all_closed_t, rbtree);
            if (pddlCostCmp(&c->g_value, cost) > 0)
                break;

            pddl_bdd_t *nall = pddlBDDNot(mgr, c->closed);
            pddlBDDAndUpdate(mgr, bdd, nall);
            pddlBDDDel(mgr, nall);
        }

    }else{
        pddl_bdd_t *nall = pddlBDDNot(mgr, states->all_closed);
        pddlBDDAndUpdate(mgr, bdd, nall);
        pddlBDDDel(mgr, nall);
    }
}

void pddlSymbolicStatesCloseState(pddl_symbolic_states_t *states,
                                  pddl_bdd_manager_t *mgr,
                                  pddl_symbolic_state_t *state)
{
    ASSERT(!state->is_closed);
    state->is_closed = 1;
    ASSERT(state->bdd != NULL);

    // Add state to the set of all closed states
    pddlBDDOrUpdate(mgr, &states->all_closed, state->bdd);

    if (states->all_closed_g != NULL){
        // Add state to the set of all closed states with the same g-value
        pddl_symbolic_all_closed_t ctest;
        ctest.g_value = state->cost;

        pddl_symbolic_all_closed_t *c;
        pddl_rbtree_node_t *node;
        if ((node = pddlRBTreeFind(states->all_closed_g, &ctest.rbtree)) == NULL){
            c = ALLOC(pddl_symbolic_all_closed_t);
            c->g_value = state->cost;
            c->closed = pddlBDDZero(mgr);
            pddlRBTreeInsert(states->all_closed_g, &c->rbtree);

        }else{
            c = pddl_container_of(node, pddl_symbolic_all_closed_t, rbtree);
        }
        pddlBDDOrUpdate(mgr, &c->closed, state->bdd);
    }

    pddlRBTreeInsert(states->closed, &state->rbtree);
    ++states->num_closed;
}

void pddlSymbolicStatesOpenState(pddl_symbolic_states_t *states,
                                 pddl_symbolic_state_t *state)
{
    ASSERT(!state->is_closed);
    pddlPairHeapAdd(states->open, &state->heap);
    pddlPairHeapAdd(states->open_cost, &state->heap_cost);
}

pddl_symbolic_state_t *
    pddlSymbolicStatesNextOpen(pddl_symbolic_states_t *states)
{
    if (pddlPairHeapEmpty(states->open))
        return NULL;

    pddl_pairheap_node_t *hstate = pddlPairHeapExtractMin(states->open);
    pddl_symbolic_state_t *state;
    state = pddl_container_of(hstate, pddl_symbolic_state_t, heap);
    pddlPairHeapRemove(states->open_cost, &state->heap_cost);
    return state;
}

pddl_symbolic_state_t *
    pddlSymbolicStatesOpenPeek(const pddl_symbolic_states_t *states)
{
    if (pddlPairHeapEmpty(states->open))
        return NULL;

    pddl_pairheap_node_t *hstate = pddlPairHeapMin(states->open);
    pddl_symbolic_state_t *state;
    state = pddl_container_of(hstate, pddl_symbolic_state_t, heap);
    return state;
}

const pddl_cost_t *
    pddlSymbolicStatesMinOpenCost(const pddl_symbolic_states_t *states)
{
    if (pddlPairHeapEmpty(states->open_cost))
        return NULL;

    pddl_pairheap_node_t *hstate = pddlPairHeapMin(states->open_cost);
    pddl_symbolic_state_t *state;
    state = pddl_container_of(hstate, pddl_symbolic_state_t, heap_cost);
    return &state->cost;
}

pddl_symbolic_state_t *pddlSymbolicStatesAdd(pddl_symbolic_states_t *states)
{
    pddl_symbolic_state_t *state;
    state = pddlExtArrGet(states->pool, states->num_states);
    state->id = states->num_states;
    state->parent_id = -1;
    state->trans_id = -1;
    pddlCostSetZero(&state->cost);
    pddlCostSetZero(&state->heur);
    pddlCostSetZero(&state->f_value);
    state->bdd = NULL;
    state->is_closed = 0;

    states->num_states++;
    return state;
}

pddl_symbolic_state_t *pddlSymbolicStatesAddBDD(pddl_symbolic_states_t *states,
                                                pddl_bdd_manager_t *mgr,
                                                pddl_bdd_t *bdd)
{
    pddl_symbolic_state_t *state = pddlSymbolicStatesAdd(states);
    if (bdd != NULL)
        state->bdd = pddlBDDClone(mgr, bdd);
    return state;
}

void pddlSymbolicStatesAddInit(pddl_symbolic_states_t *states,
                               pddl_bdd_manager_t *mgr,
                               pddl_bdd_t *bdd,
                               const pddl_cost_t *heur)
{
    pddl_symbolic_state_t *state;
    state = pddlSymbolicStatesAddBDD(states, mgr, bdd);
    pddlCostSetZero(&state->cost);

    pddlCostSetZero(&state->heur);
    if (heur != NULL)
        state->heur = *heur;

    state->f_value = state->cost;
    if (pddlCostCmp(&state->heur, &pddl_cost_zero) > 0)
        pddlCostSumSat(&state->f_value, &state->heur);
    pddlSymbolicStatesOpenState(states, state);
}
