/***
 * cpddl
 * -------
 * Copyright (c)2016 Daniel Fiser <danfis@danfis.cz>,
 * AI Center, Department of Computer Science,
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

#ifndef __PDDL_ACTION_H__
#define __PDDL_ACTION_H__

#include <pddl/lisp.h>
#include <pddl/obj.h>
#include <pddl/param.h>
#include <pddl/fm.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * Lifted action
 */
struct pddl_action {
    char *name;
    pddl_params_t param;
    pddl_fm_t *pre;
    pddl_fm_t *eff;
    int id;
};
typedef struct pddl_action pddl_action_t;

struct pddl_actions {
    pddl_action_t *action;
    int action_size;
    int action_alloc;
};
typedef struct pddl_actions pddl_actions_t;

/**
 * Initializes empty action
 */
void pddlActionInit(pddl_action_t *a);

/**
 * Frees allocated memory
 */
void pddlActionFree(pddl_action_t *a);

/**
 * Creates an exact copy of the given action.
 */
void pddlActionInitCopy(pddl_action_t *dst, const pddl_action_t *src);

/**
 * Normalize .pre and .eff (see pddlFmNormalize()).
 */
void pddlActionNormalize(pddl_action_t *a, const pddl_t *pddl);

/**
 * Parses actions from domain PDDL.
 */
int pddlActionsParse(pddl_t *pddl, pddl_err_t *err);

/**
 * Initializes dst as a deep copy of src.
 */
void pddlActionsInitCopy(pddl_actions_t *dst, const pddl_actions_t *src);

/**
 * Free allocated memory.
 */
void pddlActionsFree(pddl_actions_t *actions);

/**
 * Adds an empty action to the list.
 * This may invalidate your current pointers to as's actions.
 */
pddl_action_t *pddlActionsAddEmpty(pddl_actions_t *as);

/**
 * Adds a new copy of the action specified by its ID.
 * This may invalidate your current pointers to as's actions.
 */
pddl_action_t *pddlActionsAddCopy(pddl_actions_t *as, int copy_id);

/**
 * Split all actions by disjunctions in .pre assuming all .pre are in DNF.
 */
void pddlActionSplit(pddl_action_t *a, pddl_t *pddl);

/**
 * Check that all actions has only a flat conjuction as its precondition.
 */
void pddlActionAssertPreConjuction(pddl_action_t *a);

/**
 * Remap object IDs.
 */
void pddlActionRemapObjs(pddl_action_t *a, const pddl_obj_id_t *remap);
void pddlActionsRemapObjs(pddl_actions_t *as, const pddl_obj_id_t *remap);
int pddlActionRemapTypesAndPreds(pddl_action_t *a,
                                 const int *type_remap,
                                 const int *pred_remap,
                                 const int *func_remap);
void pddlActionsRemapTypesAndPreds(pddl_actions_t *as,
                                   const int *type_remap,
                                   const int *pred_remap,
                                   const int *func_remap);

/**
 * Remove the set of actions specified by their IDs.
 */
void pddlActionsRemoveSet(pddl_actions_t *as, const pddl_iset_t *ids);

void pddlActionPrint(const pddl_t *pddl, const pddl_action_t *a, FILE *fout);
void pddlActionsPrint(const pddl_t *pddl,
                      const pddl_actions_t *actions,
                      FILE *fout);

/**
 * Print actions in PDDL format.
 */
void pddlActionsPrintPDDL(const pddl_actions_t *actions,
                          const pddl_t *pddl,
                          FILE *fout);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_ACTION_H__ */
