/***
 * cpddl
 * -------
 * Copyright (c)2018 Daniel Fiser <danfis@danfis.cz>,
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

#include "internal.h"
#include "pddl/disambiguation.h"

static void selectExactlyOneMGroups(pddl_mgroups_t *mg,
                                    const pddl_mgroups_t *mgroup)
{
    for (int mi = 0; mi < mgroup->mgroup_size; ++mi){
        const pddl_mgroup_t *m = mgroup->mgroup + mi;
        // exactly-one mutex groups of size one is a simply a static fact
        // that is true in all states, so we can skip this one
        if (pddlISetSize(&m->mgroup) > 1
                && (m->is_exactly_one
                        || (m->is_fam_group && m->is_goal))){
            pddlMGroupsAdd(mg, &m->mgroup);
        }
    }
}

int pddlDisambiguateInit(pddl_disambiguate_t *dis,
                         int fact_size,
                         const pddl_mutex_pairs_t *mutex,
                         const pddl_mgroups_t *mgroup_in)
{
    pddl_mgroups_t mgroup;

    ZEROIZE(dis);

    pddlMGroupsInitEmpty(&mgroup);
    selectExactlyOneMGroups(&mgroup, mgroup_in);
    if (mgroup.mgroup_size == 0){
        pddlMGroupsFree(&mgroup);
        return -1;
    }

    dis->fact_size = fact_size;
    dis->mgroup_size = mgroup.mgroup_size;

    dis->fact = CALLOC_ARR(pddl_disambiguate_fact_t, dis->fact_size);
    for (int fact_id = 0; fact_id < fact_size; ++fact_id){
        pddl_disambiguate_fact_t *f = dis->fact + fact_id;
        pddlBitsetInit(&f->mgroup, dis->mgroup_size);
        pddlBitsetInit(&f->not_mgroup, dis->mgroup_size);
        pddlBitsetInit(&f->not_mutex_fact, dis->fact_size);
    }

    // Set .mgroup to mgroups the fact belongs
    dis->mgroup = CALLOC_ARR(pddl_disambiguate_mgroup_t, dis->mgroup_size);
    for (int mi = 0; mi < mgroup.mgroup_size; ++mi){
        const pddl_mgroup_t *mg = mgroup.mgroup + mi;
        pddl_disambiguate_mgroup_t *dm = dis->mgroup + mi;
        pddlISetUnion(&dm->mgroup, &mg->mgroup);
        pddlBitsetInit(&dm->fact, dis->fact_size);

        int fact_id;
        PDDL_ISET_FOR_EACH(&mg->mgroup, fact_id){
            pddlBitsetSetBit(&dm->fact, fact_id);
            pddlBitsetSetBit(&dis->fact[fact_id].mgroup, mi);
            pddlBitsetSetBit(&dis->fact[fact_id].not_mgroup, mi);
        }
    }

    pddlBitsetInit(&dis->all_facts, dis->fact_size);
    for (int fact_id = 0; fact_id < fact_size; ++fact_id)
        pddlBitsetSetBit(&dis->all_facts, fact_id);
    pddlBitsetInit(&dis->all_mgroups, dis->mgroup_size);
    for (int mi = 0; mi < dis->mgroup_size; ++mi)
        pddlBitsetSetBit(&dis->all_mgroups, mi);

    // Add mutexes from mutex groups
    for (int mi = 0; mi < mgroup_in->mgroup_size; ++mi){
        const pddl_mgroup_t *mg = mgroup_in->mgroup + mi;
        int mgroup_size = pddlISetSize(&mg->mgroup);
        for (int i = 0; i < mgroup_size; ++i){
            int fact1 = pddlISetGet(&mg->mgroup, i);
            for (int j = i + 1; j < mgroup_size; ++j){
                int fact2 = pddlISetGet(&mg->mgroup, j);
                pddlBitsetSetBit(&dis->fact[fact1].not_mutex_fact, fact2);
                pddlBitsetSetBit(&dis->fact[fact2].not_mutex_fact, fact1);
            }
        }
    }

    // Add mutexes from mutex structure
    PDDL_MUTEX_PAIRS_FOR_EACH(mutex, fact1, fact2){
        pddlBitsetSetBit(&dis->fact[fact1].not_mutex_fact, fact2);
        pddlBitsetSetBit(&dis->fact[fact2].not_mutex_fact, fact1);
    }

    // Negate .not_mgroup and .not_mutex_fact bitsets to get what we really want
    for (int fact_id = 0; fact_id < fact_size; ++fact_id){
        pddlBitsetNeg(&dis->fact[fact_id].not_mgroup);
        pddlBitsetNeg(&dis->fact[fact_id].not_mutex_fact);
    }

    pddlBitsetInit(&dis->cur_mgroup, dis->mgroup_size);
    pddlBitsetInit(&dis->cur_mgroup_it, dis->mgroup_size);
    pddlBitsetInit(&dis->cur_allowed_facts, dis->fact_size);
    pddlBitsetInit(&dis->cur_allowed_facts_from_mgroup, dis->fact_size);
    pddlBitsetInit(&dis->tmp_fact_bitset, dis->fact_size);
    pddlBitsetInit(&dis->tmp_mgroup_bitset, dis->mgroup_size);

    pddlMGroupsFree(&mgroup);
    return 0;
}

void pddlDisambiguateFree(pddl_disambiguate_t *dis)
{
    for (int i = 0; i < dis->fact_size; ++i){
        pddlBitsetFree(&dis->fact[i].mgroup);
        pddlBitsetFree(&dis->fact[i].not_mgroup);
        pddlBitsetFree(&dis->fact[i].not_mutex_fact);
    }
    if (dis->fact != NULL)
        FREE(dis->fact);

    for (int i = 0; i < dis->mgroup_size; ++i){
        pddlBitsetFree(&dis->mgroup[i].fact);
        pddlISetFree(&dis->mgroup[i].mgroup);
    }
    if (dis->mgroup != NULL)
        FREE(dis->mgroup);

    pddlBitsetFree(&dis->all_mgroups);
    pddlBitsetFree(&dis->all_facts);

    pddlBitsetFree(&dis->cur_mgroup);
    pddlBitsetFree(&dis->cur_mgroup_it);
    pddlBitsetFree(&dis->cur_allowed_facts);
    pddlBitsetFree(&dis->cur_allowed_facts_from_mgroup);
    pddlBitsetFree(&dis->tmp_fact_bitset);
    pddlBitsetFree(&dis->tmp_mgroup_bitset);
}

/** Initialize .cur_mgroup and .cur_allowed_facts */
static void disambInitCur(pddl_disambiguate_t *dis,
                          const pddl_iset_t *facts,
                          const pddl_iset_t *mgroup_select,
                          int only_disjunct_mgroups)
{
    pddlBitsetCopy(&dis->cur_mgroup, &dis->all_mgroups);
    pddlBitsetCopy(&dis->cur_allowed_facts, &dis->all_facts);

    int fact_id;
    PDDL_ISET_FOR_EACH(facts, fact_id){
        if (only_disjunct_mgroups)
            pddlBitsetAnd(&dis->cur_mgroup, &dis->fact[fact_id].not_mgroup);
        pddlBitsetAnd(&dis->cur_allowed_facts,
                      &dis->fact[fact_id].not_mutex_fact);
    }

    if (mgroup_select != NULL){
        pddlBitsetZeroize(&dis->tmp_mgroup_bitset);
        int fact_id;
        PDDL_ISET_FOR_EACH(mgroup_select, fact_id)
            pddlBitsetOr(&dis->tmp_mgroup_bitset, &dis->fact[fact_id].mgroup);
        pddlBitsetAnd(&dis->cur_mgroup, &dis->tmp_mgroup_bitset);
    }
}

static void updateCurAllowedFacts(pddl_disambiguate_t *dis,
                                  const pddl_iset_t *set)
{
    int set_size = pddlISetSize(set);
    if (set_size == 0)
        return;

    int fact_id = pddlISetGet(set, 0);
    pddlBitsetCopy(&dis->tmp_fact_bitset, &dis->fact[fact_id].not_mutex_fact);
    for (int i = 1; i < set_size; ++i){
        int fact_id = pddlISetGet(set, i);
        pddlBitsetOr(&dis->tmp_fact_bitset, &dis->fact[fact_id].not_mutex_fact);
    }
    pddlBitsetAnd(&dis->cur_allowed_facts, &dis->tmp_fact_bitset);
}

int pddlDisambiguate(pddl_disambiguate_t *dis,
                     const pddl_iset_t *set,
                     const pddl_iset_t *mgroup_select,
                     int only_disjunct_mgroups,
                     int single_fact_disamb,
                     pddl_set_iset_t *disamb_sets,
                     pddl_iset_t *exactly_one)
{
    if (dis->mgroup_size == 0)
        return 0;

    int change = 0;
    int mgroup_id;
    pddl_iset_t *disamb_set;

    disambInitCur(dis, set, mgroup_select, only_disjunct_mgroups);
    if (pddlBitsetCnt(&dis->cur_mgroup) == 0)
        return 0;

    disamb_set = CALLOC_ARR(pddl_iset_t, dis->mgroup_size);
    pddlBitsetCopy(&dis->cur_mgroup_it, &dis->cur_mgroup);
    pddlBitsetItStart(&dis->cur_mgroup_it);
    while ((mgroup_id = pddlBitsetItNext(&dis->cur_mgroup_it)) >= 0)
        pddlISetUnion(disamb_set + mgroup_id, &dis->mgroup[mgroup_id].mgroup);

    for (int local_change = 1; local_change;){
        local_change = 0;
        // Start iterating over mgroups that has empty intersection with
        // the disambiguated set.
        pddlBitsetCopy(&dis->cur_mgroup_it, &dis->cur_mgroup);
        pddlBitsetItStart(&dis->cur_mgroup_it);

        while ((mgroup_id = pddlBitsetItNext(&dis->cur_mgroup_it)) >= 0){
            pddlBitsetAnd2(&dis->cur_allowed_facts_from_mgroup,
                           &dis->mgroup[mgroup_id].fact,
                           &dis->cur_allowed_facts);
            int fact_size = pddlBitsetCnt(&dis->cur_allowed_facts_from_mgroup);

            // If there are no possible facts from the mutex group, the set
            // is mutex
            if (fact_size == 0){
                for (int i = 0; i < dis->mgroup_size; ++i)
                    pddlISetFree(disamb_set + i);
                FREE(disamb_set);
                return -1;
            }

            pddl_iset_t *disamb = disamb_set + mgroup_id;
            if (fact_size == pddlISetSize(disamb))
                continue;


            //fprintf(stderr, "%d --> %d / %d\n", mgroup_id, fact_size,
            //        pddlISetSize(&dis->mgroup[mgroup_id].mgroup));

            // Extract the disambiguated set
            pddlISetEmpty(disamb);
            pddlBitsetItStart(&dis->cur_allowed_facts_from_mgroup);
            int ext = pddlBitsetItNext(&dis->cur_allowed_facts_from_mgroup);
            while (ext >= 0){
                ASSERT(ext >= 0 && ext < dis->fact_size);
                pddlISetAdd(disamb, ext);
                ext = pddlBitsetItNext(&dis->cur_allowed_facts_from_mgroup);
            }

            // Update allowed facts to those that are not mutex with at
            // least one of the facts
            if (!single_fact_disamb || pddlISetSize(disamb) <= 1)
                updateCurAllowedFacts(dis, disamb);
            local_change = 1;
        }
    }

    if (single_fact_disamb){
        // If we want only single-fact disambiguations, reset the
        // disambiguations containing more than one fact to the full
        // original mutex group
        for (int mi = 0; mi < dis->mgroup_size; ++mi){
            if (pddlISetSize(disamb_set + mi) > 1)
                pddlISetUnion(disamb_set + mi, &dis->mgroup[mi].mgroup);
        }
    }

    for (int i = 0; i < dis->mgroup_size; ++i){
        int set_size = pddlISetSize(disamb_set + i);
        if (disamb_sets != NULL && set_size > 0){
            pddlSetISetAdd(disamb_sets, disamb_set + i);
            if (set_size != pddlISetSize(&dis->mgroup[i].mgroup))
                change = 1;
        }

        if (exactly_one != NULL && set_size == 1){
            pddlISetAdd(exactly_one, pddlISetGet(disamb_set + i, 0));
            change = 1;
        }

        pddlISetFree(disamb_set + i);
    }
    FREE(disamb_set);

    return change;
}

void pddlDisambiguateAddMutex(pddl_disambiguate_t *dis, int f1, int f2)
{
    pddlBitsetClearBit(&dis->fact[f1].not_mutex_fact, f2);
    pddlBitsetClearBit(&dis->fact[f2].not_mutex_fact, f1);
}
