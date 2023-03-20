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

#ifndef __PDDL_SYMBOLIC_STATE_H__
#define __PDDL_SYMBOLIC_STATE_H__

#include <pddl/extarr.h>
#include <pddl/pairheap.h>
#include <pddl/rbtree.h>
#include <pddl/iset.h>
#include <pddl/err.h>
#include <pddl/bdd.h>
#include <pddl/cost.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct pddl_symbolic_state {
    int id; /*!< ID of this state */
    int parent_id; /*!< Parent state ID */
    pddl_iset_t parent_ids; /*!< IDs of parent state if this is a merge-state */
    int trans_id; /*!< ID of the transitions that achieved this state */
    pddl_cost_t cost; /*!< Cost of the state: g value + zero cost g value */
    pddl_cost_t heur; /*!< Heuristic estimate */
    pddl_cost_t f_value; /*!< .cost + .heur */
    pddl_bdd_t *bdd; /*!< BDD representing the state */
    int is_closed; /*!< True if the state is closed */
    pddl_pairheap_node_t heap;
    pddl_pairheap_node_t heap_cost;
    pddl_rbtree_node_t rbtree;
};
typedef struct pddl_symbolic_state pddl_symbolic_state_t;

struct pddl_symbolic_all_closed {
    pddl_bdd_t *closed;
    pddl_cost_t g_value;
    pddl_rbtree_node_t rbtree;
};
typedef struct pddl_symbolic_all_closed pddl_symbolic_all_closed_t;

struct pddl_symbolic_states {
    pddl_extarr_t *pool; /*!< Data pool */
    int num_states; /*!< Number of states stored in .pool */
    pddl_pairheap_t *open; /*!< Open list */
    pddl_pairheap_t *open_cost; /*!< Costs of states in the open list */
    pddl_rbtree_t *closed; /*!< Closed states stored with increasing cost */
    int num_closed; /*!< Number of closed states */
    pddl_bdd_t *all_closed; /*!< BDD representing all closed states */
    pddl_rbtree_t *all_closed_g; /*!< All closed states for each g-value */
    pddl_cost_t bound; /*!< Bound for the cost of the plan */
};
typedef struct pddl_symbolic_states pddl_symbolic_states_t;

/**
 * Initializes a state space.
 */
void pddlSymbolicStatesInit(pddl_symbolic_states_t *states,
                            pddl_bdd_manager_t *mgr,
                            int use_heur_inconsistent,
                            pddl_err_t *err);

/**
 * Frees allocated memory.
 */
void pddlSymbolicStatesFree(pddl_symbolic_states_t *states,
                            pddl_bdd_manager_t *mgr);

/**
 * Returns a specified symbolic state.
 */
pddl_symbolic_state_t *
    pddlSymbolicStatesGet(pddl_symbolic_states_t *states, int id);

/**
 * Remove all closed states from the given BDD state of given g-value.
 */
void pddlSymbolicStatesRemoveClosedStates(pddl_symbolic_states_t *states,
                                          pddl_bdd_manager_t *mgr,
                                          pddl_bdd_t **state,
                                          const pddl_cost_t *g_value);

/**
 * Close the given state and add it to the set of all closed states.
 */
void pddlSymbolicStatesCloseState(pddl_symbolic_states_t *states,
                                  pddl_bdd_manager_t *mgr,
                                  pddl_symbolic_state_t *state);

/**
 * Open the given state.
 */
void pddlSymbolicStatesOpenState(pddl_symbolic_states_t *states,
                                 pddl_symbolic_state_t *state);

/**
 * Removes from the heap and returns the next open state.
 */
pddl_symbolic_state_t *
    pddlSymbolicStatesNextOpen(pddl_symbolic_states_t *states);

/**
 * Returns the next open state without removing it from the heap.
 */
pddl_symbolic_state_t *
    pddlSymbolicStatesOpenPeek(const pddl_symbolic_states_t *states);

/**
 * Returns the minimum cost (g->value) from all open states.
 */
const pddl_cost_t *
    pddlSymbolicStatesMinOpenCost(const pddl_symbolic_states_t *states);

/**
 * Adds a new empty state.
 */
pddl_symbolic_state_t *pddlSymbolicStatesAdd(pddl_symbolic_states_t *states);

/**
 * Adds a new state with the corresponding BDD
 */
pddl_symbolic_state_t *pddlSymbolicStatesAddBDD(pddl_symbolic_states_t *states,
                                                pddl_bdd_manager_t *mgr,
                                                pddl_bdd_t *bdd);

/**
 * Adds the initial state with the given heuristic value (if non-NULL).
 */
void pddlSymbolicStatesAddInit(pddl_symbolic_states_t *states,
                               pddl_bdd_manager_t *mgr,
                               pddl_bdd_t *bdd,
                               const pddl_cost_t *heur);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_SYMBOLIC_STATE_H__ */
