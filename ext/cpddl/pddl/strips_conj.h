/***
 * Copyright (c)2022 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

#ifndef __PDDL_STRIPS_CONJ_H__
#define __PDDL_STRIPS_CONJ_H__

#include <pddl/strips.h>
#include <pddl/set.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * References:
 *
 * Fickert, M. and Hoffmann, J. and Steinmetz, M. (2016). Combining the
 * Delete Relaxation with Critical-Path Heuristics: A Direct
 * Characterization. JAIR (56), 269-327
 *
 * Haslum, P. (2012). Incremental lower bounds for additive cost planning
 * problems. In ICAPS'12, 74-82.
 */

struct pddl_strips_conj_config {
    /** List of non-singleton conj */
    pddl_set_iset_t conj;
    /** If non-NULL, mutexes are used to prune unreachable operators */
    const pddl_mutex_pairs_t *mutex;
};
typedef struct pddl_strips_conj_config pddl_strips_conj_config_t;

/**
 * Initialize default configuration
 */
void pddlStripsConjConfigInit(pddl_strips_conj_config_t *cfg);

/**
 * Free allocated memory
 */
void pddlStripsConjConfigFree(pddl_strips_conj_config_t *cfg);

/**
 * Adds a conjunction
 */
void pddlStripsConjConfigAddConj(pddl_strips_conj_config_t *cfg,
                                 const pddl_iset_t *conj);


struct pddl_strips_conj {
    /** \Pi^C STRIPS planning task */
    pddl_strips_t strips;
    /** Mapping from each meta-fact \pi_c to the corresponding conjunction c */
    pddl_iset_t *fact_to_conj;
    /** Mapping from a conjunction c to the meta-fact \pi_c */
    pddl_set_iset_t conj_to_fact;
    /** Number of singleton meta-facts. Each singleton meta-fact has the
     *  same ID as the corresponding original fact. */
    int num_singletons;
};
typedef struct pddl_strips_conj pddl_strips_conj_t;

/**
 * Initialize \Pi^C planning task
 */
void pddlStripsConjInit(pddl_strips_conj_t *task,
                        const pddl_strips_t *in_task,
                        const pddl_strips_conj_config_t *cfg,
                        pddl_err_t *err);

/**
 * Free allocated memory.
 */
void pddlStripsConjFree(pddl_strips_conj_t *task);

/**
 * Transform {in_mutex} for the original planning task to mutexes for the
 * {task}.
 */
void pddlStripsConjMutexPairsInitCopy(pddl_mutex_pairs_t *mutex,
                                      const pddl_mutex_pairs_t *in_mutex,
                                      const pddl_strips_conj_t *task);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_STRIPS_CONJ_H__ */
