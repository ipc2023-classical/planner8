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

#include "pddl/hfunc.h"
#include "pddl/strips_state_space.h"
#include "internal.h"


#define PAGESIZE_MULTIPLY 1024
#define MIN_STATES_PER_BLOCK (1024 * 1024)


struct state_node {
    pddl_state_id_t id;
    pddl_state_id_t parent_id; /*!< ID of the parent state */
    int op_id:30; /*!< ID of the operator reaching this state */
    pddl_strips_state_space_status_t status:2;/*!< PDDL_STRIPS_STATE_SPACE_STATUS_* */
    int g_value; /*!< Cost of the path from init to this state */
    pddl_iset_t state;

    pddl_list_t htable;
    pddl_htable_key_t hash;
};
typedef struct state_node state_node_t;

static pddl_htable_key_t stateHash(const pddl_iset_t *state)
{
    return pddlCityHash_64(state->s, sizeof(int) * state->size);
}

static pddl_htable_key_t htableHash(const pddl_list_t *k, void *_)
{
    const state_node_t *sn = PDDL_LIST_ENTRY(k, state_node_t, htable);
    return sn->hash;
}

static int htableEq(const pddl_list_t *k1, const pddl_list_t *k2, void *_)
{
    const state_node_t *sn1 = PDDL_LIST_ENTRY(k1, state_node_t, htable);
    const state_node_t *sn2 = PDDL_LIST_ENTRY(k2, state_node_t, htable);
    return pddlISetEq(&sn1->state, &sn2->state);
}

void pddlStripsStateSpaceInit(pddl_strips_state_space_t *state_space,
                              pddl_err_t *err)
{
    ZEROIZE(state_space);
    state_space->htable = pddlHTableNew(htableHash, htableEq, NULL);
    state_space->node = pddlExtArrNew2(sizeof(state_node_t), PAGESIZE_MULTIPLY,
                                       MIN_STATES_PER_BLOCK,
                                       NULL, NULL);

    PDDL_INFO(err, "State space created. bytes per state node: %d",
              (int)sizeof(state_node_t));
}

void pddlStripsStateSpaceFree(pddl_strips_state_space_t *state_space)
{
    for (int id = 0; id < state_space->num_states; ++id){
        state_node_t *sn = pddlExtArrGet(state_space->node, id);
        pddlISetFree(&sn->state);
    }
    if (state_space->htable != NULL)
        pddlHTableDel(state_space->htable);
    if (state_space->node != NULL)
        pddlExtArrDel(state_space->node);
}

pddl_state_id_t pddlStripsStateSpaceInsert(
                            pddl_strips_state_space_t *state_space,
                            const pddl_iset_t *state)
{
    state_node_t *sn = pddlExtArrGet(state_space->node, state_space->num_states);
    ZEROIZE(sn);
    pddlISetSet(&sn->state, state);
    sn->hash = stateHash(state);

    pddl_list_t *snl = pddlHTableInsertUnique(state_space->htable, &sn->htable);
    if (snl == NULL){
        sn->id = state_space->num_states++;
        sn->parent_id = PDDL_NO_STATE_ID;
        sn->op_id = -1;
        sn->status = PDDL_STRIPS_STATE_SPACE_STATUS_NEW;
        sn->g_value = -1;
    }else{
        pddlISetFree(&sn->state);
        sn = PDDL_LIST_ENTRY(snl, state_node_t, htable);
    }
    return sn->id;
}

static void getNoState(const pddl_strips_state_space_t *state_space,
                       pddl_state_id_t state_id,
                       const state_node_t *sn,
                       pddl_strips_state_space_node_t *node)
{
    node->id = state_id;
    node->parent_id = sn->parent_id;
    node->op_id = sn->op_id;
    node->g_value = sn->g_value;
    node->status = sn->status;
}

void pddlStripsStateSpaceGet(const pddl_strips_state_space_t *state_space,
                             pddl_state_id_t state_id,
                             pddl_strips_state_space_node_t *node)
{
    const state_node_t *sn = pddlExtArrGet(state_space->node, state_id);
    getNoState(state_space, state_id, sn, node);
    pddlISetSet(&node->state, &sn->state);
}

void pddlStripsStateSpaceGetNoState(const pddl_strips_state_space_t *state_space,
                                    pddl_state_id_t state_id,
                                    pddl_strips_state_space_node_t *node)
{
    ASSERT_RUNTIME(state_id >= 0
                   && state_id < state_space->num_states);
    const state_node_t *sn = pddlExtArrGet(state_space->node, state_id);
    getNoState(state_space, state_id, sn, node);
}

void pddlStripsStateSpaceSet(pddl_strips_state_space_t *state_space,
                             const pddl_strips_state_space_node_t *node)
{
    ASSERT_RUNTIME(node->id >= 0
                   && node->id < state_space->num_states);
    state_node_t *sn = pddlExtArrGet(state_space->node, node->id);
    sn->parent_id = node->parent_id;
    sn->op_id = node->op_id;
    sn->g_value = node->g_value;
    sn->status = node->status;
}


void pddlStripsStateSpaceNodeInit(pddl_strips_state_space_node_t *node,
                                  const pddl_strips_state_space_t *state_space)
{
    ZEROIZE(node);
    pddlISetInit(&node->state);
}

void pddlStripsStateSpaceNodeFree(pddl_strips_state_space_node_t *node)
{
    pddlISetFree(&node->state);
}
