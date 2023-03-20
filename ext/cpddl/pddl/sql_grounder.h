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

#ifndef __PDDL_SQL_GROUNDER_H__
#define __PDDL_SQL_GROUNDER_H__

#include <pddl/common.h>
#include <pddl/pddl_struct.h>
#include <pddl/ground_atom.h>
#include <pddl/prep_action.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct pddl_sql_grounder pddl_sql_grounder_t;

/**
 * Create a new grounder.
 */
pddl_sql_grounder_t *pddlSqlGrounderNew(const pddl_t *pddl, pddl_err_t *err);

/**
 * Free all allocated memory
 */
void pddlSqlGrounderDel(pddl_sql_grounder_t *g);

/**
 * Returns the number of prep-actions
 */
int pddlSqlGrounderPrepActionSize(const pddl_sql_grounder_t *g);

/**
 * Returns specified prep-action object.
 */
const pddl_prep_action_t *pddlSqlGrounderPrepAction(
                const pddl_sql_grounder_t *g, int action_id);

/**
 * Insert ground atom
 */
int pddlSqlGrounderInsertAtomArgs(pddl_sql_grounder_t *g,
                                  int pred_id,
                                  const pddl_obj_id_t *args,
                                  pddl_err_t *err);
int pddlSqlGrounderInsertGroundAtom(pddl_sql_grounder_t *g,
                                    const pddl_ground_atom_t *ga,
                                    pddl_err_t *err);
int pddlSqlGrounderInsertAtom(pddl_sql_grounder_t *g,
                              const pddl_fm_atom_t *a,
                              pddl_err_t *err);

/**
 * Remove all atoms from non-static predicates.
 */
int pddlSqlGrounderClearNonStatic(pddl_sql_grounder_t *g, pddl_err_t *err);

/**
 * Start grounding the specified action.
 * Returns 0 on success.
 */
int pddlSqlGrounderActionStart(pddl_sql_grounder_t *g,
                               int action_id,
                               pddl_err_t *err);

/**
 * Fetch next grounded action.
 * Returns 1 on success, 0 if there is nothing more to ground, and -1 on
 * error.
 * This function can be called only after *ActionStart().
 */
int pddlSqlGrounderActionNext(pddl_sql_grounder_t *g,
                              pddl_obj_id_t *args,
                              pddl_err_t *err);


#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_SQL_GROUNDER_H__ */
