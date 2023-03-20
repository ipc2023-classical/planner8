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

#include "pddl/open_list.h"
#include "pddl/fdr_app_op.h"
#include "search.h"
#include "internal.h"

struct pddl_search_astar {
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
typedef struct pddl_search_astar pddl_search_astar_t;

static void pddlSearchAStarDel(pddl_search_t *s);
static int pddlSearchAStarInitStep(pddl_search_t *s);
static int pddlSearchAStarStep(pddl_search_t *s);
static int pddlSearchAStarExtractPlan(pddl_search_t *s, pddl_plan_t *plan);
static void pddlSearchAStarStat(const pddl_search_t *s,
                                pddl_search_stat_t *stat);


pddl_search_t *pddlSearchAStar(const pddl_fdr_t *fdr,
                               pddl_heur_t *heur,
                               pddl_err_t *err)
{
    pddl_search_astar_t *astar;

    astar = ZALLOC(pddl_search_astar_t);
    _pddlSearchInit(&astar->search,
                    pddlSearchAStarDel,
                    pddlSearchAStarInitStep,
                    pddlSearchAStarStep,
                    pddlSearchAStarExtractPlan,
                    pddlSearchAStarStat);
    astar->fdr = fdr;
    astar->heur = heur;
    astar->err = err;

    pddlFDRStateSpaceInit(&astar->state_space, &fdr->var, err);
    astar->list = pddlOpenListSplayTree2();

    pddlFDRAppOpInit(&astar->app_op, &fdr->var, &fdr->op, &fdr->goal);

    astar->goal_state_id = PDDL_NO_STATE_ID;

    pddlISetInit(&astar->applicable);
    pddlFDRStateSpaceNodeInit(&astar->cur_node, &astar->state_space);
    pddlFDRStateSpaceNodeInit(&astar->next_node, &astar->state_space);

    return &astar->search;
}

static void pddlSearchAStarDel(pddl_search_t *s)
{
    pddl_search_astar_t *astar
        = pddl_container_of(s, pddl_search_astar_t, search);
    pddlFDRAppOpFree(&astar->app_op);
    if (astar->list)
        pddlOpenListDel(astar->list);
    pddlFDRStateSpaceNodeFree(&astar->cur_node);
    pddlFDRStateSpaceNodeFree(&astar->next_node);
    pddlFDRStateSpaceFree(&astar->state_space);
    pddlISetFree(&astar->applicable);
    FREE(astar);
}

static void push(pddl_search_astar_t *astar,
                 pddl_fdr_state_space_node_t *node,
                 int h_value)
{
    int cost[2];
    cost[0] = node->g_value + h_value;
    cost[1] = h_value;
    if (node->status == PDDL_FDR_STATE_SPACE_STATUS_CLOSED)
        --astar->_stat.closed;
    node->status = PDDL_FDR_STATE_SPACE_STATUS_OPEN;
    pddlOpenListPush(astar->list, cost, node->id);
    ++astar->_stat.open;
}

static int pddlSearchAStarInitStep(pddl_search_t *s)
{
    pddl_search_astar_t *astar
        = pddl_container_of(s, pddl_search_astar_t, search);
    int ret = PDDL_SEARCH_CONT;
    pddl_state_id_t state_id;
    state_id = pddlFDRStateSpaceInsert(&astar->state_space, astar->fdr->init);
    ASSERT_RUNTIME(state_id == 0);
    pddlFDRStateSpaceGet(&astar->state_space, state_id, &astar->cur_node);
    astar->cur_node.parent_id = PDDL_NO_STATE_ID;
    astar->cur_node.op_id = -1;
    astar->cur_node.g_value = 0;

    int h_value = pddlHeurEstimate(astar->heur,
                                   &astar->cur_node,
                                   &astar->state_space);
    PDDL_INFO(astar->err, "Heuristic value for the initial state: %d", h_value);
    ++astar->_stat.evaluated;
    if (h_value == PDDL_COST_DEAD_END){
        ++astar->_stat.dead_end;
        ret = PDDL_SEARCH_UNSOLVABLE;
    }

    ASSERT_RUNTIME(astar->cur_node.status == PDDL_FDR_STATE_SPACE_STATUS_NEW);
    push(astar, &astar->cur_node, h_value);
    pddlFDRStateSpaceSet(&astar->state_space, &astar->cur_node);
    return ret;
}

static int isGoal(const pddl_search_astar_t *astar)
{
    return pddlFDRPartStateIsConsistentWithState(&astar->fdr->goal,
                                                 astar->cur_node.state);
}

static void insertNextState(pddl_search_astar_t *astar,
                            const pddl_fdr_op_t *op)
{
    // Compute its g() value
    int next_g_value = astar->cur_node.g_value + op->cost;

    // Skip if we have better state already
    if (astar->next_node.status != PDDL_FDR_STATE_SPACE_STATUS_NEW
            && astar->next_node.g_value <= next_g_value){
        return;
    }

    astar->next_node.parent_id = astar->cur_node.id;
    astar->next_node.op_id = op->id;
    astar->next_node.g_value = next_g_value;

    int h_value = pddlHeurEstimate(astar->heur, &astar->next_node,
                                   &astar->state_space);
    ++astar->_stat.evaluated;

    if (h_value == PDDL_COST_DEAD_END){
        ++astar->_stat.dead_end;
        if (astar->next_node.status == PDDL_FDR_STATE_SPACE_STATUS_OPEN)
            --astar->_stat.open;
        astar->next_node.status = PDDL_FDR_STATE_SPACE_STATUS_CLOSED;
        ++astar->_stat.closed;

    }else if (astar->next_node.status == PDDL_FDR_STATE_SPACE_STATUS_NEW
                || astar->next_node.status == PDDL_FDR_STATE_SPACE_STATUS_OPEN){
        push(astar, &astar->next_node, h_value);

    }else if (astar->next_node.status == PDDL_FDR_STATE_SPACE_STATUS_CLOSED){
        push(astar, &astar->next_node, h_value);
        ++astar->_stat.reopen;
    }

    pddlFDRStateSpaceSet(&astar->state_space, &astar->next_node);
}

static int pddlSearchAStarStep(pddl_search_t *s)
{
    pddl_search_astar_t *astar
        = pddl_container_of(s, pddl_search_astar_t, search);

    ++astar->_stat.steps;

    // Get next state from open list
    int cur_cost[2];
    pddl_state_id_t cur_state_id;
    if (pddlOpenListPop(astar->list, &cur_state_id, cur_cost) != 0)
        return PDDL_SEARCH_UNSOLVABLE;

    // Load the current state
    pddlFDRStateSpaceGet(&astar->state_space, cur_state_id, &astar->cur_node);

    // Skip already closed nodes
    if (astar->cur_node.status != PDDL_FDR_STATE_SPACE_STATUS_OPEN)
        return PDDL_SEARCH_CONT;

    // Close the current node
    astar->cur_node.status = PDDL_FDR_STATE_SPACE_STATUS_CLOSED;
    pddlFDRStateSpaceSet(&astar->state_space, &astar->cur_node);
    --astar->_stat.open;
    ++astar->_stat.closed;
    astar->_stat.last_f_value = cur_cost[0];

    // Check whether it is a goal
    if (isGoal(astar)){
        astar->goal_state_id = cur_state_id;
        return PDDL_SEARCH_FOUND;
    }

    // Find all applicable operators
    pddlISetEmpty(&astar->applicable);
    pddlFDRAppOpFind(&astar->app_op, astar->cur_node.state, &astar->applicable);
    ++astar->_stat.expanded;

    int op_id;
    PDDL_ISET_FOR_EACH(&astar->applicable, op_id){
        const pddl_fdr_op_t *op = astar->fdr->op.op[op_id];

        // Create a new state
        pddlFDROpApplyOnState(op, astar->next_node.var_size,
                              astar->cur_node.state,
                              astar->next_node.state);

        // Insert the new state
        pddl_state_id_t next_state_id;
        next_state_id = pddlFDRStateSpaceInsert(&astar->state_space,
                                                astar->next_node.state);
        pddlFDRStateSpaceGetNoState(&astar->state_space,
                                    next_state_id, &astar->next_node);
        insertNextState(astar, op);
    }
    return PDDL_SEARCH_CONT;
}

static int pddlSearchAStarExtractPlan(pddl_search_t *s, pddl_plan_t *plan)
{
    pddl_search_astar_t *astar
        = pddl_container_of(s, pddl_search_astar_t, search);
    if (astar->goal_state_id == PDDL_NO_STATE_ID)
        return -1;
    pddlPlanLoadBacktrack(plan, astar->goal_state_id, &astar->state_space);
    return 0;
}

static void pddlSearchAStarStat(const pddl_search_t *s,
                                pddl_search_stat_t *stat)
{
    pddl_search_astar_t *astar
        = pddl_container_of(s, pddl_search_astar_t, search);
    *stat = astar->_stat;
    stat->generated = astar->state_space.state_pool.num_states;
}
