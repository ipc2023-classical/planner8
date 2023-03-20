/***
 * cpddl
 * -------
 * Copyright (c)2020 Daniel Fiser <danfis@danfis.cz>,
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

#include "pddl/open_list.h"
#include "pddl/fdr_app_op.h"
#include "search.h"
#include "internal.h"

struct pddl_search_lazy {
    pddl_search_t search;
    const pddl_fdr_t *fdr;
    pddl_heur_t *heur;
    pddl_err_t *err;
    pddl_fdr_state_space_t state_space;
    pddl_open_list_t *list;
    pddl_fdr_app_op_t app_op;

    pddl_state_id_t goal_state_id;

    pddl_iset_t applicable;
    pddl_fdr_state_space_node_t cur_node;
    pddl_fdr_state_space_node_t next_node;
    pddl_search_stat_t _stat;
};
typedef struct pddl_search_lazy pddl_search_lazy_t;

static void pddlSearchLazyDel(pddl_search_t *s);
static int pddlSearchLazyInitStep(pddl_search_t *s);
static int pddlSearchLazyStep(pddl_search_t *s);
static int pddlSearchLazyExtractPlan(pddl_search_t *s, pddl_plan_t *plan);
static void pddlSearchLazyStat(const pddl_search_t *s,
                               pddl_search_stat_t *stat);


pddl_search_t *pddlSearchLazy(const pddl_fdr_t *fdr,
                                   pddl_heur_t *heur,
                                   pddl_err_t *err)
{
    pddl_search_lazy_t *lazy;

    lazy = ZALLOC(pddl_search_lazy_t);
    _pddlSearchInit(&lazy->search,
                    pddlSearchLazyDel,
                    pddlSearchLazyInitStep,
                    pddlSearchLazyStep,
                    pddlSearchLazyExtractPlan,
                    pddlSearchLazyStat);
    lazy->fdr = fdr;
    lazy->heur = heur;
    lazy->err = err;

    pddlFDRStateSpaceInit(&lazy->state_space, &fdr->var, err);
    lazy->list = pddlOpenListSplayTree1();

    pddlFDRAppOpInit(&lazy->app_op, &fdr->var, &fdr->op, &fdr->goal);

    lazy->goal_state_id = PDDL_NO_STATE_ID;

    pddlISetInit(&lazy->applicable);
    pddlFDRStateSpaceNodeInit(&lazy->cur_node, &lazy->state_space);
    pddlFDRStateSpaceNodeInit(&lazy->next_node, &lazy->state_space);

    return &lazy->search;
}

static void pddlSearchLazyDel(pddl_search_t *s)
{
    pddl_search_lazy_t *lazy = pddl_container_of(s, pddl_search_lazy_t, search);
    pddlFDRAppOpFree(&lazy->app_op);
    if (lazy->list)
        pddlOpenListDel(lazy->list);
    pddlFDRStateSpaceNodeFree(&lazy->cur_node);
    pddlFDRStateSpaceNodeFree(&lazy->next_node);
    pddlFDRStateSpaceFree(&lazy->state_space);
    pddlISetFree(&lazy->applicable);
    FREE(lazy);
}

static void push(pddl_search_lazy_t *lazy,
                 pddl_fdr_state_space_node_t *node,
                 int h_value)
{
    int cost;
    cost = h_value;
    if (node->status == PDDL_FDR_STATE_SPACE_STATUS_CLOSED)
        --lazy->_stat.closed;
    node->status = PDDL_FDR_STATE_SPACE_STATUS_OPEN;
    pddlOpenListPush(lazy->list, &cost, node->id);
    ++lazy->_stat.open;
}

static int pddlSearchLazyInitStep(pddl_search_t *s)
{
    pddl_search_lazy_t *lazy = pddl_container_of(s, pddl_search_lazy_t, search);
    int ret = PDDL_SEARCH_CONT;
    pddl_state_id_t state_id;
    state_id = pddlFDRStateSpaceInsert(&lazy->state_space, lazy->fdr->init);
    ASSERT_RUNTIME(state_id == 0);
    pddlFDRStateSpaceGet(&lazy->state_space, state_id, &lazy->cur_node);
    lazy->cur_node.parent_id = PDDL_NO_STATE_ID;
    lazy->cur_node.op_id = -1;
    lazy->cur_node.g_value = 0;

    int h_value = pddlHeurEstimate(lazy->heur,
                                   &lazy->cur_node,
                                   &lazy->state_space);
    PDDL_INFO(lazy->err, "Heuristic value for the initial state: %d", h_value);
    ++lazy->_stat.evaluated;
    if (h_value == PDDL_COST_DEAD_END){
        ++lazy->_stat.dead_end;
        ret = PDDL_SEARCH_UNSOLVABLE;
    }

    ASSERT_RUNTIME(lazy->cur_node.status == PDDL_FDR_STATE_SPACE_STATUS_NEW);
    push(lazy, &lazy->cur_node, h_value);
    pddlFDRStateSpaceSet(&lazy->state_space, &lazy->cur_node);
    return ret;
}

static int isGoal(const pddl_search_lazy_t *lazy)
{
    return pddlFDRPartStateIsConsistentWithState(&lazy->fdr->goal,
                                                 lazy->cur_node.state);
}

static void insertNextState(pddl_search_lazy_t *lazy,
                            const pddl_fdr_op_t *op,
                            int h_value)
{
    // Compute its g() value
    int next_g_value = lazy->cur_node.g_value + op->cost;

    // Skip if we have better state already
    if (lazy->next_node.status != PDDL_FDR_STATE_SPACE_STATUS_NEW
            && lazy->next_node.g_value <= next_g_value){
        return;
    }
    if (lazy->next_node.status == PDDL_FDR_STATE_SPACE_STATUS_CLOSED)
        return;

    lazy->next_node.parent_id = lazy->cur_node.id;
    lazy->next_node.op_id = op->id;
    lazy->next_node.g_value = next_g_value;

    if (lazy->next_node.status == PDDL_FDR_STATE_SPACE_STATUS_NEW
            || lazy->next_node.status == PDDL_FDR_STATE_SPACE_STATUS_OPEN){
        push(lazy, &lazy->next_node, h_value);
    }

    pddlFDRStateSpaceSet(&lazy->state_space, &lazy->next_node);
}

static int pddlSearchLazyStep(pddl_search_t *s)
{
    pddl_search_lazy_t *lazy = pddl_container_of(s, pddl_search_lazy_t, search);

    ++lazy->_stat.steps;

    // Get next state from open list
    int cur_cost[2];
    pddl_state_id_t cur_state_id;
    if (pddlOpenListPop(lazy->list, &cur_state_id, cur_cost) != 0)
        return PDDL_SEARCH_UNSOLVABLE;

    // Load the current state
    pddlFDRStateSpaceGet(&lazy->state_space, cur_state_id, &lazy->cur_node);

    // Skip already closed nodes
    if (lazy->cur_node.status != PDDL_FDR_STATE_SPACE_STATUS_OPEN)
        return PDDL_SEARCH_CONT;

    // Close the current node
    lazy->cur_node.status = PDDL_FDR_STATE_SPACE_STATUS_CLOSED;
    pddlFDRStateSpaceSet(&lazy->state_space, &lazy->cur_node);
    --lazy->_stat.open;
    ++lazy->_stat.closed;

    // Check whether it is a goal
    if (isGoal(lazy)){
        lazy->goal_state_id = cur_state_id;
        return PDDL_SEARCH_FOUND;
    }

    // Find all applicable operators
    pddlISetEmpty(&lazy->applicable);
    pddlFDRAppOpFind(&lazy->app_op, lazy->cur_node.state, &lazy->applicable);
    ++lazy->_stat.expanded;

    // Compute heuristic value
    int h_value = pddlHeurEstimate(lazy->heur,
                                   &lazy->cur_node,
                                   &lazy->state_space);
    ++lazy->_stat.evaluated;
    if (h_value == PDDL_COST_DEAD_END){
        ++lazy->_stat.dead_end;
        if (lazy->cur_node.status == PDDL_FDR_STATE_SPACE_STATUS_OPEN)
            --lazy->_stat.open;
        lazy->cur_node.status = PDDL_FDR_STATE_SPACE_STATUS_CLOSED;
        ++lazy->_stat.closed;
        pddlFDRStateSpaceSet(&lazy->state_space, &lazy->cur_node);
        return PDDL_SEARCH_CONT;
    }

    int op_id;
    PDDL_ISET_FOR_EACH(&lazy->applicable, op_id){
        const pddl_fdr_op_t *op = lazy->fdr->op.op[op_id];

        // Create a new state
        pddlFDROpApplyOnState(op, lazy->next_node.var_size,
                              lazy->cur_node.state,
                              lazy->next_node.state);

        // Insert the new state
        pddl_state_id_t next_state_id;
        next_state_id = pddlFDRStateSpaceInsert(&lazy->state_space,
                                                lazy->next_node.state);
        pddlFDRStateSpaceGetNoState(&lazy->state_space,
                                    next_state_id, &lazy->next_node);
        insertNextState(lazy, op, h_value);
    }
    return PDDL_SEARCH_CONT;
}

static int pddlSearchLazyExtractPlan(pddl_search_t *s, pddl_plan_t *plan)
{
    pddl_search_lazy_t *lazy
        = pddl_container_of(s, pddl_search_lazy_t, search);
    if (lazy->goal_state_id == PDDL_NO_STATE_ID)
        return -1;
    pddlPlanLoadBacktrack(plan, lazy->goal_state_id, &lazy->state_space);
    return 0;
}

static void pddlSearchLazyStat(const pddl_search_t *s,
                               pddl_search_stat_t *stat)
{
    pddl_search_lazy_t *lazy = pddl_container_of(s, pddl_search_lazy_t, search);
    *stat = lazy->_stat;
    stat->generated = lazy->state_space.state_pool.num_states;
}

