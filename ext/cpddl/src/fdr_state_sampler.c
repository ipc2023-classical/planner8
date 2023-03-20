/***
 * Copyright (c)2022 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

#include "internal.h"
#include "pddl/fdr_state_sampler.h"

static const uint32_t default_seed = 524287;

static void nextRandomWalk(pddl_fdr_state_sampler_t *s, int *state)
{
    pddlRandomWalkSampleState(&s->random_walk,
                              s->fdr->init,
                              s->random_walk_max_steps,
                              state);
}

static void nextSyntactic(pddl_fdr_state_sampler_t *s, int *state)
{
    for (int var = 0; var < s->fdr->var.var_size; ++var){
        int val = pddlRand(&s->rnd, 0, s->fdr->var.var[var].val_size);
        val = PDDL_MIN(val, s->fdr->var.var[var].val_size - 1);
        state[var] = val;
    }
}

static void nextSyntacticMutex(pddl_fdr_state_sampler_t *s, int *state)
{
    ASSERT(s->mutex != NULL);
    PDDL_ISET(sstate);
    unsigned long count = 0UL;
    do {
        pddlISetEmpty(&sstate);
        for (int var = 0; var < s->fdr->var.var_size; ++var){
            int val = pddlRand(&s->rnd, 0, s->fdr->var.var[var].val_size);
            val = PDDL_MIN(val, s->fdr->var.var[var].val_size - 1);
            state[var] = val;
            pddlISetAdd(&sstate, s->fdr->var.var[var].val[val].global_id);
        }
        if (++count % 100000UL == 0UL)
            PDDL_INFO(s->err, "state-sampler: tried %lu random states", count);
    } while (pddlMutexPairsIsMutexSet(s->mutex, &sstate));
    pddlISetFree(&sstate);
}

static void init(pddl_fdr_state_sampler_t *s,
                 const pddl_fdr_t *fdr,
                 const pddl_mutex_pairs_t *mutex,
                 pddl_err_t *err)
{
    ZEROIZE(s);
    s->err = err;
    s->fdr = fdr;
    s->mutex = mutex;
    s->seed = default_seed;
}

void pddlFDRStateSamplerInitRandomWalk(pddl_fdr_state_sampler_t *s,
                                       const pddl_fdr_t *fdr,
                                       int max_steps,
                                       pddl_err_t *err)
{
    init(s, fdr, NULL, err);
    pddlRandomWalkInitSeed(&s->random_walk, fdr, NULL, s->seed);
    s->use_random_walk = 1;
    s->random_walk_max_steps = max_steps;
    s->next_fn = nextRandomWalk;
}

void pddlFDRStateSamplerInitSyntactic(pddl_fdr_state_sampler_t *s,
                                      const pddl_fdr_t *fdr,
                                      pddl_err_t *err)
{
    init(s, fdr, NULL, err);
    pddlRandInit(&s->rnd, s->seed);
    s->next_fn = nextSyntactic;
}

void pddlFDRStateSamplerInitSyntacticMutex(pddl_fdr_state_sampler_t *s,
                                           const pddl_fdr_t *fdr,
                                           const pddl_mutex_pairs_t *mutex,
                                           pddl_err_t *err)
{
    init(s, fdr, mutex, err);
    pddlRandInit(&s->rnd, s->seed);
    s->next_fn = nextSyntacticMutex;
}

void pddlFDRStateSamplerFree(pddl_fdr_state_sampler_t *s)
{
    if (s->use_random_walk)
        pddlRandomWalkFree(&s->random_walk);
}

void pddlFDRStateSamplerNext(pddl_fdr_state_sampler_t *s, int *state)
{
    s->next_fn(s, state);
}

int pddlFDRStateSamplerComputeMaxStepsFromHeurInit(const pddl_fdr_t *fdr,
                                                   int hinit)
{
    double avg_op_cost = 0.;
    for (int oi = 0; oi < fdr->op.op_size; ++oi)
        avg_op_cost += fdr->op.op[oi]->cost;
    avg_op_cost /= fdr->op.op_size;
    if (avg_op_cost < 1E-2){
        return 10;
    }else{
        return (ceil(hinit / avg_op_cost) + .5) * 4;
    }
}
