/***
 * Boruvka
 * --------
 * Copyright (c)2014 Daniel Fiser <danfis@danfis.cz>
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

#ifndef __PDDL_HASH_TABLE_H__
#define __PDDL_HASH_TABLE_H__

#include <pddl/list.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * Hash Table
 * ====================
 *
 * See pddl_hash_table_t.
 */


/**
 * Definition of the hash key type.
 */
typedef uint64_t pddl_htable_key_t;

/**
 * Callbacks
 * ----------
 */

/**
 * Hash function.
 */
typedef pddl_htable_key_t (*pddl_htable_hash_fn)(const pddl_list_t *key,
                                                 void *userdata);

/**
 * Returns true if two given keys are same.
 * If this callback is set to NULL, exact values of keys are compared.
 */
typedef int (*pddl_htable_eq_fn)(const pddl_list_t *key1, const pddl_list_t *key2,
                                 void *userdata);


/**
 * Hash table structure.
 */
struct _pddl_htable_t {
    pddl_list_t *table;
    size_t size;
    size_t num_elements;

    pddl_htable_hash_fn hash;
    pddl_htable_eq_fn eq;
    void *data;
};
typedef struct _pddl_htable_t pddl_htable_t;

/**
 * Functions
 * ----------
 */

/**
 * Creates hash table
 */
pddl_htable_t *pddlHTableNew(pddl_htable_hash_fn hash_func,
                             pddl_htable_eq_fn eq_func,
                             void *userdata);

/**
 * Deletes a table.
 * Content of the given table is not touched, i.e., it is responsibility of
 * a caller to free inserted elements.
 */
void pddlHTableDel(pddl_htable_t *h);

/**
 * Returns size of hash map
 */
_pddl_inline size_t pddlHTableSize(const pddl_htable_t *t);

/**
 * Returns number of elements stored in hash table.
 */
_pddl_inline size_t pddlHTableNumElements(const pddl_htable_t *t);

/**
 * Fill given {list} with all elements from hash table.
 * After calling this, hash table will be empty.
 */
void pddlHTableGather(pddl_htable_t *m, pddl_list_t *list);

/**
 * Insert an element into the hash table.
 */
_pddl_inline void pddlHTableInsert(pddl_htable_t *m, pddl_list_t *key1);

/**
 * Insert an element into the hash table only if the same element isn't
 * already there.
 * Returns the equal element if already on hash table or NULL the given
 * element was inserted.
 */
_pddl_inline pddl_list_t *pddlHTableInsertUnique(pddl_htable_t *m,
                                                 pddl_list_t *key1);

/**
 * Remove the given element from the table.
 */
_pddl_inline void pddlHTableRemove(pddl_htable_t *m, pddl_list_t *key);

/**
 * Same as pddlHTableInsert() but does not resize hash table.
 */
_pddl_inline void pddlHTableInsertNoResize(pddl_htable_t *m, pddl_list_t *key1);

/**
 * Insert the key into the specified bucket.
 * The bucket must be smaller then the size of the table.
 */
_pddl_inline void pddlHTableInsertBucket(pddl_htable_t *m, size_t bucket,
                                         pddl_list_t *key1);

/**
 * Same as pddlHTableInsertBucket() but does not resize hash table.
 */
_pddl_inline void pddlHTableInsertBucketNoResize(pddl_htable_t *m,
                                                 size_t bucket,
                                                 pddl_list_t *key1);

/**
 * Removes an element from the hash table.
 * Return 0 if such a key was stored in hash table and -1 otherwise.
 */
_pddl_inline int pddlHTableErase(pddl_htable_t *m, pddl_list_t *key1);

/**
 * Removes an element from the specified bucket.
 * Return 0 if such an element was stored in the hash table and -1
 * otherwise.
 */
_pddl_inline int pddlHTableEraseBucket(pddl_htable_t *m, size_t bucket,
                                       pddl_list_t *key1);

/**
 * Returns a key from hash table that equals to {key1} or NULL if there is no
 * such key. The first found element is returned.
 *
 * Note that {.eq} callback is used for this and {key1} is used as key1
 * argument.
 */
_pddl_inline pddl_list_t *pddlHTableFind(const pddl_htable_t *m,
                                         const pddl_list_t *key1);

/**
 * Same as pddlHTableFind() but searches only the specified bucket.
 */
_pddl_inline pddl_list_t *pddlHTableFindBucket(const pddl_htable_t *m,
                                               size_t bucket,
                                               const pddl_list_t *key1);

/**
 * Searches for all elements that are equal to the provided.
 * The elements from the hash table are stored in out_arr array which can
 * be provided in two ways.
 * 1. Either preallocated array can be provided and then the size argument
 *    must equal to the size of that array (and thus the maximal number of
 *    elements that can be stored in the array).
 * 2. Or *out_arr equals to NULL and in that case the function allocates
 *    enough memory for the array and stores its size in size argument.
 *    Then it is the caller's responsibility to call PDDL_FREE() on the
 *    *out_arr array.
 *
 * Returns number of elements were found (regardless of number of elements
 * that were actually stored in output array).
 */
size_t pddlHTableFindAll(const pddl_htable_t *m, const pddl_list_t *key1,
                         pddl_list_t ***out_arr, size_t *size);


/**
 * Returns a bucket number corresponding to the given element.
 */
_pddl_inline size_t pddlHTableBucket(const pddl_htable_t *m,
                                     const pddl_list_t *key1);

/**
 * Resize hash table to the specifed size.
 */
void pddlHTableResize(pddl_htable_t *m, size_t size);

/**
 * Returns next prime suitable for the hash table size that is not lower
 * than hint.
 */
_pddl_inline size_t pddlHTableNextPrime(size_t hint);


/**** INLINES ****/
_pddl_inline size_t pddlHTableSize(const pddl_htable_t *t)
{
    return t->size;
}

_pddl_inline size_t pddlHTableNumElements(const pddl_htable_t *t)
{
    return t->num_elements;
}

_pddl_inline void pddlHTableInsert(pddl_htable_t *m, pddl_list_t *key1)
{
    size_t bucket;
    bucket = pddlHTableBucket(m, key1);
    pddlHTableInsertBucket(m, bucket, key1);
}

_pddl_inline pddl_list_t *pddlHTableInsertUnique(pddl_htable_t *m,
                                                 pddl_list_t *key1)
{
    size_t bucket;
    pddl_list_t *item;

    bucket = pddlHTableBucket(m, key1);
    item = pddlHTableFindBucket(m, bucket, key1);
    if (item == NULL){
        pddlHTableInsertBucket(m, bucket, key1);
    }

    return item;
}

_pddl_inline void pddlHTableInsertNoResize(pddl_htable_t *m, pddl_list_t *key1)
{
    size_t bucket;
    bucket = pddlHTableBucket(m, key1);
    pddlHTableInsertBucketNoResize(m, bucket, key1);
}

_pddl_inline void pddlHTableInsertBucket(pddl_htable_t *m, size_t bucket,
                                         pddl_list_t *key1)
{
    size_t size;

    // resize table if necessary
    if (m->num_elements + 1 > m->size){
        size = pddlHTableNextPrime(m->num_elements + 1);
        if (size > m->size){
            pddlHTableResize(m, size);

            // re-compute bucket id because of resize
            bucket = pddlHTableBucket(m, key1);
        }
    }

    // put item into table
    pddlHTableInsertBucketNoResize(m, bucket, key1);
}

_pddl_inline void pddlHTableInsertBucketNoResize(pddl_htable_t *m,
                                                 size_t bucket,
                                                 pddl_list_t *key1)
{
    pddlListAppend(&m->table[bucket], key1);
    ++m->num_elements;
}

_pddl_inline int pddlHTableErase(pddl_htable_t *m, pddl_list_t *key1)
{
    size_t bucket;
    bucket = pddlHTableBucket(m, key1);
    return pddlHTableEraseBucket(m, bucket, key1);
}

_pddl_inline int pddlHTableEraseBucket(pddl_htable_t *m, size_t bucket,
                                       pddl_list_t *key1)
{
    pddl_list_t *item;

    item = pddlHTableFindBucket(m, bucket, key1);
    if (item){
        pddlListDel(item);
        --m->num_elements;
        return 0;
    }
    return -1;
}

_pddl_inline pddl_list_t *pddlHTableFind(const pddl_htable_t *m,
                                         const pddl_list_t *key1)
{
    size_t bucket;
    bucket = pddlHTableBucket(m, key1);
    return pddlHTableFindBucket(m, bucket, key1);
}

_pddl_inline pddl_list_t *pddlHTableFindBucket(const pddl_htable_t *m,
                                               size_t bucket,
                                               const pddl_list_t *key1)
{
    pddl_list_t *item;

    PDDL_LIST_FOR_EACH(&m->table[bucket], item){
        if (m->eq(key1, item, m->data))
            return item;
    }

    return NULL;
}

_pddl_inline void pddlHTableRemove(pddl_htable_t *m, pddl_list_t *item)
{
    pddlListDel(item);
    --m->num_elements;
}






_pddl_inline size_t pddlHTableBucket(const pddl_htable_t *m,
                                     const pddl_list_t *key1)
{
    return m->hash(key1, m->data) % (pddl_htable_key_t)m->size;
}

_pddl_inline size_t pddlHTableNextPrime(size_t hint)
{
    static size_t primes[] = {
        5ul,         53ul,         97ul,         193ul,       389ul,
        769ul,       1543ul,       3079ul,       6151ul,      12289ul,
        24593ul,     49157ul,      98317ul,      196613ul,    393241ul,
        786433ul,    1572869ul,    3145739ul,    6291469ul,   12582917ul,
        25165843ul,  50331653ul,   100663319ul,  201326611ul, 402653189ul,
        805306457ul, 1610612741ul, 3221225473ul, 4294967291ul
    };
    static size_t primes_size = sizeof(primes) / sizeof(size_t);

    size_t i;
    for (i = 0; i < primes_size; ++i){
        if (primes[i] >= hint)
            return primes[i];
    }
    return primes[primes_size - 1];
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __PDDL_HASH_TABLE_H__ */
