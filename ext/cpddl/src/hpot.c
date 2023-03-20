/***
 * Copyright (c)2022 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

#include "internal.h"
#include "_heur.h"
#include "pddl/fdr_state_sampler.h"
#include "pddl/set.h"
#include "pddl/hpot.h"

#define INIT_STATE_RHS_DECREASE_STEP 0.125
#define INIT_STATE_RHS_DECREASE_MAX_STEPS 8

static const uint32_t rand_diverse_seed = 131071;
static const uint32_t rand_seed = 131071;

struct state_sampler {
    pddl_fdr_state_sampler_t state_sampler_random_walk;
    int state_sampler_random_walk_set;
    pddl_fdr_state_sampler_t state_sampler_syntactic;
    int state_sampler_syntactic_set;
    pddl_fdr_state_sampler_t state_sampler_mutex;
    int state_sampler_mutex_set;
};
typedef struct state_sampler state_sampler_t;


#define CLONE_OPT(dst, src, type) \
    do { \
        CONTAINER_OF_CONST(__c, src, type, cfg); \
        type *__newc = ALLOC(type); \
        *__newc = *__c; \
        dst = &__newc->cfg; \
    } while (0)

static _pddl_hpot_config_t *hpotConfigClone(const _pddl_hpot_config_t *c)
{
    _pddl_hpot_config_t *newc = NULL;
    switch(c->type){
        case PDDL_HPOT_OPT_STATE_TYPE:
            CLONE_OPT(newc, c, pddl_hpot_config_opt_state_t);
            break;
        case PDDL_HPOT_OPT_ALL_SYNTACTIC_STATES_TYPE:
            CLONE_OPT(newc, c, pddl_hpot_config_opt_all_syntactic_states_t);
            break;
        case PDDL_HPOT_OPT_ALL_STATES_MUTEX_TYPE:
            CLONE_OPT(newc, c, pddl_hpot_config_opt_all_states_mutex_t);
            break;
        case PDDL_HPOT_OPT_SAMPLED_STATES_TYPE:
            CLONE_OPT(newc, c, pddl_hpot_config_opt_sampled_states_t);
            break;
        case PDDL_HPOT_OPT_ENSEMBLE_SAMPLED_STATES_TYPE:
            CLONE_OPT(newc, c, pddl_hpot_config_opt_ensemble_sampled_states_t);
            break;
        case PDDL_HPOT_OPT_ENSEMBLE_DIVERSIFICATION_TYPE:
            CLONE_OPT(newc, c, pddl_hpot_config_opt_ensemble_diversification_t);
            break;
        case PDDL_HPOT_OPT_ENSEMBLE_ALL_STATES_MUTEX_TYPE:
            CLONE_OPT(newc, c, pddl_hpot_config_opt_ensemble_all_states_mutex_t);
            break;
    }
    return newc;
}

void pddlHPotConfigInit(pddl_hpot_config_t *cfg)
{
    pddl_hpot_config_t init = PDDL_HPOT_CONFIG_INIT;
    *cfg = init;
}

void pddlHPotConfigInitCopy(pddl_hpot_config_t *dst,
                            const pddl_hpot_config_t *src)
{
    *dst = *src;
    dst->cfg_size = 0;

    for (int i = 0; i < src->cfg_size; ++i)
        dst->cfg[dst->cfg_size++] = hpotConfigClone(src->cfg[i]);
}

void pddlHPotConfigFree(pddl_hpot_config_t *cfg)
{
    for (int i = 0; i < cfg->cfg_size; ++i)
        FREE(cfg->cfg[i]);
    cfg->cfg_size = 0;
}

void pddlHPotConfigSetDisambiguation(pddl_hpot_config_t *cfg)
{
    cfg->disambiguation = 1;
    cfg->weak_disambiguation = 0;
}

void pddlHPotConfigSetWeakDisambiguation(pddl_hpot_config_t *cfg)
{
    cfg->disambiguation = 0;
    cfg->weak_disambiguation = 1;
}

void pddlHPotConfigSetOpPot(pddl_hpot_config_t *cfg)
{
    cfg->op_pot = 1;
}

void pddlHPotConfigSetOpPotReal(pddl_hpot_config_t *cfg)
{
    cfg->op_pot = 1;
    cfg->op_pot_real = 1;
}

void pddlHPotConfigAdd(pddl_hpot_config_t *cfg,
                       const _pddl_hpot_config_t *cfg_add)
{
    ASSERT_RUNTIME(cfg->cfg_size < PDDL_HPOT_CONFIG_MAX_OPT_CONFIGS);
    cfg->cfg[cfg->cfg_size++] = hpotConfigClone(cfg_add);
}

void pddlHPotConfigReplaceInitConstrWithState(pddl_hpot_config_t *cfg,
                                              const int *fdr_state)
{
    for (int i = 0; i < cfg->cfg_size; ++i){
        _pddl_hpot_config_t *_c = cfg->cfg[i];
        switch (_c->type){
            case PDDL_HPOT_OPT_STATE_TYPE:
                {
                    CONTAINER_OF(c, _c, pddl_hpot_config_opt_state_t, cfg);
                    if (c->fdr_state == NULL)
                        c->fdr_state = fdr_state;
                }
                break;
            case PDDL_HPOT_OPT_ALL_SYNTACTIC_STATES_TYPE:
                {
                    CONTAINER_OF(c, _c, pddl_hpot_config_opt_all_syntactic_states_t, cfg);
                    if (c->add_init_state_constr){
                        c->add_init_state_constr = 0;
                        c->add_fdr_state_constr = fdr_state;
                    }
                }
                break;
            case PDDL_HPOT_OPT_ALL_STATES_MUTEX_TYPE:
                {
                    CONTAINER_OF(c, _c, pddl_hpot_config_opt_all_states_mutex_t, cfg);
                    if (c->add_init_state_constr){
                        c->add_init_state_constr = 0;
                        c->add_fdr_state_constr = fdr_state;
                    }
                }
                break;
            case PDDL_HPOT_OPT_SAMPLED_STATES_TYPE:
                {
                    CONTAINER_OF(c, _c, pddl_hpot_config_opt_sampled_states_t, cfg);
                    if (c->add_init_state_constr){
                        c->add_init_state_constr = 0;
                        c->add_fdr_state_constr = fdr_state;
                    }
                }
                break;
            case PDDL_HPOT_OPT_ENSEMBLE_SAMPLED_STATES_TYPE:
            case PDDL_HPOT_OPT_ENSEMBLE_DIVERSIFICATION_TYPE:
            case PDDL_HPOT_OPT_ENSEMBLE_ALL_STATES_MUTEX_TYPE:
                break;
        }
    }
}

static void hpotConfigLogOptState(const pddl_hpot_config_opt_state_t *c,
                                  pddl_err_t *err)
{
    LOG(err, "type: %{type}s", "state");
}

static void hpotConfigLogOptAllSyntacticStates(
                const pddl_hpot_config_opt_all_syntactic_states_t *c,
                pddl_err_t *err)
{
    LOG(err, "type: %{type}s", "all-syntactic-states");
    LOG_CONFIG_BOOL(c, add_init_state_constr, err);
    if (c->add_fdr_state_constr != NULL){
        LOG(err, "add_state_constr: %{add_state_constr}b", 1);
        LOG(err, "add_state_constr_coef: %{add_state_constr}.2f",
            c->add_state_coef);
    }else{
        LOG(err, "add_state_constr: %{add_state_constr}b", 0);
    }
}

static void hpotConfigLogOptAllStatesMutex(
                const pddl_hpot_config_opt_all_states_mutex_t *c,
                pddl_err_t *err)
{
    LOG(err, "type: %{type}s", "all-states-mutex");
    LOG_CONFIG_INT(c, mutex_size, err);
    LOG_CONFIG_BOOL(c, add_init_state_constr, err);
    
    if (c->add_fdr_state_constr != NULL){
        LOG(err, "add_state_constr: %{add_state_constr}b", 1);
        LOG(err, "add_state_constr_coef: %{add_state_constr}.2f",
            c->add_state_coef);
    }else{
        LOG(err, "add_state_constr: %{add_state_constr}b", 0);
    }
}

static void hpotConfigLogOptSampledStates(
                const pddl_hpot_config_opt_sampled_states_t *c,
                pddl_err_t *err)
{
    LOG(err, "type: %{type}s", "sampled-states");
    LOG_CONFIG_INT(c, num_samples, err);
    LOG_CONFIG_BOOL(c, use_random_walk, err);
    LOG_CONFIG_BOOL(c, use_syntactic_samples, err);
    LOG_CONFIG_BOOL(c, use_mutex_samples, err);
    LOG_CONFIG_BOOL(c, add_init_state_constr, err);
    
    if (c->add_fdr_state_constr != NULL){
        LOG(err, "add_state_constr: %{add_state_constr}b", 1);
        LOG(err, "add_state_constr_coef: %{add_state_constr}.2f",
            c->add_state_coef);
    }else{
        LOG(err, "add_state_constr: %{add_state_constr}b", 0);
    }
}

static void hpotConfigLogOptEnsembleSampledStates(
                const pddl_hpot_config_opt_ensemble_sampled_states_t *c,
                pddl_err_t *err)
{
    LOG(err, "type: %{type}s", "ensemble-sampled-states");
    LOG_CONFIG_INT(c, num_samples, err);
    LOG_CONFIG_BOOL(c, use_random_walk, err);
    LOG_CONFIG_BOOL(c, use_syntactic_samples, err);
    LOG_CONFIG_BOOL(c, use_mutex_samples, err);
}

static void hpotConfigLogOptEnsembleDiversification(
                const pddl_hpot_config_opt_ensemble_diversification_t *c,
                pddl_err_t *err)
{
    LOG(err, "type: %{type}s", "ensemble-diversification");
    LOG_CONFIG_INT(c, num_samples, err);
    LOG_CONFIG_BOOL(c, use_random_walk, err);
    LOG_CONFIG_BOOL(c, use_syntactic_samples, err);
    LOG_CONFIG_BOOL(c, use_mutex_samples, err);
}

static void hpotConfigLogOptEnsembleAllStatesMutex(
                const pddl_hpot_config_opt_ensemble_all_states_mutex_t *c,
                pddl_err_t *err)
{
    LOG(err, "type: %{type}s", "all-states-mutex");
    LOG_CONFIG_INT(c, cond_size, err);
    LOG_CONFIG_INT(c, mutex_size, err);
    LOG_CONFIG_INT(c, num_rand_samples, err);
}

#define CONFIG_LOG(func, type) \
    func(pddl_container_of(_c, type, cfg), err)
static void hpotConfigLog(const _pddl_hpot_config_t *_c, pddl_err_t *err)
{
    switch(_c->type){
        case PDDL_HPOT_OPT_STATE_TYPE:
            CONFIG_LOG(hpotConfigLogOptState, pddl_hpot_config_opt_state_t);
            break;
        case PDDL_HPOT_OPT_ALL_SYNTACTIC_STATES_TYPE:
            CONFIG_LOG(hpotConfigLogOptAllSyntacticStates,
                       pddl_hpot_config_opt_all_syntactic_states_t);
            break;
        case PDDL_HPOT_OPT_ALL_STATES_MUTEX_TYPE:
            CONFIG_LOG(hpotConfigLogOptAllStatesMutex,
                       pddl_hpot_config_opt_all_states_mutex_t);
            break;
        case PDDL_HPOT_OPT_SAMPLED_STATES_TYPE:
            CONFIG_LOG(hpotConfigLogOptSampledStates,
                       pddl_hpot_config_opt_sampled_states_t);
            break;
        case PDDL_HPOT_OPT_ENSEMBLE_SAMPLED_STATES_TYPE:
            CONFIG_LOG(hpotConfigLogOptEnsembleSampledStates,
                       pddl_hpot_config_opt_ensemble_sampled_states_t);
            break;
        case PDDL_HPOT_OPT_ENSEMBLE_DIVERSIFICATION_TYPE:
            CONFIG_LOG(hpotConfigLogOptEnsembleDiversification,
                       pddl_hpot_config_opt_ensemble_diversification_t);
            break;
        case PDDL_HPOT_OPT_ENSEMBLE_ALL_STATES_MUTEX_TYPE:
            CONFIG_LOG(hpotConfigLogOptEnsembleAllStatesMutex,
                       pddl_hpot_config_opt_ensemble_all_states_mutex_t);
            break;
    }
}

void pddlHPotConfigLog(const pddl_hpot_config_t *cfg, pddl_err_t *err)
{
    LOG_CONFIG_BOOL(cfg, disambiguation, err);
    LOG_CONFIG_BOOL(cfg, weak_disambiguation, err);
    LOG_CONFIG_BOOL(cfg, op_pot, err);
    LOG_CONFIG_BOOL(cfg, op_pot_real, err);
    LOG_CONFIG_DBL(cfg, time_limit, err);
    for (int idx = 0; idx < cfg->cfg_size; ++idx){
        CTX_NO_TIME_F(err, "opt_%d", "Opt[%d]", idx);
        hpotConfigLog(cfg->cfg[idx], err);
        CTXEND(err);
    }
}

int pddlHPotConfigCheck(const pddl_hpot_config_t *cfg, pddl_err_t *err)
{
    if (pddlHPotConfigIsEmpty(cfg))
        ERR_RET(err, -1, "Missing configuration of potential heuristics");

    if (cfg->fdr == NULL){
        ERR_RET(err, -1, "Config Error: .fdr is not set");
    }
    if (cfg->disambiguation && cfg->weak_disambiguation){
        ERR_RET(err, -1, "Config Error: Only one of .disambiguation and"
                 " .weak_disambiguation can be set");
    }
    if (cfg->disambiguation || cfg->weak_disambiguation){
        if (cfg->mg_strips == NULL || cfg->mutex == NULL){
            ERR_RET(err, -1, "Config Error: Disambiguation requires"
                     " .mg_strips and .mutex");
        }
    }

    for (int i = 0; i < cfg->cfg_size; ++i){
        if (cfg->cfg[i]->type == PDDL_HPOT_OPT_SAMPLED_STATES_TYPE){
            CONTAINER_OF_CONST(c, cfg->cfg[i], pddl_hpot_config_opt_sampled_states_t, cfg);
            if (c->use_mutex_samples && cfg->mutex == NULL){
                ERR_RET(err, -1, "Config Error: .use_mutex_samples"
                         " requires .mutex to be set");
            }
        }else if (cfg->cfg[i]->type == PDDL_HPOT_OPT_ENSEMBLE_SAMPLED_STATES_TYPE){
            CONTAINER_OF_CONST(c, cfg->cfg[i], pddl_hpot_config_opt_ensemble_sampled_states_t, cfg);
            if (c->use_mutex_samples && cfg->mutex == NULL){
                ERR_RET(err, -1, "Config Error: .use_mutex_samples"
                         " requires .mutex to be set");
            }
            if (c->num_samples <= 0)
                ERR_RET(err, -1, "Config Error: .num_samples must be > 0.");

        }else if (cfg->cfg[i]->type == PDDL_HPOT_OPT_ENSEMBLE_DIVERSIFICATION_TYPE){
            CONTAINER_OF_CONST(c, cfg->cfg[i],
                               pddl_hpot_config_opt_ensemble_diversification_t, cfg);
            if (c->use_mutex_samples && cfg->mutex == NULL){
                ERR_RET(err, -1, "Config Error: .use_mutex_samples"
                         " requires .mutex to be set");
            }
            if (c->num_samples <= 0)
                ERR_RET(err, -1, "Config Error: .num_samples must be > 0.");

        }else if (cfg->cfg[i]->type == PDDL_HPOT_OPT_ALL_STATES_MUTEX_TYPE){
            if (cfg->mutex == NULL){
                ERR_RET(err, -1, "Config Error: all-states-mutex"
                         " requires .mutex to be set");
            }

        }else if (cfg->cfg[i]->type == PDDL_HPOT_OPT_ENSEMBLE_ALL_STATES_MUTEX_TYPE){
            CONTAINER_OF_CONST(c, cfg->cfg[i],
                               pddl_hpot_config_opt_ensemble_all_states_mutex_t, cfg);
            if (cfg->mutex == NULL){
                ERR_RET(err, -1, "Config Error: ensemble-all-states-mutex"
                         " requires .mutex to be set");
            }
            if (c->mutex_size < 1 || c->mutex_size > 2){
                ERR_RET(err, -1, "Config Error: .mutex_size must be 1 or 2.");
            }
        }
    }
    return 0;
}

int pddlHPotConfigNeedMGStrips(const pddl_hpot_config_t *cfg)
{
    if (cfg->disambiguation || cfg->weak_disambiguation)
        return 1;
    return 0;
}

int pddlHPotConfigNeedMutex(const pddl_hpot_config_t *cfg)
{
    if (cfg->disambiguation || cfg->weak_disambiguation)
        return 1;

    for (int i = 0; i < cfg->cfg_size; ++i){
        if (cfg->cfg[i]->type == PDDL_HPOT_OPT_SAMPLED_STATES_TYPE){
            CONTAINER_OF_CONST(c, cfg->cfg[i], pddl_hpot_config_opt_sampled_states_t, cfg);
            if (c->use_mutex_samples)
                return 1;

        }else if (cfg->cfg[i]->type == PDDL_HPOT_OPT_ENSEMBLE_SAMPLED_STATES_TYPE){
            CONTAINER_OF_CONST(c, cfg->cfg[i], pddl_hpot_config_opt_ensemble_sampled_states_t, cfg);
            if (c->use_mutex_samples)
                return 1;

        }else if (cfg->cfg[i]->type == PDDL_HPOT_OPT_ENSEMBLE_DIVERSIFICATION_TYPE){
            CONTAINER_OF_CONST(c, cfg->cfg[i],
                               pddl_hpot_config_opt_ensemble_diversification_t, cfg);
            if (c->use_mutex_samples)
                return 1;

        }else if (cfg->cfg[i]->type == PDDL_HPOT_OPT_ALL_STATES_MUTEX_TYPE){
            if (cfg->mutex == NULL)
                return 1;

        }else if (cfg->cfg[i]->type == PDDL_HPOT_OPT_ENSEMBLE_ALL_STATES_MUTEX_TYPE){
            if (cfg->mutex == NULL)
                return 1;
        }
    }
    return 0;
}

int pddlHPotConfigIsEmpty(const pddl_hpot_config_t *cfg)
{
    return cfg->cfg_size == 0;
}

int pddlHPotConfigIsEnsemble(const pddl_hpot_config_t *cfg)
{
    if (cfg->cfg_size == 0)
        return 0;
    if (cfg->cfg_size > 1)
        return 1;

    switch (cfg->cfg[0]->type){
        case PDDL_HPOT_OPT_ENSEMBLE_SAMPLED_STATES_TYPE:
        case PDDL_HPOT_OPT_ENSEMBLE_DIVERSIFICATION_TYPE:
        case PDDL_HPOT_OPT_ENSEMBLE_ALL_STATES_MUTEX_TYPE:
            return 1;
        default:
            return 0;
    }
    return 0;
}

static void setStateToFDRState(const pddl_iset_t *state,
                               int *fdr_state,
                               const pddl_fdr_t *fdr)
{
    int fact_id;
    PDDL_ISET_FOR_EACH(state, fact_id){
        const pddl_fdr_val_t *v = fdr->var.global_id_to_val[fact_id];
        fdr_state[v->var_id] = v->val_id;
    }
}

static double heurForState(pddl_pot_t *pot,
                           const pddl_fdr_t *fdr,
                           const int *fdr_state,
                           pddl_err_t *err)
{
    pddlPotResetLowerBoundConstr(pot);
    pddlPotSetObjFDRState(pot, &fdr->var, fdr_state);
    pddl_pot_solution_t sol;
    pddlPotSolutionInit(&sol);
    int ret = pddlPotSolve(pot, &sol, err);
    if (ret != 0){
        PDDL_INFO(err, "No optimal solution for the initial state");
        return -1.;
    }

    double h = pddlPotSolutionEvalFDRStateFlt(&sol, &fdr->var, fdr_state);
    PDDL_INFO(err, "Solved for the initial state: sum: %.4f (%a),"
              " objval: %.4f (%a)", h, h, sol.objval, sol.objval);
    h = sol.objval;
    pddlPotSolutionFree(&sol);
    return h;
}

static void stateSamplerFree(state_sampler_t *ss)
{
    if (ss->state_sampler_random_walk_set)
        pddlFDRStateSamplerFree(&ss->state_sampler_random_walk);
    if (ss->state_sampler_syntactic_set)
        pddlFDRStateSamplerFree(&ss->state_sampler_syntactic);
    if (ss->state_sampler_mutex_set)
        pddlFDRStateSamplerFree(&ss->state_sampler_mutex);
}

static pddl_fdr_state_sampler_t *stateSamplerGet(state_sampler_t *ss,
                                                 pddl_pot_t *pot,
                                                 const pddl_fdr_t *fdr,
                                                 const pddl_mutex_pairs_t *mutex,
                                                 int use_random_walk,
                                                 int use_syntactic_samples,
                                                 int use_mutex_samples,
                                                 pddl_err_t *err)
{
    if (use_random_walk){
        if (ss->state_sampler_random_walk_set)
            return &ss->state_sampler_random_walk;
        double hinit_flt = heurForState(pot, fdr, fdr->init, err);
        int hinit = pddlPotSolutionRoundHValue(hinit_flt);
        int max_steps = pddlFDRStateSamplerComputeMaxStepsFromHeurInit(fdr, hinit);
        pddlFDRStateSamplerInitRandomWalk(&ss->state_sampler_random_walk,
                                          fdr, max_steps, err);
        ss->state_sampler_random_walk_set = 1;
        return &ss->state_sampler_random_walk;

    }else if (use_syntactic_samples){
        if (ss->state_sampler_syntactic_set)
            return &ss->state_sampler_syntactic;
        pddlFDRStateSamplerInitSyntactic(&ss->state_sampler_syntactic, fdr, err);
        return &ss->state_sampler_syntactic;

    }else if (use_mutex_samples){
        if (ss->state_sampler_mutex_set)
            return &ss->state_sampler_mutex;
        pddlFDRStateSamplerInitSyntacticMutex(&ss->state_sampler_mutex,
                                              fdr, mutex, err);
        return &ss->state_sampler_mutex;
    }

    PANIC("No state sampler specified!");
    return NULL;
}

static void setStateConstr(pddl_pot_t *pot,
                           const pddl_fdr_t *fdr,
                           int add_init_state,
                           const int *add_fdr_state,
                           double add_state_coef,
                           pddl_err_t *err)
{
    ASSERT_RUNTIME_M(add_state_coef >= 0.
                        && (!add_init_state || add_fdr_state == NULL),
                     "Invalid hpot configuration");
    pddlPotResetLowerBoundConstr(pot);
    if (!add_init_state && add_fdr_state == NULL)
        return;

    const int *add_state = NULL;
    if (add_init_state){
        add_state = fdr->init;
    }else{
        add_state = add_fdr_state;
    }

    if (add_state_coef <= 0.)
        add_state_coef = 1.;
    double h_value = heurForState(pot, fdr, add_state, err);
    double rhs = h_value * add_state_coef;

    PDDL_ISET(vars);
    for (int var = 0; var < fdr->var.var_size; ++var){
        int v = fdr->var.var[var].val[add_state[var]].global_id;
        pddlISetAdd(&vars, v);
    }
    rhs -= INIT_STATE_RHS_DECREASE_STEP;
    pddlPotSetLowerBoundConstr(pot, &vars, rhs);
    PDDL_INFO(err, "added lower bound constraint with rhs: %.4f (%a)",
              rhs, rhs);
    pddlISetFree(&vars);
}

static int solveAndAdd(pddl_pot_solutions_t *sols,
                       pddl_pot_t *pot,
                       const pddl_hpot_config_t *cfg,
                       pddl_err_t *err)
{
    pddl_pot_solution_t sol;
    pddlPotSolutionInit(&sol);
    int ret = pddlPotSolve(pot, &sol, err);
    if (ret == 0){
        PDDL_INFO(err, "Have a solution. objval: %.4f", sol.objval);
        pddlPotSolutionsAdd(sols, &sol);
    }else{
        PDDL_INFO(err, "Solution not found.");
    }
    pddlPotSolutionFree(&sol);
    return ret;
}

static int solveAndAddWithStateConstr(pddl_pot_solutions_t *sols,
                                      pddl_pot_t *pot,
                                      const pddl_hpot_config_t *cfg,
                                      const int *add_fdr_state,
                                      pddl_err_t *err)
{
    if (add_fdr_state == NULL)
        return solveAndAdd(sols, pot, cfg, err);

    int ret = solveAndAdd(sols, pot, cfg, err);
    for (int i = 0; i < INIT_STATE_RHS_DECREASE_MAX_STEPS && ret != 0; ++i){
        pddlPotDecreaseLowerBoundConstrRHS(pot, INIT_STATE_RHS_DECREASE_STEP);
        double rhs = pddlPotSetLowerBoundConstrRHS(pot);
        PDDL_INFO(err, "Solution not found. Setting lower bound constraint to "
                  "%.4f (%a)", rhs, rhs);
        ret = solveAndAdd(sols, pot, cfg, err);
    }
    return ret;
}

static int initPot(pddl_pot_t *pot,
                   const pddl_hpot_config_t *cfg,
                   pddl_err_t *err)
{
    if (cfg->weak_disambiguation){
        if (cfg->mg_strips == NULL){
            ERR_RET(err, -1, "Config Error: .mg_strips needs to be set if"
                     " the weak disambiguation is used");
        }
        if (cfg->mutex == NULL){
            ERR_RET(err, -1, "Config Error: .mutex needs to be set if"
                     " the weak disambiguation is used");
        }

        if (pddlPotInitMGStripsSingleFactDisamb(pot, cfg->mg_strips, cfg->mutex) == 0){
            PDDL_INFO(err, "Initialized with weak-disambiguation."
                      " vars: %d, op-constr: %d,"
                      " goal-constr: %d, maxpots: %d",
                      pot->var_size,
                      pot->constr_op.size,
                      pot->constr_goal.size,
                      pot->maxpot_size);
        }else{
            PDDL_INFO(err, "Disambiguation proved the task unsolvable.");
            return -1;
        }

    }else if (cfg->disambiguation){
        if (cfg->mg_strips == NULL){
            ERR_RET(err, -1, "Config Error: .mg_strips needs to be set if"
                     " the weak disambiguation is used");
        }
        if (cfg->mutex == NULL){
            ERR_RET(err, -1, "Config Error: .mutex needs to be set if"
                     " the weak disambiguation is used");
        }
        if (pddlPotInitMGStrips(pot, cfg->mg_strips, cfg->mutex) == 0){
            PDDL_INFO(err, "Initialized with disambiguation."
                      " vars: %d, op-constr: %d,"
                      " goal-constr: %d, maxpots: %d",
                      pot->var_size,
                      pot->constr_op.size,
                      pot->constr_goal.size,
                      pot->maxpot_size);
        }else{
            PDDL_INFO(err, "Disambiguation proved the task unsolvable.");
            return -1;
        }

    }else{
        if (cfg->fdr == NULL){
            ERR_RET(err, -1, "Config Error: .fdr needs to be set if"
                     " the weak disambiguation is used");
        }
        pddlPotInitFDR(pot, cfg->fdr);
        PDDL_INFO(err, "Initialized without disambiguation."
                  " vars: %d, op-constr: %d,"
                  " goal-constr: %d, maxpots: %d",
                  pot->var_size,
                  pot->constr_op.size,
                  pot->constr_goal.size,
                  pot->maxpot_size);
    }

    if (cfg->op_pot)
        pddlPotEnableOpPot(pot, 1, cfg->op_pot_real);

    return 0;
}

static int _hpotOptState(pddl_pot_solutions_t *sols,
                         pddl_pot_t *pot,
                         const pddl_hpot_config_t *cfg,
                         const int *fdr_state,
                         pddl_err_t *err)
{
    pddlPotResetLowerBoundConstr(pot);

    pddlPotSetObjFDRState(pot, &cfg->fdr->var, fdr_state);
    int ret = solveAndAdd(sols, pot, cfg, err);
    ASSERT_RUNTIME_M(ret == 0, "Could not find a solution. This seems like a bug!");
    return ret;
}

static int hpotOptState(pddl_pot_solutions_t *sols,
                        pddl_pot_t *pot,
                        const pddl_hpot_config_t *cfg,
                        const _pddl_hpot_config_t *_cfg,
                        pddl_err_t *err)
{
    CONTAINER_OF_CONST(cfg_opt, _cfg, pddl_hpot_config_opt_state_t, cfg);
    const int *state = cfg_opt->fdr_state;
    if (state == NULL)
        state = cfg->fdr->init;
    return _hpotOptState(sols, pot, cfg, state, err);
}

static int hpotOptAllSyntacticStates(pddl_pot_solutions_t *sols,
                                     pddl_pot_t *pot,
                                     const pddl_hpot_config_t *cfg,
                                     const _pddl_hpot_config_t *_cfg,
                                     pddl_err_t *err)
{
    CONTAINER_OF_CONST(cfg_opt, _cfg, pddl_hpot_config_opt_all_syntactic_states_t, cfg);
    setStateConstr(pot, cfg->fdr, cfg_opt->add_init_state_constr,
                   cfg_opt->add_fdr_state_constr,
                   cfg_opt->add_state_coef, err);

    pddlPotSetObjFDRAllSyntacticStates(pot, &cfg->fdr->var);
    int ret = solveAndAddWithStateConstr(sols, pot, cfg,
                                         cfg_opt->add_fdr_state_constr, err);
    ASSERT_RUNTIME_M(ret == 0, "Could not find a solution. This seems like a bug!");
    return 0;
}

static double countStatesMutex(const pddl_mgroups_t *mgs,
                               const pddl_mutex_pairs_t *mutex,
                               const pddl_iset_t *fixed)
{
    if (pddlMutexPairsIsMutexSet(mutex, fixed))
        return 0.;

    if (fixed == NULL || pddlISetSize(fixed) == 0){
        double num = pddlISetSize(&mgs->mgroup[0].mgroup);
        for (int i = 1; i < mgs->mgroup_size; ++i)
            num *= pddlISetSize(&mgs->mgroup[i].mgroup);
        return num;
    }

    double num = 1.;
    for (int mgi = 0; mgi < mgs->mgroup_size; ++mgi){
        int mg_size = 0;
        int fact;
        PDDL_ISET_FOR_EACH(&mgs->mgroup[mgi].mgroup, fact){
            if (!pddlMutexPairsIsMutexFactSet(mutex, fact, fixed))
                mg_size += 1;
        }
        num *= (double)mg_size;
    }
    return num;
}

static void setObjAllStatesMutex1(pddl_pot_t *pot,
                                  const pddl_mgroups_t *mgs,
                                  const pddl_mutex_pairs_t *mutex)
{
    double *coef = CALLOC_ARR(double, pot->var_size);
    PDDL_ISET(fixed);

    for (int mgi = 0; mgi < mgs->mgroup_size; ++mgi){
        const pddl_mgroup_t *mg = mgs->mgroup + mgi;
        double sum = 0.;
        int fixed_fact;
        PDDL_ISET_FOR_EACH(&mg->mgroup, fixed_fact){
            pddlISetEmpty(&fixed);
            pddlISetAdd(&fixed, fixed_fact);
            coef[fixed_fact] = countStatesMutex(mgs, mutex, &fixed);
            sum += coef[fixed_fact];
        }
        PDDL_ISET_FOR_EACH(&mg->mgroup, fixed_fact){
            coef[fixed_fact] /= sum;
            if (coef[fixed_fact] < 1E-6)
                coef[fixed_fact] = 0.;
        }
    }

    pddlPotSetObj(pot, coef);

    pddlISetFree(&fixed);
    if (coef != NULL)
        FREE(coef);
}

static void setObjAllStatesMutex2(pddl_pot_t *pot,
                                  const pddl_mgroups_t *mgs,
                                  int fact_size,
                                  const pddl_mutex_pairs_t *mutex)
{
    double *coef = CALLOC_ARR(double, pot->var_size);
    PDDL_ISET(fixed);

    for (int mgi = 0; mgi < mgs->mgroup_size; ++mgi){
        const pddl_mgroup_t *mg = mgs->mgroup + mgi;
        double sum = 0.;
        int fixed_fact;
        PDDL_ISET_FOR_EACH(&mg->mgroup, fixed_fact){
            coef[fixed_fact] = 0.;
            for (int f = 0; f < fact_size; ++f){
                if (f == fixed_fact)
                    continue;

                pddlISetEmpty(&fixed);
                pddlISetAdd(&fixed, fixed_fact);
                pddlISetAdd(&fixed, f);
                ASSERT(pddlISetSize(&fixed) == 2);
                coef[fixed_fact] += countStatesMutex(mgs, mutex, &fixed);
            }
            sum += coef[fixed_fact];
        }
        PDDL_ISET_FOR_EACH(&mg->mgroup, fixed_fact){
            coef[fixed_fact] /= sum;
            if (coef[fixed_fact] < 1E-6)
                coef[fixed_fact] = 0.;
        }
    }

    pddlPotSetObj(pot, coef);

    pddlISetFree(&fixed);
    if (coef != NULL)
        FREE(coef);
}

static int hpotOptAllStatesMutex(pddl_pot_solutions_t *sols,
                                 pddl_pot_t *pot,
                                 const pddl_hpot_config_t *cfg,
                                 const _pddl_hpot_config_t *_cfg,
                                 pddl_err_t *err)
{
    // TODO: refactor
    CONTAINER_OF_CONST(cfg_opt, _cfg, pddl_hpot_config_opt_all_states_mutex_t, cfg);
    setStateConstr(pot, cfg->fdr, cfg_opt->add_init_state_constr,
                   cfg_opt->add_fdr_state_constr,
                   cfg_opt->add_state_coef, err);

    if (cfg_opt->mutex_size == 1){
        setObjAllStatesMutex1(pot, &cfg->mg_strips->mg, cfg->mutex);

    }else if (cfg_opt->mutex_size == 2){
        setObjAllStatesMutex2(pot, &cfg->mg_strips->mg,
                              cfg->mg_strips->strips.fact.fact_size,
                              cfg->mutex);

    }else{
        PANIC("All states mutex optimization not supported for"
              " mutex_size=%d", cfg_opt->mutex_size);
    }

    return solveAndAddWithStateConstr(sols, pot, cfg,
                                      cfg_opt->add_fdr_state_constr, err);
}

static int hpotOptSampledStates(pddl_pot_solutions_t *sols,
                                pddl_pot_t *pot,
                                state_sampler_t *state_sampler,
                                const pddl_hpot_config_t *cfg,
                                const _pddl_hpot_config_t *_cfg,
                                pddl_err_t *err)
{
    CONTAINER_OF_CONST(cfg_opt, _cfg, pddl_hpot_config_opt_sampled_states_t, cfg);
    setStateConstr(pot, cfg->fdr, cfg_opt->add_init_state_constr,
                   cfg_opt->add_fdr_state_constr,
                   cfg_opt->add_state_coef, err);

    pddl_fdr_state_sampler_t *sampler;
    sampler = stateSamplerGet(state_sampler, pot, cfg->fdr, cfg->mutex,
                              cfg_opt->use_random_walk,
                              cfg_opt->use_syntactic_samples,
                              cfg_opt->use_mutex_samples,
                              err);

    int state[cfg->fdr->var.var_size];
    int num_states = 0;
    double *coef = CALLOC_ARR(double, pot->var_size);
    for (int si = 0; si < cfg_opt->num_samples; ++si){
        pddlFDRStateSamplerNext(sampler, state);
        for (int var = 0; var < cfg->fdr->var.var_size; ++var)
            coef[cfg->fdr->var.var[var].val[state[var]].global_id] += 1.;
        ++num_states;
    }

    pddlPotSetObj(pot, coef);
    if (coef != NULL)
        FREE(coef);

    int ret = solveAndAddWithStateConstr(sols, pot, cfg,
                                         cfg_opt->add_fdr_state_constr, err);
    ASSERT_RUNTIME_M(ret == 0, "Could not find a solution. This seems like a bug!");
    LOG(err, "Solved for average over %d states", num_states);
    return 0;
}

static int hpotOptEnsembleSampledStates(pddl_pot_solutions_t *sols,
                                        pddl_pot_t *pot,
                                        state_sampler_t *state_sampler,
                                        const pddl_hpot_config_t *cfg,
                                        const _pddl_hpot_config_t *_cfg,
                                        pddl_err_t *err)
{
    CONTAINER_OF_CONST(cfg_opt, _cfg, pddl_hpot_config_opt_ensemble_sampled_states_t, cfg);
    pddlPotResetLowerBoundConstr(pot);

    pddl_fdr_state_sampler_t *sampler;
    sampler = stateSamplerGet(state_sampler, pot, cfg->fdr, cfg->mutex,
                              cfg_opt->use_random_walk,
                              cfg_opt->use_syntactic_samples,
                              cfg_opt->use_mutex_samples,
                              err);

    int ret = 0;
    int state[cfg->fdr->var.var_size];
    int num_states = 0;
    for (int si = 0; si < cfg_opt->num_samples; ++si){
        pddlFDRStateSamplerNext(sampler, state);
        ret = _hpotOptState(sols, pot, cfg, state, err);
        if (ret != 0)
            break;
        if ((si + 1) % 100 == 0){
            PDDL_INFO(err, "Solved for state: %d/%d",
                      num_states, cfg_opt->num_samples);
        }
        ++num_states;
    }
    PDDL_INFO(err, "Solved for state: %d/%d", num_states, cfg_opt->num_samples);

    // TODO: remove dead-ends
    ASSERT_RUNTIME_M(ret == 0, "Could not find a solution. This seems like a bug!");
    LOG(err, "Solved for %d states", num_states);
    return ret;
}

struct diverse_pot {
    double *coef;
    pddl_pot_solutions_t func;
    pddl_pot_solution_t avg_func;
    int *state_est;
    pddl_set_iset_t states;
    int active_states;
    pddl_rand_t rnd;
};
typedef struct diverse_pot diverse_pot_t;

static void diverseInit(diverse_pot_t *div,
                        const pddl_pot_t *pot,
                        const pddl_fdr_t *fdr,
                        int num_samples)
{
    div->coef = ALLOC_ARR(double, pot->var_size);
    pddlPotSolutionsInit(&div->func);
    pddlPotSolutionInit(&div->avg_func);
    div->state_est = CALLOC_ARR(int, num_samples);
    pddlSetISetInit(&div->states);
    div->active_states = 0;
    pddlRandInit(&div->rnd, rand_diverse_seed);
}

static void diverseFree(diverse_pot_t *div)
{
    FREE(div->coef);
    pddlPotSolutionsFree(&div->func);
    pddlPotSolutionFree(&div->avg_func);
    FREE(div->state_est);
    pddlSetISetFree(&div->states);
}

static void diverseGenStates(diverse_pot_t *div,
                             pddl_pot_t *pot,
                             pddl_fdr_state_sampler_t *sampler,
                             const pddl_fdr_t *fdr,
                             int num_samples,
                             pddl_err_t *err)
{
    PDDL_INFO(err, "generating %d samples and computing potentials...",
              num_samples);

    // Samples states, filter out dead-ends and compute estimate for each
    // state
    int num_states = 0;
    int num_dead_ends = 0;
    int num_duplicates = 0;
    int fdr_state[fdr->var.var_size];
    PDDL_ISET(state);
    for (int si = 0; si < num_samples; ++si){
        pddlFDRStateSamplerNext(sampler, fdr_state);

        pddlISetEmpty(&state);
        ZEROIZE_ARR(div->coef, pot->var_size);
        for (int var = 0; var < fdr->var.var_size; ++var){
            int id = fdr->var.var[var].val[fdr_state[var]].global_id;
            div->coef[id] = 1.;
            pddlISetAdd(&state, id);
        }

        if (pddlSetISetFind(&div->states, &state) >= 0){
            // Ignore duplicates
            ++num_duplicates;
            continue;
        }

        // Compute heuristic estimate
        pddlPotSetObj(pot, div->coef);
        pddl_pot_solution_t sol;
        pddlPotSolutionInit(&sol);
        if (pddlPotSolve(pot, &sol, err) == 0){
            int h = pddlPotSolutionEvalFDRState(&sol, &fdr->var, fdr_state);
            if (h != PDDL_COST_DEAD_END){
                // Add state to the set of states and store heuristic estimate
                int state_id = pddlSetISetAdd(&div->states, &state);
                ASSERT(state_id == num_states);
                div->state_est[state_id] = h;
                ASSERT_RUNTIME(div->state_est[state_id] >= 0);
                pddlPotSolutionsAdd(&div->func, &sol);
                ++num_states;

                if ((si + 1) % 100 == 0){
                    PDDL_INFO(err, "Diverse: %d/%d (dead-ends: %d)",
                              num_states, num_samples, num_dead_ends);
                }

            }else{
                ++num_dead_ends;
            }
        }else{
            // Dead-ends are simply skipped
            ++num_dead_ends;
        }
        pddlPotSolutionFree(&sol);
    }
    PDDL_INFO(err, "Detected dead-ends: %d", num_dead_ends);
    PDDL_INFO(err, "Detected duplicates: %d", num_duplicates);
    ASSERT(num_states == pddlSetISetSize(&div->states));
    div->active_states = pddlSetISetSize(&div->states);
    pddlISetFree(&state);
}



static int diverseAvg(diverse_pot_t *div,
                      pddl_pot_t *pot,
                      pddl_err_t *err)
{
    ZEROIZE_ARR(div->coef, pot->var_size);
    const pddl_iset_t *state;
    PDDL_SET_ISET_FOR_EACH_ID_SET(&div->states, i, state){
        if (div->state_est[i] < 0)
            continue;
        int fact_id;
        PDDL_ISET_FOR_EACH(state, fact_id)
            div->coef[fact_id] += 1.;
    }
    pddlPotSetObj(pot, div->coef);
    return pddlPotSolve(pot, &div->avg_func, err);
}

static const pddl_pot_solution_t *diverseSelectFunc(diverse_pot_t *div,
                                                    pddl_pot_t *pot,
                                                    const pddl_fdr_t *fdr,
                                                    pddl_err_t *err)
{
    if (diverseAvg(div, pot, err) != 0)
        return NULL;

    int *fdr_state = ALLOC_ARR(int, fdr->var.var_size);
    const pddl_iset_t *state;
    PDDL_SET_ISET_FOR_EACH_ID_SET(&div->states, si, state){
        if (div->state_est[si] < 0)
            continue;
        setStateToFDRState(state, fdr_state, fdr);

        int hest;
        hest = pddlPotSolutionEvalFDRState(&div->avg_func, &fdr->var, fdr_state);
        if (hest == div->state_est[si]){
            FREE(fdr_state);
            return &div->avg_func;
        }
    }

    int sid = pddlRand(&div->rnd, 0, div->active_states);
    PDDL_SET_ISET_FOR_EACH_ID(&div->states, si){
        if (div->state_est[si] < 0)
            continue;
        if (sid-- == 0){
            FREE(fdr_state);
            return div->func.sol + si;
        }
    }
    ASSERT_RUNTIME_M(0, "The number of active states is invalid!");
    return NULL;
}

static void diverseFilterOutStates(diverse_pot_t *div,
                                   const pddl_fdr_t *fdr,
                                   const pddl_pot_solution_t *func,
                                   pddl_err_t *err)
{
    int *fdr_state = ALLOC_ARR(int, fdr->var.var_size);
    const pddl_iset_t *state;
    PDDL_SET_ISET_FOR_EACH_ID_SET(&div->states, si, state){
        if (div->state_est[si] < 0)
            continue;
        setStateToFDRState(state, fdr_state, fdr);
        int hest = pddlPotSolutionEvalFDRState(func, &fdr->var, fdr_state);
        if (hest >= div->state_est[si]){
            div->state_est[si] = -1;
            --div->active_states;
        }
    }
    FREE(fdr_state);
}

static int hpotOptEnsembleDiversification(pddl_pot_solutions_t *sols,
                                          pddl_pot_t *pot,
                                          state_sampler_t *state_sampler,
                                          const pddl_hpot_config_t *cfg,
                                          const _pddl_hpot_config_t *_cfg,
                                          pddl_err_t *err)
{
    // TODO: refactor
    CONTAINER_OF_CONST(cfg_opt, _cfg, pddl_hpot_config_opt_ensemble_diversification_t, cfg);
    PDDL_INFO(err, "Diverse potentials with %d samples", cfg_opt->num_samples);
    pddlPotResetLowerBoundConstr(pot);

    pddl_fdr_state_sampler_t *sampler;
    sampler = stateSamplerGet(state_sampler, pot, cfg->fdr, cfg->mutex,
                              cfg_opt->use_random_walk,
                              cfg_opt->use_syntactic_samples,
                              cfg_opt->use_mutex_samples,
                              err);
    diverse_pot_t div;
    diverseInit(&div, pot, cfg->fdr, cfg_opt->num_samples);
    diverseGenStates(&div, pot, sampler, cfg->fdr, cfg_opt->num_samples, err);
    while (div.active_states > 0){
        const pddl_pot_solution_t *func = diverseSelectFunc(&div, pot, cfg->fdr, err);
        if (func == NULL)
            return -1;
        pddlPotSolutionsAdd(sols, func);
        diverseFilterOutStates(&div, cfg->fdr, func, err);
    }
    diverseFree(&div);
    PDDL_INFO(err, "Computed diverse potentials with %d functions",
              sols->sol_size);
    return 0;
}

static void setObjAllStatesMutexConditioned(pddl_pot_t *pot,
                                            const pddl_iset_t *cond,
                                            const pddl_mg_strips_t *s,
                                            const pddl_mutex_pairs_t *mutex,
                                            int mutex_size)
{
    pddl_mgroups_t mgs;
    pddlMGroupsInitEmpty(&mgs);
    PDDL_ISET(mg);
    for (int mgi = 0; mgi < s->mg.mgroup_size; ++mgi){
        int fact_id;
        pddlISetEmpty(&mg);
        PDDL_ISET_FOR_EACH(&s->mg.mgroup[mgi].mgroup, fact_id){
            if (!pddlMutexPairsIsMutexFactSet(mutex, fact_id, cond))
                pddlISetAdd(&mg, fact_id);
        }

        if (pddlISetSize(&mg) == 0){
            pddlISetFree(&mg);
            pddlMGroupsFree(&mgs);
            return;
        }

        pddlMGroupsAdd(&mgs, &mg);
    }
    pddlISetFree(&mg);

    if (mutex_size == 1){
        setObjAllStatesMutex1(pot, &mgs, mutex);
    }else if (mutex_size == 2){
        setObjAllStatesMutex2(pot, &mgs, s->strips.fact.fact_size, mutex);
    }else{
        ASSERT_RUNTIME_M(0, "mutex-size >= 3 is not supported!");
    }
    pddlMGroupsFree(&mgs);
}

static int allStatesMutexCond(pddl_pot_solutions_t *sols,
                              pddl_pot_t *pot,
                              const pddl_mg_strips_t *mg_strips,
                              const pddl_mutex_pairs_t *mutex,
                              const pddl_hpot_config_t *cfg,
                              int mutex_size,
                              const pddl_iset_t *facts,
                              pddl_err_t *err)
{
    PDDL_ISET(cond);
    int fact_id;
    int count = 0;
    PDDL_ISET_FOR_EACH(facts, fact_id){
        pddlISetEmpty(&cond);
        pddlISetAdd(&cond, fact_id);
        setObjAllStatesMutexConditioned(pot, &cond, mg_strips, mutex,
                                        mutex_size);
        solveAndAdd(sols, pot, cfg, err);
        if (++count % 10 == 0){
            PDDL_INFO(err, "Computed conditioned func %d/%d and generated %d"
                      " potential functions",
                      count, pddlISetSize(facts), sols->sol_size);
        }
    }
    PDDL_INFO(err, "Computed conditioned func %d/%d and generated %d"
              " potential functions",
              count, pddlISetSize(facts), sols->sol_size);
    pddlISetFree(&cond);

    if (sols->sol_size > 0)
        return 0;
    return -1;
}

static int allStatesMutexCond2(pddl_pot_solutions_t *sols,
                               pddl_pot_t *pot,
                               const pddl_mg_strips_t *mg_strips,
                               const pddl_mutex_pairs_t *mutex,
                               const pddl_hpot_config_t *cfg,
                               int mutex_size,
                               int num_samples,
                               pddl_err_t *err)
{
    pddl_rand_t rnd;
    pddlRandInit(&rnd, rand_seed);
    PDDL_ISET(cond);
    int count = 0;
    int fact_size = mg_strips->strips.fact.fact_size;
    for (int i = 0; i < num_samples; ++i){
        int f1 = pddlRand(&rnd, 0, fact_size);
        int f2 = pddlRand(&rnd, 0, fact_size);
        if (pddlMutexPairsIsMutex(mutex, f1, f2))
            continue;
        pddlISetEmpty(&cond);
        pddlISetAdd(&cond, f1);
        pddlISetAdd(&cond, f2);

        setObjAllStatesMutexConditioned(pot, &cond, mg_strips, mutex,
                                        mutex_size);
        solveAndAdd(sols, pot, cfg, err);
        if (++count % 10 == 0){
            PDDL_INFO(err, "Computed conditioned func^2 %d and generated %d"
                      " potential functions",
                      count, sols->sol_size);
        }
    }
    PDDL_INFO(err, "Computed conditioned func^2 %d and generated %d"
              " potential functions",
              count, sols->sol_size);
    pddlISetFree(&cond);

    if (sols->sol_size > 0)
        return 0;
    return -1;
}

static int hpotOptEnsembleAllStatesMutex(pddl_pot_solutions_t *sols,
                                         pddl_pot_t *pot,
                                         state_sampler_t *state_sampler,
                                         const pddl_hpot_config_t *cfg,
                                         const _pddl_hpot_config_t *_cfg,
                                         pddl_err_t *err)
{
    // TODO: refactor
    CONTAINER_OF_CONST(cfg_opt, _cfg, pddl_hpot_config_opt_ensemble_all_states_mutex_t, cfg);
    pddlPotResetLowerBoundConstr(pot);

    int ret = 0;
    if (cfg_opt->num_rand_samples == 0){
        PDDL_ISET(facts);
        for (int f = 0; f < cfg->mg_strips->strips.fact.fact_size; ++f)
            pddlISetAdd(&facts, f);
        ret = allStatesMutexCond(sols, pot, cfg->mg_strips, cfg->mutex, cfg,
                                 cfg_opt->mutex_size, &facts, err);
        pddlISetFree(&facts);

    }else if (cfg_opt->cond_size == 1){
        pddl_rand_t rnd;
        pddlRandInit(&rnd, rand_seed);
        PDDL_ISET(facts);
        int fact_size = cfg->mg_strips->strips.fact.fact_size;
        for (int i = 0; i < cfg_opt->num_rand_samples; ++i)
            pddlISetAdd(&facts, pddlRand(&rnd, 0, fact_size));

        ret = allStatesMutexCond(sols, pot, cfg->mg_strips, cfg->mutex, cfg,
                                 cfg_opt->mutex_size, &facts, err);
        pddlISetFree(&facts);

    }else if (cfg_opt->cond_size == 2){
        ret = allStatesMutexCond2(sols, pot, cfg->mg_strips, cfg->mutex, cfg,
                                  cfg_opt->mutex_size,
                                  cfg_opt->num_rand_samples, err);
    }
    return ret;
}

int pddlHPot(pddl_pot_solutions_t *sols,
             const pddl_hpot_config_t *cfg,
             pddl_err_t *err)
{
    if (pddlHPotConfigCheck(cfg, err) != 0)
        TRACE_RET(err, -1);

    CTX(err, "hpot", "HPot");
    CTX_NO_TIME(err, "cfg", "Cfg");
    pddlHPotConfigLog(cfg, err);
    CTXEND(err);

    pddl_pot_t pot;
    if (initPot(&pot, cfg, err) != 0){
        sols->unsolvable = 1;
        CTXEND(err);
        return 0;
    }

    state_sampler_t sampler;
    ZEROIZE(&sampler);

    int ret = 0;
    for (int i = 0; i < cfg->cfg_size; ++i){
        const _pddl_hpot_config_t *_cfg = cfg->cfg[i];

        switch (_cfg->type){
            case PDDL_HPOT_OPT_STATE_TYPE:
                ret = hpotOptState(sols, &pot, cfg, _cfg, err);
                break;

            case PDDL_HPOT_OPT_ALL_SYNTACTIC_STATES_TYPE:
                ret = hpotOptAllSyntacticStates(sols, &pot, cfg, _cfg, err);
                break;

            case PDDL_HPOT_OPT_ALL_STATES_MUTEX_TYPE:
                ret = hpotOptAllStatesMutex(sols, &pot, cfg, _cfg, err);
                break;

            case PDDL_HPOT_OPT_SAMPLED_STATES_TYPE:
                ret = hpotOptSampledStates(sols, &pot, &sampler,
                                           cfg, _cfg, err);
                break;

            case PDDL_HPOT_OPT_ENSEMBLE_SAMPLED_STATES_TYPE:
                ret = hpotOptEnsembleSampledStates(sols, &pot, &sampler,
                                                   cfg, _cfg, err);
                break;

            case PDDL_HPOT_OPT_ENSEMBLE_DIVERSIFICATION_TYPE:
                ret = hpotOptEnsembleDiversification(sols, &pot, &sampler,
                                                     cfg, _cfg, err);
                break;

            case PDDL_HPOT_OPT_ENSEMBLE_ALL_STATES_MUTEX_TYPE:
                ret = hpotOptEnsembleAllStatesMutex(sols, &pot, &sampler,
                                                    cfg, _cfg, err);
                break;
        }
    }

    stateSamplerFree(&sampler);
    pddlPotFree(&pot);
    CTXEND(err);
    return ret;
}

struct pddl_heur_pot {
    pddl_heur_t heur;
    pddl_pot_solutions_t sols;
    pddl_fdr_vars_t vars;
};
typedef struct pddl_heur_pot pddl_heur_pot_t;

static void heurDel(pddl_heur_t *_h)
{
    pddl_heur_pot_t *h = pddl_container_of(_h, pddl_heur_pot_t, heur);
    _pddlHeurFree(&h->heur);
    pddlPotSolutionsFree(&h->sols);
    FREE(h);
}

static int heurEstimate(pddl_heur_t *_h,
                        const pddl_fdr_state_space_node_t *node,
                        const pddl_fdr_state_space_t *state_space)
{
    pddl_heur_pot_t *h = pddl_container_of(_h, pddl_heur_pot_t, heur);
    int est = pddlPotSolutionsEvalMaxFDRState(&h->sols, &h->vars, node->state);
    return est;
}

pddl_heur_t *pddlHeurPot(const pddl_hpot_config_t *cfg, pddl_err_t *err)
{
    pddl_heur_pot_t *h = ALLOC(pddl_heur_pot_t);
    pddlPotSolutionsInit(&h->sols);
    int ret = pddlHPot(&h->sols, cfg, err);
    if (ret != 0){
        pddlPotSolutionsFree(&h->sols);
        TRACE_RET(err, NULL);
    }
    pddlFDRVarsInitCopy(&h->vars, &cfg->fdr->var);
    _pddlHeurInit(&h->heur, heurDel, heurEstimate);
    return &h->heur;
}
