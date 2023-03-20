/***
 * Copyright (c)2022 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

#ifndef __PDDL_ASNETS_TRAIN_DATA_H__
#define __PDDL_ASNETS_TRAIN_DATA_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <pddl/htable.h>
#include <pddl/plan.h>
#include <pddl/fdr.h>
#include <pddl/heur.h>

typedef struct pddl_asnets_train_data_sample pddl_asnets_train_data_sample_t;

struct pddl_asnets_train_data {
    pddl_asnets_train_data_sample_t **sample;
    int sample_size;
    int sample_alloc;

    pddl_htable_t *htable;
    pddl_htable_t *fail_cache;
};
typedef struct pddl_asnets_train_data pddl_asnets_train_data_t;

void pddlASNetsTrainDataInit(pddl_asnets_train_data_t *td);
void pddlASNetsTrainDataFree(pddl_asnets_train_data_t *td);

int pddlASNetsTrainDataGetSample(const pddl_asnets_train_data_t *td,
                                 int sample_id,
                                 int *ground_task_id,
                                 int *selected_op_id,
                                 int *fdr_state_size,
                                 const int **fdr_state);

void pddlASNetsTrainDataAdd(pddl_asnets_train_data_t *td,
                            int ground_task_id,
                            const int *state,
                            int state_size,
                            int selected_op_id);

void pddlASNetsTrainDataAddPlan(pddl_asnets_train_data_t *td,
                                int ground_task_id,
                                int state_size,
                                const int *init_state,
                                const pddl_fdr_ops_t *ops,
                                const pddl_iarr_t *plan);

void pddlASNetsTrainDataShuffle(pddl_asnets_train_data_t *td);

int pddlASNetsTrainDataRolloutAStar(pddl_asnets_train_data_t *td,
                                    int ground_task_id,
                                    const int *state,
                                    const pddl_fdr_t *fdr,
                                    const pddl_heur_config_t *cfg,
                                    float max_time,
                                    pddl_err_t *err);

int pddlASNetsTrainDataRolloutAStarLMCut(pddl_asnets_train_data_t *td,
                                         int ground_task_id,
                                         const int *state,
                                         const pddl_fdr_t *fdr,
                                         float max_time,
                                         pddl_err_t *err);
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __PDDL_ASNETS_TRAIN_DATA_H__ */
