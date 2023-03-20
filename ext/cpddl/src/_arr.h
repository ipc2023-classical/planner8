/***
 * Copyright (c)2017 Daniel Fiser <danfis@danfis.cz>
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

#ifndef __PDDL_ARR_H__
#define __PDDL_ARR_H__

#include "pddl/common.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct pddl_arr {
    TYPE *arr;
    int size;
    int alloc;
};
typedef struct pddl_arr pddl_arr_t;

#define PDDL_ARR_INIT { NULL, 0, 0 }
#define PDDL_ARR(NAME) pddl_arr_t NAME = PDDL_ARR_INIT

#define PDDL_ARR_FOR_EACH(S, V) \
    for (int __i = 0; __i < (S)->size && ((V) = (S)->arr[__i], 1); ++__i)

/**
 * Initialize the array.
 */
_pddl_inline void pddlArrInit(pddl_arr_t *a);

/**
 * Frees allocated memory.
 */
void pddlArrFree(pddl_arr_t *a);

/**
 * Allocate enough memory for size elements.
 * (It does not change a->size.)
 */
void pddlArrRealloc(pddl_arr_t *a, int size);

/**
 * Resize the array to the specified number of elements.
 */
_pddl_inline void pddlArrResize(pddl_arr_t *a, int size);

/**
 * Returns ith element from the array.
 */
_pddl_inline TYPE pddlArrGet(const pddl_arr_t *a, int i);

/**
 * Sets ith element from the array.
 */
_pddl_inline void pddlArrSet(pddl_arr_t *a, int i, TYPE val);

/**
 * Returns size of the array.
 */
_pddl_inline int pddlArrSize(const pddl_arr_t *a);

/**
 * Returns array stored in the structure.
 */
_pddl_inline TYPE *pddlArrGetArr(const pddl_arr_t *a);


/**
 * Returns true if val \in s
 */
_pddl_inline int pddlArrIn(TYPE val, const pddl_arr_t *s);

/**
 * Makes the array empty.
 */
_pddl_inline void pddlArrEmpty(pddl_arr_t *s);

/**
 * Adds element at the end of the array.
 */
_pddl_inline void pddlArrAdd(pddl_arr_t *s, TYPE val);

/**
 * Adds element at the begginig of the array.
 */
_pddl_inline void pddlArrPrepend(pddl_arr_t *s, TYPE val);

/**
 * Removes last element of the array.
 */
_pddl_inline void pddlArrRmLast(pddl_arr_t *a);

/**
 * Returns true if the array are equal.
 */
_pddl_inline int pddlArrEq(const pddl_arr_t *s1, const pddl_arr_t *s2);

/**
 * Compares arrays, return values are the same as by memcmp().
 */
_pddl_inline int pddlArrCmp(const pddl_arr_t *s1, const pddl_arr_t *s2);

/**
 * Append app after a.
 */
_pddl_inline void pddlArrAppendArr(pddl_arr_t *a, const pddl_arr_t *app);

/**
 * Removes and returns the last element of the array, but does not check the
 * size!
 */
_pddl_inline TYPE pddlArrPopLast(pddl_arr_t *a);

/**
 * Reverse the array
 */
_pddl_inline void pddlArrReverse(pddl_arr_t *a);


/**** INLINES: ****/
_pddl_inline void _pddlArrEnsureSize(pddl_arr_t *a, int size)
{
    if (size <= a->alloc)
        return;
    pddlArrRealloc(a, size);
}

_pddl_inline void pddlArrInit(pddl_arr_t *a)
{
    PDDL_ZEROIZE(a);
}

_pddl_inline void pddlArrResize(pddl_arr_t *a, int size)
{
    if (size > a->alloc)
        pddlArrRealloc(a, size);
    a->size = size;
}

_pddl_inline TYPE pddlArrGet(const pddl_arr_t *a, int i)
{
    return a->arr[i];
}

_pddl_inline void pddlArrSet(pddl_arr_t *a, int i, TYPE val)
{
    if (a->size <= i)
        pddlArrResize(a, i + 1);
    a->arr[i] = val;
}

_pddl_inline int pddlArrSize(const pddl_arr_t *a)
{
    return a->size;
}

_pddl_inline TYPE *pddlArrGetArr(const pddl_arr_t *a)
{
    return a->arr;
}

_pddl_inline int pddlArrIn(TYPE val, const pddl_arr_t *a)
{
    for (int i = 0; i < a->size; ++i){
        if (a->arr[i] == val)
            return 1;
    }
    return 0;
}

_pddl_inline void pddlArrEmpty(pddl_arr_t *a)
{
    a->size = 0;
}

_pddl_inline void pddlArrAdd(pddl_arr_t *a, TYPE val)
{
    _pddlArrEnsureSize(a, a->size + 1);
    a->arr[a->size++] = val;
}

_pddl_inline void pddlArrPrepend(pddl_arr_t *a, TYPE val)
{
    _pddlArrEnsureSize(a, a->size + 1);
    for (int i = a->size; i > 0; --i)
        a->arr[i] = a->arr[i - 1];
    ++a->size;
    a->arr[0] = val;
}

_pddl_inline void pddlArrRmLast(pddl_arr_t *a)
{
    if (a->size > 0)
        --a->size;
}

_pddl_inline int pddlArrEq(const pddl_arr_t *s1, const pddl_arr_t *s2)
{
    return s1->size == s2->size
            && memcmp(s1->arr, s2->arr, sizeof(TYPE) * s1->size) == 0;
}

_pddl_inline int pddlArrCmp(const pddl_arr_t *s1, const pddl_arr_t *s2)
{
    int cmp;
    cmp = memcmp(s1->arr, s2->arr,
                 sizeof(TYPE) * (s1->size < s2->size ?  s1->size : s2->size));
    if (cmp == 0)
        return s1->size - s2->size;
    return cmp;
}

_pddl_inline void pddlArrAppendArr(pddl_arr_t *a, const pddl_arr_t *app)
{
    pddlArrRealloc(a, a->size + app->size);
    memcpy(a->arr + a->size, app->arr, sizeof(TYPE) * app->size);
    a->size += app->size;
}

_pddl_inline TYPE pddlArrPopLast(pddl_arr_t *a)
{
    return a->arr[--a->size];
}

_pddl_inline void pddlArrReverse(pddl_arr_t *a)
{
    TYPE tmp;
    for (int i = 0; i < a->size / 2; ++i)
        PDDL_SWAP(a->arr[i], a->arr[a->size - i - 1], tmp);
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __PDDL_ARR_H__ */
