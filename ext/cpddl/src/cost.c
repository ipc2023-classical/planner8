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

#include "pddl/cost.h"

pddl_cost_t pddl_cost_zero = { 0, 0 };
pddl_cost_t pddl_cost_max = { PDDL_COST_MAX, PDDL_COST_MAX };
pddl_cost_t pddl_cost_dead_end = { PDDL_COST_DEAD_END, PDDL_COST_DEAD_END };

const char *pddlCostFmt(const pddl_cost_t *c, char *s, size_t s_size)
{
    snprintf(s, s_size, "%d:%d", c->cost, c->zero_cost);
    return s;
}
