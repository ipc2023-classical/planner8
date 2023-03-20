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

#ifndef __PDDL_STRIPS_GROUND_H__
#define __PDDL_STRIPS_GROUND_H__

#include <pddl/common.h>
#include <pddl/strips.h>
#include <pddl/ground_atom.h>
#include <pddl/prep_action.h>
#include <pddl/strips_ground_tree.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** Forward declaration */
typedef struct pddl_strips_ground_atree pddl_strips_ground_atree_t;
typedef struct pddl_strips_ground_args pddl_strips_ground_args_t;

struct pddl_strips_ground_args_arr {
    pddl_strips_ground_args_t *arg;
    int size;
    int alloc;
};
typedef struct pddl_strips_ground_args_arr pddl_strips_ground_args_arr_t;

typedef void (*pddl_strips_ground_unify_new_atom_fn)
                    (const pddl_ground_atom_t *a, void *);

struct pddl_strips_ground {
    const pddl_t *pddl;
    pddl_ground_config_t cfg;
    pddl_err_t *err;
    pddl_prep_actions_t action;
    pddl_lifted_mgroups_t goal_mgroup;

    pddl_ground_atoms_t static_facts;
    int static_facts_unified;
    pddl_ground_atoms_t facts;
    int unify_start_idx;
    pddl_strips_ground_unify_new_atom_fn unify_new_atom_fn;
    void *unify_new_atom_data;

    int *ground_atom_to_fact_id;
    pddl_ground_atoms_t funcs;
    pddl_strips_ground_atree_t *atree;
    pddl_strips_ground_args_arr_t ground_args;
};
typedef struct pddl_strips_ground pddl_strips_ground_t;

/**
 * Ground PDDL into STRIPS.
 * It runs:
 *  pddlStripsGroundStart()
 *  pddlStripsGroundUnifyStep()
 *  pddlStripsGroundFinalize()
 */
int pddlStripsGround(pddl_strips_t *strips,
                     const pddl_t *pddl,
                     const pddl_ground_config_t *cfg,
                     pddl_err_t *err);


/**
 * Starts grounding.
 */
int pddlStripsGroundStart(pddl_strips_ground_t *g,
                          const pddl_t *pddl,
                          const pddl_ground_config_t *cfg,
                          pddl_err_t *err,
                          pddl_strips_ground_unify_new_atom_fn new_atom,
                          void *new_atom_data);

/**
 * Performs one cycle of fixpoint grounding.
 * For each newly generated fact, *_unify_new_atom_fn callback specified in
 * pddlStripsGroundStart() is called.
 */
int pddlStripsGroundUnifyStep(pddl_strips_ground_t *g);

/**
 * Adds a grounded atom to the set of facts.
 * Returns -1 on error, 0 if the grounded atom was already there and 1 if
 * this was a new atom.
 * This call does *not* trigger unify_new_atom callback.
 */
int pddlStripsGroundAddGroundAtom(pddl_strips_ground_t *g, int pred,
                                  const pddl_obj_id_t *arg, int arg_size);

/**
 * Finalizes grounding and writes the output STRIPS.
 */
int pddlStripsGroundFinalize(pddl_strips_ground_t *g, pddl_strips_t *strips);


#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_STRIPS_GROUND_H__ */
