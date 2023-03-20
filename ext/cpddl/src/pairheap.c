/***
 * Copyright (c)2011 Daniel Fiser <danfis@danfis.cz>
 *
 *  This file is part of cpddl.
 *
 *  Distributed under the OSI-approved BSD License (the "License");
 *  see accompanying file BDS-LICENSE for details or see
 *  <http://www.opensource.org/licenses/bsd-license.php>.
 *
 *  This software is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the License for more information.
 */

#include "pddl/pairheap.h"
#include "internal.h"

pddl_pairheap_t *pddlPairHeapNew(pddl_pairheap_lt less_than, void *data)
{
    pddl_pairheap_t *ph;

    ph = ALLOC(pddl_pairheap_t);
    pddlListInit(&ph->root);
    ph->lt = less_than;
    ph->data = data;

    return ph;
}

void pddlPairHeapDel(pddl_pairheap_t *ph)
{
    FREE(ph);
}

void pddlPairHeapRemove(pddl_pairheap_t *ph, pddl_pairheap_node_t *n)
{
    pddl_pairheap_node_t *c;
    pddl_list_t *list, *item, *item_tmp;

    list = &n->children;
    PDDL_LIST_FOR_EACH_SAFE(list, item, item_tmp){
        c = PDDL_LIST_ENTRY(item, pddl_pairheap_node_t, list);

        // remove from n
        pddlListDel(&c->list);
        // add it to root list
        pddlListAppend(&ph->root, &c->list);
    }

    // remove n itself
    pddlListDel(&n->list);
}

void __pddlPairHeapConsolidate(pddl_pairheap_t *ph)
{
    pddl_list_t *root, *item, *item_next;
    pddl_pairheap_node_t *n1, *n2;

    root = &ph->root;

    // 1. First pairing from left to righ
    item = pddlListNext(root);
    item_next = pddlListNext(item);
    while (item != root && item_next != root){
        // get nodes
        n1 = PDDL_LIST_ENTRY(item, pddl_pairheap_node_t, list);
        n2 = PDDL_LIST_ENTRY(item_next, pddl_pairheap_node_t, list);

        // compare them
        if (ph->lt(n1, n2, ph->data)){ // n1 < n2
            pddlListDel(&n2->list);
            pddlListAppend(&n1->children, &n2->list);
            item = pddlListNext(&n1->list);
        }else{
            pddlListDel(&n1->list);
            pddlListAppend(&n2->children, &n1->list);
            item = pddlListNext(&n2->list);
        }

        item_next = pddlListNext(item);
    }

    // 2. Finish mergin from right to left
    // To be honest, I really don't understand should it be from right to
    // left, so let's do it ordinary way...
    item = pddlListNext(root);
    item_next = pddlListNext(item);
    while (item != root && item_next != root){
        // get nodes
        n1 = PDDL_LIST_ENTRY(item, pddl_pairheap_node_t, list);
        n2 = PDDL_LIST_ENTRY(item_next, pddl_pairheap_node_t, list);

        if (ph->lt(n1, n2, ph->data)){ // n1 < n2
            pddlListDel(&n2->list);
            pddlListAppend(&n1->children, &n2->list);
        }else{
            pddlListDel(&n1->list);
            pddlListAppend(&n2->children, &n1->list);
            item = item_next;
        }
        item_next = pddlListNext(item);
    }
}

static void recursiveClear(pddl_pairheap_node_t *node,
                           pddl_pairheap_clear clear_fn,
                           void *user_data)
{
    pddl_list_t *lnode, *ltmp;
    pddl_pairheap_node_t *ch_node;

    PDDL_LIST_FOR_EACH_SAFE(&node->children, lnode, ltmp){
        ch_node = PDDL_LIST_ENTRY(lnode, pddl_pairheap_node_t, list);
        recursiveClear(ch_node, clear_fn, user_data);
    }

    pddlListDel(&node->list);
    clear_fn(node, user_data);
}

void pddlPairHeapClear(pddl_pairheap_t *ph,
                       pddl_pairheap_clear clear_fn,
                       void *user_data)
{
    pddl_list_t *lnode, *ltmp;
    pddl_pairheap_node_t *node;

    PDDL_LIST_FOR_EACH_SAFE(&ph->root, lnode, ltmp){
        node = PDDL_LIST_ENTRY(lnode, pddl_pairheap_node_t, list);
        recursiveClear(node, clear_fn, user_data);
    }
}
