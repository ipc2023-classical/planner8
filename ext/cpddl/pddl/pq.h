/***
 * cpddl
 * -------
 * Copyright (c)2017 Daniel Fiser <danfis@danfis.cz>,
 * AIC, Department of Computer Science,
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

#ifndef __PDDL_PQ_H__
#define __PDDL_PQ_H__

#include <pddl/pairheap.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * Adaptive Priority Queue
 * ========================
 */

/**
 * Number of buckets available in bucket-based queue.
 * Inserting key greater or equal then this constant will end up in program
 * termination.
 */
#define PDDL_PQ_BUCKET_SIZE 1024

/**
 * Initial size of one bucket in bucket-queue.
 */
#define PDDL_PQ_BUCKET_INIT_SIZE 32

/**
 * Expansion factor of a bucket.
 */
#define PDDL_PQ_BUCKET_EXPANSION_FACTOR 2

struct pddl_pq_el {
    int key;
    union {
        int bucket;
        pddl_pairheap_node_t heap;
    } conn;
};
typedef struct pddl_pq_el pddl_pq_el_t;

/**
 * Bucket for storing int values.
 */
struct pddl_pq_bucket {
    pddl_pq_el_t **el; /*!< Stored elements */
    int size;   /*!< Number of stored values */
    int alloc;  /*!< Size of the allocated array */
};
typedef struct pddl_pq_bucket pddl_pq_bucket_t;

/**
 * Bucket based priority queue.
 */
struct pddl_pq_bucket_queue {
    pddl_pq_bucket_t *bucket; /*!< Array of buckets */
    int bucket_size;          /*!< Number of buckets */
    int lowest_key;           /*!< Lowest key so far */
    int size;                 /*!< Number of elements stored in queue */
};
typedef struct pddl_pq_bucket_queue pddl_pq_bucket_queue_t;

/**
 * Heap-based priority queue.
 */
struct pddl_pq_heap_queue {
    pddl_pairheap_t *heap;
};
typedef struct pddl_pq_heap_queue pddl_pq_heap_queue_t;


struct pddl_pq {
    pddl_pq_bucket_queue_t bucket_queue;
    pddl_pq_heap_queue_t heap_queue;
    int bucket;
};
typedef struct pddl_pq pddl_pq_t;

/**
 * Initializes priority queue.
 */
void pddlPQInit(pddl_pq_t *q);

/**
 * Frees allocated resources.
 */
void pddlPQFree(pddl_pq_t *q);

/**
 * Inserts an element into queue.
 */
void pddlPQPush(pddl_pq_t *q, int key, pddl_pq_el_t *el);

/**
 * Removes and returns the lowest element.
 */
pddl_pq_el_t *pddlPQPop(pddl_pq_t *q, int *key);

/**
 * Update the element if it is already in the heap.
 */
void pddlPQUpdate(pddl_pq_t *q, int new_key, pddl_pq_el_t *el);

/**
 * Returns true if the queue is empty.
 */
_pddl_inline int pddlPQEmpty(const pddl_pq_t *q);



/**** INLINES ****/
_pddl_inline int pddlPQBucketQueueEmpty(const pddl_pq_bucket_queue_t *q)
{
    return q->size == 0;
}

_pddl_inline int pddlPQHeapQueueEmpty(const pddl_pq_heap_queue_t *q)
{
    return pddlPairHeapEmpty(q->heap);
}

_pddl_inline int pddlPQEmpty(const pddl_pq_t *q)
{
    if (q->bucket){
        return pddlPQBucketQueueEmpty(&q->bucket_queue);
    }else{
        return pddlPQHeapQueueEmpty(&q->heap_queue);
    }
}

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_PQ_H__ */
