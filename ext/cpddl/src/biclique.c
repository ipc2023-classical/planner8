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
#include "pddl/set.h"
#include "pddl/biclique.h"

static void otherSide(const pddl_graph_simple_t *g,
                      const pddl_iset_t *L,
                      pddl_iset_t *R)
{
    pddlISetEmpty(R);
    if (pddlISetSize(L) == 0)
        return;
    pddlISetUnion(R, &g->node[pddlISetGet(L, 0)]);
    for (int i = 1; i < pddlISetSize(L); ++i)
        pddlISetIntersect(R, &g->node[pddlISetGet(L, i)]);
}

static void expandStars(const pddl_set_iset_t *stars,
                        pddl_set_iset_t *next,
                        pddl_set_iset_t *bicliques)
{
    PDDL_ISET(inter);
    pddlSetISetFree(next);
    pddlSetISetInit(next);

    for (int i = 0; i < pddlSetISetSize(stars); ++i){
        const pddl_iset_t *c1 = pddlSetISetGet(stars, i);
        for (int j = i + 1; j < pddlSetISetSize(stars); ++j){
            const pddl_iset_t *c2 = pddlSetISetGet(stars, j);
            pddlISetIntersect2(&inter, c1, c2);
            if (pddlISetSize(&inter) > 0
                    && pddlSetISetFind(bicliques, &inter) < 0
                    && pddlSetISetFind(next, &inter) < 0){
                pddlSetISetAdd(next, &inter);
                pddlSetISetAdd(bicliques, &inter);
            }
        }
    }
    pddlISetFree(&inter);
}

static void expand(const pddl_set_iset_t *stars,
                   const pddl_set_iset_t *cur,
                   pddl_set_iset_t *next,
                   pddl_set_iset_t *bicliques)
{
    PDDL_ISET(inter);
    pddlSetISetFree(next);
    pddlSetISetInit(next);

    for (int i = 0; i < pddlSetISetSize(stars); ++i){
        const pddl_iset_t *c1 = pddlSetISetGet(stars, i);
        for (int j = 0; j < pddlSetISetSize(cur); ++j){
            const pddl_iset_t *c2 = pddlSetISetGet(cur, j);
            pddlISetIntersect2(&inter, c1, c2);
            if (pddlISetSize(&inter) > 0
                    && pddlSetISetFind(bicliques, &inter) < 0
                    && pddlSetISetFind(next, &inter) < 0){
                pddlSetISetAdd(next, &inter);
                pddlSetISetAdd(bicliques, &inter);
            }
        }
    }
    pddlISetFree(&inter);
}

void pddlBicliqueFindMaximal(const pddl_graph_simple_t *g,
                             void (*cb)(const pddl_iset_t *left,
                                        const pddl_iset_t *right, void *ud),
                             void *ud)
{
    pddl_set_iset_t stars; // A list of stars
    pddl_set_iset_t bicliques;
    pddl_set_iset_t cur[2];

    // Construct stars from each node
    pddlSetISetInit(&stars);
    for (int v = 0; v < g->node_size; ++v){
        if (pddlISetSize(&g->node[v]) > 0)
            pddlSetISetAdd(&stars, &g->node[v]);
    }

    pddlSetISetInit(&bicliques);
    pddlSetISetUnion(&bicliques, &stars);

    pddlSetISetInit(cur + 0);
    pddlSetISetInit(cur + 1);

    expandStars(&stars, cur + 0, &bicliques);

    for (int curi = 0; pddlSetISetSize(cur + curi) > 0; curi = (curi + 1) % 2){
        int otheri = (curi + 1) % 2;
        expand(&stars, cur + curi, cur + otheri, &bicliques);
    }

    PDDL_ISET(R);
    for (int i = 0; i < pddlSetISetSize(&bicliques); ++i){
        const pddl_iset_t *L = pddlSetISetGet(&bicliques, i);
        otherSide(g, L, &R);
        if (pddlISetSize(L) > 0
                && pddlISetSize(&R) > 0
                && pddlISetGet(L, 0) < pddlISetGet(&R, 0)){
            cb(L, &R, ud);
        }
    }
    pddlISetFree(&R);

    pddlSetISetFree(cur + 0);
    pddlSetISetFree(cur + 1);
    pddlSetISetFree(&bicliques);
    pddlSetISetFree(&stars);
}
