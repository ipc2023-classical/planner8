/***
 * Copyright (c)2022 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

#ifndef __PDDL_ASNETS_TASK_H__
#define __PDDL_ASNETS_TASK_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <pddl/pddl_struct.h>
#include <pddl/strips.h>
#include <pddl/fdr.h>
#include <pddl/fdr_app_op.h>
#include <pddl/iarr.h>

struct pddl_asnets_action {
    int action_id;
    /** Array of related atoms, index is a position */
    const pddl_fm_atom_t **related_atom;
    int related_atom_size;
    int related_atom_alloc;
};
typedef struct pddl_asnets_action pddl_asnets_action_t;

struct pddl_asnets_action_pos {
    int action_id;
    int pos;
};
typedef struct pddl_asnets_action_pos pddl_asnets_action_pos_t;

struct pddl_asnets_pred {
    int pred_id;
    /** List of related actions and corresponding positions */
    pddl_asnets_action_pos_t *related_action;
    int related_action_size;
    int related_action_alloc;
};
typedef struct pddl_asnets_pred pddl_asnets_pred_t;

struct pddl_asnets_lifted_task {
    pddl_t pddl;
    pddl_asnets_action_t *action;
    int action_size;
    pddl_asnets_pred_t *pred;
    int pred_size;
};
typedef struct pddl_asnets_lifted_task pddl_asnets_lifted_task_t;

struct pddl_asnets_op {
    int op_id;
    const pddl_asnets_action_t *action;
    /** Array of related facts, index is the position */
    int *related_fact;
    int related_fact_size;
};
typedef struct pddl_asnets_op pddl_asnets_op_t;

struct pddl_asnets_fact {
    int fact_id;
    const pddl_asnets_pred_t *pred;
    /** Array of related operators, index is a position */
    pddl_iarr_t *related_op;
    int related_op_size;
};
typedef struct pddl_asnets_fact pddl_asnets_fact_t;

struct pddl_asnets_ground_task {
    pddl_t pddl;
    pddl_strips_t strips;
    pddl_fdr_t fdr;
    pddl_fdr_app_op_t fdr_app_op;
    pddl_iset_t static_fact;

    const pddl_asnets_lifted_task_t *lifted_task;
    pddl_asnets_op_t *op;
    int op_size;
    pddl_asnets_fact_t *fact;
    int fact_size;
};
typedef struct pddl_asnets_ground_task pddl_asnets_ground_task_t;

int pddlASNetsLiftedTaskInit(pddl_asnets_lifted_task_t *lt,
                             const char *domain_fn,
                             pddl_err_t *err);
void pddlASNetsLiftedTaskFree(pddl_asnets_lifted_task_t *lt);
void pddlASNetsLiftedTaskToSHA256(const pddl_asnets_lifted_task_t *lt,
                                  char *hash_str);

int pddlASNetsGroundTaskInit(pddl_asnets_ground_task_t *gt,
                             const pddl_asnets_lifted_task_t *lt,
                             const char *domain_fn,
                             const char *problem_fn,
                             pddl_err_t *err);
void pddlASNetsGroundTaskFree(pddl_asnets_ground_task_t *lt);

void pddlASNetsGroundTaskFDRStateToStrips(const pddl_asnets_ground_task_t *gt,
                                          const int *fdr_state,
                                          pddl_iset_t *strips_state);

void pddlASNetsGroundTaskFDRApplicableOps(const pddl_asnets_ground_task_t *gt,
                                          const int *fdr_state,
                                          pddl_iset_t *ops);

void pddlASNetsGroundTaskFDRGoal(const pddl_asnets_ground_task_t *gt,
                                 pddl_iset_t *strips_goal);

void pddlASNetsGroundTaskFDRApplyOp(const pddl_asnets_ground_task_t *gt,
                                    const int *state,
                                    int op_id,
                                    int *out_state);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __PDDL_ASNETS_TASK_H__ */
