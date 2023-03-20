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
#include "pddl/pq.h"
#include "pddl/trans_system_graph.h"
#include "internal.h"

static void pddlTransSystemGraphEdgesAdd(pddl_trans_system_graph_edges_t *e,
                                         int end,
                                         int cost)
{
    if (e->edge_size == e->edge_alloc){
        if (e->edge_alloc == 0)
            e->edge_alloc = 2;
        e->edge_alloc *= 2;
        e->edge = REALLOC_ARR(e->edge, pddl_trans_system_graph_edge_t,
                              e->edge_alloc);
    }
    pddl_trans_system_graph_edge_t *edge = e->edge + e->edge_size++;
    edge->end = end;
    edge->cost = cost;
}


static int edgesCmp(const void *a, const void *b, void *_)
{
    const pddl_trans_system_graph_edge_t *e1 = a;
    const pddl_trans_system_graph_edge_t *e2 = b;
    int cmp = e1->end - e2->end;
    if (cmp == 0)
        cmp = e1->cost - e2->cost;
    return cmp;
}

static void pddlTransSystemGraphEdgesSort(pddl_trans_system_graph_edges_t *e)
{
    pddlSort(e->edge, e->edge_size, sizeof(*e->edge), edgesCmp, NULL);
}

void pddlTransSystemGraphInit(pddl_trans_system_graph_t *g,
                              const pddl_trans_system_t *t)
{
    ZEROIZE(g);
    g->num_states = t->num_states;
    g->fw = CALLOC_ARR(pddl_trans_system_graph_edges_t, g->num_states);
    g->bw = CALLOC_ARR(pddl_trans_system_graph_edges_t, g->num_states);
    for (int lti = 0; lti < t->trans.trans_size; ++lti){
        int cost = t->trans.trans[lti].label->cost;
        const pddl_transitions_t *trs = &t->trans.trans[lti].trans;
        for (int ti = 0; ti < trs->trans_size; ++ti){
            const pddl_transition_t *tr = trs->trans + ti;
            pddlTransSystemGraphEdgesAdd(g->fw + tr->from, tr->to, cost);
            pddlTransSystemGraphEdgesAdd(g->bw + tr->to, tr->from, cost);
        }
    }

    for (int v = 0; v < g->num_states; ++v){
        pddlTransSystemGraphEdgesSort(g->fw + v);
        pddlTransSystemGraphEdgesSort(g->bw + v);
    }

    g->init = t->init_state;
    pddlISetUnion(&g->goal, &t->goal_states);
}

void pddlTransSystemGraphFree(pddl_trans_system_graph_t *t)
{
    pddlISetFree(&t->goal);
    for (int i = 0; i < t->num_states; ++i){
        if (t->fw[i].edge != NULL)
            FREE(t->fw[i].edge);
        if (t->bw[i].edge != NULL)
            FREE(t->bw[i].edge);
    }
    if (t->fw != NULL)
        FREE(t->fw);
    if (t->bw != NULL)
        FREE(t->bw);
}

static void computeDist(int num_states,
                        const pddl_trans_system_graph_edges_t *edges,
                        const pddl_iset_t *init,
                        int *dist)
{
    pddl_pq_el_t *els = CALLOC_ARR(pddl_pq_el_t, num_states);
    for (int v = 0; v < num_states; ++v)
        dist[v] = -1;

    pddl_pq_t queue;
    pddlPQInit(&queue);

    int v;
    PDDL_ISET_FOR_EACH(init, v){
        dist[v] = 0;
        pddlPQPush(&queue, dist[v], els + v);
    }

    while (!pddlPQEmpty(&queue)){
        int v_dist;
        pddl_pq_el_t *v_el = pddlPQPop(&queue, &v_dist);
        int v = v_el - els;
        ASSERT(v >= 0 && v < num_states && els + v == v_el);
        ASSERT(v_dist == dist[v]);

        for (int ei = 0; ei < edges[v].edge_size; ++ei){
            const pddl_trans_system_graph_edge_t *edge = edges[v].edge + ei;
            int next_dist = v_dist + edge->cost;
            if (dist[edge->end] < 0){
                dist[edge->end] = next_dist;
                pddlPQPush(&queue, dist[edge->end], els + edge->end);

            }else if (next_dist < dist[edge->end]){
                dist[edge->end] = next_dist;
                pddlPQUpdate(&queue, dist[edge->end], els + edge->end);
            }
        }
    }

    pddlPQFree(&queue);
    FREE(els);
}

void pddlTransSystemGraphFwDist(pddl_trans_system_graph_t *g, int *dist)
{
    PDDL_ISET(init);
    pddlISetAdd(&init, g->init);
    computeDist(g->num_states, g->fw, &init, dist);
    pddlISetFree(&init);
}

void pddlTransSystemGraphBwDist(pddl_trans_system_graph_t *g, int *dist)
{
    computeDist(g->num_states, g->bw, &g->goal, dist);
}


/** Strongly connected components */
struct scc {
    pddl_iset_t *comp; /*!< List of components */
    int comp_size; /*!< Number of components */
    int comp_alloc;
};
typedef struct scc scc_t;

/** Context for DFS during computing SCC */
struct scc_dfs {
    int cur_index;
    int *index;
    int *lowlink;
    int *in_stack;
    int *stack;
    int stack_size;
};
typedef struct scc_dfs scc_dfs_t;

static void sccTarjanStrongconnect(scc_t *scc,
                                   scc_dfs_t *dfs,
                                   const pddl_trans_system_graph_edges_t *edges,
                                   int vert)
{
    dfs->index[vert] = dfs->lowlink[vert] = dfs->cur_index++;
    dfs->stack[dfs->stack_size++] = vert;
    dfs->in_stack[vert] = 1;

    int i;
    for (i = 0; i < edges[vert].edge_size; ++i){
        int w = edges[vert].edge[i].end;
        if (dfs->index[w] == -1){
            sccTarjanStrongconnect(scc, dfs, edges, w);
            dfs->lowlink[vert] = PDDL_MIN(dfs->lowlink[vert], dfs->lowlink[w]);
        }else if (dfs->in_stack[w]){
            dfs->lowlink[vert] = PDDL_MIN(dfs->lowlink[vert], dfs->lowlink[w]);
        }
    }

    if (dfs->index[vert] == dfs->lowlink[vert]){
        // Find how deep unroll stack
        for (i = dfs->stack_size - 1; dfs->stack[i] != vert; --i)
            dfs->in_stack[dfs->stack[i]] = 0;
        dfs->in_stack[dfs->stack[i]] = 0;

        // Create new component if necessary
        if (scc->comp_size == scc->comp_alloc){
            if (scc->comp_alloc == 0)
                scc->comp_alloc = 2;
            scc->comp_alloc *= 2;
            scc->comp = REALLOC_ARR(scc->comp, pddl_iset_t, scc->comp_alloc);
        }
        pddl_iset_t *comp = scc->comp + scc->comp_size++;

        // Copy vertext IDs from the stack to the component
        pddlISetInit(comp);
        for (int j = i; j < dfs->stack_size; ++j)
            pddlISetAdd(comp, dfs->stack[j]);

        // Shrink stack
        dfs->stack_size = i;
    }
}

static void sccTarjan(int num_states,
                      const pddl_trans_system_graph_edges_t *edges,
                      pddl_set_iset_t *sset)
{
    scc_t scc;
    ZEROIZE(&scc);

    scc_dfs_t dfs;

    // Initialize structure for Tarjan's algorithm
    dfs.cur_index = 0;
    dfs.index    = ALLOC_ARR(int, 4 * num_states);
    dfs.lowlink  = dfs.index + num_states;
    dfs.in_stack = dfs.lowlink + num_states;
    dfs.stack    = dfs.in_stack + num_states;
    dfs.stack_size = 0;
    for (int i = 0; i < num_states; ++i){
        dfs.index[i] = dfs.lowlink[i] = -1;
        dfs.in_stack[i] = 0;
    }

    for (int node = 0; node < num_states; ++node){
        if (dfs.index[node] == -1)
            sccTarjanStrongconnect(&scc, &dfs, edges, node);
    }

    FREE(dfs.index);

    for (int i = 0; i < scc.comp_size; ++i){
        pddlSetISetAdd(sset, scc.comp + i);
        pddlISetFree(scc.comp + i);
    }
    if (scc.comp != NULL)
        FREE(scc.comp);
}

void pddlTransSystemGraphFwSCC(const pddl_trans_system_graph_t *g,
                               pddl_set_iset_t *sset)
{
    sccTarjan(g->num_states, g->fw, sset);
}

void pddlTransSystemGraphBwSCC(const pddl_trans_system_graph_t *g,
                               pddl_set_iset_t *sset)
{
    sccTarjan(g->num_states, g->bw, sset);
}

void pddlTransSystemGraphFwReachability(const pddl_trans_system_graph_t *g,
                                        pddl_iset_t *reachable_from,
                                        int consider_empty_paths)
{
    for (int s = 0; s < g->num_states; ++s){
        if (consider_empty_paths)
            pddlISetAdd(reachable_from + s, s);
        for (int ei = 0; ei < g->bw[s].edge_size; ++ei){
            int next_s = g->bw[s].edge[ei].end;
            pddlISetAdd(reachable_from + s, next_s);
        }
    }

    int changed = 1;
    while (changed){
        changed = 0;
        for (int s = 0; s < g->num_states; ++s){
            int prev_size = pddlISetSize(reachable_from + s);
            for (int ei = 0; ei < g->bw[s].edge_size; ++ei){
                int next_s = g->bw[s].edge[ei].end;
                if (next_s != s)
                    pddlISetUnion(reachable_from + s, reachable_from + next_s);
            }
            if (prev_size != pddlISetSize(reachable_from + s))
                changed = 1;
        }
    }
}
