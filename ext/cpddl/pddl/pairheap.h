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

#ifndef __PDDL_PAIRHEAP_H__
#define __PDDL_PAIRHEAP_H__

#include <pddl/list.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * PairHeap - Pairing Heap
 * ========================
 *
 * Fredman, Michael L.; Sedgewick, Robert; Sleator, Daniel D.; Tarjan,
 * Robert E. (1986), "The pairing heap: a new form of self-adjusting heap",
 * Algorithmica 1 (1): 111–129.
 *
 */

/** vvvv */
struct _pddl_pairheap_node_t {
    pddl_list_t children;             /*!< List of children */
    pddl_list_t list;                 /*!< Connection into list of siblings */
};
typedef struct _pddl_pairheap_node_t pddl_pairheap_node_t;

/**
 * Callback that should return true if {n1} is smaller than {n2}.
 */
typedef int (*pddl_pairheap_lt)(const pddl_pairheap_node_t *n1,
                               const pddl_pairheap_node_t *n2,
                               void *data);

/**
 * Callback for pddlPairHeapClear() function.
 */
typedef void (*pddl_pairheap_clear)(pddl_pairheap_node_t *n,
                                   void *data);

struct _pddl_pairheap_t {
    pddl_list_t root; /*!< List of root nodes. In fact, pairing heap has
                          always one root node, but we need this to make
                          effecient (lazy) merging. */
    pddl_pairheap_lt lt; /*!< "Less than" callback provided by user */
    void *data;
};
typedef struct _pddl_pairheap_t pddl_pairheap_t;
/** ^^^^ */


/**
 * Functions
 * ----------
 */

/**
 * Creates new empty Pairing heap.
 * Callback for comparison must be provided.
 */
pddl_pairheap_t *pddlPairHeapNew(pddl_pairheap_lt less_than, void *data);


/**
 * Deletes pairing heap.
 * Note that individual nodes are not disconnected from heap.
 */
void pddlPairHeapDel(pddl_pairheap_t *ph);

/**
 * Returns true if heap is empty.
 */
_pddl_inline int pddlPairHeapEmpty(const pddl_pairheap_t *ph);

/**
 * Returns minimal node.
 */
_pddl_inline pddl_pairheap_node_t *pddlPairHeapMin(pddl_pairheap_t *ph);

/**
 * Adds node to heap.
 */
_pddl_inline void pddlPairHeapAdd(pddl_pairheap_t *ph, pddl_pairheap_node_t *n);

/**
 * Removes and returns minimal node from heap.
 */
_pddl_inline pddl_pairheap_node_t *pddlPairHeapExtractMin(pddl_pairheap_t *ph);

/**
 * Update position of node in heap in case its value was decreased.
 * If value wasn't decreased (or you are not sure) call pddlPairHeapUpdate()
 * instead.
 */
_pddl_inline void pddlPairHeapDecreaseKey(pddl_pairheap_t *ph, pddl_pairheap_node_t *n);

/**
 * Updates position of node in heap.
 */
_pddl_inline void pddlPairHeapUpdate(pddl_pairheap_t *ph, pddl_pairheap_node_t *n);

/**
 * Del node from heap.
 */
void pddlPairHeapRemove(pddl_pairheap_t *ph, pddl_pairheap_node_t *n);

/**
 * Removes all data from the heap call for each disconnected node a given
 * callback. There is no guarantee for any particular order of the removed
 * nodes.
 */
void pddlPairHeapClear(pddl_pairheap_t *ph,
                      pddl_pairheap_clear clear_fn,
                      void *user_data);


/**
 * Consolidates pairing heap.
 */
void __pddlPairHeapConsolidate(pddl_pairheap_t *ph);


/**** INLINES ****/
_pddl_inline int pddlPairHeapEmpty(const pddl_pairheap_t *ph)
{
    return pddlListEmpty(&ph->root);
}

_pddl_inline pddl_pairheap_node_t *pddlPairHeapMin(pddl_pairheap_t *ph)
{
    pddl_pairheap_node_t *el;
    pddl_list_t *item;

    if (pddlPairHeapEmpty(ph))
        return NULL;

    item = pddlListNext(&ph->root);

    /* if root doesn't contain only one node, heap must be consolidated */
    if (pddlListNext(item) != &ph->root){
        __pddlPairHeapConsolidate(ph);
        item = pddlListNext(&ph->root);
    }

    el = PDDL_LIST_ENTRY(item, pddl_pairheap_node_t, list);
    return el;
}

_pddl_inline void pddlPairHeapAdd(pddl_pairheap_t *ph, pddl_pairheap_node_t *n)
{
    pddlListInit(&n->children);
    pddlListAppend(&ph->root, &n->list);
}

_pddl_inline void pddlPairHeapDecreaseKey(pddl_pairheap_t *ph, pddl_pairheap_node_t *n)
{
    pddlListDel(&n->list);
    pddlListAppend(&ph->root, &n->list);
}

_pddl_inline void pddlPairHeapUpdate(pddl_pairheap_t *ph, pddl_pairheap_node_t *n)
{
    pddlPairHeapRemove(ph, n);
    pddlPairHeapAdd(ph, n);
}

_pddl_inline pddl_pairheap_node_t *pddlPairHeapExtractMin(pddl_pairheap_t *ph)
{
    pddl_pairheap_node_t *n;

    n = pddlPairHeapMin(ph);
    pddlPairHeapRemove(ph, n);

    return n;
}

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_PAIRHEAP_H__ */
