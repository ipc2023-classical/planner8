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

#include "internal.h"
#include "pddl/hfunc.h"
#include "pddl/fdr_state_space.h"


#define PAGESIZE_MULTIPLY 1024
#define MIN_STATES_PER_BLOCK (1024 * 1024)


struct state_node {
    pddl_state_id_t parent_id; /*!< ID of the parent state */
    int op_id:30; /*!< ID of the operator reaching this state */
    pddl_fdr_state_space_status_t status:2;/*!< PDDL_FDR_STATE_SPACE_STATUS_* */
    int g_value; /*!< Cost of the path from init to this state */
} pddl_packed;
typedef struct state_node state_node_t;

void pddlFDRStateSpaceInit(pddl_fdr_state_space_t *state_space,
                           const pddl_fdr_vars_t *vars,
                           pddl_err_t *err)
{
    ZEROIZE(state_space);
    pddlFDRStatePoolInit(&state_space->state_pool, vars, err);
    state_space->node = pddlExtArrNew2(sizeof(state_node_t), PAGESIZE_MULTIPLY,
                                       MIN_STATES_PER_BLOCK,
                                       NULL, NULL);

    PDDL_INFO(err, "State space created. bytes per state node: %d",
              (int)sizeof(state_node_t));
}

void pddlFDRStateSpaceFree(pddl_fdr_state_space_t *state_space)
{
    if (state_space->node != NULL)
        pddlExtArrDel(state_space->node);
    pddlFDRStatePoolFree(&state_space->state_pool);
}

pddl_state_id_t pddlFDRStateSpaceInsert(pddl_fdr_state_space_t *state_space,
                                        const int *state)
{
    pddl_state_id_t id, num;

    num = state_space->state_pool.num_states;
    id = pddlFDRStatePoolInsert(&state_space->state_pool, state);
    ASSERT(id <= num);
    if (id == num){
        state_node_t *sn = pddlExtArrGet(state_space->node, id);
        sn->parent_id = PDDL_NO_STATE_ID;
        sn->op_id = -1;
        sn->status = PDDL_FDR_STATE_SPACE_STATUS_NEW;
        sn->g_value = -1;
    }
    return id;
}

static void getNoState(const pddl_fdr_state_space_t *state_space,
                       pddl_state_id_t state_id,
                       const state_node_t *sn,
                       pddl_fdr_state_space_node_t *node)
{
    node->id = state_id;
    node->parent_id = sn->parent_id;
    node->op_id = sn->op_id;
    node->g_value = sn->g_value;
    node->status = sn->status;
}

void pddlFDRStateSpaceGet(const pddl_fdr_state_space_t *state_space,
                          pddl_state_id_t state_id,
                          pddl_fdr_state_space_node_t *node)
{
    const state_node_t *sn = pddlExtArrGet(state_space->node, state_id);
    getNoState(state_space, state_id, sn, node);
    pddlFDRStatePoolGet(&state_space->state_pool, state_id, node->state);
}

void pddlFDRStateSpaceGetNoState(const pddl_fdr_state_space_t *state_space,
                                 pddl_state_id_t state_id,
                                 pddl_fdr_state_space_node_t *node)
{
    ASSERT_RUNTIME(state_id >= 0
                   && state_id < state_space->state_pool.num_states);
    const state_node_t *sn = pddlExtArrGet(state_space->node, state_id);
    getNoState(state_space, state_id, sn, node);
}

void pddlFDRStateSpaceSet(pddl_fdr_state_space_t *state_space,
                          const pddl_fdr_state_space_node_t *node)
{
    ASSERT_RUNTIME(node->id >= 0
                   && node->id < state_space->state_pool.num_states);
    state_node_t *sn = pddlExtArrGet(state_space->node, node->id);
    sn->parent_id = node->parent_id;
    sn->op_id = node->op_id;
    sn->g_value = node->g_value;
    sn->status = node->status;
}


void pddlFDRStateSpaceNodeInit(pddl_fdr_state_space_node_t *node,
                               const pddl_fdr_state_space_t *state_space)
{
    ZEROIZE(node);
    node->var_size = state_space->state_pool.packer.num_vars;
    node->state = CALLOC_ARR(int, node->var_size);
}

void pddlFDRStateSpaceNodeFree(pddl_fdr_state_space_node_t *node)
{
    if (node->state != NULL)
        FREE(node->state);
}
