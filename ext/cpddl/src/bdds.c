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

#include "internal.h"
#include "pddl/bdds.h"
#include "pddl/sort.h"

void pddlBDDsInit(pddl_bdds_t *bdds)
{
    ZEROIZE(bdds);
}

void pddlBDDsFree(pddl_bdd_manager_t *mgr, pddl_bdds_t *bdds)
{
    for (int i = 0; i < bdds->bdd_size; ++i)
        pddlBDDDel(mgr, bdds->bdd[i]);
    if (bdds->bdd != NULL)
        FREE(bdds->bdd);
}

void pddlBDDsAdd(pddl_bdd_manager_t *mgr, pddl_bdds_t *bdds, pddl_bdd_t *bdd)
{
    if (bdds->bdd_size == bdds->bdd_alloc){
        if (bdds->bdd_alloc == 0)
            bdds->bdd_alloc = 8;
        bdds->bdd_alloc *= 2;
        bdds->bdd = REALLOC_ARR(bdds->bdd, pddl_bdd_t *, bdds->bdd_alloc);
    }
    bdds->bdd[bdds->bdd_size++] = pddlBDDClone(mgr, bdd);
}

long pddlBDDsSize(pddl_bdds_t *bdds)
{
    long nodes = 0;
    for (int i = 0; i < bdds->bdd_size; ++i)
        nodes += pddlBDDSize(bdds->bdd[i]);
    return nodes;
}

void pddlBDDsAndUpdate(pddl_bdd_manager_t *mgr,
                       pddl_bdds_t *bdds,
                       pddl_bdd_t **bdd)
{
    for (int i = 0; i < bdds->bdd_size; ++i)
        pddlBDDAndUpdate(mgr, bdd, bdds->bdd[i]);
}

void pddlBDDsMergeAnd(pddl_bdd_manager_t *mgr,
                      pddl_bdds_t *bdds,
                      int max_nodes,
                      float max_time)
{
    if (bdds->bdd_size == 0)
        return;

    pddl_bdd_t **bdd = CALLOC_ARR(pddl_bdd_t *, bdds->bdd_size);
    int bdd_size = bdds->bdd_size;
    memcpy(bdd, bdds->bdd, sizeof(pddl_bdd_t *) * bdd_size);
    bdds->bdd_size = 0;

    pddl_time_limit_t time_limit;
    pddlTimeLimitInit(&time_limit);
    pddlTimeLimitSet(&time_limit, max_time);
    while (bdd_size > 1){
        if (pddlTimeLimitCheck(&time_limit) < 0)
            break;

        int ins = 0;
        for (int i = 0; i < bdd_size; i = i + 2){
            if (i + 1 >= bdd_size){
                bdd[ins++] = bdd[i];
                continue;
            }

            pddl_bdd_t *bdd1 = bdd[i];
            pddl_bdd_t *bdd2 = bdd[i + 1];
            if (bdd1 == NULL && bdd2 == NULL){
                bdd[ins] = NULL;

            }else if (bdd1 == NULL){
                bdd[ins] = bdd2;

            }else if (bdd2 == NULL){
                bdd[ins] = bdd1;

            }else{
                pddl_bdd_t *res = NULL;
                if (pddlBDDSize(bdd1) < max_nodes
                        && pddlBDDSize(bdd2) < max_nodes){
                    res = pddlBDDAndLimit(mgr, bdd1, bdd2, max_nodes,
                                          &time_limit);
                }

                if (res != NULL){
                    bdd[ins] = res;
                }else{
                    pddlBDDsAdd(mgr, bdds, bdd1);
                    pddlBDDsAdd(mgr, bdds, bdd2);
                    bdd[ins] = NULL;
                }
                pddlBDDDel(mgr, bdd1);
                pddlBDDDel(mgr, bdd2);
            }
            ++ins;
        }
        bdd_size = ins;
    }

    for (int i = 0; i < bdd_size; ++i){
        if (bdd[i] != NULL){
            pddlBDDsAdd(mgr, bdds, bdd[i]);
            pddlBDDDel(mgr, bdd[i]);
        }
    }

    FREE(bdd);
}


void pddlBDDsCostsInit(pddl_bdds_costs_t *bdds)
{
    ZEROIZE(bdds);
}

void pddlBDDsCostsFree(pddl_bdd_manager_t *mgr, pddl_bdds_costs_t *bdds)
{
    for (int i = 0; i < bdds->bdd_size; ++i)
        pddlBDDDel(mgr, bdds->bdd[i].bdd);
    if (bdds->bdd != NULL)
        FREE(bdds->bdd);
}

void pddlBDDsCostsAdd(pddl_bdd_manager_t *mgr,
                      pddl_bdds_costs_t *bdds,
                      pddl_bdd_t *bdd,
                      const pddl_cost_t *cost)
{
    if (bdds->bdd_size == bdds->bdd_alloc){
        if (bdds->bdd_alloc == 0)
            bdds->bdd_alloc = 8;
        bdds->bdd_alloc *= 2;
        bdds->bdd = REALLOC_ARR(bdds->bdd, pddl_bdd_cost_t, bdds->bdd_alloc);
    }
    pddl_bdd_cost_t *b = bdds->bdd + bdds->bdd_size++;
    b->bdd = pddlBDDClone(mgr, bdd);
    if (cost != NULL){
        b->cost = *cost;
    }else{
        pddlCostSetZero(&b->cost);
    }
}

static int bddsCostsCmp(const void *a, const void *b, void *_)
{
    const pddl_bdd_cost_t *b1 = a;
    const pddl_bdd_cost_t *b2 = b;
    return pddlCostCmp(&b1->cost, &b2->cost);
}

void pddlBDDsCostsSortUniq(pddl_bdd_manager_t *mgr, pddl_bdds_costs_t *bdds)
{
    pddlSort(bdds->bdd, bdds->bdd_size, sizeof(*bdds->bdd), bddsCostsCmp, NULL);
    int last = 0;
    for (int i = 1; i < bdds->bdd_size; ++i){
        if (pddlCostCmp(&bdds->bdd[last].cost, &bdds->bdd[i].cost) == 0){
            pddlBDDOrUpdate(mgr, &bdds->bdd[last].bdd, bdds->bdd[i].bdd);
            pddlBDDDel(mgr, bdds->bdd[i].bdd);
        }else{
            bdds->bdd[++last] = bdds->bdd[i];
        }
    }
    bdds->bdd_size = last + 1;
}
