/***
 * cpddl
 * -------
 * Copyright (c)2020 Daniel Fiser <danfis@danfis.cz>,
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
#include "pddl/label.h"

static pddl_htable_key_t htableHash(const pddl_list_t *key, void *ud)
{
    pddl_label_set_t *s = PDDL_LIST_ENTRY(key, pddl_label_set_t, htable);
    return s->key;
}

static int htableEq(const pddl_list_t *key1, const pddl_list_t *key2, void *ud)
{
    pddl_label_set_t *s1 = PDDL_LIST_ENTRY(key1, pddl_label_set_t, htable);
    pddl_label_set_t *s2 = PDDL_LIST_ENTRY(key2, pddl_label_set_t, htable);
    return pddlISetEq(&s1->label, &s2->label);
}


pddl_label_set_t *pddlLabelSetNew(const pddl_iset_t *s)
{
    pddl_label_set_t *ls;

    ls = ZALLOC(pddl_label_set_t);
    pddlISetUnion(&ls->label, s);
    ls->cost = 0;
    ls->ref = 1;
    ls->key = pddlFastHash_64(s->s, sizeof(int) * s->size, 7583);
    pddlListInit(&ls->htable);
    return ls;
}

void pddlLabelSetDel(pddl_label_set_t *s)
{
    pddlISetFree(&s->label);
    FREE(s);
}

void pddlLabelSetCost(pddl_labels_t *lbs, pddl_label_set_t *s)
{
    if (pddlISetSize(&s->label) == 0){
        s->cost = 0;
        return;
    }

    int l;
    s->cost = INT_MAX;
    PDDL_ISET_FOR_EACH(&s->label, l)
        s->cost = PDDL_MIN(s->cost, lbs->label[l].cost);
}

void pddlLabelsInitFromStripsOps(pddl_labels_t *lbs,
                                 const pddl_strips_ops_t *ops)
{
    lbs->label_size = lbs->label_alloc = ops->op_size;
    lbs->label = ALLOC_ARR(pddl_label_t, lbs->label_alloc);
    for (int op_id = 0; op_id < ops->op_size; ++op_id){
        lbs->label[op_id].op_id = op_id;
        lbs->label[op_id].cost = ops->op[op_id]->cost;
    }

    lbs->label_set = pddlHTableNew(htableHash, htableEq, lbs);
}

void pddlLabelsFree(pddl_labels_t *lbs)
{
    if (lbs->label != NULL)
        FREE(lbs->label);

    pddl_list_t list, *item;
    pddlListInit(&list);
    pddlHTableGather(lbs->label_set, &list);
    while (!pddlListEmpty(&list)){
        item = pddlListNext(&list);
        pddlListDel(item);
        pddl_label_set_t *s
            = PDDL_LIST_ENTRY(item, pddl_label_set_t, htable);
        pddlLabelSetDel(s);
    }

    pddlHTableDel(lbs->label_set);
}

pddl_label_set_t *pddlLabelsAddSet(pddl_labels_t *lbs,
                                   const pddl_iset_t *labels)
{
    pddl_label_set_t *ls;
    ls = pddlLabelSetNew(labels);

    pddl_list_t *found;
    if ((found = pddlHTableInsertUnique(lbs->label_set, &ls->htable)) == NULL){
        ls->ref = 1;
        pddlLabelSetCost(lbs, ls);
        return ls;
    }else{
        pddlLabelSetDel(ls);
        ls = PDDL_LIST_ENTRY(found, pddl_label_set_t, htable);
        ls->ref += 1;
        return ls;
    }
}

void pddlLabelsSetDecRef(pddl_labels_t *lbs, pddl_label_set_t *set)
{
    if (--set->ref == 0){
        pddlHTableErase(lbs->label_set, &set->htable);
        pddlLabelSetDel(set);
    }
}

void pddlLabelsSetIncRef(pddl_labels_t *lbs, pddl_label_set_t *set)
{
    ++set->ref;
}
