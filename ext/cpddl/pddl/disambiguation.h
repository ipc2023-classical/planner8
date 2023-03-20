/***
 * cpddl
 * -------
 * Copyright (c)2016 Daniel Fiser <danfis@danfis.cz>,
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

/**
 * This module implementes disambiguation as described in:
 *
 * Alcázar, V., Borrajo, D., Fernández, S., & Fuentetaja, R. (2013).
 * Revisiting regression in planning. In Proceedings of the Twenty-Third
 * International Joint Conference on Artificial Intelligence (IJCAI), pp.
 * 2254–2260.
 */

#ifndef __PDDL_DISAMBIGUATION_H__
#define __PDDL_DISAMBIGUATION_H__

#include <pddl/strips.h>
#include <pddl/mutex_pair.h>
#include <pddl/mgroup.h>
#include <pddl/bitset.h>
#include <pddl/set.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct pddl_disambiguate_fact {
    pddl_bitset_t mgroup; /*!< mgroups contianing this fact */
    pddl_bitset_t not_mgroup; /*!< mgroups that do NOT contian this fact */
    pddl_bitset_t not_mutex_fact; /*!< facts that are NOT mutex with this fact*/
};
typedef struct pddl_disambiguate_fact pddl_disambiguate_fact_t;

struct pddl_disambiguate_mgroup {
    pddl_bitset_t fact; /*!< Facts contained in the mutex group */
    pddl_iset_t mgroup;
};
typedef struct pddl_disambiguate_mgroup pddl_disambiguate_mgroup_t;

struct pddl_disambiguate {
    pddl_disambiguate_fact_t *fact;
    int fact_size;
    pddl_disambiguate_mgroup_t *mgroup;
    int mgroup_size;
    pddl_bitset_t all_mgroups;
    pddl_bitset_t all_facts;

    /** Preallocated bitset for internal computation */
    pddl_bitset_t cur_mgroup;
    pddl_bitset_t cur_mgroup_it;
    pddl_bitset_t cur_allowed_facts;
    pddl_bitset_t cur_allowed_facts_from_mgroup;
    pddl_bitset_t tmp_fact_bitset;
    pddl_bitset_t tmp_mgroup_bitset;
};
typedef struct pddl_disambiguate pddl_disambiguate_t;


/**
 * Initialize disambiguation object.
 * Returns 0 on success, -1 if there are no exactly-one mutex groups in
 * which case dis is not properly intitialized.
 */
int pddlDisambiguateInit(pddl_disambiguate_t *dis,
                         int fact_size,
                         const pddl_mutex_pairs_t *mutex,
                         const pddl_mgroups_t *mgroup);

/**
 * Free allocated memory.
 */
void pddlDisambiguateFree(pddl_disambiguate_t *dis);

/**
 * Disambiguate the given {set} by the selected mutex groups.
 * If {mgroup_select} is non-NULL, only the mutex groups having non-empty
 * intersection with {mgroup_select} are selected.
 * If {only_disjunct_mgroups} is true, then only mutex groups with empty
 * intersection with {set} are considered.
 * selected.
 * If {single_fact_disamb} is set to true, then the fixpoint disambiguation
 * uses only the disambiguated sets of size at most one.
 * If {mgroup_select} is NULL, then all mutex groups having empty
 * intersection with {set} are selected.
 * If {disamb_sets} is non-NULL, it is filled with the subsets of the selected
 * mutex groups, that are not mutex with {set}.
 * {disamb_set} must be initialized with borHashSetInitISet().
 * If {can_extend_with} is non-NULL, then it is filled with the facts that can
 * extend {set}, because they are the only possible facts from the
 * corresponding selected mutex groups.
 * {cand_extend_with} and {set} may point to the same set.
 *
 * Return 0 if nothing was found,
 *        1 if a disambiguation happened,
 *        -1 if {set} was detected to be a mutex.
 */
int pddlDisambiguate(pddl_disambiguate_t *dis,
                     const pddl_iset_t *set,
                     const pddl_iset_t *mgroup_select,
                     int only_disjunct_mgroups,
                     int single_fact_disamb,
                     pddl_set_iset_t *disamb_sets,
                     pddl_iset_t *can_extend_with);

/**
 * Disambiguate a set of facts.
 * Return 0 if nothing was changed,
 *        1 if some facts were added
 *        -1 if the set was detected to be mutex
 */
_pddl_inline int pddlDisambiguateSet(pddl_disambiguate_t *dis, pddl_iset_t *set)
{
    return pddlDisambiguate(dis, set, NULL, 1, 0, NULL, set);
}

/**
 * Update structure with additional mutex {f1, f2}.
 */
void pddlDisambiguateAddMutex(pddl_disambiguate_t *dis, int f1, int f2);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_DISAMBIGUATION_H__ */
