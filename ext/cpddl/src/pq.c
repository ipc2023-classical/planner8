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

#include "pddl/err.h"
#include "pddl/pq.h"
#include "internal.h"

static void pddlPQBucketQueueInit(pddl_pq_bucket_queue_t *q);
static void pddlPQBucketQueueFree(pddl_pq_bucket_queue_t *q);
static void pddlPQBucketQueuePush(pddl_pq_bucket_queue_t *q,
                                  int key, pddl_pq_el_t *el);
static pddl_pq_el_t *pddlPQBucketQueuePop(pddl_pq_bucket_queue_t *q, int *key);
static void pddlPQBucketQueueUpdate(pddl_pq_bucket_queue_t *q,
                                    int key, pddl_pq_el_t *el);
/** Convets bucket queue to heap queue */
static void pddlPQBucketQueueToHeapQueue(pddl_pq_bucket_queue_t *b,
                                         pddl_pq_heap_queue_t *h);

static void pddlPQHeapQueueInit(pddl_pq_heap_queue_t *q);
static void pddlPQHeapQueueFree(pddl_pq_heap_queue_t *q);
static void pddlPQHeapQueuePush(pddl_pq_heap_queue_t *q,
                                int key, pddl_pq_el_t *el);
static pddl_pq_el_t *pddlPQHeapQueuePop(pddl_pq_heap_queue_t *q, int *key);
static void pddlPQHeapQueueUpdate(pddl_pq_heap_queue_t *q,
                                  int key, pddl_pq_el_t *el);

void pddlPQInit(pddl_pq_t *q)
{
    pddlPQBucketQueueInit(&q->bucket_queue);
    q->bucket = 1;
}

void pddlPQFree(pddl_pq_t *q)
{
    if (q->bucket){
        pddlPQBucketQueueFree(&q->bucket_queue);
    }else{
        pddlPQHeapQueueFree(&q->heap_queue);
    }
}

void pddlPQPush(pddl_pq_t *q, int key, pddl_pq_el_t *el)
{
    if (q->bucket){
        if (key >= PDDL_PQ_BUCKET_SIZE){
            pddlPQHeapQueueInit(&q->heap_queue);
            pddlPQBucketQueueToHeapQueue(&q->bucket_queue, &q->heap_queue);
            pddlPQBucketQueueFree(&q->bucket_queue);
            q->bucket = 0;
            pddlPQHeapQueuePush(&q->heap_queue, key, el);
        }else{
            pddlPQBucketQueuePush(&q->bucket_queue, key, el);
        }
    }else{
        pddlPQHeapQueuePush(&q->heap_queue, key, el);
    }
}

pddl_pq_el_t *pddlPQPop(pddl_pq_t *q, int *key)
{
    if (q->bucket){
        return pddlPQBucketQueuePop(&q->bucket_queue, key);
    }else{
        return pddlPQHeapQueuePop(&q->heap_queue, key);
    }
}

void pddlPQUpdate(pddl_pq_t *q, int key, pddl_pq_el_t *el)
{
    if (q->bucket){
        pddlPQBucketQueueUpdate(&q->bucket_queue, key, el);
    }else{
        pddlPQHeapQueueUpdate(&q->heap_queue, key, el);
    }
}

static void pddlPQBucketQueueInit(pddl_pq_bucket_queue_t *q)
{
    q->bucket_size = PDDL_PQ_BUCKET_SIZE;
    q->bucket = CALLOC_ARR(pddl_pq_bucket_t, q->bucket_size);
    q->lowest_key = q->bucket_size;
    q->size = 0;
}

static void pddlPQBucketQueueFree(pddl_pq_bucket_queue_t *q)
{
    int i;

    for (i = 0; i < q->bucket_size; ++i){
        if (q->bucket[i].el)
            FREE(q->bucket[i].el);
    }
    FREE(q->bucket);
}

static void pddlPQBucketQueuePush(pddl_pq_bucket_queue_t *q,
                                  int key, pddl_pq_el_t *el)
{
    pddl_pq_bucket_t *bucket;

    if (key >= PDDL_PQ_BUCKET_SIZE){
        PANIC("pddlPQBucketQueue: key %d is over a size of"
                   " the bucket queue, which is %d.",
                   key, PDDL_PQ_BUCKET_SIZE);
    }

    bucket = q->bucket + key;
    if (bucket->size == bucket->alloc){
        if (bucket->alloc == 0){
            bucket->alloc = PDDL_PQ_BUCKET_INIT_SIZE;
        }else{
            bucket->alloc *= PDDL_PQ_BUCKET_EXPANSION_FACTOR;
        }
        bucket->el = REALLOC_ARR(bucket->el, pddl_pq_el_t *,
                                 bucket->alloc);

    }
    el->key = key;
    el->conn.bucket = bucket->size;
    bucket->el[bucket->size++] = el;
    ++q->size;

    if (key < q->lowest_key)
        q->lowest_key = key;
}

static pddl_pq_el_t *pddlPQBucketQueuePop(pddl_pq_bucket_queue_t *q, int *key)
{
    pddl_pq_bucket_t *bucket;
    pddl_pq_el_t *el;

    if (q->size == 0)
        return NULL;

    bucket = q->bucket + q->lowest_key;
    while (bucket->size == 0){
        ++q->lowest_key;
        bucket += 1;
    }

    el = bucket->el[--bucket->size];
    if (key)
        *key = q->lowest_key;
    --q->size;
    return el;
}

static void pddlPQBucketQueueUpdate(pddl_pq_bucket_queue_t *q,
                                    int key, pddl_pq_el_t *el)
{
    pddl_pq_bucket_t *bucket;

    bucket = q->bucket + el->key;
    bucket->el[el->conn.bucket] = bucket->el[--bucket->size];
    bucket->el[el->conn.bucket]->conn.bucket = el->conn.bucket;
    --q->size;
    if (q->size == 0)
        q->lowest_key = q->bucket_size;
    pddlPQBucketQueuePush(q, key, el);
}

static void pddlPQBucketQueueToHeapQueue(pddl_pq_bucket_queue_t *b,
                                         pddl_pq_heap_queue_t *h)
{
    pddl_pq_bucket_t *bucket;
    int i, j;

    for (i = b->lowest_key; i < b->bucket_size; ++i){
        bucket = b->bucket + i;
        for (j = 0; j < bucket->size; ++j){
            pddlPQHeapQueuePush(h, i, bucket->el[j]);
        }
        if (bucket->el != NULL)
            FREE(bucket->el);
        bucket->el = NULL;
        bucket->size = bucket->alloc = 0;
    }
    b->size = 0;
}


static int heapLT(const pddl_pairheap_node_t *_n1,
                  const pddl_pairheap_node_t *_n2, void *_)
{
    pddl_pq_el_t *e1 = pddl_container_of(_n1, pddl_pq_el_t, conn.heap);
    pddl_pq_el_t *e2 = pddl_container_of(_n2, pddl_pq_el_t, conn.heap);
    return e1->key <= e2->key;
}

static void pddlPQHeapQueueInit(pddl_pq_heap_queue_t *q)
{
    q->heap = pddlPairHeapNew(heapLT, NULL);
}

static void pddlPQHeapQueueFree(pddl_pq_heap_queue_t *q)
{
    pddlPairHeapDel(q->heap);
}

static void pddlPQHeapQueuePush(pddl_pq_heap_queue_t *q,
                                int key, pddl_pq_el_t *el)
{
    el->key = key;
    pddlPairHeapAdd(q->heap, &el->conn.heap);
}

static pddl_pq_el_t *pddlPQHeapQueuePop(pddl_pq_heap_queue_t *q, int *key)
{
    pddl_pairheap_node_t *hn;
    pddl_pq_el_t *el;

    hn = pddlPairHeapExtractMin(q->heap);
    el = pddl_container_of(hn, pddl_pq_el_t, conn.heap);
    if (key)
        *key = el->key;
    return el;
}

static void pddlPQHeapQueueUpdate(pddl_pq_heap_queue_t *q,
                                  int key, pddl_pq_el_t *el)
{
    el->key = key;
    pddlPairHeapUpdate(q->heap, &el->conn.heap);
}
