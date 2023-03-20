/***
 * cpddl
 * -------
 * Copyright (c)2019 Daniel Fiser <danfis@danfis.cz>,
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

#ifndef __PDDL_MGROUP_H__
#define __PDDL_MGROUP_H__

#include <pddl/iset.h>
#include <pddl/lifted_mgroup.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct pddl_fdr_vars;

struct pddl_mgroup {
    pddl_iset_t mgroup; /*!< Set of facts forming the mutex group */
    int lifted_mgroup_id; /*!< ID refering to the corresponding lifted
                               mutex group in pddl_mgroups_t or
                               -1 if there is none */
    int is_exactly_one; /*!< True if the mutex groups is "exactly-one" */
    int is_fam_group; /*!< True if it is fam-group */
    int is_goal; /*!< Has non-empty intersection with the goal */
};
typedef struct pddl_mgroup pddl_mgroup_t;

struct pddl_mgroups {
    pddl_lifted_mgroups_t lifted_mgroup;
    pddl_mgroup_t *mgroup;
    int mgroup_size;
    int mgroup_alloc;
};
typedef struct pddl_mgroups pddl_mgroups_t;

/**
 * Initialize an empty set of mutex groups.
 */
void pddlMGroupsInitEmpty(pddl_mgroups_t *mg);

/**
 * Initialize dst as a copy of src.
 */
void pddlMGroupsInitCopy(pddl_mgroups_t *dst, const pddl_mgroups_t *src);

/**
 * Ground lifted mutex groups using reachable facts.
 */
void pddlMGroupsGround(pddl_mgroups_t *mg,
                       const pddl_t *pddl,
                       const pddl_lifted_mgroups_t *lifted_mg,
                       const pddl_strips_t *strips);

/**
 * Free allocated memory.
 */
void pddlMGroupsFree(pddl_mgroups_t *mg);

/**
 * Adds a new mutex group consisting of the given set of facts.
 */
pddl_mgroup_t *pddlMGroupsAdd(pddl_mgroups_t *mg, const pddl_iset_t *fact);

/**
 * Sorts mutex groups and removes duplicates.
 */
void pddlMGroupsSortUniq(pddl_mgroups_t *mg);

/**
 * Sort mutex groups by their size in descending order.
 */
void pddlMGroupsSortBySizeDesc(pddl_mgroups_t *mg);

/**
 * Sets .is_exactly_one flags for "exactly-one" mutex groups.
 * Returns the number of exactly-one mutex groups found.
 */
int pddlMGroupsSetExactlyOne(pddl_mgroups_t *mgs, const pddl_strips_t *strips);

/**
 * Sets .is_goal flags for mutex groups having non-empty intersection with
 * the goal. Returns the number of "goal" mutex groups found.
 */
int pddlMGroupsSetGoal(pddl_mgroups_t *mgs, const pddl_strips_t *strips);

/**
 * Adds to {set} all facts from all exactly-one mutex groups.
 */
void pddlMGroupsGatherExactlyOneFacts(const pddl_mgroups_t *mgs,
                                      pddl_iset_t *set);

/**
 * Remove the specified facts and remap the rest to the new IDs.
 * Note that the flags are not reset to 0.
 */
void pddlMGroupsReduce(pddl_mgroups_t *mgs, const pddl_iset_t *rm_facts);

/**
 * Remove specified facts from all mutex groups.
 * Note that all flags are kept untouched.
 */
void pddlMGroupsRemoveSet(pddl_mgroups_t *mgs, const pddl_iset_t *rm);

/**
 * Removes all mutex groups containing at most size facts.
 */
void pddlMGroupsRemoveSmall(pddl_mgroups_t *mgs, int size);

/**
 * Removes empty mutex groups
 */
void pddlMGroupsRemoveEmpty(pddl_mgroups_t *mgs);

/**
 * Remove mgroups that are subset of some other mgroup in the set.
 */
void pddlMGroupsRemoveSubsets(pddl_mgroups_t *mgs);

/**
 * Add FDR variables as mutex groups.
 */
void pddlMGroupsAddFDRVars(pddl_mgroups_t *mgs,
                           const struct pddl_fdr_vars *vars);

/**
 * Returns mutex group cover number, i.e., minimal number of mutex groups
 * needed to cover all facts.
 */
int pddlMGroupsCoverNumber(const pddl_mgroups_t *mgs, int fact_size);

/**
 * Returns the number of exactly-one mutex groups.
 */
int pddlMGroupsNumExactlyOne(const pddl_mgroups_t *mgs);

/**
 * Find facts that are part of only one mutex group.
 */
void pddlMGroupsEssentialFacts(const pddl_mgroups_t *mgroup, pddl_iset_t *ess);

/**
 * Fill cover_set with mutex groups from mgs such that all facts from mgs
 * are covered and no mutex groups overlap.
 * Largest mutex groups are prioritized.
 */
void pddlMGroupsExtractCoverLargest(const pddl_mgroups_t *mgs,
                                    pddl_mgroups_t *cover_set);

/**
 * Fill cover_set with mutex groups from mgs such that all facts from mgs
 * are covered and no mutex groups overlap.
 * Essential facts are prioritized.
 */
void pddlMGroupsExtractCoverEssential(const pddl_mgroups_t *mgs,
                                      pddl_mgroups_t *cover_set);
/**
 * Returns number of facts covered by the mutex groups.
 */
int pddlMGroupsNumCoveredFacts(const pddl_mgroups_t *mgs);

/**
 * Split the mutex groups by intersection with the given set of facts:
 * for every mgroup M in src:
 *     add M \cap fset to dst if non-empty
 *     add M \setminus fset to dst if non-empty
 */
void pddlMGroupsSplitByIntersection(pddl_mgroups_t *dst,
                                    const pddl_mgroups_t *src,
                                    const pddl_iset_t *fset);

/**
 * Debug print out
 */
void pddlMGroupsPrint(const pddl_t *pddl,
                      const pddl_strips_t *strips,
                      const pddl_mgroups_t *mg,
                      FILE *fout);
void pddlMGroupPrint(const pddl_t *pddl,
                     const pddl_strips_t *strips,
                     const pddl_mgroup_t *mg,
                     FILE *fout);
void pddlMGroupsPrintTable(const pddl_t *pddl,
                           const pddl_strips_t *strips,
                           const pddl_mgroups_t *mg,
                           FILE *fout,
                           pddl_err_t *err);
#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_MGROUP_H__ */
