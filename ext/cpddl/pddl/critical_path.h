/***
 * cpddl
 * -------
 * Copyright (c)2019 Daniel Fiser <danfis@danfis.cz>,
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

#ifndef __PDDL_CRITICAL_PATH_H__
#define __PDDL_CRITICAL_PATH_H__

#include <pddl/common.h>
#include <pddl/mgroup.h>
#include <pddl/mutex_pair.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct pddl_strips;

/**
 * Compute h^1 (i.e., a simple relaxed) reachability.
 */
int pddlH1(const pddl_strips_t *strips,
           pddl_iset_t *unreachable_facts,
           pddl_iset_t *unreachable_ops,
           pddl_err_t *err);

/**
 * Compute h^2 reachability.
 * unreachable_facts and unreachable_ops can be set to NULL.
 *
 * @param[in]     strips            Input planning task
 * @param[in,out] mutex             TODO
 * @param[in,out] unreachable_facts TODO
 * @param[in,out] unreachable_ops   TODO
 * @param[in]     time_limit_in_s   TODO
 * @param         err               TODO
 * @return TODO
 */
int pddlH2(const pddl_strips_t *strips,
           pddl_mutex_pairs_t *mutex,
           pddl_iset_t *unreachable_facts,
           pddl_iset_t *unreachable_ops,
           float time_limit_in_s,
           pddl_err_t *err);

/**
 * Returns true if the given state is detected as a dead-end state with h^2
 */
int pddlH2IsDeadEnd(const pddl_strips_t *strips, const pddl_iset_t *state);

/**
 * Compute h^2 reachability in forward/backward.
 * unreachable_facts and unreachable_ops can be set to NULL.
 *
 * See Alcázar, V., and Torralba, Á. 2015. A reminder about the importance
 * of computing and exploiting invariants in planning. In Proc. ICAPS’15, 2–6.
 */
int pddlH2FwBw(const pddl_strips_t *strips,
               const pddl_mgroups_t *mgroup,
               pddl_mutex_pairs_t *m,
               pddl_iset_t *unreachable_facts,
               pddl_iset_t *unreachable_ops,
               float time_limit_in_s,
               pddl_err_t *err);

/**
 * Compute h^3 reachability.
 * unreachable_facts and unreachable_ops can be set to NULL.
 * time_limit is disabled by setting it to <= 0.
 */
int pddlH3(const pddl_strips_t *strips,
           pddl_mutex_pairs_t *m,
           pddl_iset_t *unreachable_facts,
           pddl_iset_t *unreachable_ops,
           float time_limit,
           size_t excess_memory,
           pddl_err_t *err);

/**
 * Computes h^m: This is a wrapper around the functions above.
 */
int pddlHm(int m,
           const pddl_strips_t *strips,
           pddl_mutex_pairs_t *mutex,
           pddl_iset_t *unreachable_facts,
           pddl_iset_t *unreachable_ops,
           float time_limit,
           size_t excess_memory,
           pddl_err_t *err);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_CRITICAL_PATH_H__ */
