/***
 * cpddl
 * -------
 * Copyright (c)2018 Daniel Fiser <danfis@danfis.cz>,
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

#include "pddl/mutex_pair.h"
#include "pddl/strips.h"
#include "pddl/clique.h"
#include "internal.h"

#define FW_MUTEX 0x2
#define BW_MUTEX 0x4

#define M(m, f1, f2) ((m)->map[(f1) * (size_t)(m)->fact_size + (f2)])

void pddlMutexPairsInit(pddl_mutex_pairs_t *m, int fact_size)
{
    ZEROIZE(m);
    m->fact_size = fact_size;
    m->map = CALLOC_ARR(char, (size_t)fact_size * fact_size);
}

void pddlMutexPairsInitStrips(pddl_mutex_pairs_t *m, const pddl_strips_t *s)
{
    pddlMutexPairsInit(m, s->fact.fact_size);
}

void pddlMutexPairsInitCopy(pddl_mutex_pairs_t *dst,
                            const pddl_mutex_pairs_t *src)
{
    pddlMutexPairsInit(dst, src->fact_size);
    memcpy(dst->map, src->map,
           sizeof(char) * (size_t)src->fact_size * src->fact_size);
    dst->num_mutex_pairs = src->num_mutex_pairs;
}

void pddlMutexPairsFree(pddl_mutex_pairs_t *m)
{
    if (m->map != NULL)
        FREE(m->map);
}

void pddlMutexPairsEmpty(pddl_mutex_pairs_t *m, int fact_size)
{
    if (fact_size > 0){
        pddlMutexPairsFree(m);
        pddlMutexPairsInit(m, fact_size);
    }else{
        ZEROIZE_ARR(m->map, (size_t)m->fact_size * m->fact_size);
    }
}

static int setMutexFlag(pddl_mutex_pairs_t *m, int f1, int f2, char flag)
{
    if (f1 >= m->fact_size || f2 >= m->fact_size)
        return -1;
    if (f1 == f2){
        for (int i = 0; i < m->fact_size; ++i){
            if (M(m, f1, i)){
                M(m, f1, i) |= flag;
                M(m, i, f1) |= flag;
            }
        }
    }else{
        if (M(m, f1, f2)){
            M(m, f1, f2) |= flag;
            M(m, f2, f1) |= flag;
        }
    }
    return 0;
}

int pddlMutexPairsAdd(pddl_mutex_pairs_t *m, int f1, int f2)
{
    if (f1 >= m->fact_size || f2 >= m->fact_size)
        return -1;
    if (f1 == f2){
        for (int i = 0; i < m->fact_size; ++i){
            if (!M(m, f1, i) && f1 != i)
                ++m->num_mutex_pairs;
            M(m, f1, i) = M(m, i, f1) = 1;
        }
    }else{
        if (!M(m, f1, f2) && f1 != f2)
            ++m->num_mutex_pairs;
        M(m, f1, f2) = M(m, f2, f1) = 1;
    }
    return 0;
}

int pddlMutexPairsSetFwMutex(pddl_mutex_pairs_t *m, int f1, int f2)
{
    return setMutexFlag(m, f1, f2, FW_MUTEX);
}

int pddlMutexPairsSetBwMutex(pddl_mutex_pairs_t *m, int f1, int f2)
{
    return setMutexFlag(m, f1, f2, BW_MUTEX);
}

int pddlMutexPairsIsMutex(const pddl_mutex_pairs_t *m, int f1, int f2)
{
    return M(m, f1, f2);
}

int pddlMutexPairsIsFwMutex(const pddl_mutex_pairs_t *m, int f1, int f2)
{
    return M(m, f1, f2) & FW_MUTEX;
}

int pddlMutexPairsIsBwMutex(const pddl_mutex_pairs_t *m, int f1, int f2)
{
    return M(m, f1, f2) & BW_MUTEX;
}


int pddlMutexPairsIsMutexSet(const pddl_mutex_pairs_t *m, const pddl_iset_t *fs)
{
    const int size = pddlISetSize(fs);
    for (int i = 0; i < size; ++i){
        int f1 = pddlISetGet(fs, i);
        for (int j = i; j < size; ++j){
            int f2 = pddlISetGet(fs, j);
            if (M(m, f1, f2))
                return 1;
        }
    }
    return 0;
}

int pddlMutexPairsIsMutexFactSet(const pddl_mutex_pairs_t *m,
                                 int fact, const pddl_iset_t *fs)
{
    int fact2;

    PDDL_ISET_FOR_EACH(fs, fact2){
        if (M(m, fact, fact2))
            return 1;
    }
    return 0;
}

int pddlMutexPairsIsMutexSetSet(const pddl_mutex_pairs_t *m,
                                const pddl_iset_t *fs1, const pddl_iset_t *fs2)
{
    int f1, f2;
    PDDL_ISET_FOR_EACH(fs1, f1){
        PDDL_ISET_FOR_EACH(fs2, f2){
            if (M(m, f1, f2))
                return 1;
        }
    }
    return 0;
}

void pddlMutexPairsGetMutexWith(const pddl_mutex_pairs_t *m,
                                int fact,
                                pddl_iset_t *mutex_with)
{
    for (int f = 0; f < m->fact_size; ++f){
        if (M(m, fact, f))
            pddlISetAdd(mutex_with, f);
    }
}

void pddlMutexPairsRemapFacts(pddl_mutex_pairs_t *m,
                              int new_fact_size,
                              const int *remap)
{
    pddl_mutex_pairs_t old = *m;

    pddlMutexPairsInit(m, new_fact_size);
    for (int i = 0; i < old.fact_size; ++i){
        if (remap[i] < 0)
            continue;
        for (int j = i + 1; j < old.fact_size; ++j){
            if (remap[j] < 0)
                continue;
            if (pddlMutexPairsIsMutex(&old, i, j)){
                pddlMutexPairsAdd(m, remap[i], remap[j]);
                if (pddlMutexPairsIsFwMutex(&old, i, j)){
                    pddlMutexPairsSetFwMutex(m, remap[i], remap[j]);
                }else if (pddlMutexPairsIsBwMutex(&old, i, j)){
                    pddlMutexPairsSetBwMutex(m, remap[i], remap[j]);
                }
            }
        }
    }

    pddlMutexPairsFree(&old);
}

void pddlMutexPairsReduce(pddl_mutex_pairs_t *m, const pddl_iset_t *rm_facts)
{
    if (pddlISetSize(rm_facts) == 0)
        return;

    int *remap = CALLOC_ARR(int, m->fact_size);
    int new_size = pddlFactsDelFactsGenRemap(m->fact_size, rm_facts, remap);
    pddlMutexPairsRemapFacts(m, new_size, remap);
    if (remap != NULL)
        FREE(remap);
}

void pddlMutexPairsAddMGroup(pddl_mutex_pairs_t *mutex,
                             const pddl_mgroup_t *mg)
{
    const pddl_iset_t *facts = &mg->mgroup;
    int size = pddlISetSize(facts);

    for (int i = 0; i < size; ++i){
        int f1 = pddlISetGet(facts, i);
        for (int j = i + 1; j < size; ++j){
            int f2 = pddlISetGet(facts, j);
            pddlMutexPairsAdd(mutex, f1, f2);
            setMutexFlag(mutex, f1, f2, FW_MUTEX);
        }
    }
}

void pddlMutexPairsAddMGroups(pddl_mutex_pairs_t *mutex,
                              const pddl_mgroups_t *mgs)
{
    for (int mgi = 0; mgi < mgs->mgroup_size; ++mgi){
        const pddl_mgroup_t *mg = mgs->mgroup + mgi;
        pddlMutexPairsAddMGroup(mutex, mg);
    }
}

static void addMGroup(const pddl_iset_t *mg, void *_mgroups)
{
    pddl_mgroups_t *mgroups = _mgroups;
    pddlMGroupsAdd(mgroups, mg);
}

void pddlMutexPairsInferMutexGroups(const pddl_mutex_pairs_t *mutex,
                                    pddl_mgroups_t *mgroups,
                                    pddl_err_t *err)
{
    CTX(err, "mg_h2", "MG-h2");
    PDDL_INFO(err, "Inference of h^2 mutex groups...");
    pddl_graph_simple_t graph;
    pddlGraphSimpleInit(&graph, mutex->fact_size);

    PDDL_MUTEX_PAIRS_FOR_EACH(mutex, f1, f2){
        if (f1 != f2)
            pddlGraphSimpleAddEdge(&graph, f1, f2);
    }
    pddlCliqueFindMaximal(&graph, addMGroup, mgroups);

    pddlGraphSimpleFree(&graph);
    PDDL_INFO(err, "Found %d h^2 mutex groups.", mgroups->mgroup_size);
    CTXEND(err);
}

