/***
 * Copyright (c)2022 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

#ifndef __PDDL_FDR_STATE_SAMPLER_H__
#define __PDDL_FDR_STATE_SAMPLER_H__

#include <pddl/fdr.h>
#include <pddl/random_walk.h>
#include <pddl/mutex_pair.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct pddl_fdr_state_sampler {
    pddl_err_t *err;
    const pddl_fdr_t *fdr;
    const pddl_mutex_pairs_t *mutex;
    pddl_random_walk_t random_walk;
    int use_random_walk;
    int random_walk_max_steps;
    pddl_rand_t rnd;
    uint32_t seed;

    void (*next_fn)(struct pddl_fdr_state_sampler *s, int *state);
};
typedef struct pddl_fdr_state_sampler pddl_fdr_state_sampler_t;

void pddlFDRStateSamplerInitRandomWalk(pddl_fdr_state_sampler_t *s,
                                       const pddl_fdr_t *fdr,
                                       int max_steps,
                                       pddl_err_t *err);

void pddlFDRStateSamplerInitSyntactic(pddl_fdr_state_sampler_t *s,
                                      const pddl_fdr_t *fdr,
                                      pddl_err_t *err);

void pddlFDRStateSamplerInitSyntacticMutex(pddl_fdr_state_sampler_t *s,
                                           const pddl_fdr_t *fdr,
                                           const pddl_mutex_pairs_t *mutex,
                                           pddl_err_t *err);

/**
 * Free allocated memory.
 */
void pddlFDRStateSamplerFree(pddl_fdr_state_sampler_t *s);

/**
 * Samples next state.
 */
void pddlFDRStateSamplerNext(pddl_fdr_state_sampler_t *s, int *state);

/**
 * Compute maximum number of steps based on the heuristic value for the
 * initial state.
 */
int pddlFDRStateSamplerComputeMaxStepsFromHeurInit(const pddl_fdr_t *fdr,
                                                   int hinit);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __PDDL_FDR_STATE_SAMPLER_H__ */
