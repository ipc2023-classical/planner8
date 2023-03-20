/***
 * Copyright (c)2022 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

#ifndef __PDDL_LIFTED_APP_ACTION_H__
#define __PDDL_LIFTED_APP_ACTION_H__

#include <pddl/pddl_struct.h>
#include <pddl/strips_maker.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

enum pddl_lifted_app_action_backend {
    PDDL_LIFTED_APP_ACTION_SQL = 0,
    PDDL_LIFTED_APP_ACTION_DL,
};
typedef enum pddl_lifted_app_action_backend pddl_lifted_app_action_backend_t;

typedef struct pddl_lifted_app_action pddl_lifted_app_action_t;

/**
 * Create structure for finding applicable lifted actions
 */
pddl_lifted_app_action_t *pddlLiftedAppActionNew(const pddl_t *pddl,
                                                 pddl_lifted_app_action_backend_t backend,
                                                 pddl_err_t *err);

/**
 * Free allocated memory
 */
void pddlLiftedAppActionDel(pddl_lifted_app_action_t *aa);

/**
 * Clear current state
 */
void pddlLiftedAppActionClearState(pddl_lifted_app_action_t *aa);

/**
 * Set the given atom to the current state
 */
int pddlLiftedAppActionSetStateAtom(pddl_lifted_app_action_t *aa,
                                    const pddl_ground_atom_t *atom);

/**
 * (Re-)Set the current state to the provided STRIPS state.
 */
int pddlLiftedAppActionSetStripsState(pddl_lifted_app_action_t *aa,
                                      const pddl_strips_maker_t *maker,
                                      const pddl_iset_t *state);

/**
 * Find applicable actions in the current state.
 * Returns 0 on success.
 */
int pddlLiftedAppActionFindAppActions(pddl_lifted_app_action_t *aa);

/**
 * Number of applicable actions stored.
 */
int pddlLiftedAppActionSize(const pddl_lifted_app_action_t *a);

/**
 * Action ID of idx's applicable action
 */
int pddlLiftedAppActionId(const pddl_lifted_app_action_t *a, int idx);

/**
 * Arguments of the idx's applicable action
 */
const pddl_obj_id_t *pddlLiftedAppActionArgs(const pddl_lifted_app_action_t *a, int idx);

/**
 * Sort applicable actions
 */
void pddlLiftedAppActionSort(pddl_lifted_app_action_t *a, const pddl_t *pddl);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_LIFTED_APP_ACTION_H__ */
