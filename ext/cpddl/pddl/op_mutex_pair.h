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

#ifndef __PDDL_OP_MUTEX_PAIR_H__
#define __PDDL_OP_MUTEX_PAIR_H__

#include <pddl/strips.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * A set of op-mutexes of size at most 1.
 */
struct pddl_op_mutex_pairs {
    int op_size;
    int *op_id_to_id; /*!< Mapping from the operator id to the internal id */
    int *id_to_op_id; /*!< Mapping froom the internal to operator id */
    int size; /*!< Number of operators registered here */
    int alloc;
    pddl_iset_t *op_mutex;
    int num_op_mutex_pairs;
};
typedef struct pddl_op_mutex_pairs pddl_op_mutex_pairs_t;

#define PDDL_OP_MUTEX_PAIRS_FOR_EACH(M, O1, O2) \
    for (int ___i = 0; ___i < (M)->size \
            && ((O1) = (M)->id_to_op_id[___i], 1); ++___i) \
        PDDL_ISET_FOR_EACH((M)->op_mutex + ___i, O2)

#define PDDL_OP_MUTEX_PAIRS_FOR_EACH_SORTED(M, O1, O2) \
    for (int ___i = 0, ___id = 0; ___i < (M)->op_size; ++___i) \
        if ((M)->op_id_to_id[___i] >= 0 \
                && ((O1) = ___i, 1) \
                && (___id = (M)->op_id_to_id[___i])) \
            PDDL_ISET_FOR_EACH((M)->op_mutex + ___id, O2)

/**
 * Returns empty set of op-mutex pairs
 */
void pddlOpMutexPairsInit(pddl_op_mutex_pairs_t *m, const pddl_strips_t *s);

/**
 * Initializes dst as a copy of src.
 */
void pddlOpMutexPairsInitCopy(pddl_op_mutex_pairs_t *dst,
                              const pddl_op_mutex_pairs_t *src);

/**
 * Free allocated memory.
 */
void pddlOpMutexPairsFree(pddl_op_mutex_pairs_t *m);

/**
 * Returns number of op mutexes stored in m.
 */
int pddlOpMutexPairsSize(const pddl_op_mutex_pairs_t *m);

/**
 * Returns set of operators that are mutex with the specified operator.
 */
void pddlOpMutexPairsMutexWith(const pddl_op_mutex_pairs_t *m, int op_id,
                               pddl_iset_t *out);

/**
 * Adds another op-mutex
 */
void pddlOpMutexPairsAdd(pddl_op_mutex_pairs_t *m, int o1, int o2);

/**
 * Adds mutex pairs from the given operator mutex group.
 */
void pddlOpMutexPairsAddGroup(pddl_op_mutex_pairs_t *m, const pddl_iset_t *g);

/**
 * Removes an op-mutex
 */
void pddlOpMutexPairsRm(pddl_op_mutex_pairs_t *m, int o1, int o2);

/**
 * Returns true if two operators are mutex.
 */
int pddlOpMutexPairsIsMutex(const pddl_op_mutex_pairs_t *m, int o1, int o2);

/**
 * m = m \setminus n
 */
void pddlOpMutexPairsMinus(pddl_op_mutex_pairs_t *m,
                           const pddl_op_mutex_pairs_t *n);

/**
 * m = m \cup n
 */
void pddlOpMutexPairsUnion(pddl_op_mutex_pairs_t *m,
                           const pddl_op_mutex_pairs_t *n);

/**
 * Generates a map op_id -> pddl_iset of op_ids.
 * If relevant_ops is non-NULL, only those operators are considered.
 * Output argument map must be array of pddl_iset_t with m->op_size
 * elements.
 */
void pddlOpMutexPairsGenMapOpToOpSet(const pddl_op_mutex_pairs_t *m,
                                     const pddl_iset_t *relevant_ops,
                                     pddl_iset_t *map);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_OP_MUTEX_PAIR_H__ */
