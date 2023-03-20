/***
 * cpddl
 * -------
 * Copyright (c)2021 Daniel Fiser <danfis@danfis.cz>,
 * Saarland University, and
 * Czech Technical University in Prague.
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

#ifndef __PDDL_STRIPS_MAKER_H__
#define __PDDL_STRIPS_MAKER_H__

#include <pddl/extarr.h>
#include <pddl/common.h>
#include <pddl/pddl_struct.h>
#include <pddl/strips.h>
#include <pddl/ground_atom.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct pddl_ground_action_args {
    int id;
    pddl_list_t htable; /*!< Connection to hash table .action_args */
    pddl_htable_key_t hash; /*!< Hash key */
    int action_id; /*!< ID of the action */
    int action_id2; /*!< Additional ID used for distinguishing conditional
                         effects */
    pddl_obj_id_t arg[]; /*!< Arguments of the action */
};
typedef struct pddl_ground_action_args pddl_ground_action_args_t;

struct pddl_strips_maker {
    int action_size; /*!< Number of actions in PDDL */
    int *action_arg_size; /*!< Mapping from action to number of arguments */
    pddl_ground_atoms_t ground_atom;
    pddl_ground_atoms_t ground_atom_static;
    pddl_ground_atoms_t ground_func;
    pddl_htable_t *action_args;
    int num_action_args;
    pddl_extarr_t *action_args_arr;
    int eq_pred;
};
typedef struct pddl_strips_maker pddl_strips_maker_t;

void pddlStripsMakerInit(pddl_strips_maker_t *sm, const pddl_t *pddl);
void pddlStripsMakerFree(pddl_strips_maker_t *sm);

/**
 * TODO
 */
pddl_ground_atom_t *pddlStripsMakerAddAtom(pddl_strips_maker_t *sm,
                                           const pddl_fm_atom_t *atom,
                                           const pddl_obj_id_t *args,
                                           int *is_new);
pddl_ground_atom_t *pddlStripsMakerAddAtomPred(pddl_strips_maker_t *sm,
                                               int pred,
                                               const pddl_obj_id_t *args,
                                               int args_size,
                                               int *is_new);

/**
 * Same pddlStripsMakerAddAtom() but adds static atom
 */
pddl_ground_atom_t *pddlStripsMakerAddStaticAtom(pddl_strips_maker_t *sm,
                                                 const pddl_fm_atom_t *atom,
                                                 const pddl_obj_id_t *args,
                                                 int *is_new);
pddl_ground_atom_t *pddlStripsMakerAddStaticAtomPred(pddl_strips_maker_t *sm,
                                                     int pred,
                                                     const pddl_obj_id_t *args,
                                                     int args_size,
                                                     int *is_new);

/**
 * Same pddlStripsMakerAddAtom() but adds fuction
 */
pddl_ground_atom_t *pddlStripsMakerAddFunc(pddl_strips_maker_t *sm,
                                           const pddl_fm_func_op_t *func,
                                           const pddl_obj_id_t *args,
                                           int *is_new);

/**
 * TODO
 */
pddl_ground_action_args_t *pddlStripsMakerAddAction(pddl_strips_maker_t *sm,
                                                    int action_id,
                                                    int action_id2,
                                                    const pddl_obj_id_t *args,
                                                    int *is_new);

/**
 * TODO
 */
int pddlStripsMakerAddInit(pddl_strips_maker_t *sm, const pddl_t *pddl);

/**
 * TODO
 */
int pddlStripsMakerMakeStrips(pddl_strips_maker_t *sm,
                              const pddl_t *pddl,
                              const pddl_ground_config_t *cfg,
                              pddl_strips_t *strips,
                              pddl_err_t *err);

pddl_ground_action_args_t *pddlStripsMakerActionArgs(pddl_strips_maker_t *sm,
                                                     int id);
pddl_ground_atom_t *pddlStripsMakerGroundAtom(pddl_strips_maker_t *sm, int id);
const pddl_ground_atom_t *pddlStripsMakerGroundAtomConst(
                const pddl_strips_maker_t *sm, int id);


/**
 * Return effects of the action {a} grounded with arguments {args} in a
 * form of STRIPS add and delete effects {add_eff} and {del_eff} obtained
 * using {smaker}.
 * It is assumed that {a} is normalized and applicable in state {state}.
 * The cost of the action {cost} is set according to (increase ...)
 * formulae.
 * Note that conditional effects are merged into {add_eff} and {del_eff}
 * based on their applicability in {state}.
 */
void pddlStripsMakerActionEffInState(pddl_strips_maker_t *smaker,
                                     const pddl_action_t *a,
                                     const pddl_obj_id_t *args,
                                     const pddl_iset_t *state,
                                     pddl_iset_t *add_eff,
                                     pddl_iset_t *del_eff,
                                     int *cost);
#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_STRIPS_MAKER_H__ */
