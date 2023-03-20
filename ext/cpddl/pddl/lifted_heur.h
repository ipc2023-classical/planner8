/***
 * Copyright (c)2022 Daniel Fiser <danfis@danfis.cz>,
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

#ifndef __PDDL_LIFTED_HEUR_H__
#define __PDDL_LIFTED_HEUR_H__

#include <pddl/pddl_struct.h>
#include "pddl/homomorphism_heur.h"
#include "pddl/cost.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct pddl_lifted_heur pddl_lifted_heur_t;

/**
 * Blind heuristic returning estimate 0 for every state.
 */
pddl_lifted_heur_t *pddlLiftedHeurBlind(void);

/**
 * h^max heuristic.
 */
pddl_lifted_heur_t *pddlLiftedHeurHMax(const pddl_t *pddl, pddl_err_t *err);

/**
 * h^add heuristic.
 */
pddl_lifted_heur_t *pddlLiftedHeurHAdd(const pddl_t *pddl, pddl_err_t *err);

/**
 * Wrapper for homomorphism-based heuristic.
 */
pddl_lifted_heur_t *pddlLiftedHeurHomomorphism(pddl_homomorphism_heur_t *h);

/**
 * Destructor
 */
void pddlLiftedHeurDel(pddl_lifted_heur_t *h);

/**
 * Computes and returns a heuristic estimate.
 */
pddl_cost_t pddlLiftedHeurEstimate(pddl_lifted_heur_t *h,
                                   const pddl_iset_t *state,
                                   const pddl_ground_atoms_t *gatoms);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_LIFTED_HEUR_H__ */
