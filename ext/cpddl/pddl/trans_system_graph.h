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

#ifndef __PDDL_TRANS_SYSTEM_GRAPH_H__
#define __PDDL_TRANS_SYSTEM_GRAPH_H__

#include <pddl/trans_system.h>
#include <pddl/set.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct pddl_trans_system_graph_edge {
    int end; /*!< ID of end vertex */
    int cost; /*!< Cost of the edge */
};
typedef struct pddl_trans_system_graph_edge pddl_trans_system_graph_edge_t;

struct pddl_trans_system_graph_edges {
    pddl_trans_system_graph_edge_t *edge;
    int edge_size;
    int edge_alloc;
};
typedef struct pddl_trans_system_graph_edges pddl_trans_system_graph_edges_t;

struct pddl_trans_system_graph {
    int num_states;
    pddl_trans_system_graph_edges_t *fw; /*!< Forward edges */
    pddl_trans_system_graph_edges_t *bw; /*!< Backward edges */
    int init;
    pddl_iset_t goal;
};
typedef struct pddl_trans_system_graph pddl_trans_system_graph_t;

void pddlTransSystemGraphInit(pddl_trans_system_graph_t *g,
                              const pddl_trans_system_t *t);
void pddlTransSystemGraphFree(pddl_trans_system_graph_t *g);

/**
 * Computes cheapest forward distances from g->init
 */
void pddlTransSystemGraphFwDist(pddl_trans_system_graph_t *g, int *dist);

/**
 * Computes cheapest backward distances from nearest goals.
 */
void pddlTransSystemGraphBwDist(pddl_trans_system_graph_t *g, int *dist);

/**
 * Find strongly connected components with forward edges.
 */
void pddlTransSystemGraphFwSCC(const pddl_trans_system_graph_t *g,
                               pddl_set_iset_t *comps);

/**
 * Find strongly connected components with backward edges.
 */
void pddlTransSystemGraphBwSCC(const pddl_trans_system_graph_t *g,
                               pddl_set_iset_t *comps);

/**
 * Computes reachability of states in forward direction.
 * {reachable_from} must be an array of pddl_iset_t structs of size
 * g->num_states and reachable_from[v] will contain all states from which v
 * is reachable.
 * If consider_empty_paths is true, then a state s will be always considered
 * reachable from s. Otherwise, s will be considered reachable from s only
 * if there is a loop edge in s.
 */
void pddlTransSystemGraphFwReachability(const pddl_trans_system_graph_t *g,
                                        pddl_iset_t *reachable_from,
                                        int consider_empty_paths);
void pddlTransSystemGraphFwReachability2(const pddl_trans_system_graph_t *g,
                                         char *reachable,
                                         int consider_empty_paths);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_TRANS_SYSTEM_GRAPH_H__ */
