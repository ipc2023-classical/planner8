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

#include "pddl/op_mutex_pair.h"
#include "internal.h"

void pddlOpMutexPairsInit(pddl_op_mutex_pairs_t *m, const pddl_strips_t *s)
{
    ZEROIZE(m);

    m->op_size = s->op.op_size;
    m->op_id_to_id = ALLOC_ARR(int, m->op_size);
    for (int i = 0; i < m->op_size; ++i)
        m->op_id_to_id[i] = -1;
}

void pddlOpMutexPairsInitCopy(pddl_op_mutex_pairs_t *dst,
                              const pddl_op_mutex_pairs_t *src)
{
    ZEROIZE(dst);

    dst->op_size = src->op_size;
    dst->op_id_to_id = ALLOC_ARR(int, dst->op_size);
    memcpy(dst->op_id_to_id, src->op_id_to_id, sizeof(int) * dst->op_size);
    dst->id_to_op_id = ALLOC_ARR(int, src->alloc);
    memcpy(dst->id_to_op_id, src->id_to_op_id, sizeof(int) * src->alloc);
    dst->size = src->size;
    dst->alloc = src->alloc;
    dst->op_mutex = CALLOC_ARR(pddl_iset_t, dst->alloc);
    for (int i = 0; i < src->size; ++i)
        pddlISetUnion(dst->op_mutex + i, src->op_mutex + i);
    dst->num_op_mutex_pairs = src->num_op_mutex_pairs;
}

void pddlOpMutexPairsFree(pddl_op_mutex_pairs_t *m)
{
    if (m->op_id_to_id != NULL)
        FREE(m->op_id_to_id);
    if (m->id_to_op_id != NULL)
        FREE(m->id_to_op_id);
    for (int i = 0; i < m->size; ++i)
        pddlISetFree(m->op_mutex + i);
    if (m->op_mutex != NULL)
        FREE(m->op_mutex);
}

int pddlOpMutexPairsSize(const pddl_op_mutex_pairs_t *m)
{
    return m->num_op_mutex_pairs;
}

void pddlOpMutexPairsMutexWith(const pddl_op_mutex_pairs_t *m, int op_id,
                               pddl_iset_t *out)
{
    if (op_id < 0)
        return;

    if (m->op_id_to_id[op_id] >= 0)
        pddlISetUnion(out, m->op_mutex + m->op_id_to_id[op_id]);
    for (int i = op_id - 1; i >= 0; --i){
        if (pddlOpMutexPairsIsMutex(m, i, op_id))
            pddlISetAdd(out, i);
    }
}

static void registerNewOp(pddl_op_mutex_pairs_t *m, int op_id)
{
    if (m->size == m->alloc){
        if (m->alloc == 0)
            m->alloc = 32;
        m->alloc *= 2;
        m->op_mutex = REALLOC_ARR(m->op_mutex, pddl_iset_t, m->alloc);
        m->id_to_op_id = REALLOC_ARR(m->id_to_op_id, int, m->alloc);
    }

    int id = m->size++;
    m->op_id_to_id[op_id] = id;
    m->id_to_op_id[id] = op_id;
    pddlISetInit(m->op_mutex + id);
}

void pddlOpMutexPairsAdd(pddl_op_mutex_pairs_t *m, int o1, int o2)
{
    if (m->op_id_to_id[o1] == -1)
        registerNewOp(m, o1);
    if (m->op_id_to_id[o2] == -1)
        registerNewOp(m, o2);
    if (o1 <= o2){
        int id1 = m->op_id_to_id[o1];
        int s = m->num_op_mutex_pairs - pddlISetSize(&m->op_mutex[id1]);
        pddlISetAdd(&m->op_mutex[id1], o2);
        m->num_op_mutex_pairs = s + pddlISetSize(&m->op_mutex[id1]);
    }else{
        int id2 = m->op_id_to_id[o2];
        int s = m->num_op_mutex_pairs - pddlISetSize(&m->op_mutex[id2]);
        pddlISetAdd(&m->op_mutex[id2], o1);
        m->num_op_mutex_pairs = s + pddlISetSize(&m->op_mutex[id2]);
    }
}

void pddlOpMutexPairsAddGroup(pddl_op_mutex_pairs_t *m, const pddl_iset_t *g)
{
    if (pddlISetSize(g) <= 1)
        return;

    PDDL_ISET(group);
    int oid, id;

    pddlISetUnion(&group, g);

    PDDL_ISET_FOR_EACH(g, oid){
        if (m->op_id_to_id[oid] == -1)
            registerNewOp(m, oid);
        id = m->op_id_to_id[oid];
        pddlISetRm(&group, oid);

        int s = m->num_op_mutex_pairs - pddlISetSize(&m->op_mutex[id]);
        pddlISetUnion(&m->op_mutex[id], &group);
        m->num_op_mutex_pairs = s + pddlISetSize(&m->op_mutex[id]);
    }

    pddlISetFree(&group);
}

void pddlOpMutexPairsRm(pddl_op_mutex_pairs_t *m, int o1, int o2)
{
    if (m->op_id_to_id[o1] == -1 || m->op_id_to_id[o2] == -1)
        return;
    if (o1 <= o2){
        int id1 = m->op_id_to_id[o1];
        int s = m->num_op_mutex_pairs - pddlISetSize(&m->op_mutex[id1]);
        pddlISetRm(&m->op_mutex[id1], o2);
        m->num_op_mutex_pairs = s + pddlISetSize(&m->op_mutex[id1]);
    }else{
        int id2 = m->op_id_to_id[o2];
        int s = m->num_op_mutex_pairs - pddlISetSize(&m->op_mutex[id2]);
        pddlISetRm(&m->op_mutex[id2], o1);
        m->num_op_mutex_pairs = s + pddlISetSize(&m->op_mutex[id2]);
    }
}

int pddlOpMutexPairsIsMutex(const pddl_op_mutex_pairs_t *m, int o1, int o2)
{
    int id1 = m->op_id_to_id[o1];
    int id2 = m->op_id_to_id[o2];
    if (id1 == -1 || id2 == -1)
        return 0;

    if (o1 <= o2){
        return pddlISetIn(o2, m->op_mutex + id1);
    }else{
        return pddlISetIn(o1, m->op_mutex + id2);
    }
}

void pddlOpMutexPairsMinus(pddl_op_mutex_pairs_t *m,
                           const pddl_op_mutex_pairs_t *n)
{
    int o1, o2;
    PDDL_OP_MUTEX_PAIRS_FOR_EACH(n, o1, o2)
        pddlOpMutexPairsRm(m, o1, o2);
}

void pddlOpMutexPairsUnion(pddl_op_mutex_pairs_t *m,
                           const pddl_op_mutex_pairs_t *n)
{
    int o1, o2;
    PDDL_OP_MUTEX_PAIRS_FOR_EACH(n, o1, o2)
        pddlOpMutexPairsAdd(m, o1, o2);
}

void pddlOpMutexPairsGenMapOpToOpSet(const pddl_op_mutex_pairs_t *m,
                                     const pddl_iset_t *relevant_ops,
                                     pddl_iset_t *map)
{
    int *relevant_ops_arr = CALLOC_ARR(int, m->op_size);
    if (relevant_ops != NULL){
        int op_id;
        PDDL_ISET_FOR_EACH(relevant_ops, op_id)
            relevant_ops_arr[op_id] = 1;
    }else{
        for (int op_id = 0; op_id < m->op_size; ++op_id)
            relevant_ops_arr[op_id] = 1;
    }

    int op_id1, op_id2;
    PDDL_OP_MUTEX_PAIRS_FOR_EACH(m, op_id1, op_id2){
        if (relevant_ops_arr[op_id1] && relevant_ops_arr[op_id2]){
            pddlISetAdd(map + op_id1, op_id2);
            pddlISetAdd(map + op_id2, op_id1);
        }
    }

    if (relevant_ops_arr != NULL)
        FREE(relevant_ops_arr);
}
