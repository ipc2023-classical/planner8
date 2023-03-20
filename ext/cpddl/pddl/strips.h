/***
 * cpddl
 * -------
 * Copyright (c)2017 Daniel Fiser <danfis@danfis.cz>,
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

#ifndef __PDDL_STRIPS_H__
#define __PDDL_STRIPS_H__

#include <pddl/common.h>
#include <pddl/iset.h>
#include <pddl/strips_op.h>
#include <pddl/lifted_mgroup.h>
#include <pddl/mutex_pair.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct pddl_ground_config {
    const pddl_lifted_mgroups_t *lifted_mgroups;
    /** If .lifted_mgroups != NULL, use lifted mutex groups to prune
     *  operators that has mutex preconditions. */
    int prune_op_pre_mutex;
    /** If .lifted_mgroups != NULL, use lifted mutex groups to prune
     *  dead-end operators. */
    int prune_op_dead_end;
    /** If true static facts are found and removed */
    int remove_static_facts;
    /** Keep action arguments in strips operators */
    int keep_action_args;
    /** Keep all static facts including the ones created from static
     *  predicates */
    int keep_all_static_facts;
};
typedef struct pddl_ground_config pddl_ground_config_t;

#define PDDL_GROUND_CONFIG_INIT { \
        NULL, /* .lifted_mgroups */ \
        1, /* .prune_op_pre_mutex */ \
        1, /* .prune_op_dead_end */ \
        1, /* .remove_static_facts */ \
        0, /* .keep_action_args */ \
        0, /* .keep_all_static_facts */ \
    }

void pddlGroundConfigLog(const pddl_ground_config_t *cfg, pddl_err_t *err);

struct pddl_strips {
    pddl_ground_config_t cfg;
    char *domain_name;
    char *problem_name;
    char *domain_file;
    char *problem_file;
    pddl_facts_t fact; /*!< Set of facts */
    pddl_strips_ops_t op; /*!< Set of operators */
    pddl_iset_t init; /*!< Initial state */
    pddl_iset_t goal; /*!< Goal specification */
    int goal_is_unreachable; /*!< True if the goal is not reachable */
    int has_cond_eff; /*!< True if the problem contains operators with
                           conditinal effects. */
};

/**
 * Initialize empty STRIPS
 */
void pddlStripsInit(pddl_strips_t *strips);

/**
 * Free allocated memory.
 */
void pddlStripsFree(pddl_strips_t *strips);

/**
 * Copy the strips structure.
 */
void pddlStripsInitCopy(pddl_strips_t *dst, const pddl_strips_t *src);

/**
 * Make the STRIPS problem artificially unsolvable.
 */
void pddlStripsMakeUnsolvable(pddl_strips_t *strips);

/**
 * Compile away conditional effects.
 */
void pddlStripsCompileAwayCondEff(pddl_strips_t *strips);

/**
 * Writes IDs of operators to the corresponding fact elements.
 * fact_arr is a beggining of the array containing structures containing
 * pddl_iset_t elements where IDs are written.
 * el_size is a size of a single element in fact_arr.
 * pre_offset is an offset of the pddl_iset_t element where operators of
 * which the fact is a precondition should be written.
 * add_offset and del_offset are the same as pre_offset instead for add and
 * delete effects, respectivelly.
 * pre_offset, add_offset and del_offset may be set to -1 in which case
 * the cross referencing is disabled.
 */
void pddlStripsCrossRefFactsOps(const pddl_strips_t *strips,
                                void *fact_arr,
                                unsigned long el_size,
                                long pre_offset,
                                long add_offset,
                                long del_offset);

/**
 * Finds the set of the operators applicable in the given state.
 */
void pddlStripsApplicableOps(const pddl_strips_t *strips,
                             const pddl_iset_t *state,
                             pddl_iset_t *app_ops);


/**
 * Returns true if the given set of facts form a fact-alternating mutex
 * group.
 */
int pddlStripsIsFAMGroup(const pddl_strips_t *strips, const pddl_iset_t *facts);

/**
 * Returns true if the given set of facts is an exactly-one mutex group.
 */
int pddlStripsIsExactlyOneMGroup(const pddl_strips_t *strips,
                                 const pddl_iset_t *facts);

/**
 * Remove conditional effects by merging them into the operator if
 * possible.
 */
int pddlStripsMergeCondEffIfPossible(pddl_strips_t *strips);

/**
 * Delete the specified facts and operators.
 */
void pddlStripsReduce(pddl_strips_t *strips,
                      const pddl_iset_t *del_facts,
                      const pddl_iset_t *del_ops);

/**
 * Remove static facts, i.e., facts that are true in all reachable states.
 * Returns the number of removed facts.
 */
int pddlStripsRemoveStaticFacts(pddl_strips_t *strips, pddl_err_t *err);

/**
 * Remove delete effects that cannot be part of the state where the
 * operator is applied by:
 * 1) If the precondition contains a fact that is negation of the delete
 *    effect, then such a delete effect can be safely removed.
 * 2) If mutex is non-NULL and the delete effect is mutex with the
 *    precondition, then the delete effect can be safely removed.
 * Returns the number of modified operators and if changed_ops is non-NULL
 * also a list of changed operators.
 */
int pddlStripsRemoveUselessDelEffs(pddl_strips_t *strips,
                                   const pddl_mutex_pairs_t *mutex,
                                   pddl_iset_t *changed_ops,
                                   pddl_err_t *err);

/**
 * Use mutexes to find unreachable operators.
 */
int pddlStripsFindUnreachableOps(const pddl_strips_t *strips,
                                 const pddl_mutex_pairs_t *mutex,
                                 pddl_iset_t *unreachable_ops,
                                 pddl_err_t *err);

/**
 * Find operators with empty add effects.
 */
void pddlStripsFindOpsEmptyAddEff(const pddl_strips_t *strips, pddl_iset_t *ops);

/**
 * Print STRIPS problem in a format easily usable from python.
 */
void pddlStripsPrintPython(const pddl_strips_t *strips, FILE *fout);

/**
 * Prints STRIPS problem as PDDL domain.
 */
void pddlStripsPrintPDDLDomain(const pddl_strips_t *strips, FILE *fout);

/**
 * Prints STRIPS problem as PDDL problem.
 */
void pddlStripsPrintPDDLProblem(const pddl_strips_t *strips, FILE *fout);


void pddlStripsPrintDebug(const pddl_strips_t *strips, FILE *fout);

void pddlStripsLogInfo(const pddl_strips_t *strips, pddl_err_t *err);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_STRIPS_H__ */
