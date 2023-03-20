/***
 * Copyright (c)2022 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

#ifndef __PDDL_COMPILE_IN_LIFTED_MGROUP_H__
#define __PDDL_COMPILE_IN_LIFTED_MGROUP_H__

#include <pddl/pddl_struct.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct pddl_compile_in_lmg_config {
    /** If true, mutexes are pruned */
    int prune_mutex;
    /** If true, dead-ends are pruned */
    int prune_dead_end;
};
typedef struct pddl_compile_in_lmg_config pddl_compile_in_lmg_config_t;

#define PDDL_COMPILE_IN_LMG_CONFIG_INIT \
    { \
        0, /* .prune_mutex */ \
        1, /* .prune_dead_end */ \
    }


/**
 * Returns -1 on error, 0 if pddl wasn't changed, and 1 if the pddl was
 * enriched with additional conditions pruning mutexes and dead-ends.
 */
int pddlCompileInLiftedMGroups(pddl_t *pddl,
                               const pddl_lifted_mgroups_t *mgroups,
                               const pddl_compile_in_lmg_config_t *cfg,
                               pddl_err_t *err);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_COMPILE_IN_LIFTED_MGROUP_H__ */
