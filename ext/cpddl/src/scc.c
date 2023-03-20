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

#include "internal.h"
#include "pddl/scc.h"

void pddlSCCGraphInit(pddl_scc_graph_t *g, int node_size)
{
    ZEROIZE(g);
    g->node_size = node_size;
    g->node = CALLOC_ARR(pddl_iset_t, g->node_size);
}

void pddlSCCGraphInitInduced(pddl_scc_graph_t *g,
                             const pddl_scc_graph_t *src,
                             const pddl_iset_t *ind)
{
    g->node_size = src->node_size;
    g->node = CALLOC_ARR(pddl_iset_t, g->node_size);
    for (int i = 0; i < g->node_size; ++i)
        pddlISetIntersect2(&g->node[i], &src->node[i], ind);
}

void pddlSCCGraphFree(pddl_scc_graph_t *g)
{
    for (int i = 0; i < g->node_size; ++i)
        pddlISetFree(g->node + i);
    if (g->node != NULL)
        FREE(g->node);
}

void pddlSCCGraphAddEdge(pddl_scc_graph_t *g, int from, int to)
{
    pddlISetAdd(&g->node[from], to);
}

/** Context for DFS during computing SCC */
struct scc_dfs {
    const pddl_scc_graph_t *graph;
    int cur_index;
    int *index;
    int *lowlink;
    int *in_stack;
    int *stack;
    int stack_size;
};
typedef struct scc_dfs scc_dfs_t;

static void sccTarjanStrongconnect(pddl_scc_t *scc, scc_dfs_t *dfs, int vert)
{
    dfs->index[vert] = dfs->lowlink[vert] = dfs->cur_index++;
    dfs->stack[dfs->stack_size++] = vert;
    dfs->in_stack[vert] = 1;

    int end_vert;
    PDDL_ISET_FOR_EACH(&dfs->graph->node[vert], end_vert){
        if (dfs->index[end_vert] == -1){
            sccTarjanStrongconnect(scc, dfs, end_vert);
            dfs->lowlink[vert] = PDDL_MIN(dfs->lowlink[vert],
                                          dfs->lowlink[end_vert]);
        }else if (dfs->in_stack[end_vert]){
            dfs->lowlink[vert] = PDDL_MIN(dfs->lowlink[vert],
                                          dfs->lowlink[end_vert]);
        }
    }

    if (dfs->index[vert] == dfs->lowlink[vert]){
        // Create a new component
        if (scc->comp_size == scc->comp_alloc){
            if (scc->comp_alloc == 0)
                scc->comp_alloc = 2;
            scc->comp_alloc *= 2;
            scc->comp = REALLOC_ARR(scc->comp, pddl_iset_t, scc->comp_alloc);
        }
        pddl_iset_t *comp = scc->comp + scc->comp_size++;
        pddlISetInit(comp);

        // Unroll stack
        int i;
        for (i = dfs->stack_size - 1; dfs->stack[i] != vert; --i){
            dfs->in_stack[dfs->stack[i]] = 0;
            pddlISetAdd(comp, dfs->stack[i]);
        }
        dfs->in_stack[dfs->stack[i]] = 0;
        pddlISetAdd(comp, dfs->stack[i]);

        // Shrink stack
        dfs->stack_size = i;
    }
}

static void sccTarjan(pddl_scc_t *scc, const pddl_scc_graph_t *graph)
{
    scc_dfs_t dfs;

    // Initialize structure for Tarjan's algorithm
    dfs.graph = graph;
    dfs.cur_index = 0;
    dfs.index    = ALLOC_ARR(int, 4 * graph->node_size);
    dfs.lowlink  = dfs.index + graph->node_size;
    dfs.in_stack = dfs.lowlink + graph->node_size;
    dfs.stack    = dfs.in_stack + graph->node_size;
    dfs.stack_size = 0;
    for (int i = 0; i < graph->node_size; ++i){
        dfs.index[i] = dfs.lowlink[i] = -1;
        dfs.in_stack[i] = 0;
    }

    for (int node = 0; node < graph->node_size; ++node){
        if (dfs.index[node] == -1)
            sccTarjanStrongconnect(scc, &dfs, node);
    }

    FREE(dfs.index);
}

void pddlSCC(pddl_scc_t *scc, const pddl_scc_graph_t *graph)
{
    ZEROIZE(scc);
    sccTarjan(scc, graph);
}

void pddlSCCFree(pddl_scc_t *scc)
{
    for (int i = 0; i < scc->comp_size; ++i)
        pddlISetFree(scc->comp + i);
    if (scc->comp != NULL)
        FREE(scc->comp);
}



static void cycleAdd(pddl_graph_simple_cycles_t *cycles,
                     const pddl_iarr_t *cycle)
{
    if (cycles->cycle_size == cycles->cycle_alloc){
        if (cycles->cycle_alloc == 0)
            cycles->cycle_alloc = 2;
        cycles->cycle_alloc *= 2;
        cycles->cycle = REALLOC_ARR(cycles->cycle, pddl_iarr_t,
                                    cycles->cycle_alloc);
    }
    pddl_iarr_t *dst = cycles->cycle + cycles->cycle_size++;

    pddlIArrInit(dst);
    int n;
    PDDL_IARR_FOR_EACH(cycle, n)
        pddlIArrAdd(dst, n);
}

static void cycleUnblock(int node, pddl_iset_t *B, int *blocked)
{
    if (blocked[node]){
        blocked[node] = 0;
        int n;
        PDDL_ISET_FOR_EACH(&B[node], n)
            cycleUnblock(n, B, blocked);
        pddlISetEmpty(&B[node]);
    }
}

static int circuit(int node,
                   int start_node,
                   const pddl_scc_graph_t *component,
                   pddl_iarr_t *path,
                   pddl_iset_t *B,
                   int *blocked,
                   pddl_graph_simple_cycle_fn fn,
                   void *userdata)
{
    int ret = 0;
    int closed = 0;
    pddlIArrAdd(path, node);
    blocked[node] = 1;

    int next_node;
    PDDL_ISET_FOR_EACH(component->node + node, next_node){
        if (next_node == start_node){
            closed = 1;
            if (fn(path, userdata) != PDDL_GRAPH_SIMPLE_CYCLE_CONT){
                ret = -1;
                break;
            }
        }else if (!blocked[next_node]){
            int r = circuit(next_node, start_node, component, path, B, blocked,
                            fn, userdata);
            if (r == 1){
                closed = 1;
            }else if (r == -1){
                ret = -1;
                break;
            }
        }
    }

    if (closed){
        cycleUnblock(node, B, blocked);
    }else{
        int next_node;
        PDDL_ISET_FOR_EACH(component->node + node, next_node){
            if (!pddlISetIn(node, &B[next_node]))
                pddlISetAdd(&B[next_node], node);
        }
    }

    pddlIArrRmLast(path);
    if (ret == 0)
        ret = closed;
    return ret;
}

void pddlGraphSimpleCyclesFn(const pddl_scc_graph_t *graph,
                             pddl_graph_simple_cycle_fn fn,
                             void *userdata)
{
    int *blocked = CALLOC_ARR(int, graph->node_size);
    pddl_iset_t *B = CALLOC_ARR(pddl_iset_t, graph->node_size);

    PDDL_ISET(active_nodes);
    for (int i = 0; i < graph->node_size; ++i){
        if (pddlISetSize(&graph->node[i]) > 0)
            pddlISetAdd(&active_nodes, i);
    }

    int cont = 0;
    while (cont != -1 && pddlISetSize(&active_nodes) > 0){
        int node = pddlISetGet(&active_nodes, pddlISetSize(&active_nodes) - 1);
        pddl_scc_graph_t subgraph;
        pddlSCCGraphInitInduced(&subgraph, graph, &active_nodes);

        pddl_scc_t scc;
        pddlSCC(&scc, &subgraph);
        const pddl_iset_t *comp = NULL;
        for (int i = 0; i < scc.comp_size; ++i){
            if (pddlISetIn(node, scc.comp + i)){
                comp = scc.comp + i;
                break;
            }
        }
        if (comp != NULL && pddlISetSize(comp) > 1){
            pddl_scc_graph_t component;
            pddlSCCGraphInitInduced(&component, graph, comp);
            int n;
            PDDL_ISET_FOR_EACH(comp, n){
                blocked[n] = 0;
                pddlISetEmpty(B + n);
            }
            PDDL_IARR(path);
            cont = circuit(node, node, &component, &path, B, blocked,
                           fn, userdata);
            pddlIArrFree(&path);
            pddlSCCGraphFree(&component);
        }

        pddlSCCFree(&scc);
        pddlSCCGraphFree(&subgraph);
        pddlISetRm(&active_nodes, node);
    }

    pddlISetFree(&active_nodes);
    for (int i = 0; i < graph->node_size; ++i)
        pddlISetFree(B + i);
    FREE(B);
    FREE(blocked);
}

static int fnAddCycle(const pddl_iarr_t *path, void *ud)
{
    pddl_graph_simple_cycles_t *cycles = ud;
    cycleAdd(cycles, path);
    return PDDL_GRAPH_SIMPLE_CYCLE_CONT;
}

void pddlGraphSimpleCycles(pddl_graph_simple_cycles_t *cycles,
                           const pddl_scc_graph_t *graph)
{
    ZEROIZE(cycles);
    pddlGraphSimpleCyclesFn(graph, fnAddCycle, cycles);
}

void pddlGraphSimpleCyclesFree(pddl_graph_simple_cycles_t *cycles)
{
    for (int i = 0; i < cycles->cycle_size; ++i)
        pddlIArrFree(cycles->cycle + i);
    if (cycles->cycle != NULL)
        FREE(cycles->cycle);
}
