/***
 * cpddl
 * -------
 * Copyright (c)2020 Daniel Fiser <danfis@danfis.cz>,
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

#include "pddl/timer.h"
#include "pddl/strips_fact_cross_ref.h"
#include "pddl/red_black_fdr.h"
#include "pddl/cg.h"
#include "internal.h"

static void prepareMutex(pddl_mutex_pairs_t *mutex,
                         const pddl_mutex_pairs_t *mutex_in,
                         const pddl_mgroups_t *mgroups_in,
                         pddl_err_t *err)
{
    pddlMutexPairsInitCopy(mutex, mutex_in);
    for (int mgi = 0; mgi < mgroups_in->mgroup_size; ++mgi)
        pddlMutexPairsAddMGroup(mutex, &mgroups_in->mgroup[mgi]);
}

static void prepareStrips(pddl_strips_t *strips,
                          const pddl_strips_t *strips_in,
                          const pddl_mutex_pairs_t *mutex,
                          const pddl_red_black_fdr_config_t *cfg,
                          pddl_err_t *err)
{
    CTX(err, "clean_strips", "Clean Strips");
    pddlStripsInitCopy(strips, strips_in);
    PDDL_ISET(unreachable_ops);
    pddlStripsFindUnreachableOps(strips, mutex, &unreachable_ops, err);
    pddlStripsReduce(strips, NULL, &unreachable_ops);
    pddlStripsRemoveUselessDelEffs(strips, mutex, NULL, err);
    pddlISetFree(&unreachable_ops);
    CTXEND(err);
}

static void prepareMGroups(pddl_mgroups_t *mgroups,
                           const pddl_black_mgroups_t *black_mgroups,
                           const pddl_mgroups_t *mgroups_in,
                           pddl_err_t *err)
{
    PDDL_ISET(black_facts);
    pddlMGroupsInitEmpty(mgroups);
    // Put black mgroups first
    for (int mgi = 0; mgi < black_mgroups->mgroup_size; ++mgi){
        const pddl_black_mgroup_t *bmg = black_mgroups->mgroup + mgi;
        pddlISetUnion(&black_facts, &bmg->mgroup);
        pddlMGroupsAdd(mgroups, &bmg->mgroup);
    }
    // Next, copy the input mgroups without black facts
    for (int mgi = 0; mgi < mgroups_in->mgroup_size; ++mgi){
        const pddl_mgroup_t *mgin = mgroups_in->mgroup + mgi;
        PDDL_ISET(m);
        pddlISetMinus2(&m, &mgin->mgroup, &black_facts);
        if (pddlISetSize(&m) > 0)
            pddlMGroupsAdd(mgroups, &m);
        pddlISetFree(&m);
    }
    //pddlMGroupsPrintTable(NULL, &strips, &mgroups, NULL, err);
    pddlISetFree(&black_facts);
    PDDL_INFO(err, "Created %d mutex groups of which %d are black",
              mgroups->mgroup_size, black_mgroups->mgroup_size);
}

static void setBlackVars(pddl_fdr_t *fdr,
                         const pddl_black_mgroups_t *black_mgroups,
                         int *none_of_those,
                         pddl_err_t *err)
{
    int num_none_of_those = 0;
    for (int mgi = 0; mgi < black_mgroups->mgroup_size; ++mgi){
        none_of_those[mgi] = -1;
        const pddl_black_mgroup_t *bmg = black_mgroups->mgroup + mgi;
        int first_fact = pddlISetGet(&bmg->mgroup, 0);
        int val_id = pddlISetGet(&fdr->var.strips_id_to_val[first_fact], 0);
        int var_id = fdr->var.global_id_to_val[val_id]->var_id;
        if (fdr->var.var[var_id].val_none_of_those >= 0){
            none_of_those[mgi] = var_id;
            ++num_none_of_those;
        }
        ASSERT(fdr->var.var[var_id].is_black != 1);
        fdr->var.var[var_id].is_black = 1;
    }
    PDDL_INFO(err, "Black variables with none-of-those: %d", num_none_of_those);
}

static void compileAwayRedDelEffs(pddl_strips_t *strips,
                                  pddl_mgroups_t *mgroups,
                                  const pddl_strips_t *strips_in,
                                  const pddl_mgroups_t *mgroups_in,
                                  const pddl_black_mgroups_t *black_mgroups,
                                  pddl_err_t *err)
{
    PDDL_ISET(black_facts);
    for (int mgi = 0; mgi < black_mgroups->mgroup_size; ++mgi)
        pddlISetUnion(&black_facts, &black_mgroups->mgroup[mgi].mgroup);

    pddlStripsInitCopy(strips, strips_in);
    for (int oi = 0; oi < strips->op.op_size; ++oi){
        pddl_strips_op_t *op = strips->op.op[oi];
        pddlISetIntersect(&op->del_eff, &black_facts);
    }

    pddlMGroupsInitEmpty(mgroups);
    for (int mgi = 0; mgi < mgroups_in->mgroup_size; ++mgi){
        const pddl_mgroup_t *mg = mgroups_in->mgroup + mgi;
        if (!pddlISetIsDisjoint(&mg->mgroup, &black_facts)){
            pddlMGroupsAdd(mgroups, &mg->mgroup);
        }
    }

    pddlISetFree(&black_facts);
}

static void setNoneOfThoseInPre(pddl_fdr_t *fdr,
                                const pddl_strips_t *strips,
                                const pddl_black_mgroups_t *black_mgroups,
                                const int *none_of_those,
                                pddl_err_t *err)
{
    int num_set = 0;
    pddl_strips_fact_cross_ref_t cref;
    pddlStripsFactCrossRefInit(&cref, strips, 0, 0, 1, 0, 0);
    for (int mgi = 0; mgi < black_mgroups->mgroup_size; ++mgi){
        int set_var = none_of_those[mgi];
        if (set_var < 0)
            continue;

        int set_val = fdr->var.var[set_var].val_none_of_those;
        ASSERT(set_val >= 0);
        const pddl_black_mgroup_t *bmg = black_mgroups->mgroup + mgi;
        int mutex_fact;
        PDDL_ISET_FOR_EACH(&bmg->mutex_facts, mutex_fact){
            int opi;
            PDDL_ISET_FOR_EACH(&cref.fact[mutex_fact].op_pre, opi){
                pddl_fdr_op_t *op = fdr->op.op[opi];
                ASSERT(!pddlFDRPartStateIsSet(&op->pre, set_var)
                       || pddlFDRPartStateGet(&op->pre, set_var) == set_val);
                if (!pddlFDRPartStateIsSet(&op->pre, set_var)){
                    pddlFDRPartStateSet(&op->pre, set_var, set_val);
                    ++num_set;
                }
            }
        }
    }
    PDDL_INFO(err, "Set %d additional none-of-those preconditions", num_set);
    pddlStripsFactCrossRefFree(&cref);
}

static void fdrStat(const pddl_fdr_t *fdr, pddl_err_t *err)
{
    int num_black_vars = 0;
    int num_black_facts = 0;
    int num_black_facts_with_none_of_those = 0;
    for (int vari = 0; vari < fdr->var.var_size; ++vari){
        const pddl_fdr_var_t *var = fdr->var.var + vari;
        if (!var->is_black)
            continue;
        ++num_black_vars;
        num_black_facts += var->val_size;
        if (var->val_none_of_those >= 0)
            --num_black_facts;
        num_black_facts_with_none_of_those += var->val_size;
    }
    PDDL_INFO(err, "Num black variables: %d", num_black_vars);
    PDDL_INFO(err, "Num black STRIPS facts: %d", num_black_facts);
    PDDL_INFO(err, "Num black FDR facts: %d",
              num_black_facts_with_none_of_those);
}

static int constructFDR(pddl_fdr_t *fdr,
                        const pddl_strips_t *strips,
                        const pddl_mgroups_t *mgroups_in,
                        const pddl_mutex_pairs_t *mutex,
                        const pddl_black_mgroups_t *black_mgroups,
                        const pddl_red_black_fdr_config_t *cfg,
                        pddl_err_t *err)
{
    // Re-create the set of mutex groups
    pddl_mgroups_t mgroups1;
    prepareMGroups(&mgroups1, black_mgroups, mgroups_in, err);
    const pddl_mgroups_t *mgroups = &mgroups1;

    pddl_strips_t strips2;
    pddl_mgroups_t mgroups2;
    if (cfg->relax_red_vars){
        compileAwayRedDelEffs(&strips2, &mgroups2, strips, mgroups,
                              black_mgroups, err);
        strips = &strips2;
        mgroups = &mgroups2;
    }

    // Construct FDR
    unsigned fdr_var_flags = PDDL_FDR_VARS_LARGEST_FIRST;
    unsigned fdr_flags = 0;
    int ret = pddlFDRInitFromStrips(fdr, strips, mgroups, mutex,
                                    fdr_var_flags, fdr_flags, err);
    ASSERT_RUNTIME(fdr->op.op_size == strips->op.op_size);

    // Find black variables and remember which of them has none-of-those value
    int *none_of_those = CALLOC_ARR(int, black_mgroups->mgroup_size);
    setBlackVars(fdr, black_mgroups, none_of_those, err);

    // Set none-of-those in preconditions of operators
    setNoneOfThoseInPre(fdr, strips, black_mgroups, none_of_those, err);

    FREE(none_of_those);
    pddlMGroupsFree(&mgroups1);
    if (cfg->relax_red_vars){
        pddlStripsFree(&strips2);
        pddlMGroupsFree(&mgroups2);
    }

    fdrStat(fdr, err);

    return ret;
}

int pddlRedBlackFDRInitFromStrips(pddl_fdr_t *fdr,
                                  const pddl_strips_t *strips_in,
                                  const pddl_mgroups_t *mgroups_in,
                                  const pddl_mutex_pairs_t *mutex_in,
                                  const pddl_red_black_fdr_config_t *cfg,
                                  pddl_err_t *err)
{
    pddl_timer_t timer;
    pddlTimerStart(&timer);
    CTX(err, "black_fdr", "Black-FDR");
    PDDL_INFO(err, "Construction of FDR with black variables...");

    // Make sure that mutex groups are contained in the mutex pairs
    pddl_mutex_pairs_t mutex;
    prepareMutex(&mutex, mutex_in, mgroups_in, err);

    // Cleanup strips planning task
    pddl_strips_t strips;
    prepareStrips(&strips, strips_in, &mutex, cfg, err);

    // Find black mutex groups
    pddl_black_mgroups_t black_mgroups[cfg->mgroup.num_solutions];
    pddlBlackMGroupsInfer(black_mgroups, &strips, mgroups_in, &mutex,
                          &cfg->mgroup, err);

    // Construct FDR
    int num_created = 0;
    for (int i = 0; i < cfg->mgroup.num_solutions; ++i){
        if (black_mgroups[i].mgroup_size == 0 && i > 0)
            break;

        if (constructFDR(fdr + i, &strips, mgroups_in, &mutex,
                         black_mgroups + i, cfg, err) != 0){
            for (int i = 0; i < cfg->mgroup.num_solutions; ++i)
                pddlBlackMGroupsFree(black_mgroups + i);
            pddlStripsFree(&strips);
            pddlMutexPairsFree(&mutex);
            CTXEND(err);
            PDDL_TRACE_RET(err, -1);
        }
        num_created += 1;
    }

    for (int i = 0; i < cfg->mgroup.num_solutions; ++i)
        pddlBlackMGroupsFree(black_mgroups + i);
    pddlStripsFree(&strips);
    pddlMutexPairsFree(&mutex);

    pddlTimerStop(&timer);
    PDDL_INFO(err, "Translation took %.2f seconds",
              pddlTimerElapsedInSF(&timer));
    CTXEND(err);
    return num_created;
}

int pddlRedBlackCheck(const pddl_fdr_t *fdr, pddl_err_t *err)
{
    pddl_cg_t cg;
    pddlCGInit(&cg, &fdr->var, &fdr->op, 1);

    pddl_cg_t black_cg;
    pddlCGInitProjectToBlackVars(&black_cg, &cg, &fdr->var);
    int is_acyclic = pddlCGIsAcyclic(&black_cg);
    if (!is_acyclic)
        PANIC("Black causal graph is not acyclic!");

    pddlCGFree(&black_cg);
    pddlCGFree(&cg);

    return is_acyclic;
}
