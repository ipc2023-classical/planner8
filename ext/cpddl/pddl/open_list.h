/***
 * cpddl
 * -------
 * Copyright (c)2015 Daniel Fiser <danfis@danfis.cz>,
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

#ifndef __PDDL_OPEN_LIST_H__
#define __PDDL_OPEN_LIST_H__

#include <pddl/common.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct pddl_open_list pddl_open_list_t;

/**
 * Destructor
 */
typedef void (*pddl_open_list_del_fn)(pddl_open_list_t *list);

/**
 * Inserts a state ID with a corresponding cost(s).
 */
typedef void (*pddl_open_list_push_fn)(pddl_open_list_t *list,
                                       const int *cost,
                                       pddl_state_id_t state_id);

/**
 * Pops element with lowest cost from the list.
 * Returns 0 on success, -1 if the list is empty.
 */
typedef int (*pddl_open_list_pop_fn)(pddl_open_list_t *list,
                                     pddl_state_id_t *state_id,
                                     int *cost);

/**
 * Peeks at the top of the list and returns an element with the lowest
 * cost.
 * Returns 0 on success, -1 if the list is empty.
 */
typedef int (*pddl_open_list_top_fn)(pddl_open_list_t *list,
                                     pddl_state_id_t *state_id,
                                     int *cost);

/**
 * Removes all elements from the list.
 */
typedef void (*pddl_open_list_clear_fn)(pddl_open_list_t *list);

struct pddl_open_list {
    pddl_open_list_del_fn del_fn;
    pddl_open_list_push_fn push_fn;
    pddl_open_list_pop_fn pop_fn;
    pddl_open_list_top_fn top_fn;
    pddl_open_list_clear_fn clear_fn;
};

/**
 * Open list based on splay-tree
 */
pddl_open_list_t *pddlOpenListSplayTree1(void);
pddl_open_list_t *pddlOpenListSplayTree2(void);

/**
 * Destroys the list.
 */
_pddl_inline void pddlOpenListDel(pddl_open_list_t *l);

/**
 * Inserts an element with the specified cost into the list.
 */
_pddl_inline void pddlOpenListPush(pddl_open_list_t *list,
                                   const int *cost,
                                   pddl_state_id_t state_id);

/**
 * Pops the next element from the list that has the lowest cost.
 * Returns 0 on success, -1 if the heap is empty.
 */
_pddl_inline int pddlOpenListPop(pddl_open_list_t *list,
                                 pddl_state_id_t *state_id,
                                 int *cost);

/**
 * Peeks at the top of the list.
 * Returns 0 on success, -1 if the heap is empty.
 */
_pddl_inline int pddlOpenListTop(pddl_open_list_t *list,
                                 pddl_state_id_t *state_id,
                                 int *cost);

/**
 * Empties the list.
 */
_pddl_inline void pddlOpenListClear(pddl_open_list_t *list);

/**** INLINES ****/
_pddl_inline void pddlOpenListDel(pddl_open_list_t *l)
{
    l->del_fn(l);
}

_pddl_inline void pddlOpenListPush(pddl_open_list_t *list,
                                   const int *cost,
                                   pddl_state_id_t state_id)
{
    list->push_fn(list, cost, state_id);
}

_pddl_inline int pddlOpenListPop(pddl_open_list_t *list,
                                 pddl_state_id_t *state_id,
                                 int *cost)
{
    return list->pop_fn(list, state_id, cost);
}

_pddl_inline int pddlOpenListTop(pddl_open_list_t *list,
                                 pddl_state_id_t *state_id,
                                 int *cost)
{
    return list->top_fn(list, state_id, cost);
}

_pddl_inline void pddlOpenListClear(pddl_open_list_t *l)
{
    l->clear_fn(l);
}


/**
 * Initializes parent object.
 */
void _pddlOpenListInit(pddl_open_list_t *l,
                       pddl_open_list_del_fn del_fn,
                       pddl_open_list_push_fn push_fn,
                       pddl_open_list_pop_fn pop_fn,
                       pddl_open_list_top_fn top_fn,
                       pddl_open_list_clear_fn clear_fn);

/**
 * Frees resources of parent object.
 */
void _pddlOpenListFree(pddl_open_list_t *l);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_OPEN_LIST_H__ */
