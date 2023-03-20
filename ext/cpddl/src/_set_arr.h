/***
 * Boruvka
 * --------
 * Copyright (c)2017 Daniel Fiser <danfis@danfis.cz>
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

#ifndef __PDDL_SET_H__
#define __PDDL_SET_H__

#include <pddl/common.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * Array-based set.
 * The elements in .s are always sorted.
 */
struct pddl_set {
    TYPE *s;
    int size;
    int alloc;
};
typedef struct pddl_set pddl_set_t;

#define PDDL_SET_INIT { NULL, 0, 0 }
#define PDDL_SET(NAME) pddl_set_t NAME = PDDL_SET_INIT

#define PDDL_SET_FOR_EACH(S, V) \
    for (int __i = 0; __i < (S)->size && ((V) = (S)->s[__i], 1); ++__i)

#define PDDL_SET_ADD(S, ...) \
    do { \
        TYPE ___pddl_set_vals[] = {__VA_ARGS__}; \
        int ___pddl_set_size = sizeof(___pddl_set_vals) / sizeof(TYPE); \
        for (int i = 0; i < ___pddl_set_size; ++i) \
            pddlSetAdd((S), ___pddl_set_vals[i]); \
    }while (0)

#define PDDL_SET_SET(S, ...) \
    do { \
        pddlSetEmpty(S); \
        PDDL_SET_ADD((S), __VA_ARGS__); \
    } while (0)

/**
 * Initialize the set.
 */
void pddlSetInit(pddl_set_t *s);

/**
 * Frees allocated memory.
 */
void pddlSetFree(pddl_set_t *s);

/**
 * Returns ith element from the set.
 */
_pddl_inline TYPE pddlSetGet(const pddl_set_t *s, int i);

/**
 * Returns size of the set.
 */
_pddl_inline int pddlSetSize(const pddl_set_t *s);


/**
 * Returns true if val \in s
 */
int pddlSetHas(const pddl_set_t *s, TYPE val);
_pddl_inline int pddlSetIn(TYPE val, const pddl_set_t *s);

/**
 * Return true if s1 \subset s2
 */
int pddlSetIsSubset(const pddl_set_t *s1, const pddl_set_t *s2);

/**
 * Returns size of s1 \cap s2.
 */
int pddlSetIntersectionSize(const pddl_set_t *s1, const pddl_set_t *s2);

/**
 * Returns true if | s1 \cap s2 | >= limit
 */
int pddlSetIntersectionSizeAtLeast(const pddl_set_t *s1, const pddl_set_t *s2,
                                   int limit);

/**
 * Returns true if | s1 \cap s2 \cap s3 | >= limit
 */
int pddlSetIntersectionSizeAtLeast3(const pddl_set_t *s1,
                                    const pddl_set_t *s2,
                                    const pddl_set_t *s3,
                                    int limit);

/**
 * Returns true if the sets are disjoint.
 */
_pddl_inline int pddlSetIsDisjunct(const pddl_set_t *s1, const pddl_set_t *s2);
_pddl_inline int pddlSetIsDisjoint(const pddl_set_t *s1, const pddl_set_t *s2);

/**
 * s = \emptyset
 */
_pddl_inline void pddlSetEmpty(pddl_set_t *s);

/**
 * d = s
 */
void pddlSetSet(pddl_set_t *d, const pddl_set_t *s);

/**
 * s = s \cup {val}
 */
void pddlSetAdd(pddl_set_t *s, TYPE val);

/**
 * s = s \setminus {val}
 * Returns true if val was found in s.
 */
int pddlSetRm(pddl_set_t *s, TYPE val);

/**
 * dst = dst \cup src
 */
void pddlSetUnion(pddl_set_t *dst, const pddl_set_t *src);

/**
 * dst = s1 \cup s2
 */
void pddlSetUnion2(pddl_set_t *dst, const pddl_set_t *s1, const pddl_set_t *s2);

/**
 * dst = dst \cap src
 */
void pddlSetIntersect(pddl_set_t *dst, const pddl_set_t *src);

/**
 * dst = s1 \cap s2
 */
void pddlSetIntersect2(pddl_set_t *dst, const pddl_set_t *s1, const pddl_set_t *s2);

/**
 * s1 = s1 \setminus s2
 */
void pddlSetMinus(pddl_set_t *s1, const pddl_set_t *s2);

/**
 * d = s1 \setminus s2
 */
void pddlSetMinus2(pddl_set_t *d, const pddl_set_t *s1, const pddl_set_t *s2);


/**
 * Returns true if the sets are equal.
 */
_pddl_inline int pddlSetEq(const pddl_set_t *s1, const pddl_set_t *s2);

/**
 * Compares sets, return values are the same as by memcmp().
 */
_pddl_inline int pddlSetCmp(const pddl_set_t *s1, const pddl_set_t *s2);

/**
 * Remaps the elements of the set using remap array containing maping from
 * the old value to the new value. The mapping must be monotonically
 * increasing and it is assumed that the values in the set are >= 0.
 */
void pddlSetRemap(pddl_set_t *s, const TYPE *remap);



/**** INLINES: ****/
_pddl_inline TYPE pddlSetGet(const pddl_set_t *s, int i)
{
    return s->s[i];
}

_pddl_inline int pddlSetSize(const pddl_set_t *s)
{
    return s->size;
}

_pddl_inline int pddlSetIn(TYPE val, const pddl_set_t *s)
{
    return pddlSetHas(s, val);
}

_pddl_inline int pddlSetIsDisjunct(const pddl_set_t *s1, const pddl_set_t *s2)
{
    return pddlSetIsDisjoint(s1, s2);
}

_pddl_inline int pddlSetIsDisjoint(const pddl_set_t *s1, const pddl_set_t *s2)
{
    return !pddlSetIntersectionSizeAtLeast(s1, s2, 1);
}

_pddl_inline void pddlSetEmpty(pddl_set_t *s)
{
    s->size = 0;
}

_pddl_inline int pddlSetEq(const pddl_set_t *s1, const pddl_set_t *s2)
{
    return s1->size == s2->size
            && memcmp(s1->s, s2->s, sizeof(TYPE) * s1->size) == 0;
}

_pddl_inline int pddlSetCmp(const pddl_set_t *s1, const pddl_set_t *s2)
{
    int cmp;
    cmp = memcmp(s1->s, s2->s,
                 sizeof(TYPE) * (s1->size < s2->size ?  s1->size : s2->size));
    if (cmp == 0)
        return s1->size - s2->size;
    return cmp;
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __PDDL_SET_H__ */
