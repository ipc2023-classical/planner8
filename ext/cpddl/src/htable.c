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

#include "pddl/htable.h"
#include "internal.h"

#define PDDL_HTABLE_INITIAL_SIZE 100


struct _pddl_htable_el_t {
    pddl_list_t list;
    void *key;
};

static int _eq(const pddl_list_t *key1, const pddl_list_t *key2, void *userdata);

pddl_htable_t *pddlHTableNew(pddl_htable_hash_fn hash_func,
                             pddl_htable_eq_fn eq_func,
                             void *userdata)
{
    pddl_htable_t *htable;
    size_t i;

    htable = ALLOC(pddl_htable_t);
    htable->size = PDDL_HTABLE_INITIAL_SIZE;
    htable->table = ALLOC_ARR(pddl_list_t, htable->size);
    htable->num_elements = 0;

    htable->hash = hash_func;
    htable->eq   = eq_func;
    htable->data = userdata;

    if (!htable->eq)
        htable->eq = _eq;

    for (i = 0; i < htable->size; i++){
        pddlListInit(htable->table + i);
    }

    return htable;
}

void pddlHTableDel(pddl_htable_t *h)
{
    size_t i;
    pddl_list_t *list, *item;

    for (i = 0; i < h->size; i++){
        list = &h->table[i];
        while (!pddlListEmpty(list)){
            item = pddlListNext(list);
            pddlListDel(item);
        }
    }

    FREE(h->table);
    FREE(h);
}

void pddlHTableGather(pddl_htable_t *m, pddl_list_t *list)
{
    size_t i;
    pddl_list_t *item;

    for (i = 0; i < m->size; i++){
        while (!pddlListEmpty(&m->table[i])){
            item = pddlListNext(&m->table[i]);
            pddlListDel(item);
            pddlListAppend(list, item);
        }
    }
    m->num_elements = 0;
}

size_t pddlHTableFindAll(const pddl_htable_t *m, const pddl_list_t *key1,
                         pddl_list_t ***out_arr, size_t *size)
{
    pddl_list_t *item;
    size_t found_size, bucket;
    int reallocate = 0;

    bucket = pddlHTableBucket(m, key1);

    if (*out_arr == 0x0)
        reallocate = 1;

    found_size = 0;
    PDDL_LIST_FOR_EACH(&m->table[bucket], item){
        if (m->eq(key1, item, m->data)){
            ++found_size;
            if (reallocate){
                (*out_arr) = REALLOC_ARR(*out_arr, pddl_list_t *,
                                         found_size);
                (*out_arr)[found_size - 1] = item;

            }else if (found_size <= *size){
                (*out_arr)[found_size - 1] = item;
            }
        }
    }

    if (reallocate)
        *size = found_size;

    return found_size;
}

void pddlHTableResize(pddl_htable_t *m, size_t size)
{
    pddl_list_t *old_table, *item;
    size_t old_size;
    size_t i;

    // remember old table and old size
    old_table = m->table;
    old_size  = m->size;

    // create a new empty table
    m->table = ALLOC_ARR(pddl_list_t, size);
    m->size  = size;
    m->num_elements = 0;

    for (i = 0; i < m->size; i++){
        pddlListInit(m->table + i);
    }

    for (i = 0; i < old_size; i++){
        while (!pddlListEmpty(&old_table[i])){
            // remove item from the old table
            item = pddlListNext(&old_table[i]);
            pddlListDel(item);

            // insert it into a new table
            pddlHTableInsertNoResize(m, item);
        }
    }

    FREE(old_table);
}

static int _eq(const pddl_list_t *key1, const pddl_list_t *key2, void *userdata)
{
    return key1 == key2;
}

