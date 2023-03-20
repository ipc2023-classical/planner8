/***
 * cpddl
 * -------
 * Copyright (c)2015 Daniel Fiser <danfis@danfis.cz>,
 * Agent Technology Center, Department of Computer Science,
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

#include "pddl/open_list.h"

void _pddlOpenListInit(pddl_open_list_t *l,
                       pddl_open_list_del_fn del_fn,
                       pddl_open_list_push_fn push_fn,
                       pddl_open_list_pop_fn pop_fn,
                       pddl_open_list_top_fn top_fn,
                       pddl_open_list_clear_fn clear_fn)
{
    l->del_fn   = del_fn;
    l->push_fn  = push_fn;
    l->pop_fn   = pop_fn;
    l->top_fn   = top_fn;
    l->clear_fn = clear_fn;
}

void _pddlOpenListFree(pddl_open_list_t *l)
{
}
