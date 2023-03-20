/***
 * Copyright (c)2022 Daniel Fiser <danfis@danfis.cz>,
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

#include "search.h"
#include "internal.h"

void _pddlSearchInit(pddl_search_t *s,
                     pddl_search_del_fn fn_del,
                     pddl_search_init_step_fn fn_init_step,
                     pddl_search_step_fn fn_step,
                     pddl_search_extract_plan_fn fn_extract_plan,
                     pddl_search_stat_fn fn_stat)
{
    ZEROIZE(s);
    s->fn_del = fn_del;
    s->fn_init_step = fn_init_step;
    s->fn_step = fn_step;
    s->fn_extract_plan = fn_extract_plan;
    s->fn_stat = fn_stat;
}

static void _pddlSearchFree(pddl_search_t *s)
{
}

void pddlSearchDel(pddl_search_t *s)
{
    s->fn_del(s);
    _pddlSearchFree(s);
}

int pddlSearchInitStep(pddl_search_t *s)
{
    return s->fn_init_step(s);
}

int pddlSearchStep(pddl_search_t *s)
{
    return s->fn_step(s);
}

int pddlSearchExtractPlan(pddl_search_t *s, pddl_plan_t *plan)
{
    return s->fn_extract_plan(s, plan);
}

void pddlSearchStat(const pddl_search_t *s, pddl_search_stat_t *stat)
{
    s->fn_stat(s, stat);
}
