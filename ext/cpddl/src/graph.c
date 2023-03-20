/***
 * cpddl
 * -------
 * Copyright (c)2020 Daniel Fiser <danfis@danfis.cz>,
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
#include "pddl/graph.h"

void pddlGraphSimpleInit(pddl_graph_simple_t *g, int node_size)
{
    ZEROIZE(g);
    g->node_size = node_size;
    g->node = CALLOC_ARR(pddl_iset_t, g->node_size);
}

void pddlGraphSimpleFree(pddl_graph_simple_t *g)
{
    for (int i = 0; i < g->node_size; ++i)
        pddlISetFree(g->node + i);
    if (g->node != NULL)
        FREE(g->node);
}

void pddlGraphSimpleAddEdge(pddl_graph_simple_t *g, int n1, int n2)
{
    pddlISetAdd(&g->node[n1], n2);
    pddlISetAdd(&g->node[n2], n1);
}
