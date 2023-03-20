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

#ifndef __PDDL_FDR_STATE_POOL_H__
#define __PDDL_FDR_STATE_POOL_H__

#include <pddl/extarr.h>
#include <pddl/fdr_var.h>
#include <pddl/fdr_state_packer.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct pddl_fdr_state_pool {
    pddl_extarr_t *pool; /*!< Data pool */
    void *htable; /*!< Hash table of the states */
    pddl_state_id_t num_states; /*!< Number of states stored so far */
    pddl_fdr_state_packer_t packer;
    pddl_err_t *err; /*!< Logger */
};
typedef struct pddl_fdr_state_pool pddl_fdr_state_pool_t;

void pddlFDRStatePoolInit(pddl_fdr_state_pool_t *state_pool,
                          const pddl_fdr_vars_t *vars,
                          pddl_err_t *err);
void pddlFDRStatePoolFree(pddl_fdr_state_pool_t *state_pool);

/**
 * Adds the given state to the pool and returns the assigned ID (or ID of
 * the same state already in the pool).
 */
pddl_state_id_t pddlFDRStatePoolInsert(pddl_fdr_state_pool_t *state_pool,
                                       const int *state);

/**
 * Fills state with the unpacked state corresponding to the given state_id.
 */
void pddlFDRStatePoolGet(const pddl_fdr_state_pool_t *state_pool,
                         pddl_state_id_t state_id,
                         int *state);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_FDR_STATE_POOL_H__ */
