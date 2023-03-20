/***
 * cpddl
 * -------
 * Copyright (c)2019 Daniel Fiser <danfis@danfis.cz>,
 * Agent Technology Center, Department of Computer Science,
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
#ifndef COST_SIZE
# error "COST_SIZE must be set!"
#endif

#include "pddl/fifo.h"
#include "pddl/open_list.h"
#include "internal.h"

/** A structure containing a stored value */
struct node {
    pddl_state_id_t state_id;
};
typedef struct node node_t;

/** A node holding a key and all the values. */
struct keynode {
    pddl_fifo_t fifo;              /*!< Structure containing all values */
    struct keynode *spe_left;  /*!< Connector to splay-tree */
    struct keynode *spe_right; /*!< Connector to splay-tree */
    int cost[COST_SIZE];
};
typedef struct keynode keynode_t;

/** Main structure */
struct pddl_open_list_splaytree {
    pddl_open_list_t list;
    keynode_t *root;        /*!< Root of splay-tree */
    keynode_t *pre_keynode; /*!< Preinitialized key-node */
};
typedef struct pddl_open_list_splaytree pddl_open_list_splaytree_t;

#define LIST_FROM_PARENT(parent) \
    pddl_container_of(parent, pddl_open_list_splaytree_t, list)


static void pddlOpenListSplayTreeDel(pddl_open_list_t *list);
static void pddlOpenListSplayTreePush(pddl_open_list_t *list,
                                      const int *cost,
                                      pddl_state_id_t state_id);
static int pddlOpenListSplayTreePop(pddl_open_list_t *list,
                                    pddl_state_id_t *state_id,
                                    int *cost);
static int pddlOpenListSplayTreeTop(pddl_open_list_t *list,
                                    pddl_state_id_t *state_id,
                                    int *cost);
static void pddlOpenListSplayTreeClear(pddl_open_list_t *list);


static keynode_t *keynodeNew(void)
{
    keynode_t *kn;
    kn = MALLOC(sizeof(keynode_t));
    pddlFifoInit(&kn->fifo, sizeof(node_t));
    return kn;
}

static void keynodeDel(keynode_t *kn)
{
    pddlFifoFree(&kn->fifo);
    FREE(kn);
}

_pddl_inline int keynodeCmp(const int *kn1, const int *kn2)
{
    int cmp;
    for (int i = 0; i < COST_SIZE && (cmp = kn1[i] - kn2[i]) == 0; ++i);
    return cmp;
}

/** Define splay-tree structure */
#define PDDL_SPLAY_TREE_NODE_T keynode_t
#define PDDL_SPLAY_TREE_T pddl_open_list_splaytree_t
#define PDDL_SPLAY_KEY_T const int *
#define PDDL_SPLAY_NODE_KEY(node) node->cost
#define PDDL_SPLAY_NODE_SET_KEY(head, node, key) \
    memcpy(node->cost, key, sizeof(int) * COST_SIZE)
#define PDDL_SPLAY_KEY_CMP(head, key1, key2) \
    keynodeCmp(key1, key2)
#include "splaytree_def.h"

pddl_open_list_t *MAIN_FN_NAME(void)
{
    pddl_open_list_splaytree_t *list;

    list = ALLOC(pddl_open_list_splaytree_t);
    _pddlOpenListInit(&list->list,
                      pddlOpenListSplayTreeDel,
                      pddlOpenListSplayTreePush,
                      pddlOpenListSplayTreePop,
                      pddlOpenListSplayTreeTop,
                      pddlOpenListSplayTreeClear);
    list->pre_keynode = keynodeNew();

    pddlSplayInit(list);

    return &list->list;
}

static void pddlOpenListSplayTreeDel(pddl_open_list_t *_list)

{
    pddl_open_list_splaytree_t *list = LIST_FROM_PARENT(_list);
    pddlOpenListSplayTreeClear(&list->list);
    if (list->pre_keynode)
        keynodeDel(list->pre_keynode);
    pddlSplayFree(list);
    _pddlOpenListFree(&list->list);
    FREE(list);
}

static void pddlOpenListSplayTreePush(pddl_open_list_t *_list,
                                      const int *cost,
                                      pddl_state_id_t state_id)
{
    pddl_open_list_splaytree_t *list = LIST_FROM_PARENT(_list);
    keynode_t *kn;
    node_t node;

    // Try to insert pre-allocated key-node
    kn = pddlSplayInsert(list, cost, list->pre_keynode);

    if (kn == NULL){
        // Insertion was successful, remember the inserted key-node and
        // preallocate next key-node for next time.
        kn = list->pre_keynode;
        list->pre_keynode = keynodeNew();
    }

    // Push next node into key-node container
    node.state_id = state_id;
    pddlFifoPush(&kn->fifo, &node);
}

static keynode_t *top(pddl_open_list_t *_list,
                      pddl_state_id_t *state_id,
                      int *cost)
{
    pddl_open_list_splaytree_t *list = LIST_FROM_PARENT(_list);
    keynode_t *kn;
    node_t *n;

    if (list->root == NULL)
        return NULL;

    // Find out minimal node
    kn = pddlSplayMin(list);

    // We know for sure that this key-node must contain some nodes because
    // an empty key-nodes are removed immediately.
    // Pop next node from the key-node.
    n = pddlFifoFront(&kn->fifo);
    *state_id = n->state_id;
    memcpy(cost, kn->cost, sizeof(int) * COST_SIZE);
    return kn;
}

static int pddlOpenListSplayTreePop(pddl_open_list_t *_list,
                                    pddl_state_id_t *state_id,
                                    int *cost)
{
    pddl_open_list_splaytree_t *list = LIST_FROM_PARENT(_list);
    keynode_t *kn = top(_list, state_id, cost);
    if (kn == NULL)
        return -1;

    pddlFifoPop(&kn->fifo);

    // If the key-node is empty, remove it from the tree
    if (pddlFifoEmpty(&kn->fifo)){
        pddlSplayRemove(list, kn);
        keynodeDel(kn);
    }

    return 0;
}

static int pddlOpenListSplayTreeTop(pddl_open_list_t *_list,
                                    pddl_state_id_t *state_id,
                                    int *cost)
{
    if (top(_list, state_id, cost) == NULL)
        return -1;
    return 0;
}

static void pddlOpenListSplayTreeClear(pddl_open_list_t *_list)
{
    pddl_open_list_splaytree_t *list = LIST_FROM_PARENT(_list);
    keynode_t *kn;

    while (list->root){
        kn = list->root;
        pddlSplayRemove(list, list->root);
        keynodeDel(kn);
    }
}

