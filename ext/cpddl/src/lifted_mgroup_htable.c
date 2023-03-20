/***
 * cpddl
 * -------
 * Copyright (c)2019 Daniel Fiser <danfis@danfis.cz>,
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

#include "pddl/hfunc.h"
#include "pddl/lifted_mgroup_htable.h"
#include "internal.h"

struct el {
    int id;
    pddl_lifted_mgroup_t mgroup;
    pddl_htable_key_t hash;
    pddl_list_t htable;
};
typedef struct el el_t;

static pddl_htable_key_t mgroupHash(const pddl_lifted_mgroup_t *m)
{
    int *buf;
    int bufsize;

    bufsize = m->param.param_size * 2;
    for (int i = 0; i < m->cond.size; ++i){
        const pddl_fm_atom_t *a = PDDL_FM_CAST(m->cond.fm[i], atom);
        bufsize += 1 + a->arg_size;
    }

    buf = ALLOC_ARR(int, bufsize);

    for (int i = 0; i < m->param.param_size; ++i){
        buf[2 * i] = m->param.param[i].type;
        buf[2 * i + 1] = m->param.param[i].is_counted_var;
    }

    int ins = 2 * m->param.param_size;
    for (int i = 0; i < m->cond.size; ++i){
        const pddl_fm_atom_t *a = PDDL_FM_CAST(m->cond.fm[i], atom);
        buf[ins++] = a->pred;
        for (int ai = 0; ai < a->arg_size; ++ai){
            if (a->arg[ai].param >= 0){
                buf[ins++] = a->arg[ai].param;
            }else{
                buf[ins++] = a->arg[ai].obj * -1;
            }
        }
    }

    ASSERT(ins == bufsize);
    pddl_htable_key_t hash = pddlCityHash_64(buf, bufsize * sizeof(int));

    FREE(buf);
    return hash;
}

static pddl_htable_key_t htableHash(const pddl_list_t *k, void *_)
{
    el_t *m = PDDL_LIST_ENTRY(k, el_t, htable);
    return m->hash;
}

static int htableEq(const pddl_list_t *k1, const pddl_list_t *k2, void *_)
{
    el_t *m1 = PDDL_LIST_ENTRY(k1, el_t, htable);
    el_t *m2 = PDDL_LIST_ENTRY(k2, el_t, htable);
    return pddlLiftedMGroupEq(&m1->mgroup, &m2->mgroup);
}


void pddlLiftedMGroupHTableInit(pddl_lifted_mgroup_htable_t *h)
{
    el_t el;

    ZEROIZE(h);
    h->htable = pddlHTableNew(htableHash, htableEq, h);

    ZEROIZE(&el);
    h->mgroup = pddlExtArrNew(sizeof(el), NULL, &el);
    h->mgroup_size = 0;
}

void pddlLiftedMGroupHTableFree(pddl_lifted_mgroup_htable_t *h)
{
    for (int i = 0; i < h->mgroup_size; ++i){
        el_t *m = pddlExtArrGet(h->mgroup, i);
        pddlLiftedMGroupFree(&m->mgroup);
    }

    pddlHTableDel(h->htable);
    pddlExtArrDel(h->mgroup);
}

int pddlLiftedMGroupHTableAdd(pddl_lifted_mgroup_htable_t *h,
                              const pddl_lifted_mgroup_t *mg)
{
    el_t *el = pddlExtArrGet(h->mgroup, h->mgroup_size);
    el->mgroup = *mg;
    el->hash = mgroupHash(mg);

    pddl_list_t *ins = pddlHTableInsertUnique(h->htable, &el->htable);
    if (ins == NULL){
        pddlLiftedMGroupInitCopy(&el->mgroup, mg);
        el->id = h->mgroup_size;
        ++h->mgroup_size;
        return el->id;

    }else{
        el = PDDL_LIST_ENTRY(ins, el_t, htable);
        return el->id;
    }
}

const pddl_lifted_mgroup_t *pddlLiftedMGroupHTableGet(
                                const pddl_lifted_mgroup_htable_t *h, int id)
{
    if (id < 0 || id >= h->mgroup_size)
        return NULL;
    const el_t *e = pddlExtArrGet(h->mgroup, id);
    return &e->mgroup;
}
