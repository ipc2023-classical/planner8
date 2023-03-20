/***
 * Boruvka
 * --------
 * Copyright (c)2010 Daniel Fiser <danfis@danfis.cz>
 *
 *  This file is part of Boruvka.
 *
 *  Distributed under the OSI-approved BSD License (the "License");
 *  see accompanying file BDS-LICENSE for details or see
 *  <http://www.opensource.org/licenses/bsd-license.php>.
 *
 *  This software is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the License for more information.
 */


#ifndef __PDDL_LIST_H__
#define __PDDL_LIST_H__

#include <pddl/common.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * List
 * =====
 */
struct _pddl_list_t {
    struct _pddl_list_t *next, *prev;
};
typedef struct _pddl_list_t pddl_list_t;

/**
 * Simliar struct as pddl_list_t but with extra member "mark" which can be
 * used for marking a particular struct.
 * In your code, you can put this struct into your own struct then use
 * pddlListMAsList() for iterating over list and finaly pddlListMFromList()
 * for backcast to this struct. (In other words retyping from pddl_list_m_t
 * to pddl_list_t is safe!).
 */
struct _pddl_list_m_t {
    struct _pddl_list_m_t *next, *prev;
    int mark;
};
typedef struct _pddl_list_m_t pddl_list_m_t;

/**
 * Example
 * --------
 *
 * First define struct that will hold head of list and struct that will be
 * member of a list.
 *
 * ~~~~~~
 * struct head_t {
 *     int a, b, c;
 *     ...
 *
 *     pddl_list_t head;
 * };
 *
 * struct member_t {
 *     int d, e, f;
 *     ...
 *
 *     pddl_list_t list;
 * };
 * ~~~~~~~
 *
 * Then initialize a list and new memeber can be added.
 * ~~~~~~
 * struct head_t head;
 * struct member_t m1, m2, m3;
 *
 * // init head of list
 * pddlListInit(&head.head);
 *
 * // append members to list
 * pddlListAppend(&head.head, &m1.list);
 * pddlListAppend(&head.head, &m2.list);
 * pddlListAppend(&head.head, &m3.list);
 * ~~~~~~
 *
 * Now you can iterate over list or do anything else.
 * ~~~~~
 * pddl_list_t *item;
 * struct member_t *m;
 *
 * PDDL_LIST_FOR_EACH(&head.head, item){
 *     m = PDDL_LIST_ENTRY(item, struct member_t, list);
 *     ....
 * }
 * ~~~~~
 */

/**
 * Functions and Macros
 * ---------------------
 */


/**
 * Static declaration of a list with initialization.
 * 
 * Example:
 * ~~~~~
 * static PDDL_LIST(name);
 * void main()
 * {
 *     pddl_list_t *item;
 *
 *     PDDL_LIST_FOR_EACH(&name, item){
 *      ...
 *     }
 * }
 */
#define PDDL_LIST(name) \
    pddl_list_t name = { &name, &name }

/**
 * Get the struct for this entry.
 * {ptr}: the &pddl_list_t pointer.
 * {type}: the type of the struct this is embedded in.
 * {member}: the name of the list_struct within the struct.
 */
#define PDDL_LIST_ENTRY(ptr, type, member) \
    pddl_container_of(ptr, type, member)

#define PDDL_LIST_M_ENTRY(ptr, type, member, offset) \
    (type *)((char *)pddl_container_of(ptr, type, member) - (sizeof(pddl_list_m_t) * offset))

/**
 * Iterates over list.
 */
#define PDDL_LIST_FOR_EACH(list, item) \
        for (item = (list)->next; \
             _pddl_prefetch((item)->next), item != (list); \
             item = (item)->next)

/**
 * Iterates over list safe against remove of list entry
 */
#define PDDL_LIST_FOR_EACH_SAFE(list, item, tmp) \
	    for (item = (list)->next, tmp = (item)->next; \
             item != (list); \
		     item = tmp, tmp = (item)->next)

/**
 * TODO
 * Iterates over list of given type.
 * {pos}:    the type * to use as a loop cursor.
 * {head}:   the head for your list.
 * {member}: the name of the list_struct within the struct.
 */
#define PDDL_LIST_FOR_EACH_ENTRY(head, postype, pos, member) \
	for (pos = PDDL_LIST_ENTRY((head)->next, postype, member);	\
	     _pddl_prefetch(pos->member.next), &pos->member != (head); 	\
	     pos = PDDL_LIST_ENTRY(pos->member.next, postype, member))

/**
 * TODO
 * Iterates over list of given type safe against removal of list entry
 * {pos}:	the type * to use as a loop cursor.
 * {n}:		another type * to use as temporary storage
 * {head}:	the head for your list.
 * {member}:the name of the list_struct within the struct.
 */
#define PDDL_LIST_FOR_EACH_ENTRY_SAFE(head, postype, pos, ntype, n, member)         \
    for (pos = PDDL_LIST_ENTRY((head)->next, postype, member),             \
		 n = PDDL_LIST_ENTRY(pos->member.next, postype, member);	\
	     &pos->member != (head); 					\
	     pos = n, n = PDDL_LIST_ENTRY(n->member.next, ntype, member))


/**
 * Initialize list.
 */
_pddl_inline void pddlListInit(pddl_list_t *l);

/**
 * Returns next element in list. If called on head first element is
 * returned.
 */
_pddl_inline pddl_list_t *pddlListNext(pddl_list_t *l);
_pddl_inline const pddl_list_t *pddlListNextConst(const pddl_list_t *l);

/**
 * Returns previous element in list. If called on head last element is
 * returned.
 */
_pddl_inline pddl_list_t *pddlListPrev(pddl_list_t *l);

/**
 * Returns true if list is empty.
 * TODO: rename to pddlListIsEmpty
 */
_pddl_inline int pddlListEmpty(const pddl_list_t *head);

/**
 * Appends item to end of the list l.
 */
_pddl_inline void pddlListAppend(pddl_list_t *l, pddl_list_t *item);

/**
 * Prepends item before first item in list.
 */
_pddl_inline void pddlListPrepend(pddl_list_t *l, pddl_list_t *item);

/**
 * Removes item from list.
 */
_pddl_inline void pddlListDel(pddl_list_t *item);


/**
 * Returns number of items in list - this takes O(n).
 */
_pddl_inline size_t pddlListSize(const pddl_list_t *head);


/**
 * Move all items from {src} to {dst}. Items will be appended to dst.
 */
_pddl_inline void pddlListMove(pddl_list_t *src, pddl_list_t *dst);


/**
 * Retypes given "M" list struct to regular list struct.
 */
_pddl_inline pddl_list_t *pddlListMAsList(pddl_list_m_t *l);

/**
 * Opposite to pddlListMAsList().
 */
_pddl_inline pddl_list_m_t *pddlListMFromList(pddl_list_t *l);




/**** INLINES ****/
_pddl_inline void pddlListInit(pddl_list_t *l)
{
    l->next = l;
    l->prev = l;
}

_pddl_inline pddl_list_t *pddlListNext(pddl_list_t *l)
{
    return l->next;
}

_pddl_inline const pddl_list_t *pddlListNextConst(const pddl_list_t *l)
{
    return l->next;
}

_pddl_inline pddl_list_t *pddlListPrev(pddl_list_t *l)
{
    return l->prev;
}

_pddl_inline int pddlListEmpty(const pddl_list_t *head)
{
    return head->next == head;
}

_pddl_inline void pddlListAppend(pddl_list_t *l, pddl_list_t *n)
{
    n->prev = l->prev;
    n->next = l;
    l->prev->next = n;
    l->prev = n;
}

_pddl_inline void pddlListPrepend(pddl_list_t *l, pddl_list_t *n)
{
    n->next = l->next;
    n->prev = l;
    l->next->prev = n;
    l->next = n;
}

_pddl_inline void pddlListDel(pddl_list_t *item)
{
    item->next->prev = item->prev;
    item->prev->next = item->next;
    item->next = item;
    item->prev = item;
}

_pddl_inline size_t pddlListSize(const pddl_list_t *head)
{
    pddl_list_t *item;
    size_t size = 0;

    PDDL_LIST_FOR_EACH(head, item){
        size++;
    }

    return size;
}

_pddl_inline void pddlListMove(pddl_list_t *src, pddl_list_t *dst)
{
    pddl_list_t *item;

    while (!pddlListEmpty(src)){
        item = pddlListNext(src);
        pddlListDel(item);
        pddlListAppend(dst, item);
    }
}

_pddl_inline pddl_list_t *pddlListMAsList(pddl_list_m_t *l)
{
    return (pddl_list_t *)l;
}

_pddl_inline pddl_list_m_t *pddlListMFromList(pddl_list_t *l)
{
    return (pddl_list_m_t *)l;
}

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_LIST_H__ */
