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

#ifndef __PDDL_SORT_H__
#define __PDDL_SORT_H__

#include <pddl/list.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * Sort Algorithms
 * ================
 */

/**
 * Function returning key for counting sort.
 */
typedef int (*pddl_sort_key)(const void *, void *arg);

/**
 * Counting sort. Keys has to be between from and to including from and to.
 * The range should be small enough to fit on stack.
 */
void pddlCountSort(void *base, size_t nmemb, size_t size, int from, int to,
                   pddl_sort_key key, void *arg);


/**
 * Compare function for sort functions.
 */
typedef int (*pddl_sort_cmp)(const void *, const void *, void *arg);

/**
 * Insertion sort.
 */
void pddlInsertSort(void *base, size_t nmemb, size_t size,
                    pddl_sort_cmp cmp, void *arg);
void pddlInsertSortInt(int *base, size_t nmemb);

/**
 * BSD heapsort.
 */
void pddlHeapSort(void *base, size_t nmemb, size_t size,
                  pddl_sort_cmp cmp, void *arg);

/**
 * BSD mergesort.
 * Requires size to be at least "sizeof(void *) / 2".
 */
int pddlMergeSort(void *base, size_t nmemb, size_t size,
                  pddl_sort_cmp cmp, void *carg);

/**
 * BSD kqsort.
 * Uses recursion.
 */
int pddlQSort(void *base, size_t nmemb, size_t size,
              pddl_sort_cmp cmp, void *carg);

/**
 * Tim sort.
 * Stable sort.
 */
int pddlTimSort(void *base, size_t nmemb, size_t size,
                pddl_sort_cmp cmp, void *carg);


/**
 * Default sorting algorithm.
 * Not guaranteed to be stable.
 */
int pddlSort(void *base, size_t nmemb, size_t size,
             pddl_sort_cmp cmp, void *carg);

/**
 * Default stable sort.
 */
int pddlStableSort(void *base, size_t nmemb, size_t size,
                   pddl_sort_cmp cmp, void *carg);

/**
 * Sorts an array of elements that contain an integer key. {size} is the
 * size of element and {offset} is an offset of the integer key within the
 * element.
 */
int pddlSortByIntKey(void *base, size_t nmemb, size_t size, size_t offset);
#define PDDL_SORT_BY_INT_KEY(base, nmemb, type, member) \
    pddlSortByIntKey((base), (nmemb), sizeof(type), \
                    pddl_offsetof(type, member))

/**
 * Same as pddlSortByIntKey() but for long keys.
 */
int pddlSortByLongKey(void *base, size_t nmemb, size_t size, size_t offset);
#define PDDL_SORT_BY_LONG_KEY(base, nmemb, type, member) \
    pddlSortByLongKey((base), (nmemb), sizeof(type), \
                     pddl_offsetof(type, member))

/**
 * Compare function for list sort functions.
 */
typedef int (*pddl_sort_list_cmp)(const pddl_list_t *,
                                  const pddl_list_t *, void *arg);

/**
 * List sort based on merge sort (from BSD).
 */
void pddlListSort(pddl_list_t *list, pddl_sort_list_cmp cmp, void *carg);

/**
 * Insertion sort for lists.
 * It sorts the list in ascending order
 */
void pddlListInsertSort(pddl_list_t *list, pddl_sort_list_cmp cmp, void *data);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_SORT_H__ */
