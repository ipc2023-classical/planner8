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
 * see accompanying file BDS-LICENSE for details or see
 * <http://www.opensource.org/licenses/bsd-license.php>.
 *
 * This software is distributed WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the License for more information.
 */

#ifndef __PDDL_SCC_H__
#define __PDDL_SCC_H__

#include <pddl/iset.h>
#include <pddl/iarr.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// TODO: Generalize to graph algorithms

/**
 * Directed graph for SCC algorithm.
 */
struct pddl_scc_graph {
    pddl_iset_t *node;
    int node_size;
};
typedef struct pddl_scc_graph pddl_scc_graph_t;

void pddlSCCGraphInit(pddl_scc_graph_t *g, int node_size);
void pddlSCCGraphInitInduced(pddl_scc_graph_t *g,
                             const pddl_scc_graph_t *src,
                             const pddl_iset_t *ind);
void pddlSCCGraphFree(pddl_scc_graph_t *g);
void pddlSCCGraphAddEdge(pddl_scc_graph_t *g, int from, int to);

/**
 * Strongly connected components
 */
struct pddl_scc {
    pddl_iset_t *comp; /*!< List of components */
    int comp_size;    /*!< Number of components */
    int comp_alloc;
};
typedef struct pddl_scc pddl_scc_t;

/**
 * Initializes scc and fills it with strongly connected components.
 * The components are found by Tarjan's algorithm so the resulting
 * components are ordered in a reverse topological order of the DAG of
 * those components.
 */
void pddlSCC(pddl_scc_t *scc, const pddl_scc_graph_t *graph);

/**
 * Free allocated memory.
 */
void pddlSCCFree(pddl_scc_t *scc);


struct pddl_graph_simple_cycles {
    pddl_iarr_t *cycle;
    int cycle_size;
    int cycle_alloc;
};
typedef struct pddl_graph_simple_cycles pddl_graph_simple_cycles_t;

/** TODO */
#define PDDL_GRAPH_SIMPLE_CYCLE_CONT 0
#define PDDL_GRAPH_SIMPLE_CYCLE_STOP 1
typedef int (*pddl_graph_simple_cycle_fn)(const pddl_iarr_t *cycle,
                                          void *userdata);

void pddlGraphSimpleCyclesFn(const pddl_scc_graph_t *graph,
                             pddl_graph_simple_cycle_fn fn,
                             void *userdata);
void pddlGraphSimpleCycles(pddl_graph_simple_cycles_t *cycles,
                           const pddl_scc_graph_t *graph);
void pddlGraphSimpleCyclesFree(pddl_graph_simple_cycles_t *cycles);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_SCC_H__ */
