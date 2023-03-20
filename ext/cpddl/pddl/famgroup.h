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

#ifndef __PDDL_FAMGROUP_H__
#define __PDDL_FAMGROUP_H__

#include <pddl/mgroup.h>
#include <pddl/strips.h>
#include <pddl/sym.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct pddl_famgroup_config {
    /** If true (default), only maximal fam-groups are inferred */
    int maximal;
    /** If true, only fam-groups with non-empty intersection with the goal
     * are inferred. */
    int goal;
    /** If set, the symmetries will be used for generation of all symmetric
     * fam-groups (instead of inferring them using LP) */
    const pddl_strips_sym_t *sym;
    /** In the case symmetries are used, only the asymetric fam-groups are
     *  stored in the output set. */
    int keep_only_asymetric;
    /** Prioritize fam-groups containing new facts */
    int prioritize_uncovered;

    /** If set to >0, limit on the number of inferred fam-groups */
    int limit;
    /** If set to >0., the time limit for the inference algorithm */
    float time_limit;
};
typedef struct pddl_famgroup_config pddl_famgroup_config_t;

#define PDDL_FAMGROUP_CONFIG_INIT { \
        1, /* .maximal */ \
        0, /* .goal */ \
        NULL, /* .sym */ \
        0, /* .keep_only_asymetric */ \
        0, /* .prioritize_uncovered */ \
        -1, /* .limit */ \
        -1., /* .time_limit */ \
    }

/**
 * Find fact-alternating mutex groups while skipping those that are already
 * in mgs.
 */
int pddlFAMGroupsInfer(pddl_mgroups_t *mgs,
                       const pddl_strips_t *strips,
                       const pddl_famgroup_config_t *cfg,
                       pddl_err_t *err);

_pddl_inline int pddlFAMGroupsInferMaximal(pddl_mgroups_t *mgs,
                                           const pddl_strips_t *strips,
                                           pddl_err_t *err)
{
    pddl_famgroup_config_t cfg = PDDL_FAMGROUP_CONFIG_INIT;
    cfg.maximal = 1;
    return pddlFAMGroupsInfer(mgs, strips, &cfg, err);
}

_pddl_inline int pddlFAMGroupsInferAll(pddl_mgroups_t *mgs,
                                       const pddl_strips_t *strips,
                                       pddl_err_t *err)
{
    pddl_famgroup_config_t cfg = PDDL_FAMGROUP_CONFIG_INIT;
    cfg.maximal = 0;
    return pddlFAMGroupsInfer(mgs, strips, &cfg, err);
}

/**
 * Find dead-end operators using the fam-groups stored in mgs.
 */
void pddlFAMGroupsDeadEndOps(const pddl_mgroups_t *mgs,
                             const pddl_strips_t *strips,
                             pddl_iset_t *dead_end_ops);


#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_FAMGROUP_H__ */
