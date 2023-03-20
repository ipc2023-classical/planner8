/***
 * cpddl
 * -------
 * Copyright (c)2016 Daniel Fiser <danfis@danfis.cz>,
 * AI Center, Department of Computer Science,
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

#include "internal.h"
#include "pddl/hfunc.h"
#include "pddl/sort.h"
#include "pddl/pddl.h"
#include "pddl/fact.h"

/** Deep copy of the fact.  */
static void pddlFactCopy(pddl_fact_t *dst, const pddl_fact_t *src);
/** Returns true if facts are equal.  */
static int pddlFactEq(const pddl_fact_t *f1, const pddl_fact_t *f2);

static pddl_htable_key_t htableKey(const pddl_list_t *key, void *_)
{
    pddl_fact_t *f = PDDL_LIST_ENTRY(key, pddl_fact_t, htable);
    return f->hash;
}

static int htableEq(const pddl_list_t *k1,
                    const pddl_list_t *k2, void *_)
{
    pddl_fact_t *f1 = PDDL_LIST_ENTRY(k1, pddl_fact_t, htable);
    pddl_fact_t *f2 = PDDL_LIST_ENTRY(k2, pddl_fact_t, htable);
    return pddlFactEq(f1, f2);
}

static pddl_htable_key_t pddlFactHash(const pddl_fact_t *f)
{
    return pddlHashSDBM(f->name);
}

static char *makeName(const pddl_ground_atom_t *ga, const pddl_t *pddl)
{
    char name[PDDL_FACT_MAX_NAME_SIZE];
    int offset;

    offset = snprintf(name, PDDL_FACT_MAX_NAME_SIZE, "%s",
                      pddl->pred.pred[ga->pred].name);
    for (int i = 0; i < ga->arg_size; ++i){
        offset += snprintf(name + offset, PDDL_FACT_MAX_NAME_SIZE - offset,
                           " %s", pddl->obj.obj[ga->arg[i]].name);
    }
    name[PDDL_FACT_MAX_NAME_SIZE - 1] = 0x0;
    return STRDUP(name);
}

static int isPrivate(const pddl_ground_atom_t *ga, const pddl_t *pddl)
{
    if (pddl->pred.pred[ga->pred].is_private)
        return 1;
    for (int i = 0; i < ga->arg_size; ++i){
        if (pddl->obj.obj[ga->arg[i]].is_private)
            return 1;
    }
    return 0;
}

static pddl_fact_t *factFromGroundAtom(const pddl_ground_atom_t *ga,
                                       const pddl_t *pddl)
{
    pddl_fact_t *f;

    f = pddlFactNew();
    f->name = makeName(ga, pddl);
    f->is_private = isPrivate(ga, pddl);
    f->ground_atom = pddlGroundAtomClone(ga);
    f->hash = pddlFactHash(f);
    return f;
}

void pddlFactInit(pddl_fact_t *f)
{
    ZEROIZE(f);
    f->neg_of = -1;
}

pddl_fact_t *pddlFactNew(void)
{
    pddl_fact_t *f = ALLOC(pddl_fact_t);
    pddlFactInit(f);
    return f;
}

void pddlFactFree(pddl_fact_t *f)
{
    if (f->name != NULL)
        FREE(f->name);
    if (f->ground_atom != NULL)
        pddlGroundAtomDel(f->ground_atom);
}

void pddlFactDel(pddl_fact_t *f)
{
    pddlFactFree(f);
    FREE(f);
}

static void pddlFactCopy(pddl_fact_t *dst, const pddl_fact_t *src)
{
    pddlFactFree(dst);
    if (src->name != NULL)
        dst->name = STRDUP(src->name);
    if (src->ground_atom != NULL)
        dst->ground_atom = pddlGroundAtomClone(src->ground_atom);
    dst->hash = pddlFactHash(dst);
    dst->is_private = src->is_private;
    dst->neg_of = src->neg_of;
}

int pddlFactCmp(const pddl_fact_t *f1, const pddl_fact_t *f2)
{
    return strcmp(f1->name, f2->name);
}

void pddlFactPrint(const pddl_fact_t *f,
                   const char *prefix,
                   const char *suffix,
                   FILE *fout)
{
    const char *priv = "";
    if (f->is_private)
        priv = "P:";
    fprintf(fout, "%s%s(%s)%s", prefix, priv, f->name, suffix);
}

static int pddlFactEq(const pddl_fact_t *f1, const pddl_fact_t *f2)
{
    return pddlFactCmp(f1, f2) == 0;
}

static void makeSpace(pddl_facts_t *fs)
{
    if (fs->fact_size >= fs->fact_alloc){
        if (fs->fact_alloc == 0){
            fs->fact_alloc = 2;
        }else{
            fs->fact_alloc *= 2;
        }
        fs->fact = REALLOC_ARR(fs->fact, pddl_fact_t *, fs->fact_alloc);
    }
}

void pddlFactsInit(pddl_facts_t *fs)
{
    ZEROIZE(fs);
    fs->htable = pddlHTableNew(htableKey, htableEq, fs);
}

void pddlFactsFree(pddl_facts_t *fs)
{
    pddl_fact_t *fact;

    pddlHTableDel(fs->htable);
    PDDL_FACTS_FOR_EACH(fs, fact)
        pddlFactDel(fact);
    if (fs->fact != NULL)
        FREE(fs->fact);
}

static int addFact(pddl_facts_t *fs, pddl_fact_t *fact)
{
    makeSpace(fs);
    fact->id = fs->fact_size;
    fs->fact[fs->fact_size] = fact;
    ++fs->fact_size;
    pddlHTableInsert(fs->htable, &fact->htable);

    return fact->id;
}

int pddlFactsAdd(pddl_facts_t *fs, const pddl_fact_t *f)
{
    pddl_list_t *hfound;
    pddl_fact_t *fact;

    if ((hfound = pddlHTableFind(fs->htable, &f->htable)) != NULL)
        return (PDDL_LIST_ENTRY(hfound, pddl_fact_t, htable))->id;

    fact = pddlFactNew();
    pddlFactCopy(fact, f);
    return addFact(fs, fact);
}

int pddlFactsAddGroundAtom(pddl_facts_t *fs, const pddl_ground_atom_t *ga,
                           const pddl_t *pddl)
{
    pddl_list_t *hfound;
    pddl_fact_t *fact;

    fact = factFromGroundAtom(ga, pddl);
    if ((hfound = pddlHTableFind(fs->htable, &fact->htable)) != NULL){
        pddlFactDel(fact);
        fact = PDDL_LIST_ENTRY(hfound, pddl_fact_t, htable);
        return fact->id;
    }

    int fact_id = addFact(fs, fact);
    fact = fs->fact[fact_id];
    int pred_neg = pddl->pred.pred[ga->pred].neg_of;
    if (pred_neg >= 0){
        for (int i = 0; i < fs->fact_size - 1; ++i){
            pddl_fact_t *fact2 = fs->fact[i];
            const pddl_ground_atom_t *ga2 = fact2->ground_atom;
            if (ga2 != NULL
                    && ga2->pred == pred_neg
                    && ga->arg_size == ga2->arg_size
                    && memcmp(ga->arg, ga2->arg,
                              sizeof(pddl_obj_id_t) * ga->arg_size) == 0){
                ASSERT_RUNTIME(fact2->neg_of == -1);
                ASSERT_RUNTIME(fact->neg_of == -1);
                ASSERT(strncmp(fact->name, "NOT-", 4) == 0
                        || strncmp(fact2->name, "NOT-", 4) == 0);
                ASSERT(strncmp(fact->name, "NOT-", 4) == 0
                        || strcmp(fact->name, fact2->name + 4) == 0);
                ASSERT(strncmp(fact2->name, "NOT-", 4) == 0
                        || strcmp(fact2->name, fact->name + 4) == 0);
                fact->neg_of = fact2->id;
                fact2->neg_of = fact->id;
                break;
            }
        }
    }

    return fact_id;
}

int pddlFactsDelFactsGenRemap(int fact_size,
                              const pddl_iset_t *del_facts,
                              int *remap)
{
    int size = 0;

    ZEROIZE_ARR(remap, fact_size);

    int fact_id;
    PDDL_ISET_FOR_EACH(del_facts, fact_id){
        if (fact_id >= fact_size)
            break;
        remap[fact_id] = -1;
    }

    for (int fact_id = 0; fact_id < fact_size; ++fact_id){
        if (remap[fact_id] != -1)
            remap[fact_id] = size++;
    }

    return size;
}

void pddlFactsDelFact(pddl_facts_t *fs, int fact_id)
{
    pddl_fact_t *f;

    if (fs->fact[fact_id] == NULL)
        return;
    f = fs->fact[fact_id];
    if (f->neg_of >= 0)
        fs->fact[f->neg_of]->neg_of = -1;
    pddlHTableErase(fs->htable, &f->htable);
    pddlFactDel(f);
    fs->fact[fact_id] = NULL;
}

void pddlFactsDelFacts(pddl_facts_t *fs, const pddl_iset_t *m, int *remap)
{
    int new_size = pddlFactsDelFactsGenRemap(fs->fact_size, m, remap);

    for (int fact_id = 0; fact_id < fs->fact_size; ++fact_id){
        if (remap[fact_id] == -1){
            pddlFactsDelFact(fs, fact_id);
        }else{
            fs->fact[fact_id]->id = remap[fact_id];
            if (fs->fact[fact_id]->neg_of >= 0)
                fs->fact[fact_id]->neg_of = remap[fs->fact[fact_id]->neg_of];
            fs->fact[remap[fact_id]] = fs->fact[fact_id];
        }
    }
    fs->fact_size = new_size;
}

void pddlFactsCopy(pddl_facts_t *dst, const pddl_facts_t *src)
{
    for (int i = 0; i < src->fact_size; ++i)
        pddlFactsAdd(dst, src->fact[i]);
}

static int factCmpByName(const void *a, const void *b, void *_)
{
    const pddl_fact_t *f1 = *(pddl_fact_t **)a;
    const pddl_fact_t *f2 = *(pddl_fact_t **)b;
    return strcmp(f1->name, f2->name);
}

void pddlFactsSort(pddl_facts_t *fs, int *remap)
{
    pddlSort(fs->fact, fs->fact_size, sizeof(pddl_fact_t *),
            factCmpByName, NULL);
    for (int i = 0; i < fs->fact_size; ++i){
        pddl_fact_t *f = fs->fact[i];
        if (remap != NULL)
            remap[f->id] = i;
        f->id = i;
    }
    for (int i = 0; i < fs->fact_size; ++i){
        pddl_fact_t *f = fs->fact[i];
        if (f->neg_of >= 0)
            f->neg_of = remap[f->neg_of];
    }
}

void pddlFactsPrint(const pddl_facts_t *fs,
                    const char *prefix,
                    const char *suffix,
                    FILE *fout)
{
    for (int i = 0; i < fs->fact_size; ++i)
        pddlFactPrint(fs->fact[i], prefix, suffix, fout);
}

void pddlFactsPrintSet(const pddl_iset_t *fact_set,
                       const pddl_facts_t *fs,
                       const char *prefix,
                       const char *suffix,
                       FILE *fout)
{
    int fid;
    PDDL_ISET_FOR_EACH(fact_set, fid)
        pddlFactPrint(fs->fact[fid], prefix, suffix, fout);
}
