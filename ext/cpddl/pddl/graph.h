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

#ifndef __PDDL_GRAPH_H__
#define __PDDL_GRAPH_H__

#include <pddl/iset.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct pddl_graph_simple {
    pddl_iset_t *node;
    int node_size;
};
typedef struct pddl_graph_simple pddl_graph_simple_t;

void pddlGraphSimpleInit(pddl_graph_simple_t *g, int node_size);
void pddlGraphSimpleFree(pddl_graph_simple_t *g);
void pddlGraphSimpleAddEdge(pddl_graph_simple_t *g, int n1, int n2);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_GRAPH_H__ */
