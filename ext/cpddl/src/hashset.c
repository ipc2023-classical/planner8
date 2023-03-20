/***
 * cpddl
 * --------
 * Copyright (c)2019 Daniel Fiser <danfis@danfis.cz>
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

#include "internal.h"
#include "pddl/hfunc.h"
#include "pddl/hashset.h"
#include "pddl/iset.h"
#include "pddl/lset.h"
#include "pddl/cset.h"

struct pddl_hashset_el {
    int id;
    void *el;
    pddl_htable_key_t hash;
    pddl_list_t htable;
};
typedef struct pddl_hashset_el pddl_hashset_el_t;

static pddl_htable_key_t hash(const pddl_list_t *key, void *_)
{
    const pddl_hashset_el_t *el;
    el = PDDL_LIST_ENTRY(key, pddl_hashset_el_t, htable);
    return el->hash;
}

static int eq(const pddl_list_t *e1, const pddl_list_t *e2, void *_hs)
{
    const pddl_hashset_t *hs = _hs;
    const pddl_hashset_el_t *s1, *s2;
    s1 = PDDL_LIST_ENTRY(e1, pddl_hashset_el_t, htable);
    s2 = PDDL_LIST_ENTRY(e2, pddl_hashset_el_t, htable);
    return hs->eq_fn(s1->el, s2->el, hs->userdata);
}

void pddlHashSetInit(pddl_hashset_t *s,
                     pddl_hashset_hash_fn hash_fn,
                     pddl_hashset_eq_fn eq_fn,
                     pddl_hashset_clone_fn clone_fn,
                     pddl_hashset_del_fn del_fn,
                     void *userdata)
{
    ZEROIZE(s);
    s->hash_fn = hash_fn;
    s->eq_fn = eq_fn;
    s->clone_fn = clone_fn;
    s->del_fn = del_fn;
    s->userdata = userdata;

    // compute best segment size
    size_t segment_size = sysconf(_SC_PAGESIZE);
    segment_size *= 8;
    while (segment_size < 32 * sizeof(pddl_hashset_el_t))
        segment_size *= 2;
    s->el = pddlSegmArrNew(sizeof(pddl_hashset_el_t), segment_size);
    s->htable = pddlHTableNew(hash, eq, s);
}

static pddl_htable_key_t isetHash(const void *el, void *_)
{
    const pddl_iset_t *s = el;
    if (pddlISetSize(s) == 0)
        return 0;
    return pddlCityHash_64(s->s, sizeof(*s->s) * s->size);
}

static int isetEq(const void *el1, const void *el2, void *_)
{
    const pddl_iset_t *s1 = el1;
    const pddl_iset_t *s2 = el2;
    return pddlISetEq(s1, s2);
}

static void *isetClone(const void *el, void *_)
{
    pddl_iset_t *s = ALLOC(pddl_iset_t);
    pddlISetInit(s);
    pddlISetUnion(s, (const pddl_iset_t *)el);
    return s;
}

static void isetDel(void *el, void *_)
{
    pddl_iset_t *s = el;
    pddlISetFree(s);
    FREE(s);
}

void pddlHashSetInitISet(pddl_hashset_t *s)
{
    pddlHashSetInit(s, isetHash, isetEq, isetClone, isetDel, NULL);
}

static pddl_htable_key_t lsetHash(const void *el, void *_)
{
    const pddl_lset_t *s = el;
    if (pddlLSetSize(s) == 0)
        return 0;
    return pddlCityHash_64(s->s, sizeof(*s->s) * s->size);
}

static int lsetEq(const void *el1, const void *el2, void *_)
{
    const pddl_lset_t *s1 = el1;
    const pddl_lset_t *s2 = el2;
    return pddlLSetEq(s1, s2);
}

static void *lsetClone(const void *el, void *_)
{
    pddl_lset_t *s = ALLOC(pddl_lset_t);
    pddlLSetInit(s);
    pddlLSetUnion(s, (const pddl_lset_t *)el);
    return s;
}

static void lsetDel(void *el, void *_)
{
    pddl_lset_t *s = el;
    pddlLSetFree(s);
    FREE(s);
}

void pddlHashSetInitLSet(pddl_hashset_t *s)
{
    pddlHashSetInit(s, lsetHash, lsetEq, lsetClone, lsetDel, NULL);
}

static pddl_htable_key_t csetHash(const void *el, void *_)
{
    const pddl_cset_t *s = el;
    if (pddlCSetSize(s) == 0)
        return 0;
    return pddlCityHash_64(s->s, sizeof(*s->s) * s->size);
}

static int csetEq(const void *el1, const void *el2, void *_)
{
    const pddl_cset_t *s1 = el1;
    const pddl_cset_t *s2 = el2;
    return pddlCSetEq(s1, s2);
}

static void *csetClone(const void *el, void *_)
{
    pddl_cset_t *s = ALLOC(pddl_cset_t);
    pddlCSetInit(s);
    pddlCSetUnion(s, (const pddl_cset_t *)el);
    return s;
}

static void csetDel(void *el, void *_)
{
    pddl_cset_t *s = el;
    pddlCSetFree(s);
    FREE(s);
}

void pddlHashSetInitCSet(pddl_hashset_t *s)
{
    pddlHashSetInit(s, csetHash, csetEq, csetClone, csetDel, NULL);
}



void pddlHashSetFree(pddl_hashset_t *s)
{
    if (s->htable != NULL)
        pddlHTableDel(s->htable);

    for (int i = 0; i < s->size; ++i){
        pddl_hashset_el_t *el = pddlSegmArrGet(s->el, i);
        s->del_fn(el->el, s->userdata);
    }
    if (s->el != NULL)
        pddlSegmArrDel(s->el);
}

int pddlHashSetAdd(pddl_hashset_t *s, const void *ins)
{
    pddl_hashset_el_t *el;
    pddl_list_t *find;

    el = pddlSegmArrGet(s->el, s->size);
    el->id = s->size;
    el->el = (void *)ins;
    el->hash = s->hash_fn(ins, s->userdata);
    pddlListInit(&el->htable);

    if ((find = pddlHTableInsertUnique(s->htable, &el->htable)) == NULL){
        el->el = s->clone_fn(ins, s->userdata);
        ++s->size;
        return el->id;
    }else{
        el = PDDL_LIST_ENTRY(find, pddl_hashset_el_t, htable);
        return el->id;
    }
}

int pddlHashSetFind(const pddl_hashset_t *s, const void *find_el)
{
    pddl_hashset_el_t el;
    pddl_list_t *find;

    el.id = s->size;
    el.el = (void *)find_el;
    el.hash = s->hash_fn(find_el, s->userdata);
    pddlListInit(&el.htable);

    if ((find = pddlHTableFind(s->htable, &el.htable)) != NULL){
        const pddl_hashset_el_t *f;
        f = PDDL_LIST_ENTRY(find, pddl_hashset_el_t, htable);
        return f->id;
    }else{
        return -1;
    }
}

const void *pddlHashSetGet(const pddl_hashset_t *s, int id)
{
    if (id < 0 || id >= s->size)
        return NULL;
    const pddl_hashset_el_t *el = pddlSegmArrConstGet(s->el, id);
    return el->el;
}
