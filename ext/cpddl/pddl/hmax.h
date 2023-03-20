/***
 * cpddl
 * -------
 * Copyright (c)2020 Daniel Fiser <danfis@danfis.cz>,
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

// TODO: Add support for conditional effects

#ifndef __PDDL_HMAX_H__
#define __PDDL_HMAX_H__

#include <pddl/iset.h>
#include <pddl/fdr.h>
#include <pddl/pq.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct pddl_hmax_op {
    pddl_iset_t eff; /*!< Facts in its effect */
    int cost;       /*!< Cost of the operator */
    int pre_size;   /*!< Number of preconditions */
    int unsat;      /*!< Number of unsatisfied preconditions */
};
typedef struct pddl_hmax_op pddl_hmax_op_t;

struct pddl_hmax_fact {
    pddl_iset_t pre_op; /*!< Operators having this fact as its precondition */
    pddl_pq_el_t heap; /*!< Connection to priority heap */
};
typedef struct pddl_hmax_fact pddl_hmax_fact_t;

struct pddl_hmax {
    pddl_hmax_fact_t *fact;
    int fact_size;
    int fact_goal;
    int fact_nopre;

    pddl_hmax_op_t *op;
    int op_size;
    int op_goal;
};
typedef struct pddl_hmax pddl_hmax_t;

/**
 * Initialize h^max
 */
void pddlHMaxInit(pddl_hmax_t *hmax, const pddl_fdr_t *fdr);
void pddlHMaxInitStrips(pddl_hmax_t *h, const pddl_strips_t *strips);

/**
 * Free allocated memory.
 */
void pddlHMaxFree(pddl_hmax_t *hmax);

/**
 * Returns h^max estimate for the given state.
 */
int pddlHMax(pddl_hmax_t *hmax,
             const int *fdr_state,
             const pddl_fdr_vars_t *vars);
int pddlHMaxStrips(pddl_hmax_t *h, const pddl_iset_t *state);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_HMAX_H__ */
