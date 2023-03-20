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
#include "pddl/config.h"
#ifdef PDDL_CUDD

#include <cudd/cudd.h>
#include "pddl/bdd.h"


#define M(P) ((DdManager *)(P))
#define PM(P) ((pddl_bdd_manager_t *)(P))
#define B(P) ((DdNode *)(P))
#define PB(P) ((pddl_bdd_t *)(P))

static void outOfMemory(size_t mem_size)
{
    fflush(stdout);
    fprintf(stderr, "Error: CUDD: Memory allocation failed"
                    " when requested %lu bytes.\n",
            (unsigned long)mem_size);
    fflush(stderr);

    struct rusage usg;
    unsigned long peak_mem = 0L;
    if (getrusage(RUSAGE_SELF, &usg) == 0)
        peak_mem = usg.ru_maxrss / 1024UL;
    PANIC("Memory allocation failed (peak memory: %luMB).", peak_mem);
}

pddl_bdd_manager_t *pddlBDDManagerNew(int bdd_var_size,
                                      unsigned int cfg_cache_size)
{
    unsigned int num_slots = CUDD_UNIQUE_SLOTS;
    unsigned int cache_size = CUDD_CACHE_SLOTS;
    size_t mem = 0;
    if (cfg_cache_size > 0){
        cache_size = cfg_cache_size;
        num_slots = cfg_cache_size / bdd_var_size;
    }
    DdManager *mgr = Cudd_Init(bdd_var_size, 0, num_slots, cache_size, mem);
    Cudd_RegisterOutOfMemoryCallback(mgr, outOfMemory);
    return PM(mgr);
}

void pddlBDDManagerDel(pddl_bdd_manager_t *mgr)
{
    ASSERT(Cudd_CheckZeroRef(M(mgr)) == 0);
    Cudd_Quit(M(mgr));
}

float pddlBDDMem(pddl_bdd_manager_t *mgr)
{
    return Cudd_ReadMemoryInUse(M(mgr)) / (1024. * 1024.);
}

int pddlBDDGCUsed(pddl_bdd_manager_t *mgr)
{
    return Cudd_ReadGarbageCollections(M(mgr));
}

void pddlBDDDel(pddl_bdd_manager_t *mgr, pddl_bdd_t *bdd)
{
    Cudd_RecursiveDeref(M(mgr), B(bdd));
}

pddl_bdd_t *pddlBDDClone(pddl_bdd_manager_t *mgr, pddl_bdd_t *bdd)
{
    Cudd_Ref(B(bdd));
    return bdd;
}

int pddlBDDIsFalse(pddl_bdd_manager_t *mgr, pddl_bdd_t *bdd)
{
    return (B(bdd) == Cudd_ReadLogicZero(M(mgr)));
}

pddl_bdd_t *pddlBDDNot(pddl_bdd_manager_t *mgr, pddl_bdd_t *bdd)
{
    DdNode *b = Cudd_Not(B(bdd));
    Cudd_Ref(b);
    return PB(b);
}

pddl_bdd_t *pddlBDDZero(pddl_bdd_manager_t *mgr)
{
    DdNode *b = Cudd_ReadLogicZero(M(mgr));
    Cudd_Ref(b);
    return PB(b);
}

pddl_bdd_t *pddlBDDOne(pddl_bdd_manager_t *mgr)
{
    DdNode *b = Cudd_ReadOne(M(mgr));
    Cudd_Ref(b);
    return PB(b);
}

pddl_bdd_t *pddlBDDVar(pddl_bdd_manager_t *mgr, int i)
{
    DdNode *b = Cudd_bddIthVar(M(mgr), i);
    Cudd_Ref(b);
    return PB(b);
}

pddl_bdd_t *pddlBDDAnd(pddl_bdd_manager_t *mgr,
                       pddl_bdd_t *bdd1,
                       pddl_bdd_t *bdd2)
{
    DdNode *b = Cudd_bddAnd(M(mgr), B(bdd1), B(bdd2));
    if (b != NULL)
        Cudd_Ref(b);
    return PB(b);
}

pddl_bdd_t *pddlBDDAndLimit(pddl_bdd_manager_t *mgr,
                            pddl_bdd_t *bdd1,
                            pddl_bdd_t *bdd2,
                            unsigned int size_limit,
                            pddl_time_limit_t *time_limit)
{
    if (time_limit != NULL && time_limit->limit < 1E10){
        unsigned long lim = pddlTimeLimitRemain(time_limit) * 1000ul;
        Cudd_SetTimeLimit(M(mgr), lim);
        Cudd_ResetStartTime(M(mgr));
    }

    DdNode *b;
    if (size_limit > 0){
        b = Cudd_bddAndLimit(M(mgr), B(bdd1), B(bdd2), size_limit);
    }else{
        b = Cudd_bddAnd(M(mgr), B(bdd1), B(bdd2));
    }
    if (b != NULL)
        Cudd_Ref(b);

    Cudd_UnsetTimeLimit(M(mgr));
    return PB(b);
}

pddl_bdd_t *pddlBDDAndAbstract(pddl_bdd_manager_t *mgr,
                               pddl_bdd_t *bdd1,
                               pddl_bdd_t *bdd2,
                               pddl_bdd_t *cube)
{
    DdNode *b = Cudd_bddAndAbstract(M(mgr), B(bdd1), B(bdd2), B(cube));
    Cudd_Ref(b);
    return PB(b);
}

pddl_bdd_t *pddlBDDAndAbstractLimit(pddl_bdd_manager_t *mgr,
                                    pddl_bdd_t *bdd1,
                                    pddl_bdd_t *bdd2,
                                    pddl_bdd_t *cube,
                                    unsigned int size_limit,
                                    pddl_time_limit_t *time_limit)
{
    if (time_limit != NULL && time_limit->limit < 1E10){
        unsigned long lim = pddlTimeLimitRemain(time_limit) * 1000ul;
        Cudd_SetTimeLimit(M(mgr), lim);
        Cudd_ResetStartTime(M(mgr));
    }

    DdNode *b;
    if (size_limit > 0){
        b = Cudd_bddAndAbstractLimit(M(mgr), B(bdd1), B(bdd2), B(cube), size_limit);
    }else{
        b = Cudd_bddAndAbstract(M(mgr), B(bdd1), B(bdd2), B(cube));
    }
    if (b != NULL)
        Cudd_Ref(b);

    Cudd_UnsetTimeLimit(M(mgr));
    return PB(b);
}

pddl_bdd_t *pddlBDDSwapVars(pddl_bdd_manager_t *mgr,
                            pddl_bdd_t *bdd,
                            pddl_bdd_t **v1,
                            pddl_bdd_t **v2,
                            int n)
{
    DdNode *b = Cudd_bddSwapVariables(M(mgr), B(bdd),
                                      (DdNode **)v1, (DdNode **)v2, n);
    Cudd_Ref(b);
    return PB(b);
}

void pddlBDDPickOneCube(pddl_bdd_manager_t *mgr, pddl_bdd_t *bdd, char *cube)
{
    Cudd_bddPickOneCube(M(mgr), B(bdd), cube);
}

int pddlBDDAndUpdate(pddl_bdd_manager_t *mgr,
                     pddl_bdd_t **bdd1,
                     pddl_bdd_t *bdd2)
{
    pddl_bdd_t *b = pddlBDDAnd(mgr, *bdd1, bdd2);
    if (b == NULL)
        return -1;
    pddlBDDDel(mgr, *bdd1);
    *bdd1 = b;
    return 0;
}

pddl_bdd_t *pddlBDDOr(pddl_bdd_manager_t *mgr,
                      pddl_bdd_t *bdd1,
                      pddl_bdd_t *bdd2)
{
    DdNode *b = Cudd_bddOr(M(mgr), B(bdd1), B(bdd2));
    if (b != NULL)
        Cudd_Ref(b);
    return PB(b);
}

pddl_bdd_t *pddlBDDOrLimit(pddl_bdd_manager_t *mgr,
                           pddl_bdd_t *bdd1,
                           pddl_bdd_t *bdd2,
                           unsigned int size_limit,
                           pddl_time_limit_t *time_limit)
{
    if (time_limit != NULL && time_limit->limit < 1E10){
        unsigned long lim = pddlTimeLimitRemain(time_limit) * 1000ul;
        Cudd_SetTimeLimit(M(mgr), lim);
        Cudd_ResetStartTime(M(mgr));
    }

    DdNode *b;
    if (size_limit > 0){
        b = Cudd_bddOrLimit(M(mgr), B(bdd1), B(bdd2), size_limit);
    }else{
        b = Cudd_bddOr(M(mgr), B(bdd1), B(bdd2));
    }
    if (b != NULL)
        Cudd_Ref(b);

    Cudd_UnsetTimeLimit(M(mgr));
    return PB(b);
}

int pddlBDDOrUpdate(pddl_bdd_manager_t *mgr,
                    pddl_bdd_t **bdd1,
                    pddl_bdd_t *bdd2)
{
    pddl_bdd_t *b = pddlBDDOr(mgr, *bdd1, bdd2);
    if (b == NULL)
        return -1;
    pddlBDDDel(mgr, *bdd1);
    *bdd1 = b;
    return 0;
}

pddl_bdd_t *pddlBDDXnor(pddl_bdd_manager_t *mgr,
                        pddl_bdd_t *bdd1,
                        pddl_bdd_t *bdd2)
{
    DdNode *b = Cudd_bddXnor(M(mgr), B(bdd1), B(bdd2));
    if (b != NULL)
        Cudd_Ref(b);
    return PB(b);
}

int pddlBDDSize(pddl_bdd_t *bdd)
{
    return Cudd_DagSize(B(bdd));
}

double pddlBDDCountMinterm(pddl_bdd_manager_t *mgr, pddl_bdd_t *bdd, int n)
{
    return Cudd_CountMinterm(M(mgr), B(bdd), n);
}

pddl_bdd_t *pddlBDDCube(pddl_bdd_manager_t *mgr, pddl_bdd_t **bdd, int n)
{
    DdNode *b = Cudd_bddComputeCube(M(mgr), (DdNode **)bdd, NULL, n);
    Cudd_Ref(b);
    return PB(b);
}

#else /* PDDL_CUDD */

#include "pddl/err.h"
#include "pddl/bdd.h"

#define ERROR PANIC("bdd module requires CUDD library")

pddl_bdd_manager_t *pddlBDDManagerNew(int bdd_var_size,
                                      unsigned int cache_size)
{
    ERROR;
    return NULL;
}

void pddlBDDManagerDel(pddl_bdd_manager_t *mgr)
{
    ERROR;
}

float pddlBDDMem(pddl_bdd_manager_t *mgr)
{
    ERROR;
    return 0.;
}

int pddlBDDGCUsed(pddl_bdd_manager_t *mgr)
{
    ERROR;
    return 0;
}


void pddlBDDDel(pddl_bdd_manager_t *mgr, pddl_bdd_t *bdd)
{
    ERROR;
}



pddl_bdd_t *pddlBDDClone(pddl_bdd_manager_t *mgr, pddl_bdd_t *bdd)
{
    ERROR;
    return NULL;
}

int pddlBDDIsFalse(pddl_bdd_manager_t *mgr, pddl_bdd_t *bdd)
{
    ERROR;
    return 0;
}

pddl_bdd_t *pddlBDDNot(pddl_bdd_manager_t *mgr, pddl_bdd_t *bdd)
{
    ERROR;
    return NULL;
}

pddl_bdd_t *pddlBDDZero(pddl_bdd_manager_t *mgr)
{
    ERROR;
    return NULL;
}

pddl_bdd_t *pddlBDDOne(pddl_bdd_manager_t *mgr)
{
    ERROR;
    return NULL;
}

pddl_bdd_t *pddlBDDVar(pddl_bdd_manager_t *mgr, int i)
{
    ERROR;
    return NULL;
}

pddl_bdd_t *pddlBDDAnd(pddl_bdd_manager_t *mgr,
                       pddl_bdd_t *bdd1,
                       pddl_bdd_t *bdd2)
{
    ERROR;
    return NULL;
}

pddl_bdd_t *pddlBDDAndLimit(pddl_bdd_manager_t *mgr,
                            pddl_bdd_t *bdd1,
                            pddl_bdd_t *bdd2,
                            unsigned int size_limit,
                            pddl_time_limit_t *time_limit)
{
    ERROR;
    return NULL;
}

pddl_bdd_t *pddlBDDAndAbstract(pddl_bdd_manager_t *mgr,
                               pddl_bdd_t *bdd1,
                               pddl_bdd_t *bdd2,
                               pddl_bdd_t *cube)
{
    ERROR;
    return NULL;
}

pddl_bdd_t *pddlBDDAndAbstractLimit(pddl_bdd_manager_t *mgr,
                                    pddl_bdd_t *bdd1,
                                    pddl_bdd_t *bdd2,
                                    pddl_bdd_t *cube,
                                    unsigned int size_limit,
                                    pddl_time_limit_t *time_limit)
{
    ERROR;
    return NULL;
}

pddl_bdd_t *pddlBDDSwapVars(pddl_bdd_manager_t *mgr,
                            pddl_bdd_t *bdd,
                            pddl_bdd_t **v1,
                            pddl_bdd_t **v2,
                            int n)
{
    ERROR;
    return NULL;
}

void pddlBDDPickOneCube(pddl_bdd_manager_t *mgr, pddl_bdd_t *bdd, char *cube)
{
    ERROR;
}

int pddlBDDAndUpdate(pddl_bdd_manager_t *mgr,
                     pddl_bdd_t **bdd1,
                     pddl_bdd_t *bdd2)
{
    ERROR;
    return 0;
}

pddl_bdd_t *pddlBDDOr(pddl_bdd_manager_t *mgr,
                      pddl_bdd_t *bdd1,
                      pddl_bdd_t *bdd2)
{
    ERROR;
    return NULL;
}

pddl_bdd_t *pddlBDDOrLimit(pddl_bdd_manager_t *mgr,
                           pddl_bdd_t *bdd1,
                           pddl_bdd_t *bdd2,
                           unsigned int size_limit,
                           pddl_time_limit_t *time_limit)
{
    ERROR;
    return NULL;
}

int pddlBDDOrUpdate(pddl_bdd_manager_t *mgr,
                    pddl_bdd_t **bdd1,
                    pddl_bdd_t *bdd2)
{
    ERROR;
    return 0;
}

pddl_bdd_t *pddlBDDXnor(pddl_bdd_manager_t *mgr,
                        pddl_bdd_t *bdd1,
                        pddl_bdd_t *bdd2)
{
    ERROR;
    return NULL;
}

int pddlBDDSize(pddl_bdd_t *bdd)
{
    ERROR;
    return 0;
}

double pddlBDDCountMinterm(pddl_bdd_manager_t *mgr, pddl_bdd_t *bdd, int n)
{
    ERROR;
    return 0.;
}

pddl_bdd_t *pddlBDDCube(pddl_bdd_manager_t *mgr, pddl_bdd_t **bdd, int n)
{
    ERROR;
    return NULL;
}

#endif /* PDDL_CUDD */
