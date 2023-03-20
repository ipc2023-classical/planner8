/***
 * cpddl
 * -------
 * Copyright (c)2017 Daniel Fiser <danfis@danfis.cz>,
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
#include "pddl/ground_atom.h"
#include "pddl/pddl_struct.h"


#define PDDL_GROUND_ATOM_STACK(NAME, ARG_SIZE) \
    pddl_ground_atom_t NAME; \
    pddl_obj_id_t __##NAME##__arg[ARG_SIZE]; \
    NAME.arg = __##NAME##__arg


/** Compares two ground atoms. */
static int pddlGroundAtomCmp(const pddl_ground_atom_t *a1,
                             const pddl_ground_atom_t *a2);


static pddl_htable_key_t htableKey(const pddl_list_t *key, void *_)
{
    pddl_ground_atom_t *a = PDDL_LIST_ENTRY(key, pddl_ground_atom_t, htable);
    return a->hash;
}

static int htableEq(const pddl_list_t *k1,
                    const pddl_list_t *k2, void *_)
{
    pddl_ground_atom_t *a1 = PDDL_LIST_ENTRY(k1, pddl_ground_atom_t, htable);
    pddl_ground_atom_t *a2 = PDDL_LIST_ENTRY(k2, pddl_ground_atom_t, htable);
    return pddlGroundAtomCmp(a1, a2) == 0;
}

static uint64_t pddlGroundAtomHash(const pddl_ground_atom_t *a)
{
    uint64_t hash;

    hash = pddlCityHash_32(&a->pred, sizeof(int));
    hash <<= 32u;
    hash |= 0xffffffffu \
                & pddlCityHash_32(a->arg, sizeof(pddl_obj_id_t) * a->arg_size);
    return hash;
}

void pddlGroundAtomDel(pddl_ground_atom_t *a)
{
    if (a->arg != NULL)
        FREE(a->arg);
    FREE(a);
}


pddl_ground_atom_t *pddlGroundAtomClone(const pddl_ground_atom_t *a)
{
    pddl_ground_atom_t *c = ALLOC(pddl_ground_atom_t);
    *c = *a;
    if (a->arg != NULL){
        c->arg = ALLOC_ARR(pddl_obj_id_t, c->arg_size);
        memcpy(c->arg, a->arg, sizeof(pddl_obj_id_t) * c->arg_size);
    }
    return c;
}

static int pddlGroundAtomCmp(const pddl_ground_atom_t *a1,
                             const pddl_ground_atom_t *a2)
{
    if (a1->pred != a2->pred)
        return a1->pred - a2->pred;
    ASSERT(a1->arg_size == a2->arg_size);
    return memcmp(a1->arg, a2->arg, sizeof(pddl_obj_id_t) * a1->arg_size);
}

static pddl_ground_atom_t *nextNewGroundAtom(pddl_ground_atoms_t *ga,
                                             const pddl_ground_atom_t *a)
{
    pddl_ground_atom_t *g;

    if (ga->atom_size >= ga->atom_alloc){
        if (ga->atom_alloc == 0){
            ga->atom_alloc = 2;
        }else{
            ga->atom_alloc *= 2;
        }
        ga->atom = REALLOC_ARR(ga->atom,
                               pddl_ground_atom_t *, ga->atom_alloc);
    }

    g = pddlGroundAtomClone(a);
    g->id = ga->atom_size;
    ga->atom[ga->atom_size++] = g;
    return g;
}

void pddlGroundAtomsInit(pddl_ground_atoms_t *ga)
{
    ZEROIZE(ga);
    ga->htable = pddlHTableNew(htableKey, htableEq, ga);
}

void pddlGroundAtomsFree(pddl_ground_atoms_t *ga)
{
    if (ga->htable != NULL)
        pddlHTableDel(ga->htable);
    for (int i = 0; i < ga->atom_size; ++i){
        if (ga->atom[i] != NULL)
            pddlGroundAtomDel(ga->atom[i]);
    }
    if (ga->atom != NULL)
        FREE(ga->atom);
}

static void groundAtom(pddl_ground_atom_t *a,
                       const pddl_fm_atom_t *c, const pddl_obj_id_t *arg)
{
    a->func_val = 0;
    a->pred = c->pred;
    a->arg_size = c->arg_size;
    a->layer = 0;
    for (int i = 0; i < c->arg_size; ++i){
        if (c->arg[i].obj >= 0){
            a->arg[i] = c->arg[i].obj;
        }else{
            ASSERT(arg != NULL);
            a->arg[i] = arg[c->arg[i].param];
        }
    }
}

pddl_ground_atom_t *pddlGroundAtomsAddAtom(pddl_ground_atoms_t *ga,
                                           const pddl_fm_atom_t *c,
                                           const pddl_obj_id_t *arg)
{
    pddl_list_t *found;
    pddl_ground_atom_t *out;
    PDDL_GROUND_ATOM_STACK(loc, c->arg_size);

    groundAtom(&loc, c, arg);
    loc.hash = pddlGroundAtomHash(&loc);
    if ((found = pddlHTableFind(ga->htable, &loc.htable)) != NULL){
        out = PDDL_LIST_ENTRY(found, pddl_ground_atom_t, htable);
        return out;
    }

    out = nextNewGroundAtom(ga, &loc);
    pddlListInit(&out->htable);
    pddlHTableInsert(ga->htable, &out->htable);
    return out;
}

pddl_ground_atom_t *pddlGroundAtomsAddPred(pddl_ground_atoms_t *ga,
                                           int pred,
                                           const pddl_obj_id_t *arg,
                                           int arg_size)
{
    pddl_list_t *found;
    pddl_ground_atom_t *out;
    PDDL_GROUND_ATOM_STACK(loc, arg_size);

    loc.func_val = 0;
    loc.pred = pred;
    memcpy(loc.arg, arg, sizeof(pddl_obj_id_t) * arg_size);
    loc.arg_size = arg_size;
    loc.layer = 0;

    loc.hash = pddlGroundAtomHash(&loc);
    if ((found = pddlHTableFind(ga->htable, &loc.htable)) != NULL){
        out = PDDL_LIST_ENTRY(found, pddl_ground_atom_t, htable);
        return out;
    }

    out = nextNewGroundAtom(ga, &loc);
    pddlListInit(&out->htable);
    pddlHTableInsert(ga->htable, &out->htable);
    return out;
}


pddl_ground_atom_t *pddlGroundAtomsFindAtom(const pddl_ground_atoms_t *ga,
                                            const pddl_fm_atom_t *c,
                                            const pddl_obj_id_t *arg)
{
    pddl_list_t *found;
    pddl_ground_atom_t *out;
    PDDL_GROUND_ATOM_STACK(loc, c->arg_size);

    groundAtom(&loc, c, arg);
    loc.hash = pddlGroundAtomHash(&loc);
    if ((found = pddlHTableFind(ga->htable, &loc.htable)) != NULL){
        out = PDDL_LIST_ENTRY(found, pddl_ground_atom_t, htable);
        return out;
    }
    return NULL;
}

pddl_ground_atom_t *pddlGroundAtomsFindPred(const pddl_ground_atoms_t *ga,
                                            int pred,
                                            const pddl_obj_id_t *arg,
                                            int arg_size)
{
    pddl_list_t *found;
    pddl_ground_atom_t *out;
    PDDL_GROUND_ATOM_STACK(loc, arg_size);

    loc.func_val = 0;
    loc.pred = pred;
    memcpy(loc.arg, arg, sizeof(pddl_obj_id_t) * arg_size);
    loc.arg_size = arg_size;
    loc.layer = 0;

    loc.hash = pddlGroundAtomHash(&loc);
    if ((found = pddlHTableFind(ga->htable, &loc.htable)) != NULL){
        out = PDDL_LIST_ENTRY(found, pddl_ground_atom_t, htable);
        return out;
    }
    return NULL;
}

void pddlGroundAtomsAddInit(pddl_ground_atoms_t *ga, const pddl_t *pddl)
{
    pddl_fm_const_it_atom_t it;
    const pddl_fm_atom_t *atom;
    PDDL_FM_FOR_EACH_ATOM(&pddl->init->fm, &it, atom){
        pddlGroundAtomsAddAtom(ga, atom, NULL);
    }
}

void pddlGroundAtomsPrint(const pddl_ground_atoms_t *ga,
                          const pddl_t *pddl,
                          FILE *fout)
{
    for (int i = 0; i < ga->atom_size; ++i){
        const pddl_ground_atom_t *a = ga->atom[i];
        fprintf(fout, "(%s", pddl->pred.pred[a->pred].name);
        for (int j = 0; j < a->arg_size; ++j)
            fprintf(fout, " %s", pddl->obj.obj[a->arg[j]].name);
        fprintf(fout, "), layer: %d\n", a->layer);
    }
}
