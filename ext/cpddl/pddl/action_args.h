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

#ifndef __PDDL_ACTION_ARGS_H__
#define __PDDL_ACTION_ARGS_H__

#include <pddl/common.h>
#include <pddl/extarr.h>
#include <pddl/htable.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct pddl_action_args {
    int num_args;
    pddl_extarr_t *arg_pool;
    pddl_htable_t *htable;
    int args_size;
};
typedef struct pddl_action_args pddl_action_args_t;

/**
 * Initialize pool of action arguments
 */
void pddlActionArgsInit(pddl_action_args_t *args, int num_args);

/**
 * Free allocated memory.
 */
void pddlActionArgsFree(pddl_action_args_t *args);

/**
 * Adds arguments to the pool and ID is returned, {a} is expected to be
 * .num_args long.
 */
int pddlActionArgsAdd(pddl_action_args_t *args, const pddl_obj_id_t *a);

/**
 * Returns arguments corresponding to the id.
 */
const pddl_obj_id_t *pddlActionArgsGet(const pddl_action_args_t *args, int id);

/**
 * Returns number of stored unique arguments.
 */
int pddlActionArgsSize(const pddl_action_args_t *args);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_ACTION_ARGS_H__ */
