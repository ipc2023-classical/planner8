/***
 * Copyright (c)2022 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

#ifndef __PDDL__LIFTED_APP_ACTION_H__
#define __PDDL__LIFTED_APP_ACTION_H__

#include "pddl/lifted_app_action.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct _pddl_lifted_app_action {
    int action_id;
    const pddl_obj_id_t args[];
};
typedef struct _pddl_lifted_app_action _pddl_lifted_app_action_t;

struct pddl_lifted_app_action {
    void (*fn_del)(pddl_lifted_app_action_t *);
    void (*fn_clear_state)(pddl_lifted_app_action_t *);
    int (*fn_set_state_atom)(pddl_lifted_app_action_t *, const pddl_ground_atom_t *);
    int (*fn_find_app_actions)(pddl_lifted_app_action_t *);

    char *data;
    size_t struct_size;
    size_t size;
    size_t bytesize;
    size_t bytealloc;
};

void pddlLiftedAppActionAdd(pddl_lifted_app_action_t *a,
                            int action_id,
                            const pddl_obj_id_t *args,
                            int args_size);

void _pddlLiftedAppActionInit(pddl_lifted_app_action_t *aa,
                              const pddl_t *pddl,
                              void (*fn_del)(pddl_lifted_app_action_t *),
                              void (*fn_clear_state)(pddl_lifted_app_action_t *),
                              int (*fn_set_state_atom)(pddl_lifted_app_action_t *,
                                                       const pddl_ground_atom_t *),
                              int (*fn_find_app_actions)(pddl_lifted_app_action_t *),
                              pddl_err_t *err);

pddl_lifted_app_action_t *pddlLiftedAppActionNewSql(const pddl_t *pddl,
                                                    pddl_err_t *err);
pddl_lifted_app_action_t *pddlLiftedAppActionNewDatalog(const pddl_t *pddl,
                                                        pddl_err_t *err);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL__LIFTED_APP_ACTION_H__ */
