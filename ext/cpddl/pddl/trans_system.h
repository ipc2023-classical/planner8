/***
 * cpddl
 * -------
 * Copyright (c)2020 Daniel Fiser <danfis@danfis.cz>,
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

#ifndef __PDDL_TRANS_SYSTEM_H__
#define __PDDL_TRANS_SYSTEM_H__

#include <pddl/mg_strips.h>
#include <pddl/cascading_table.h>
#include <pddl/transition.h>
#include <pddl/label.h>
#include <pddl/labeled_transition.h>
#include <pddl/trans_system_abstr_map.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct pddl_mgroup_idx_pair {
    int mg_id;
    int fact_idx;
};
typedef struct pddl_mgroup_idx_pair pddl_mgroup_idx_pair_t;

struct pddl_mgroup_idx_pairs {
    pddl_mgroup_idx_pair_t *mgroup_idx;
    int mgroup_idx_size;
    int mgroup_idx_alloc;
};
typedef struct pddl_mgroup_idx_pairs pddl_mgroup_idx_pairs_t;

void pddlMGroupIdxPairsFree(pddl_mgroup_idx_pairs_t *p);
void pddlMGroupIdxPairsAdd(pddl_mgroup_idx_pairs_t *p, int mg_id, int idx);

typedef struct pddl_trans_systems pddl_trans_systems_t;
struct pddl_trans_system {
    pddl_trans_systems_t *trans_systems;
    pddl_iset_t mgroup_ids; /*!< IDs of this TS's mutex groups */
    int num_states;
    pddl_cascading_table_t *repr; /*!< Representation of states using
                                       cascading tables */
    pddl_labeled_transitions_set_t trans; /*!< Labeled transitions */
    int init_state;
    pddl_iset_t goal_states;
    int dead_labels_collected;
};
typedef struct pddl_trans_system pddl_trans_system_t;

struct pddl_trans_systems {
    int fact_size;
    /** Mutex groups covering all facts */
    pddl_mgroups_t mgroup;
    /** Mapping from a fact to mgroup and its position with the mgroup */
    pddl_mgroup_idx_pairs_t *fact_to_mgroup;
    /** A list of labels corresponding to the input operators */
    pddl_labels_t label;
    /** A set of labels that are either unreachable or they lead to a
     *  dead-end state */
    pddl_iset_t dead_labels;
    /** List of transition systems */
    pddl_trans_system_t **ts;
    int ts_size;
    int ts_alloc;
};


/**
 * Initialize transition systems as a factored transition system of the
 * given MG-STRIPS where each transition system is a projection to a
 * mutex group.
 */
void pddlTransSystemsInit(pddl_trans_systems_t *tss,
                          const pddl_mg_strips_t *mg_strips,
                          const pddl_mutex_pairs_t *mutex);

/**
 * Free all allocated memory
 */
void pddlTransSystemsFree(pddl_trans_systems_t *tss);

/**
 * Clones the specified transition system and returns ID of the new
 * resulting transition system.
 */
int pddlTransSystemsCloneTransSystem(pddl_trans_systems_t *tss, int tid);

/**
 * Delete the specified transition system.
 */
void pddlTransSystemsDelTransSystem(pddl_trans_systems_t *tss, int tid);

/**
 * Remove deleted transition systems from tss->ts[] array -- may change IDs
 * of transition systems.
 */
void pddlTransSystemsCleanDeletedTransSystems(pddl_trans_systems_t *tss);

/**
 * Creates a new transition system that is a synchronized product of TSs t1
 * and t2 (indexes into tss->ts).
 * Returns index of the newly added transition system.
 */
int pddlTransSystemsMerge(pddl_trans_systems_t *tss, int t1, int t2);

/**
 * Applies abstraction mapping.
 */
void pddlTransSystemsAbstract(pddl_trans_systems_t *tss,
                              int ts_id,
                              const pddl_trans_system_abstr_map_t *map);

/**
 * Collect dead labels from the specified transition system -- all labels
 * that are not used in the specified transition system are added to
 * tss->dead_labels.
 * Returns 1 if any new dead labels were found, 0 otherwise.
 */
int pddlTransSystemsCollectDeadLabels(pddl_trans_systems_t *tss, int ts_id);
int pddlTransSystemsCollectDeadLabelsFromAll(pddl_trans_systems_t *tss);

/**
 * Remove dead labels from the specified transition system.
 */
void pddlTransSystemsRemoveDeadLabels(pddl_trans_systems_t *tss, int ts_id);
void pddlTransSystemsRemoveDeadLabelsFromAll(pddl_trans_systems_t *tss);

/**
 * Returns ID of the TS's state corresponding to strips_state.
 */
int pddlTransSystemsStripsState(const pddl_trans_systems_t *tss,
                                int ts_id,
                                const pddl_iset_t *strips_state);

void pddlTransSystemsPrintDebug1(const pddl_trans_systems_t *tss,
                                 const pddl_strips_t *strips,
                                 FILE *fout);
void pddlTransSystemPrintDebug1(const pddl_trans_systems_t *tss,
                                const pddl_strips_t *strips,
                                int ts_id,
                                FILE *fout);
void pddlTransSystemsPrintDebug2(const pddl_trans_systems_t *tss, FILE *fout);
void pddlTransSystemPrintDebug2(const pddl_trans_systems_t *tss,
                                int ts_id,
                                FILE *fout);
#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_TRANS_SYSTEM_H__ */
