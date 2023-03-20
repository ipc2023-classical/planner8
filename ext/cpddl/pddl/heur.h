/***
 * cpddl
 * -------
 * Copyright (c)2019 Daniel Fiser <danfis@danfis.cz>,
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

#ifndef __PDDL_HEUR_H__
#define __PDDL_HEUR_H__

#include <pddl/fdr_state_space.h>
#include <pddl/hpot.h>
#include <pddl/op_mutex_pair.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct pddl_heur pddl_heur_t;

enum pddl_heur_type {
    PDDL_HEUR_BLIND = 1,
    PDDL_HEUR_DEAD_END,
    PDDL_HEUR_POT,
    PDDL_HEUR_FLOW,
    PDDL_HEUR_LM_CUT,
    PDDL_HEUR_HMAX,
    PDDL_HEUR_HADD,
    PDDL_HEUR_HFF,
    PDDL_HEUR_OP_MUTEX,
};
typedef enum pddl_heur_type pddl_heur_type_t;

typedef struct pddl_heur_config pddl_heur_config_t;

struct pddl_heur_op_mutex_config {
    /** Set of operator mutexes */
    const pddl_op_mutex_pairs_t *op_mutex;
    /** Configuration of the underlying heuristic(s) */
    const pddl_heur_config_t *cfg;
};
typedef struct pddl_heur_op_mutex_config pddl_heur_op_mutex_config_t;

#define PDDL_HEUR_CONFIG_OP_MUTEX_INIT { 0 }

struct pddl_heur_config {
    /** Input planning task */
    const pddl_fdr_t *fdr;
    /** Input MG-Strips representation of .fdr */
    const pddl_mg_strips_t *mg_strips;
    /** Set of mutexes */
    const pddl_mutex_pairs_t *mutex;
    /** Type of the heuristic */
    pddl_heur_type_t heur;
    /** Configuration for the PDDL_HEUR_POT heuristic */
    pddl_hpot_config_t pot;
    /** Configuration for the PDDL_HEUR_OP_MUTEX heuristic */
    pddl_heur_op_mutex_config_t op_mutex;
};

#define PDDL_HEUR_CONFIG_INIT \
    { \
        NULL, /* .fdr */ \
        NULL, /* .mg_strips */ \
        NULL, /* .mutex */ \
        PDDL_HEUR_BLIND, /* .heur */ \
        PDDL_HPOT_CONFIG_INIT, /* .pot */ \
        PDDL_HEUR_CONFIG_OP_MUTEX_INIT, /* .op_mutex */ \
    }


/**
 * Create a heuristic based on the configuration
 */
pddl_heur_t *pddlHeur(const pddl_heur_config_t *cfg, pddl_err_t *err);

/**
 * Blind heuristic returning estimate 0 for every state.
 */
pddl_heur_t *pddlHeurBlind(void);

/**
 * Heuristic returning dead-end value for all states.
 */
pddl_heur_t *pddlHeurDeadEnd(void);

/**
 * Potential heuristic.
 */
pddl_heur_t *pddlHeurPot(const pddl_hpot_config_t *cfg, pddl_err_t *err);

/**
 * Flow heuristic
 */
pddl_heur_t *pddlHeurFlow(const pddl_fdr_t *fdr, pddl_err_t *err);

/**
 * LM-Cut heuristic
 */
pddl_heur_t *pddlHeurLMCut(const pddl_fdr_t *fdr, pddl_err_t *err);

/**
 * h^max heuristic
 */
pddl_heur_t *pddlHeurHMax(const pddl_fdr_t *fdr, pddl_err_t *err);

/**
 * h^add heuristic
 */
pddl_heur_t *pddlHeurHAdd(const pddl_fdr_t *fdr, pddl_err_t *err);

/**
 * h^ff heuristic
 */
pddl_heur_t *pddlHeurHFF(const pddl_fdr_t *fdr, pddl_err_t *err);

/**
 * TODO
 */
pddl_heur_t *pddlHeurOpMutex(const pddl_fdr_t *fdr,
                             const pddl_mutex_pairs_t *mutex,
                             const pddl_heur_op_mutex_config_t *cfg,
                             pddl_err_t *err);

/**
 * Destructor
 */
void pddlHeurDel(pddl_heur_t *h);

/**
 * Computes and returns a heuristic estimate.
 */
int pddlHeurEstimate(pddl_heur_t *h,
                     const pddl_fdr_state_space_node_t *node,
                     const pddl_fdr_state_space_t *state_space);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_HEUR_H__ */
