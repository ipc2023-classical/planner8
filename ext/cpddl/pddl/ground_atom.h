/***
 * cpddl
 * -------
 * Copyright (c)2017 Daniel Fiser <danfis@danfis.cz>,
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

#ifndef __PDDL_GROUND_ATOM_H__
#define __PDDL_GROUND_ATOM_H__

#include <pddl/common.h>
#include <pddl/lisp.h>
#include <pddl/obj.h>
#include <pddl/pred.h>
#include <pddl/fm.h>
#include <pddl/iset.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define PDDL_GROUND_ATOM_MAX_NAME_SIZE 256

struct pddl_ground_atom {
    int id;
    uint64_t hash;
    pddl_list_t htable;

    int func_val; /*!< Assigned value in the case of function */
    int pred;     /*!< Predicate ID */
    int arg_size; /*!< Number of arguments */
    pddl_obj_id_t *arg; /*!< Object IDs are arguments */

    int layer; /*!< Layer in RPG */
};
typedef struct pddl_ground_atom pddl_ground_atom_t;

/**
 * Frees allocated memory.
 */
void pddlGroundAtomDel(pddl_ground_atom_t *);

/**
 * Clones a ground atom.
 */
pddl_ground_atom_t *pddlGroundAtomClone(const pddl_ground_atom_t *);

struct pddl_ground_atoms {
    pddl_ground_atom_t **atom;
    int atom_size;
    int atom_alloc;
    pddl_htable_t *htable;
};
typedef struct pddl_ground_atoms pddl_ground_atoms_t;


/**
 * Initialize set of facts.
 */
void pddlGroundAtomsInit(pddl_ground_atoms_t *fs);

/**
 * Free allocated resources.
 */
void pddlGroundAtomsFree(pddl_ground_atoms_t *fs);

/**
 * Adds a unique ground atom. Returns the newly added atom or the atom that
 * was added in the past.
 */
pddl_ground_atom_t *pddlGroundAtomsAddAtom(pddl_ground_atoms_t *ga,
                                           const pddl_fm_atom_t *c,
                                           const pddl_obj_id_t *arg);

/**
 * Adds a unique ground predicate atom (fact). Returns the newly added atom
 * or the atom that was added in the past if it was already there.
 */
pddl_ground_atom_t *pddlGroundAtomsAddPred(pddl_ground_atoms_t *ga,
                                           int pred,
                                           const pddl_obj_id_t *arg,
                                           int arg_size);

/**
 * Find the grounded fact.
 */
pddl_ground_atom_t *pddlGroundAtomsFindAtom(const pddl_ground_atoms_t *ga,
                                            const pddl_fm_atom_t *c,
                                            const pddl_obj_id_t *arg);
pddl_ground_atom_t *pddlGroundAtomsFindPred(const pddl_ground_atoms_t *ga,
                                            int pred,
                                            const pddl_obj_id_t *arg,
                                            int arg_size);

/**
 * Add all atoms from the initial state.
 */
void pddlGroundAtomsAddInit(pddl_ground_atoms_t *ga, const pddl_t *pddl);

void pddlGroundAtomsPrint(const pddl_ground_atoms_t *ga,
                          const pddl_t *pddl,
                          FILE *fout);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_GROUND_ATOM_H__ */
