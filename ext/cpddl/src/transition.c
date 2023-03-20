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

#include "pddl/sort.h"
#include "pddl/transition.h"
#include "internal.h"

void pddlTransitionsInit(pddl_transitions_t *ts)
{
    ZEROIZE(ts);
}

void pddlTransitionsFree(pddl_transitions_t *ts)
{
    if (ts->trans != NULL)
        FREE(ts->trans);
}

void pddlTransitionsEmpty(pddl_transitions_t *ts)
{
    ts->trans_size = 0;
}

void pddlTransitionsAdd(pddl_transitions_t *ts, int from, int to)
{
    if (ts->trans_size == ts->trans_alloc){
        if (ts->trans_alloc == 0)
            ts->trans_alloc = 1;
        ts->trans_alloc *= 2;
        ts->trans = REALLOC_ARR(ts->trans, pddl_transition_t,
                                ts->trans_alloc);
    }

    pddl_transition_t *t = ts->trans + ts->trans_size++;
    t->from = from;
    t->to = to;
}

void pddlTransitionsUnion(pddl_transitions_t *ts,
                          const pddl_transitions_t *src)
{
    for (int i = 0; i < src->trans_size; ++i)
        pddlTransitionsAdd(ts, src->trans[i].from, src->trans[i].to);
}

static int cmp(const void *a, const void *b, void *arg)
{
    const pddl_transition_t *t1 = a;
    const pddl_transition_t *t2 = b;
    int cmp = t1->from - t2->from;
    if (cmp == 0)
        cmp = t1->to - t2->to;
    return cmp;
}

void pddlTransitionsSort(pddl_transitions_t *ts)
{
    pddlSort(ts->trans, ts->trans_size, sizeof(pddl_transition_t), cmp, NULL);
}
