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
#include "pddl/clique.h"

struct bk_stack_el {
    pddl_iset_t clique;
    pddl_iset_t P;
    pddl_iset_t X;
};
typedef struct bk_stack_el bk_stack_el_t;

struct bk_stack {
    bk_stack_el_t **stack;
    int stack_size;
    int stack_alloc;
};
typedef struct bk_stack bk_stack_t;

static void stackElDel(bk_stack_el_t *e)
{
    pddlISetFree(&e->clique);
    pddlISetFree(&e->P);
    pddlISetFree(&e->X);
    FREE(e);
}

static void stackFree(bk_stack_t *st)
{
    for (int i = 0; i < st->stack_size; ++i)
        stackElDel(st->stack[i]);
    if (st->stack != NULL)
        FREE(st->stack);
}

static bk_stack_el_t *stackPop(bk_stack_t *st)
{
    if (st->stack_size == 0)
        return NULL;
    return st->stack[--st->stack_size];
}

static void stackPush(bk_stack_t *st,
                      const pddl_iset_t *clique,
                      const pddl_iset_t *P,
                      const pddl_iset_t *X,
                      int v,
                      const pddl_iset_t *v_N)
{
    if (st->stack_size == st->stack_alloc){
        if (st->stack_alloc == 0)
            st->stack_alloc = 2;
        st->stack_alloc *= 2;
        st->stack = REALLOC_ARR(st->stack, bk_stack_el_t *,
                                st->stack_alloc);
    }
    bk_stack_el_t *s = ALLOC(bk_stack_el_t);
    pddlISetInit(&s->clique);
    pddlISetInit(&s->P);
    pddlISetInit(&s->X);

    pddlISetUnion(&s->clique, clique);
    if (v >= 0)
        pddlISetAdd(&s->clique, v);

    if (v_N != NULL){
        pddlISetIntersect2(&s->P, P, v_N);
        pddlISetIntersect2(&s->X, X, v_N);
    }else{
        pddlISetUnion(&s->P, P);
        pddlISetUnion(&s->X, X);
    }

    st->stack[st->stack_size++] = s;
}

static int selectPivot(const pddl_graph_simple_t *graph,
                       const pddl_iset_t *P,
                       const pddl_iset_t *X)
{
    int pivot = -1;
    int pivot_size = -1;
    int fact;
    PDDL_ISET_FOR_EACH(P, fact){
        int size = pddlISetIntersectionSize(P, &graph->node[fact]);
        if (size > pivot_size){
            pivot_size = size;
            pivot = fact;
        }
    }
    PDDL_ISET_FOR_EACH(X, fact){
        int size = pddlISetIntersectionSize(P, &graph->node[fact]);
        if (size > pivot_size){
            pivot_size = size;
            pivot = fact;
        }
    }

    return pivot;
}

static void inferCliques(const pddl_graph_simple_t *graph,
                         bk_stack_t *stack,
                         void (*cb)(const pddl_iset_t *clique, void *userdata),
                         void *userdata)
{
    bk_stack_el_t *s;

    while ((s = stackPop(stack)) != NULL){
        int pivot = selectPivot(graph, &s->P, &s->X);

        PDDL_ISET(P_next);
        PDDL_ISET(X_next);
        pddlISetUnion(&P_next, &s->P);
        pddlISetUnion(&X_next, &s->X);

        const pddl_iset_t *pivot_N = &graph->node[pivot];
        int size = pddlISetSize(&s->P);
        int pivot_size = pddlISetSize(pivot_N);
        for (int i = 0, pi = 0; i < size; ++i){
            int P_v = pddlISetGet(&s->P, i);
            // skip pivot's neighbors
            for (; pi < pivot_size && pddlISetGet(pivot_N, pi) < P_v; ++pi);
            if (pi < pivot_size && pddlISetGet(pivot_N, pi) == P_v){
                ++pi;
                continue;
            }

            const pddl_iset_t *P_v_N = &graph->node[P_v];
            if (pddlISetIntersectionSizeAtLeast(&P_next, P_v_N, 1)){
                stackPush(stack, &s->clique, &P_next, &X_next, P_v, P_v_N);

            }else if (!pddlISetIntersectionSizeAtLeast(&X_next, P_v_N, 1)){
                // {P,X}_next \cap P_v_N = \emptyset so s->clique \cup {P_v}
                // forms a maximal clique
                PDDL_ISET(mg);
                pddlISetUnion(&mg, &s->clique);
                pddlISetAdd(&mg, P_v);
                if (pddlISetSize(&mg) > 1)
                    cb(&mg, userdata);
                pddlISetFree(&mg);
            }

            pddlISetRm(&P_next, P_v);
            pddlISetAdd(&X_next, P_v);
        }

        pddlISetFree(&P_next);
        pddlISetFree(&X_next);

        stackElDel(s);
    }
}


// TODO: Add interface to cliquer library
void pddlCliqueFindMaximal(const pddl_graph_simple_t *g,
                           void (*cb)(const pddl_iset_t *clique, void *userdata),
                           void *userdata)
{

    PDDL_ISET(all_facts);
    PDDL_ISET(empty);
    for (int i = 0; i < g->node_size; ++i)
        pddlISetAdd(&all_facts, i);

    bk_stack_t stack;
    ZEROIZE(&stack);
    stackPush(&stack, &empty, &all_facts, &empty, -1, NULL);
    inferCliques(g, &stack, cb, userdata);
    stackFree(&stack);
    pddlISetFree(&all_facts);
}

#ifdef PDDL_CLIQUER
# include <cliquer/cliquer.h>
struct cliquer_data {
    void (*cb)(const pddl_iset_t *clique, void *userdata);
    void *userdata;
};

static boolean cliquerCB(set_t clq, graph_t *G, clique_options *opts)
{
    struct cliquer_data *d = opts->user_data;
    PDDL_ISET(clique);
    for (int i = -1; (i = set_return_next(clq, i)) >= 0;)
        pddlISetAdd(&clique, i);
    d->cb(&clique, d->userdata);
    pddlISetFree(&clique);
    return 1;
}

void pddlCliqueFindMaximalCliquer(const pddl_graph_simple_t *g,
                                  void (*cb)(const pddl_iset_t *clique, void *userdata),
                                  void *userdata)
{
    graph_t *G = graph_new(g->node_size);
    for (int v = 0; v < g->node_size; ++v){
        int w;
        PDDL_ISET_FOR_EACH(&g->node[v], w){
            if (v < w)
                GRAPH_ADD_EDGE(G, v, w);
        }
    }

    struct cliquer_data d = { cb, userdata };
    clique_options opts = *clique_default_options;
    opts.user_function = cliquerCB;
    opts.user_data = &d;
    opts.time_function = NULL;
    opts.output = NULL;
    clique_find_all(G, 2, g->node_size, 1, &opts);
    graph_free(G);
}
#else /* PDDL_CLIQUER */
void pddlCliqueFindMaximalCliquer(const pddl_graph_simple_t *g,
                                  void (*cb)(const pddl_iset_t *clique, void *userdata),
                                  void *userdata)
{
    PANIC("Cliquer library is not linked!");
}
#endif /* PDDL_CLIQUER */

struct biclique_ud {
    void (*cb)(const pddl_iset_t *left,
               const pddl_iset_t *right, void *ud);
    void *ud;
    int node_size;
};

static void bicliqueCB(const pddl_iset_t *clique, void *ud)
{
    struct biclique_ud *bud = ud;
    PDDL_ISET(left);
    PDDL_ISET(right);
    int n;
    PDDL_ISET_FOR_EACH(clique, n){
        if (n < bud->node_size){
            pddlISetAdd(&left, n);
        }else{
            pddlISetAdd(&right, n - bud->node_size);
        }
    }
    if (pddlISetSize(&left) > 0
            && pddlISetSize(&right) > 0
            && pddlISetGet(&left, 0) < pddlISetGet(&right, 0)){
        bud->cb(&left, &right, bud->ud);
    }
    pddlISetFree(&left);
    pddlISetFree(&right);
}

void pddlCliqueFindMaximalBicliques(const pddl_graph_simple_t *g,
                                    void (*cb)(const pddl_iset_t *left,
                                               const pddl_iset_t *right,
                                               void *ud),
                                    void *ud)
{
    pddl_graph_simple_t graph;
    pddlGraphSimpleInit(&graph, g->node_size * 2);
    for (int i = 0; i < g->node_size; ++i){
        for (int j = i + 1; j < g->node_size; ++j){
            pddlGraphSimpleAddEdge(&graph, i, j);
            pddlGraphSimpleAddEdge(&graph, g->node_size + i, g->node_size + j);
        }
        int n;
        PDDL_ISET_FOR_EACH(&g->node[i], n){
            pddlGraphSimpleAddEdge(&graph, i, g->node_size + n);
            pddlGraphSimpleAddEdge(&graph, g->node_size + i, n);
        }
    }

    struct biclique_ud bud = { cb, ud, g->node_size };
    pddlCliqueFindMaximal(&graph, bicliqueCB, (void *)&bud);
    pddlGraphSimpleFree(&graph);
}
