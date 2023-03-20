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

#ifndef __PDDL_STRIPS_GROUND_TREE_H__
#define __PDDL_STRIPS_GROUND_TREE_H__

#include <pddl/common.h>
#include <pddl/iset.h>
#include <pddl/ground_atom.h>
#include <pddl/prep_action.h>
#include <pddl/action_args.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct pddl_strips_ground_tnode {
    pddl_action_param_size_t param; /*!< Parameter this node corresponds to */
    pddl_obj_id_t obj_id; /*!< Object ID assigned to this param */
    int pre_unified:29; /*!< Number of unified preconditions */
    unsigned char flag_blocked:1; /*!< True if no new children are allowed */
    unsigned char flag_pre_unified:1; /*!< True if the node unified
                                           a new precondition */
    unsigned char flag_static_arg:1; /*!< True if the node corresponds to an
                                          argument of a static fact */
    pddl_obj_size_t child_size;
    pddl_obj_size_t child_alloc;
    struct pddl_strips_ground_tnode **child;
} pddl_packed;
typedef struct pddl_strips_ground_tnode pddl_strips_ground_tnode_t;

struct pddl_strips_ground_tree {
    const pddl_t *pddl;
    const pddl_prep_action_t *action;
    pddl_iset_t param; /*!< Set of parameters that are considered */
    int pre_size; /*!< Number of considered preconditions */
    int pre_static_size; /*!< Number of considered static preconditions */
    pddl_iset_t *pred_to_pre; /*!< Mapping from predicate ID to
                                  corresponding preconditions */
    pddl_strips_ground_tnode_t *root; /*!< Root of the tree */
    pddl_action_args_t args; /*!< Pool of grounded action arguments */
};
typedef struct pddl_strips_ground_tree pddl_strips_ground_tree_t;

/**
 * TODO
 */
void pddlStripsGroundTreeInit(pddl_strips_ground_tree_t *tr,
                              const pddl_t *pddl,
                              const pddl_prep_action_t *a,
                              const pddl_iset_t *params);

/**
 * Free allocated memory
 */
void pddlStripsGroundTreeFree(pddl_strips_ground_tree_t *tr);

/**
 * TODO
 */
void pddlStripsGroundTreeUnifyFact(pddl_strips_ground_tree_t *tr,
                                   const pddl_ground_atom_t *fact,
                                   int static_fact);

/**
 * TODO
 */
void pddlStripsGroundTreeBlockStatic(pddl_strips_ground_tree_t *tr);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_STRIPS_GROUND_TREE_H__ */
