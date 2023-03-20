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

#include "pddl/symbolic_constr.h"
#include "internal.h"

static void separateFwBwMutex(const pddl_mutex_pairs_t *mutex,
                              pddl_mutex_pairs_t *fw_mutex,
                              pddl_mutex_pairs_t *bw_mutex)
{
    PDDL_MUTEX_PAIRS_FOR_EACH(mutex, f1, f2){
        if (pddlMutexPairsIsMutex(mutex, f1, f2)){
            if (pddlMutexPairsIsBwMutex(mutex, f1, f2)){
                pddlMutexPairsAdd(bw_mutex, f1, f2);
            }else if (pddlMutexPairsIsFwMutex(mutex, f1, f2)){
                pddlMutexPairsAdd(fw_mutex, f1, f2);
            }else{
                pddlMutexPairsAdd(bw_mutex, f1, f2);
                pddlMutexPairsAdd(fw_mutex, f1, f2);
            }
        }
    }
}

static void bddsAddMutex(pddl_symbolic_vars_t *vars,
                         pddl_bdds_t *bdds,
                         int fact1,
                         int fact2)
{
    pddl_bdd_t *mutex;
    mutex = pddlSymbolicVarsCreateMutexPre(vars, fact1, fact2);
    pddlBDDsAdd(vars->mgr, bdds, mutex);
    pddlBDDDel(vars->mgr, mutex);
}

static void bddsAddExactlyOneMGroup(pddl_symbolic_vars_t *vars,
                                    pddl_bdds_t *bdds,
                                    const pddl_iset_t *mg)
{
    pddl_bdd_t *bdd;
    bdd = pddlSymbolicVarsCreateExactlyOneMGroupPre(vars, mg);
    pddlBDDsAdd(vars->mgr, bdds, bdd);
    pddlBDDDel(vars->mgr, bdd);
}

static int constrConstructMutex(pddl_symbolic_vars_t *vars,
                                pddl_bdds_t *bdds,
                                const pddl_mutex_pairs_t *mutex,
                                int max_nodes,
                                float max_time)
{
    int num_mutexes = 0;
    for (int f1 = 0; f1 < vars->fact_size; ++f1){
        int fact1 = vars->ordered_facts[f1];
        for (int f2 = f1 + 1; f2 < vars->fact_size; ++f2){
            int fact2 = vars->ordered_facts[f2];
            if (pddlMutexPairsIsMutex(mutex, fact1, fact2)){
                bddsAddMutex(vars, bdds, fact1, fact2);
                ++num_mutexes;
            }
        }
    }

    pddlBDDsMergeAnd(vars->mgr, bdds, max_nodes, max_time);
    return num_mutexes;
}

static int constrConstructFwMGroup(pddl_symbolic_vars_t *vars,
                                   pddl_bdds_t *bdds,
                                   const pddl_mgroups_t *mgroup,
                                   int max_nodes,
                                   float max_time)
{
    int num_mgroups = 0;
    for (int mgi = 0; mgi < mgroup->mgroup_size; ++mgi){
        const pddl_mgroup_t *mg = mgroup->mgroup + mgi;
        if (mg->is_fam_group && mg->is_goal && !mg->is_exactly_one){
            bddsAddExactlyOneMGroup(vars, bdds, &mg->mgroup);
            ++num_mgroups;
        }
    }

    pddlBDDsMergeAnd(vars->mgr, bdds, max_nodes, max_time);
    return num_mgroups;
}

static int constrConstructBwMGroup(pddl_symbolic_vars_t *vars,
                                   pddl_bdds_t *bdds,
                                   const pddl_mgroups_t *mgroup,
                                   int max_nodes,
                                   float max_time)
{
    int num_mgroups = 0;
    for (int mgi = 0; mgi < mgroup->mgroup_size; ++mgi){
        const pddl_mgroup_t *mg = mgroup->mgroup + mgi;
        if (mg->is_exactly_one){
            bddsAddExactlyOneMGroup(vars, bdds, &mg->mgroup);
            ++num_mgroups;
        }
    }

    pddlBDDsMergeAnd(vars->mgr, bdds, max_nodes, max_time);
    return num_mgroups;
}

static pddl_bdd_t *constructGroupMutex(pddl_symbolic_constr_t *constr,
                                       int group_id)
{
    pddl_bdd_t *bdd = pddlBDDOne(constr->vars->mgr);
    int fid;
    PDDL_ISET_FOR_EACH(&constr->vars->group[group_id].fact, fid){
        int fact_id2;
        PDDL_ISET_FOR_EACH(constr->fact_mutex_bw + fid, fact_id2){
            pddl_bdd_t *mutex;
            mutex = pddlSymbolicVarsCreateMutexPre(constr->vars, fid, fact_id2);
            pddlBDDAndUpdate(constr->vars->mgr, &bdd, mutex);
            pddlBDDDel(constr->vars->mgr, mutex);
        }
    }
    return bdd;
}

static pddl_bdd_t *constructGroupMGroup(pddl_symbolic_constr_t *constr,
                                        int group_id)
{
    pddl_bdd_t *bdd = pddlBDDOne(constr->vars->mgr);
    PDDL_ISET(mgroups);
    PDDL_ISET(mgroups_bw);
    int fid;
    PDDL_ISET_FOR_EACH(&constr->vars->group[group_id].fact, fid){
        for (int mgi = 0; mgi < constr->mgroup.mgroup_size; ++mgi){
            const pddl_mgroup_t *mg = constr->mgroup.mgroup + mgi;
            if (!pddlISetIn(fid, &mg->mgroup))
                continue;
            if (mg->is_exactly_one)
                pddlISetAdd(&mgroups, mgi);
            if (mg->is_fam_group && mg->is_goal && !mg->is_exactly_one)
                pddlISetAdd(&mgroups_bw, mgi);
        }
    }

    int mgi;
    PDDL_ISET_FOR_EACH(&mgroups, mgi){
        const pddl_mgroup_t *mg = constr->mgroup.mgroup + mgi;
        pddl_bdd_t *mgbdd;
        mgbdd = pddlSymbolicVarsCreateExactlyOneMGroupPre(constr->vars,
                                                          &mg->mgroup);
        pddlBDDAndUpdate(constr->vars->mgr, &bdd, mgbdd);
        pddlBDDDel(constr->vars->mgr, mgbdd);
    }
    PDDL_ISET_FOR_EACH(&mgroups_bw, mgi){
        const pddl_mgroup_t *mg = constr->mgroup.mgroup + mgi;
        pddl_bdd_t *mgbdd;
        mgbdd = pddlSymbolicVarsCreateExactlyOneMGroupEff(constr->vars,
                                                          &mg->mgroup);
        pddlBDDAndUpdate(constr->vars->mgr, &bdd, mgbdd);
        pddlBDDDel(constr->vars->mgr, mgbdd);
    }
    pddlISetFree(&mgroups);
    pddlISetFree(&mgroups_bw);
    return bdd;
}

void pddlSymbolicConstrInit(pddl_symbolic_constr_t *constr,
                            pddl_symbolic_vars_t *vars,
                            const pddl_mutex_pairs_t *mutex,
                            const pddl_mgroups_t *mgroup,
                            int max_nodes,
                            float max_time,
                            pddl_err_t *err)
{
    PDDL_INFO(err, "Constructing constraint BDDs ...");

    constr->vars = vars;

    pddlMGroupsInitCopy(&constr->mgroup, mgroup);
    constr->fact_mutex = CALLOC_ARR(pddl_iset_t, vars->fact_size);
    constr->fact_mutex_fw = CALLOC_ARR(pddl_iset_t, vars->fact_size);
    constr->fact_mutex_bw = CALLOC_ARR(pddl_iset_t, vars->fact_size);
    PDDL_MUTEX_PAIRS_FOR_EACH(mutex, f1, f2){
        pddlISetAdd(constr->fact_mutex + f1, f2);
        pddlISetAdd(constr->fact_mutex + f2, f1);
        if (pddlMutexPairsIsFwMutex(mutex, f1, f2)){
            pddlISetAdd(constr->fact_mutex_fw + f1, f2);
            pddlISetAdd(constr->fact_mutex_fw + f2, f1);
        }
        if (pddlMutexPairsIsBwMutex(mutex, f1, f2)){
            pddlISetAdd(constr->fact_mutex_bw + f1, f2);
            pddlISetAdd(constr->fact_mutex_bw + f2, f1);
        }
        if (!pddlMutexPairsIsFwMutex(mutex, f1, f2)
                && !pddlMutexPairsIsBwMutex(mutex, f1, f2)){
            pddlISetAdd(constr->fact_mutex_fw + f1, f2);
            pddlISetAdd(constr->fact_mutex_fw + f2, f1);
            pddlISetAdd(constr->fact_mutex_bw + f1, f2);
            pddlISetAdd(constr->fact_mutex_bw + f2, f1);
        }
    }
    PDDL_INFO(err, "Mutex maps created.");

    if (pddlDisambiguateInit(&constr->disambiguate, vars->fact_size,
                             mutex, mgroup) != 0){
        PANIC("Disambiguation failed because there are"
                    " no exactly-1 mutex groups");
    }
    PDDL_INFO(err, "Disambiguation created.");

    pddlBDDsInit(&constr->fw_mutex);
    pddlBDDsInit(&constr->fw_mgroup);
    pddlBDDsInit(&constr->bw_mutex);
    pddlBDDsInit(&constr->bw_mgroup);

    pddl_mutex_pairs_t fw_mutex;
    pddl_mutex_pairs_t bw_mutex;
    pddlMutexPairsInit(&fw_mutex, vars->fact_size);
    pddlMutexPairsInit(&bw_mutex, vars->fact_size);
    separateFwBwMutex(mutex, &fw_mutex, &bw_mutex);

    PDDL_INFO(err, "Mutexes separated: fw-mutex pairs: %d, bw-mutex pairs: %d",
              fw_mutex.num_mutex_pairs,
              bw_mutex.num_mutex_pairs);

    if (bw_mutex.num_mutex_pairs > 0){
        int num = constrConstructMutex(vars, &constr->fw_mutex, &bw_mutex,
                                       max_nodes, max_time);
        PDDL_INFO(err, "Created %d fw-mutex BDDs from %d mutexes. nodes: %lu",
                  constr->fw_mutex.bdd_size, num,
                  pddlBDDsSize(&constr->fw_mutex));
    }

    if (fw_mutex.num_mutex_pairs > 0){
        int num = constrConstructMutex(vars, &constr->bw_mutex, &fw_mutex,
                                       max_nodes, max_time);
        PDDL_INFO(err, "Created %d bw-mutex BDDs from %d mutexes nodes: %lu",
                  constr->bw_mutex.bdd_size, num,
                  pddlBDDsSize(&constr->bw_mutex));
    }

    if (mgroup != NULL){
        int num_fw = constrConstructFwMGroup(vars, &constr->fw_mgroup, mgroup,
                                             max_nodes, max_time);
        PDDL_INFO(err, "Created %d fw-mgroup BDDs from %d mgroups nodes: %lu",
                  constr->fw_mgroup.bdd_size, num_fw,
                  pddlBDDsSize(&constr->fw_mgroup));

        int num_bw = constrConstructBwMGroup(vars, &constr->bw_mgroup, mgroup,
                                             max_nodes, max_time);
        PDDL_INFO(err, "Created %d bw-mgroup BDDs from %d mgroups nodes: %lu",
                  constr->bw_mgroup.bdd_size, num_bw,
                  pddlBDDsSize(&constr->bw_mgroup));
    }

    pddlMutexPairsFree(&fw_mutex);
    pddlMutexPairsFree(&bw_mutex);

    constr->group_mutex = CALLOC_ARR(pddl_bdd_t *, vars->group_size);
    constr->group_mgroup = CALLOC_ARR(pddl_bdd_t *, vars->group_size);
    for (int i = 0; i < constr->vars->group_size; ++i){
        constr->group_mutex[i] = constructGroupMutex(constr, i);
        constr->group_mgroup[i] = constructGroupMGroup(constr, i);
    }
}

void pddlSymbolicConstrFree(pddl_symbolic_constr_t *constr)
{
    for (int i = 0; i < constr->vars->group_size; ++i){
        pddlBDDDel(constr->vars->mgr, constr->group_mutex[i]);
        pddlBDDDel(constr->vars->mgr, constr->group_mgroup[i]);
    }
    FREE(constr->group_mutex);
    FREE(constr->group_mgroup);

    pddlMGroupsFree(&constr->mgroup);

    for (int i = 0; i < constr->vars->fact_size; ++i){
        pddlISetFree(constr->fact_mutex + i);
        pddlISetFree(constr->fact_mutex_fw + i);
        pddlISetFree(constr->fact_mutex_bw + i);
    }
    FREE(constr->fact_mutex);
    FREE(constr->fact_mutex_fw);
    FREE(constr->fact_mutex_bw);

    pddlBDDsFree(constr->vars->mgr, &constr->fw_mutex);
    pddlBDDsFree(constr->vars->mgr, &constr->fw_mgroup);
    pddlBDDsFree(constr->vars->mgr, &constr->bw_mutex);
    pddlBDDsFree(constr->vars->mgr, &constr->bw_mgroup);

    pddlDisambiguateFree(&constr->disambiguate);
}

void pddlSymbolicConstrApplyFw(pddl_symbolic_constr_t *constr, pddl_bdd_t **bdd)
{
    pddlBDDsAndUpdate(constr->vars->mgr, &constr->fw_mutex, bdd);
    pddlBDDsAndUpdate(constr->vars->mgr, &constr->fw_mgroup, bdd);
}

void pddlSymbolicConstrApplyBw(pddl_symbolic_constr_t *constr, pddl_bdd_t **bdd)
{
    pddlBDDsAndUpdate(constr->vars->mgr, &constr->bw_mutex, bdd);
    pddlBDDsAndUpdate(constr->vars->mgr, &constr->bw_mgroup, bdd);
}

int pddlSymbolicConstrApplyBwLimit(pddl_symbolic_constr_t *constr,
                                   pddl_bdd_t **out,
                                   float max_time)
{
    pddl_time_limit_t time_limit;
    pddlTimeLimitInit(&time_limit);
    if (max_time > 0.)
        pddlTimeLimitSet(&time_limit, max_time);

    pddl_bdd_t *bdd = pddlBDDClone(constr->vars->mgr, *out);

    for (int i = 0; i < constr->bw_mutex.bdd_size; ++i){
        pddl_bdd_t *res;
        res = pddlBDDAndLimit(constr->vars->mgr, bdd, constr->bw_mutex.bdd[i],
                              0, &time_limit);
        if (res == NULL){
            pddlBDDDel(constr->vars->mgr, bdd);
            return -1;
        }
        pddlBDDDel(constr->vars->mgr, bdd);
        bdd = res;
    }

    for (int i = 0; i < constr->bw_mgroup.bdd_size; ++i){
        pddl_bdd_t *res;
        res = pddlBDDAndLimit(constr->vars->mgr, bdd, constr->bw_mgroup.bdd[i],
                              0, &time_limit);
        if (res == NULL){
            pddlBDDDel(constr->vars->mgr, bdd);
            return -1;
        }
        pddlBDDDel(constr->vars->mgr, bdd);
        bdd = res;
    }

    pddlBDDDel(constr->vars->mgr, *out);
    *out = bdd;
    return 0;
}
