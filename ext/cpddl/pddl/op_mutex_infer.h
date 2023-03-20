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

#ifndef __PDDL_OP_MUTEX_INFER_H__
#define __PDDL_OP_MUTEX_INFER_H__

#include <pddl/op_mutex_pair.h>
#include <pddl/mgroup.h>
#include <pddl/trans_system.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * Compute op-mutexes on projections to each fam-group separately (no merges).
 */
int pddlOpMutexInferFAMGroups(pddl_op_mutex_pairs_t *m,
                              const pddl_strips_t *strips,
                              const pddl_mgroups_t *mgroup,
                              pddl_err_t *err);

/**
 * Compute op-mutexes from projections on a single fact that is not covered
 * by any mgroup.
 */
int pddlOpMutexInferUncoveredFacts(pddl_op_mutex_pairs_t *m,
                                   const pddl_strips_t *strips,
                                   const pddl_mgroups_t *mgroup,
                                   pddl_err_t *err);

/**
 * Create a modified problem where each operator o_i has added a brand new
 * fact f_i into its add_eff. Then run h^2 and mutexes {f_i,f_j} correspond
 * to op-mutexes {o_i,o_j}.
 */
int pddlOpMutexInferHmOpFactCompilation(pddl_op_mutex_pairs_t *opm,
                                        int m,
                                        const pddl_strips_t *strips,
                                        pddl_err_t *err);

/**
 * Run h^2 for each operator from everything that is not mutex with the
 * effect of the operator.
 */
int pddlOpMutexInferHmFromEachOp(pddl_op_mutex_pairs_t *opm,
                                 int m,
                                 const pddl_strips_t *strips_in,
                                 const pddl_mutex_pairs_t *mutex,
                                 const pddl_iset_t *ops,
                                 pddl_err_t *err);

/**
 * Infer op-mutexes from all abstractions that are constructed as a
 * synchronized product of {merge_size} atomic abstractions.
 * If {max_mem_in_mb} is non-zero, the inference is done in a separate
 * process where the overall limit on the memory heap is set to the
 * specified limit.
 * If prune_dead_labels is set to true dead labels are detected during the
 * creationg of the abstractions and they are used to generate op-mutexes.
 */
int pddlOpMutexInferTransSystems(pddl_op_mutex_pairs_t *m,
                                 const pddl_mg_strips_t *mg_strips,
                                 const pddl_mutex_pairs_t *mutex,
                                 int merge_size,
                                 size_t max_mem_in_mb,
                                 int prune_dead_labels,
                                 pddl_err_t *err);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_OP_MUTEX_INFER_H__ */
