/***
 * Copyright (c)2022 Daniel Fiser <danfis@danfis.cz>,
 * All rights reserved.
 *
 * This file is part of cpddl.
 *
 * Distributed under the OSI-approved BSD License (the "License");
 * see accompanying file BDS-LICENSE for details or see
 * <http://www.opensource.org/licenses/bsd-license.php>.
 *
 * This software is distributed WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the License for more information.
 */

#include "pddl/rbtree.h"
#include "internal.h"


#define RB_NODE pddl_rbtree_node_t
#define RB_TREE pddl_rbtree_t
#define RB_DEL pddlRBTreeDel
#define RB_INSERT RB_NODE *pddlRBTreeInsert(RB_TREE *rbtree, RB_NODE *n)
#define RB_REMOVE pddlRBTreeRemove
#define RB_SET_UP_KEY
#define RB_CMP rbtree->cmp(n, parent, rbtree->data)
#define RB_CP(dst, src) *(dst) = *(src);

#define RB_BLACK 0
#define RB_RED   1

#define RB_LEFT(node)   (node)->rbe_left
#define RB_RIGHT(node)  (node)->rbe_right
#define RB_PARENT(node) (node)->rbe_parent
#define RB_COLOR(node)  (node)->rbe_color

#define RB_SET_RED(node)   (RB_COLOR(node) = RB_RED)
#define RB_SET_BLACK(node) (RB_COLOR(node) = RB_BLACK)
#define RB_IS_RED(node)    (RB_COLOR(node) == RB_RED)
#define RB_IS_BLACK(node)  (RB_COLOR(node) == RB_BLACK)
#define RB_COPY_COLOR(dst, src) ((dst)->rbe_color = (src)->rbe_color)

#include "_rbtree.c"

RB_TREE *pddlRBTreeNew(pddl_rbtree_cmp cmp, void *data)
{
    RB_TREE *rb;

    rb = ALLOC(RB_TREE);
    pddlRBTreeInit(rb, cmp, data);

    return rb;
}

void pddlRBTreeInit(pddl_rbtree_t *rb, pddl_rbtree_cmp cmp, void *data)
{
    rb->root = NULL;
    rb->cmp  = cmp;
    rb->data = data;
}

void pddlRBTreeFree(pddl_rbtree_t *rb)
{
    rb->root = NULL;
    rb->cmp  = NULL;
    rb->data = NULL;
}

