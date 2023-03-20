/***
 * Copyright (c)2022 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

#ifndef __PDDL_ASNETS_H__
#define __PDDL_ASNETS_H__

#include <pddl/iarr.h>
#include <pddl/asnets_task.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct pddl_asnets pddl_asnets_t;

enum pddl_asnets_trainer {
    PDDL_ASNETS_TRAINER_ASTAR_LMCUT = 0,
};
typedef enum pddl_asnets_trainer pddl_asnets_trainer_t;

struct pddl_asnets_config {
    /** Domain PDDL file. Set using *SetDomain() */
    char *domain_pddl;
    /** Number of input problem PDDL files (i.e., size of .problem_pddl[]) */
    int problem_pddl_size;
    /** Problem PDDL files. Set using *AddProblem() */
    char **problem_pddl;

    /** Output size of the hidden layers. Default: 16 */
    int hidden_dimension;
    /** Number of the layers. Default: 2 */
    int num_layers;

    /* Training parameters: */
    /** Fixed random seed. Default: 6961 */
    int random_seed;
    /** Weigth decay rate for regularization. Default: 2E-4 */
    float weight_decay;
    /** Dropout rate if set to >0. Default: 0.1 */
    float dropout_rate;
    /** Number of samples in a minibatch. Default: 64 */
    int batch_size;
    /** Double .batch_size every specified number of epochs. Default: 0 */
    int double_batch_size_every_epoch;
    /** Maximum number of epochs used for training. Default: 300 */
    int max_train_epochs;
    /** Number of train cycles within each epoch. Default: 700 */
    int train_steps;
    /** Limit on the number of steps for the policy rollout. Default: 1000 */
    int policy_rollout_limit;
    /** Time limit in seconds for the teacher to solve the given task.
     *  Default: 10.f */
    float teacher_timeout;
    /** Minimum success rate in .early_termination_epochs to terminate
     *  early. Default: 0.999 */
    float early_termination_success_rate;
    /** Number of epochs in which the success rate must be at higher than
     *  .early_termination_success_rate. Default: 20 */
    int early_termination_epochs;

    /** Which trainer will be used. One of PDDL_ASNETS_TRAINER_* */
    pddl_asnets_trainer_t trainer;
    /** If set to non-NULL, pddlASNetsTrain() saves a model to the path
     *  with this prefix every time it finds a model with improved success
     *  rate */
    const char *save_model_prefix;
};
typedef struct pddl_asnets_config pddl_asnets_config_t;

void pddlASNetsConfigLog(const pddl_asnets_config_t *cfg, pddl_err_t *err);
void pddlASNetsConfigInit(pddl_asnets_config_t *cfg);
void pddlASNetsConfigInitCopy(pddl_asnets_config_t *dst,
                              const pddl_asnets_config_t *src);
int pddlASNetsConfigInitFromFile(pddl_asnets_config_t *cfg,
                                 const char *filename,
                                 pddl_err_t *err);
void pddlASNetsConfigFree(pddl_asnets_config_t *cfg);

void pddlASNetsConfigSetDomain(pddl_asnets_config_t *cfg, const char *fn);
void pddlASNetsConfigAddProblem(pddl_asnets_config_t *cfg,
                                const char *problem_fn);
void pddlASNetsConfigWrite(const pddl_asnets_config_t *cfg, FILE *fout);



/**
 * Creates a new instance of ASNets according to the configuration
 */
pddl_asnets_t *pddlASNetsNew(const pddl_asnets_config_t *cfg, pddl_err_t *err);

/**
 * Free allocated memory.
 */
void pddlASNetsDel(pddl_asnets_t *a);

/**
 * Save ASNets model into the given file.
 */
int pddlASNetsSave(const pddl_asnets_t *a, const char *fn, pddl_err_t *err);

/**
 * Load ASNets from the given file.
 */
int pddlASNetsLoad(pddl_asnets_t *a, const char *fn, pddl_err_t *err);

/**
 * Load model information from the given file and print it out.
 */
int pddlASNetsPrintModelInfo(const char *fn, pddl_err_t *err);

/**
 * Returns number of ground tasks stored in the given object.
 */
int pddlASNetsNumGroundTasks(const pddl_asnets_t *a);

/**
 * Returns ASNets task with the given ID
 */
const pddl_asnets_ground_task_t *
pddlASNetsGetGroundTask(const pddl_asnets_t *a, int id);

/**
 * Run policy on the given state from the given task.
 * If {out_state} is non-NULL, it is filled with the resulting state.
 * Returns ID of the selected operator, or -1 if no operator is applicable.
 */
int pddlASNetsRunPolicy(pddl_asnets_t *a,
                        const pddl_asnets_ground_task_t *task,
                        const int *in_state,
                        int *out_state);

/**
 * Try to solve the task using the ASNets policy.
 * {trace} is filled with the policy trace.
 * Return true if a plan was found, and false otherwise.
 */
int pddlASNetsSolveTask(pddl_asnets_t *a,
                        const pddl_asnets_ground_task_t *task,
                        pddl_iarr_t *trace,
                        pddl_err_t *err);

/**
 * Train ASNets according to the configuration it was created with.
 */
int pddlASNetsTrain(pddl_asnets_t *a, pddl_err_t *err);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __PDDL_ASNETS_H__ */
