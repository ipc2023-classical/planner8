/***
 * Copyright (c)2022 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

#ifndef __PDDL_HPOT_H__
#define __PDDL_HPOT_H__

#include <pddl/pot.h>
#include <pddl/task.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define PDDL_HPOT_CONFIG_MAX_OPT_CONFIGS 10

enum pddl_hpot_type {
    PDDL_HPOT_OPT_STATE_TYPE = 1,
    PDDL_HPOT_OPT_ALL_SYNTACTIC_STATES_TYPE,
    PDDL_HPOT_OPT_ALL_STATES_MUTEX_TYPE,
    PDDL_HPOT_OPT_SAMPLED_STATES_TYPE,
    PDDL_HPOT_OPT_ENSEMBLE_SAMPLED_STATES_TYPE,
    PDDL_HPOT_OPT_ENSEMBLE_DIVERSIFICATION_TYPE,
    PDDL_HPOT_OPT_ENSEMBLE_ALL_STATES_MUTEX_TYPE,
};
typedef enum pddl_hpot_type pddl_hpot_type_t;

struct _pddl_hpot_config {
    pddl_hpot_type_t type;
};
typedef struct _pddl_hpot_config _pddl_hpot_config_t;

#define _PDDL_HPOT_CONFIG_INIT(type) \
    { \
        (type), /* .type */ \
    }

/**
 * Maximize the h-value for the given state
 */
struct pddl_hpot_config_opt_state {
    _pddl_hpot_config_t cfg;
    /** State for which to optimize. If NULL, initial state is used. */
    const int *fdr_state;
};
typedef struct pddl_hpot_config_opt_state pddl_hpot_config_opt_state_t;

#define PDDL_HPOT_CONFIG_OPT_STATE_INIT \
    { \
        _PDDL_HPOT_CONFIG_INIT(PDDL_HPOT_OPT_STATE_TYPE), /* .cfg */ \
        NULL, /* .fdr_state */ \
    }
    

/**
 * Maximize the average h-value over all syntactic states
 */
struct pddl_hpot_config_opt_all_syntactic_states {
    _pddl_hpot_config_t cfg;
    /** Add constraint maximizing h-value for the initial state. default: false */
    int add_init_state_constr;
    /** If set to non-NULL, add the constraint maximizing h-value for the
     *  given state. default: NULL */
    const int *add_fdr_state_constr;
    /** Coefficient used for the added state constraint. default: 1 */
    double add_state_coef;
};
typedef struct pddl_hpot_config_opt_all_syntactic_states
    pddl_hpot_config_opt_all_syntactic_states_t;

#define PDDL_HPOT_CONFIG_OPT_ALL_SYNTACTIC_STATES_INIT \
    { \
        _PDDL_HPOT_CONFIG_INIT(PDDL_HPOT_OPT_ALL_SYNTACTIC_STATES_TYPE), /* .cfg */ \
        0, /* .add_init_state_constr */ \
        NULL, /* .add_fdr_state_constr */ \
        1., /* .add_state_coef */ \
    }

/**
 * Maximize the average h-value over reachable states estimated using
 * mutexes.
 */
struct pddl_hpot_config_opt_all_states_mutex {
    _pddl_hpot_config_t cfg;
    /** TODO */
    int mutex_size;
    /** Add constraint maximizing h-value for the initial state. default: false */
    int add_init_state_constr;
    /** If set to non-NULL, add the constraint maximizing h-value for the
     *  given state. default: NULL */
    const int *add_fdr_state_constr;
    /** Coefficient used for the added state constraint. default: 1 */
    double add_state_coef;
};
typedef struct pddl_hpot_config_opt_all_states_mutex
    pddl_hpot_config_opt_all_states_mutex_t;

#define PDDL_HPOT_CONFIG_OPT_ALL_STATES_MUTEX_INIT \
    { \
        _PDDL_HPOT_CONFIG_INIT(PDDL_HPOT_OPT_ALL_STATES_MUTEX_TYPE), /* .cfg */ \
        2, /* .mutex_size */ \
        0, /* .add_init_state_constr */ \
        NULL, /* .add_fdr_state_constr */ \
        1., /* .add_state_coef */ \
    }

/**
 * Maximize the average h-value over the sampled states.
 */
struct pddl_hpot_config_opt_sampled_states {
    _pddl_hpot_config_t cfg;
    /** Number of sampled states. default: 1000 */
    int num_samples;
    /** True if random walk should be used. default: true */
    int use_random_walk;
    /** Sample states by uniform sampling over syntactic states. default: false */
    int use_syntactic_samples;
    /** Sample (syntactic) states while removing mutex states */
    int use_mutex_samples;
    /** Add constraint maximizing h-value for the initial state. default: false */
    int add_init_state_constr;
    /** If set to non-NULL, add the constraint maximizing h-value for the
     *  given state. default: NULL */
    const int *add_fdr_state_constr;
    /** Coefficient used for the added state constraint. default: 1 */
    double add_state_coef;
};
typedef struct pddl_hpot_config_opt_sampled_states
    pddl_hpot_config_opt_sampled_states_t;

#define PDDL_HPOT_CONFIG_OPT_SAMPLED_STATES_INIT \
    { \
        _PDDL_HPOT_CONFIG_INIT(PDDL_HPOT_OPT_SAMPLED_STATES_TYPE), /* .cfg */ \
        1000, /* .num_samples */ \
        1, /* .use_random_walk */ \
        0, /* .use_syntactic_samples */ \
        0, /* .use_mutex_samples */ \
        0, /* .add_init_state_constr */ \
        NULL, /* .add_fdr_state_constr */ \
        1., /* .add_state_coef */ \
    }

/**
 * Ensemble each maximizing for a sampled state
 */
struct pddl_hpot_config_opt_ensemble_sampled_states {
    _pddl_hpot_config_t cfg;
    /** Number of sampled states. default: 1000 */
    int num_samples;
    /** True if random walk should be used. default: true */
    int use_random_walk;
    /** Sample states by uniform sampling over syntactic states. default: false */
    int use_syntactic_samples;
    /** Sample (syntactic) states while removing mutex states */
    int use_mutex_samples;
};
typedef struct pddl_hpot_config_opt_ensemble_sampled_states
    pddl_hpot_config_opt_ensemble_sampled_states_t;

#define PDDL_HPOT_CONFIG_OPT_ENSEMBLE_SAMPLED_STATES_INIT \
    { \
        _PDDL_HPOT_CONFIG_INIT(PDDL_HPOT_OPT_ENSEMBLE_SAMPLED_STATES_TYPE), /* .cfg */ \
        1000, /* .num_samples */ \
        1, /* .use_random_walk */ \
        0, /* .use_syntactic_samples */ \
        0, /* .mutex */ \
    }

/**
 * Ensemble constructed with the diversification algorithm
 */
struct pddl_hpot_config_opt_ensemble_diversification {
    _pddl_hpot_config_t cfg;
    /** Number of sampled states. default: 1000 */
    int num_samples;
    /** True if random walk should be used. default: true */
    int use_random_walk;
    /** Sample states by uniform sampling over syntactic states. default: false */
    int use_syntactic_samples;
    /** Sample (syntactic) states while removing mutex states. default: false */
    int use_mutex_samples;
};
typedef struct pddl_hpot_config_opt_ensemble_diversification
    pddl_hpot_config_opt_ensemble_diversification_t;

#define PDDL_HPOT_CONFIG_OPT_ENSEMBLE_DIVERSIFICATION_INIT \
    { \
        _PDDL_HPOT_CONFIG_INIT(PDDL_HPOT_OPT_ENSEMBLE_DIVERSIFICATION_TYPE), /* .cfg */ \
        1000, /* .num_samples */ \
        1, /* .use_random_walk */ \
        0, /* .use_syntactic_samples */ \
        0, /* .mutex */ \
    }

/**
 * Ensemble constructed with the diversification algorithm
 */
struct pddl_hpot_config_opt_ensemble_all_states_mutex {
    _pddl_hpot_config_t cfg;
    /** TODO */
    int cond_size;
    /** TODO */
    int mutex_size;
    /** Number of sampled states conditioned on random sets of facts.
     *  default: 0, i.e., disabled */
    int num_rand_samples;
};
typedef struct pddl_hpot_config_opt_ensemble_all_states_mutex
    pddl_hpot_config_opt_ensemble_all_states_mutex_t;

#define PDDL_HPOT_CONFIG_OPT_ENSEMBLE_ALL_STATES_MUTEX_INIT \
    { \
        _PDDL_HPOT_CONFIG_INIT(PDDL_HPOT_OPT_ENSEMBLE_ALL_STATES_MUTEX_TYPE), /* .cfg */ \
        1, /* .cond_size */ \
        2, /* .mutex_size */ \
        0, /* .num_rand_samples */ \
    }

struct pddl_hpot_config {
    /** Input FDR planning task */
    const pddl_fdr_t *fdr;
    /** Input MG-Strips representation of the corresponding .fdr task */
    const pddl_mg_strips_t *mg_strips;
    /** Input set of mutexes */
    const pddl_mutex_pairs_t *mutex;
    /** A list of configurations (see above) */
    _pddl_hpot_config_t *cfg[PDDL_HPOT_CONFIG_MAX_OPT_CONFIGS];
    /** Number of configs set in .cfg[] */
    int cfg_size;
    /** If true, disambiguation is used. default: true */
    int disambiguation;
    /** If true, weak disambiguation is used. default: false */
    int weak_disambiguation;
    /** Infer operator potentials. default: false */
    int op_pot;
    /** Infer real-valued operator potentials. default: false */
    int op_pot_real;
    /** Time limit for each round of LP solver. default: disabled */
    // TODO: Not implemented yet
    float time_limit;
};
typedef struct pddl_hpot_config pddl_hpot_config_t;

#define PDDL_HPOT_CONFIG_INIT { \
        NULL, /* .fdr */ \
        NULL, /* .mg_strips */ \
        NULL, /* .mutex */ \
        { 0 }, /* .cfg[] */ \
        0, /* .cfg_size */ \
        1, /* .disambiguation */ \
        0, /* .weak_disambiguation */ \
        0, /* .op_pot */ \
        0, /* .op_pot_real */ \
        -1., /* .time_limit */ \
    }

/**
 * Add potential heuritic configuration config_el to the main configuration
 * struct config.
 */
#define PDDL_HPOT_CONFIG_ADD(config, config_el) \
    do { \
        if ((config)->cfg_size >= PDDL_HPOT_CONFIG_MAX_OPT_CONFIGS){ \
            fprintf(stderr, "Fatal Error: hpot can have at most %d configs", \
                    PDDL_HPOT_CONFIG_MAX_OPT_CONFIGS); \
            exit(-1); \
        } \
        (config)->cfg[(config)->cfg_size++] = &(config_el)->cfg; \
    } while (0)

/**
 * Initialize hpot configuration
 */
void pddlHPotConfigInit(pddl_hpot_config_t *cfg);

/**
 * Initialize dst as a deep copy of src.
 */
void pddlHPotConfigInitCopy(pddl_hpot_config_t *dst,
                            const pddl_hpot_config_t *src);

/**
 * Free allocated memory. This can be called only if pddlHPotConfig*()
 * functions were used.
 */
void pddlHPotConfigFree(pddl_hpot_config_t *cfg);

/**
 * Setters for the hpot configuration
 */
void pddlHPotConfigSetDisambiguation(pddl_hpot_config_t *cfg);
void pddlHPotConfigSetWeakDisambiguation(pddl_hpot_config_t *cfg);
void pddlHPotConfigSetOpPot(pddl_hpot_config_t *cfg);
void pddlHPotConfigSetOpPotReal(pddl_hpot_config_t *cfg);

/**
 * Adds potential function configuration.
 */
void pddlHPotConfigAdd(pddl_hpot_config_t *cfg,
                       const _pddl_hpot_config_t *cfg_add);

/**
 * Replace inital state with the specified state in all configurations.
 */
void pddlHPotConfigReplaceInitStateWithState(pddl_hpot_config_t *cfg,
                                             const int *fdr_state);


/**
 * Log the given configuration
 */
void pddlHPotConfigLog(const pddl_hpot_config_t *cfg, pddl_err_t *err);

/**
 * Check the configuration.
 * Returns 0 if everything is ok.
 */
int pddlHPotConfigCheck(const pddl_hpot_config_t *cfg, pddl_err_t *err);

/**
 * Returns true if the configuration requires .mg_strips
 */
int pddlHPotConfigNeedMGStrips(const pddl_hpot_config_t *cfg);

/**
 * Returns true if the configuration requires .mutex
 */
int pddlHPotConfigNeedMutex(const pddl_hpot_config_t *cfg);

/**
 * Return true if there is no potential function added to the configuration.
 */
int pddlHPotConfigIsEmpty(const pddl_hpot_config_t *cfg);

/**
 * Returns true if the config produces an ensamble of potential
 * heuristics.
 */
int pddlHPotConfigIsEnsemble(const pddl_hpot_config_t *cfg);

/**
 * Compute potential functions corresponding to the provided configuration.
 */
int pddlHPot(pddl_pot_solutions_t *sols,
             const pddl_hpot_config_t *cfg,
             pddl_err_t *err);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_HPOT_H__ */
