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
#include "pddl/sort.h"
#include "pddl/labeled_transition.h"

void pddlLabeledTransitionsSetInit(pddl_labeled_transitions_set_t *t)
{
    ZEROIZE(t);
}

void pddlLabeledTransitionsSetFree(pddl_labeled_transitions_set_t *t)
{
    for (int i = 0; i < t->trans_size; ++i){
        pddlTransitionsFree(&t->trans[i].trans);
    }
    if (t->trans != NULL)
        FREE(t->trans);
}

pddl_labeled_transitions_t *
    pddlLabeledTransitionsSetAddLabel(pddl_labeled_transitions_set_t *t,
                                      pddl_label_set_t *label,
                                      int *added)
{
    for (int i = 0; i < t->trans_size; ++i){
        if (t->trans[i].label == label){
            *added = 0;
            return t->trans + i;
        }
    }

    if (t->trans_size == t->trans_alloc){
        if (t->trans_alloc == 0)
            t->trans_alloc = 1;
        t->trans_alloc *= 2;
        t->trans = REALLOC_ARR(t->trans, pddl_labeled_transitions_t,
                                   t->trans_alloc);
    }
    pddl_labeled_transitions_t *tr = t->trans + t->trans_size++;
    tr->label = label;
    pddlTransitionsInit(&tr->trans);
    *added = 1;
    return tr;
}

int pddlLabeledTransitionsSetAdd(pddl_labeled_transitions_set_t *t,
                                 pddl_label_set_t *label,
                                 int from,
                                 int to)
{
    pddl_labeled_transitions_t *tr;
    int added;
    tr = pddlLabeledTransitionsSetAddLabel(t, label, &added);
    pddlTransitionsAdd(&tr->trans, from, to);
    if (added)
        return 0;
    return 1;
}

static int cmp(const void *a, const void *b, void *arg)
{
    const pddl_labeled_transitions_t *t1 = a;
    const pddl_labeled_transitions_t *t2 = b;
    return pddlISetCmp(&t1->label->label, &t2->label->label);
}

void pddlLabeledTransitionsSetSort(pddl_labeled_transitions_set_t *t)
{
    pddlSort(t->trans, t->trans_size, sizeof(pddl_labeled_transitions_t),
            cmp, NULL);
    for (int i = 0; i < t->trans_size; ++i)
        pddlTransitionsSort(&t->trans[i].trans);
}
