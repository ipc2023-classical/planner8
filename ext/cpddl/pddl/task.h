/***
 * Copyright (c)2022 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

#ifndef __PDDL_TASK_H__
#define __PDDL_TASK_H__

#include <pddl/pddl_struct.h>
#include <pddl/fdr.h>
#include <pddl/mg_strips.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct pddl_task pddl_task_t;

/**
 * TODO
 */
pddl_task_t *pddlTaskNew(pddl_err_t *err);

/**
 * TODO
 */
pddl_task_t *pddlTaskNewFDR(const pddl_fdr_t *fdr, pddl_err_t *err);

/**
 * Makes a deep copy of the input task struct.
 */
pddl_task_t *pddlTaskClone(const pddl_task_t *task, pddl_err_t *err);

/**
 * Free allocated memory.
 */
void pddlTaskDel(pddl_task_t *task);

/**
 * TODO
 */
pddl_err_t *pddlTaskErr(pddl_task_t *task);

/**
 * TODO
 */
void pddlTaskPrintErr(pddl_task_t *task, FILE *fout);


/**
 * TODO
 */
pddl_status_t pddlTaskSetConfigPddl(pddl_task_t *task,
                                    const char *domain_pddl,
                                    const char *problem_pddl,
                                    const pddl_config_t *cfg);

/**
 * TODO
 * @param[in,out] task TODO
 * @param[out] pddl_out TODO
 * @return TODO
 */
pddl_status_t pddlTaskPddl(pddl_task_t *task, const pddl_t **pddl_out);


/**
 * TODO
 */
//int pddlTaskSetConfigLiftedMGroups(pddl_task_t *task, ...);

/**
 * TODO
 * @param[in,out] task TODO
 * @param[out] out TODO
 * @return TODO
 */
pddl_status_t pddlTaskLiftedMGroups(pddl_task_t *task,
                                    const pddl_lifted_mgroups_t **out);


// TODO: Lifted endomorphism
// TODO: Compile-in lifted mgroups

// TODO: Strips, grounding
// TODO: Mutex groups
// TODO: Pruning of strips --> bin/process_strips

/**
 * TODO
 */
const pddl_fdr_t *pddlTaskFDR(pddl_task_t *task);

/**
 * TODO
 */
const pddl_mg_strips_t *pddlTaskMGStrips(pddl_task_t *task);

/**
 * TODO
 * TODO: Replace time_limit, excess_memory with config and refactor with
 *       critical_path.{h,c}
 */
const pddl_mutex_pairs_t *pddlTaskHmMutex(pddl_task_t *task,
                                          int m,
                                          float time_limit,
                                          size_t excess_memory);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __PDDL_TASK_H__ */
