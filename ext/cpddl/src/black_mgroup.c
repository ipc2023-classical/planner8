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

#include "internal.h"
#include "pddl/lp.h"
#include "pddl/invertibility.h"
#include "pddl/scc.h"
#include "pddl/strips_fact_cross_ref.h"
#include "pddl/black_mgroup.h"
#include "pddl/mgroup_projection.h"
#include "pddl/hff.h"
#include "pddl/relaxed_plan.h"

struct fact_vertex {
    int fact;
    int mgroup;
    float weight;
};
typedef struct fact_vertex fact_vertex_t;

struct black_vars {
    int fact_size;
    pddl_iset_t invertible_facts;
    fact_vertex_t *fact_vertex;
    int fact_vertex_size;
    pddl_iset_t *fact_to_fact_vertex;
    pddl_scc_graph_t cg;
};
typedef struct black_vars black_vars_t;

static int numFactVertices(const pddl_mgroups_t *mgroups,
                           const pddl_iset_t *invertible_facts)
{
    int num_vert = 0;
    PDDL_ISET(facts);
    PDDL_ISET(mgfacts);
    for (int mgi = 0; mgi < mgroups->mgroup_size; ++mgi){
        pddlISetIntersect2(&mgfacts, invertible_facts,
                           &mgroups->mgroup[mgi].mgroup);
        num_vert += pddlISetSize(&mgfacts);
        pddlISetUnion(&facts, &mgfacts);
    }
    num_vert += pddlISetSize(invertible_facts) - pddlISetSize(&facts);
    pddlISetFree(&facts);
    pddlISetFree(&mgfacts);
    return num_vert;
}

static void blackVarsCGInit(black_vars_t *bv,
                            pddl_scc_graph_t *cg,
                            const pddl_strips_t *strips,
                            const pddl_mgroups_t *mgroups)
{
    pddlSCCGraphInit(cg, bv->fact_vertex_size);

    pddl_iset_t *from = CALLOC_ARR(pddl_iset_t, bv->fact_vertex_size);
    pddl_iset_t *to = CALLOC_ARR(pddl_iset_t, bv->fact_vertex_size);

    PDDL_ISET(facts);
    for (int op_id = 0; op_id < strips->op.op_size; ++op_id){
        const pddl_strips_op_t *op = strips->op.op[op_id];
        pddlISetUnion2(&facts, &op->add_eff, &op->del_eff);
        int fact;
        PDDL_ISET_FOR_EACH(&facts, fact){
            int vert_id;
            PDDL_ISET_FOR_EACH(bv->fact_to_fact_vertex + fact, vert_id)
                pddlISetAdd(&to[vert_id], op_id);
        }

        pddlISetUnion(&facts, &op->pre);
        PDDL_ISET_FOR_EACH(&facts, fact){
            int vert_id;
            PDDL_ISET_FOR_EACH(bv->fact_to_fact_vertex + fact, vert_id)
                pddlISetAdd(&from[vert_id], op_id);
        }

        for (int mgi = 0; mgi < mgroups->mgroup_size; ++mgi){
            if (!mgroups->mgroup[mgi].is_fam_group)
                continue;

            const pddl_iset_t *mg = &mgroups->mgroup[mgi].mgroup;
            if (!pddlISetIsDisjoint(mg, &op->pre)){
                PDDL_ISET_FOR_EACH(mg, fact){
                    int vert_id;
                    PDDL_ISET_FOR_EACH(bv->fact_to_fact_vertex + fact, vert_id){
                        if (bv->fact_vertex[vert_id].mgroup == mgi)
                            pddlISetAdd(&from[vert_id], op_id);
                    }
                }
            }
        }
    }
    pddlISetFree(&facts);

    for (int vfrom = 0; vfrom < bv->fact_vertex_size; ++vfrom){
        for (int vto = 0; vto < bv->fact_vertex_size; ++vto){
            if (vfrom == vto)
                continue;
            if (!pddlISetIsDisjoint(from + vfrom, to + vto))
                pddlSCCGraphAddEdge(cg, vfrom, vto);
        }
    }

    for (int fact = 0; fact < strips->fact.fact_size; ++fact){
        const pddl_iset_t *vert_set = bv->fact_to_fact_vertex + fact;
        for (int i = 0; i < pddlISetSize(vert_set); ++i){
            int vert1 = pddlISetGet(vert_set, i);
            for (int j = i + 1; j < pddlISetSize(vert_set); ++j){
                int vert2 = pddlISetGet(vert_set, j);
                pddlSCCGraphAddEdge(cg, vert1, vert2);
                pddlSCCGraphAddEdge(cg, vert2, vert1);
            }
        }
    }

    for (int f = 0; f < bv->fact_vertex_size; ++f){
        pddlISetFree(from + f);
        pddlISetFree(to + f);
    }
    FREE(from);
    FREE(to);
}

static void uncoveredDelEffs(const pddl_strips_t *strips, pddl_iset_t *facts)
{
    PDDL_ISET(deleff);
    pddlISetEmpty(facts);
    for (int opi = 0; opi < strips->op.op_size; ++opi){
        const pddl_strips_op_t *op = strips->op.op[opi];
        pddlISetMinus2(&deleff, &op->del_eff, &op->pre);
        pddlISetUnion(facts, &deleff);
    }
    pddlISetFree(&deleff);
}

static void findRelaxedPlan(const black_vars_t *bv,
                            const pddl_strips_t *strips,
                            pddl_iset_t *plan_set,
                            int *conflicts,
                            pddl_err_t *err)
{
    PDDL_IARR(plan);
    pddl_hff_t hff;
    pddlHFFInitStrips(&hff, strips);
    pddlHFFStripsPlan(&hff, &strips->init, &plan);
    pddlHFFFree(&hff);

    if (conflicts != NULL){
        pddlRelaxedPlanCountConflictsStrips(&plan, &strips->init, &strips->goal,
                                            &strips->op, 1, conflicts);
        for (int i = 0; i < strips->fact.fact_size; ++i){
            if (conflicts[i] > 0
                    && pddlISetSize(&bv->fact_to_fact_vertex[i]) > 0){
                LOG(err, "Conflict count: %d:(%s) = %d",
                    i, strips->fact.fact[i]->name, conflicts[i]);
            }
        }
    }

    if (plan_set != NULL){
        int op;
        PDDL_IARR_FOR_EACH(&plan, op)
            pddlISetAdd(plan_set, op);
    }

    pddlIArrFree(&plan);
}

static int maxOutdegreeInProjectionToRelaxedPlan(
                const pddl_strips_t *strips,
                const pddl_mutex_pairs_t *mutex,
                const pddl_strips_fact_cross_ref_t *cref,
                const pddl_iset_t *mgroup,
                const pddl_iset_t *relaxed_plan)
{
    pddl_mgroup_projection_t proj;
    pddlMGroupProjectionInit(&proj, strips, mgroup, mutex, cref);
    pddlMGroupProjectionRestrictOps(&proj, relaxed_plan);
    int max_outdegree = pddlMGroupProjectionMaxOutdegree(&proj);
    pddlMGroupProjectionFree(&proj);
    return max_outdegree;
}

static void setWeightWithProjectionsToRelaxedPlan(
                black_vars_t *bv,
                const pddl_strips_t *strips,
                const pddl_mgroups_t *mgroups,
                const pddl_mutex_pairs_t *mutex,
                pddl_err_t *err)
{
    LOG(err, "Setting weights using projections to a relaxed plan ...");
    if (mgroups->mgroup_size == 0)
        return;

    pddl_iset_t *mgs = CALLOC_ARR(pddl_iset_t, mgroups->mgroup_size);
    pddl_iset_t *mgs_vert = CALLOC_ARR(pddl_iset_t, mgroups->mgroup_size);
    for (int vert_id = 0; vert_id < bv->fact_vertex_size; ++vert_id){
        const fact_vertex_t *vert = bv->fact_vertex + vert_id;
        if (vert->mgroup >= 0){
            pddlISetAdd(mgs + vert->mgroup, vert->fact);
            pddlISetAdd(mgs_vert + vert->mgroup, vert_id);
        }
    }

    PDDL_ISET(plan_set);
    findRelaxedPlan(bv, strips, &plan_set, NULL, err);

    pddl_strips_fact_cross_ref_t cref;
    pddlStripsFactCrossRefInit(&cref, strips, 0, 0, 1, 1, 1);
    for (int mgi = 0; mgi < mgroups->mgroup_size; ++mgi){
        if (pddlISetSize(mgs + mgi) <= 1)
            continue;
        float deg = maxOutdegreeInProjectionToRelaxedPlan(strips, mutex, &cref,
                                                          mgs + mgi, &plan_set);
        if (deg > 1)
            deg *= bv->fact_vertex_size;
        int vert_id;
        PDDL_ISET_FOR_EACH(mgs_vert + mgi, vert_id)
            bv->fact_vertex[vert_id].weight = PDDL_MAX(1, deg);
        if (deg > 1){
            LOG(err, "Change weight of mutex group (%s), ... to %.2f",
                strips->fact.fact[pddlISetGet(mgs + mgi, 0)]->name,
                bv->fact_vertex[pddlISetGet(mgs_vert + mgi, 0)].weight);
        }
    }
    pddlStripsFactCrossRefFree(&cref);
    pddlISetFree(&plan_set);

    for (int i = 0; i < mgroups->mgroup_size; ++i){
        pddlISetFree(mgs + i);
        pddlISetFree(mgs_vert + i);
    }
    FREE(mgs);
    FREE(mgs_vert);
}

static void setWeightWithConflictsInRelaxedPlan(
                black_vars_t *bv,
                const pddl_strips_t *strips,
                const pddl_mgroups_t *mgroups,
                const pddl_mutex_pairs_t *mutex,
                pddl_err_t *err)
{
    LOG(err, "Setting weights using conflicts in a relaxed plan ...");
    if (mgroups->mgroup_size == 0)
        return;

    // TODO: refactor
    pddl_iset_t *mgs = CALLOC_ARR(pddl_iset_t, mgroups->mgroup_size);
    pddl_iset_t *mgs_vert = CALLOC_ARR(pddl_iset_t, mgroups->mgroup_size);
    for (int vert_id = 0; vert_id < bv->fact_vertex_size; ++vert_id){
        const fact_vertex_t *vert = bv->fact_vertex + vert_id;
        if (vert->mgroup >= 0){
            pddlISetAdd(mgs + vert->mgroup, vert->fact);
            pddlISetAdd(mgs_vert + vert->mgroup, vert_id);
        }
    }

    int *conflicts = CALLOC_ARR(int, strips->fact.fact_size);
    findRelaxedPlan(bv, strips, NULL, conflicts, err);

    for (int mgi = 0; mgi < mgroups->mgroup_size; ++mgi){
        if (pddlISetSize(mgs + mgi) <= 0)
            continue;
        float weight = 0.;
        int fact_id;
        PDDL_ISET_FOR_EACH(mgs + mgi, fact_id)
            weight += conflicts[fact_id];
        weight = weight * bv->fact_vertex_size;
        weight /= pddlISetSize(mgs_vert + mgi);

        int vert_id;
        PDDL_ISET_FOR_EACH(mgs_vert + mgi, vert_id)
            bv->fact_vertex[vert_id].weight = PDDL_MAX(1, weight);
        if (weight > 0.){
            LOG(err, "Change weight of mutex group (%s), ... to %.2f",
                strips->fact.fact[pddlISetGet(mgs + mgi, 0)]->name,
                bv->fact_vertex[pddlISetGet(mgs_vert + mgi, 0)].weight);
        }
    }

    for (int vert_id = 0; vert_id < bv->fact_vertex_size; ++vert_id){
        int mgroup = bv->fact_vertex[vert_id].mgroup;
        int fact = bv->fact_vertex[vert_id].fact;
        if (mgroup == -1 && conflicts[fact] > 0){
            float weight = conflicts[fact] * bv->fact_vertex_size;
            bv->fact_vertex[vert_id].weight = weight;
            LOG(err, "Change weight of fact (%s) to %.2f",
                strips->fact.fact[fact]->name,
                bv->fact_vertex[vert_id].weight);
        }
    }

    // Distribute weights to mutex groups from the same lifted mutex group
    PDDL_ISET(lifted_ids);
    for (int mgi = 0; mgi < mgroups->mgroup_size; ++mgi){
        if (pddlISetSize(mgs + mgi) <= 0)
            continue;
        if (mgroups->mgroup[mgi].lifted_mgroup_id >= 0)
            pddlISetAdd(&lifted_ids, mgroups->mgroup[mgi].lifted_mgroup_id);
    }
    int lifted_id;
    PDDL_ISET_FOR_EACH(&lifted_ids, lifted_id){
        float max_weight = 1.;
        for (int mgi = 0; mgi < mgroups->mgroup_size; ++mgi){
            if (mgroups->mgroup[mgi].lifted_mgroup_id != lifted_id)
                continue;
            if (pddlISetSize(mgs_vert + mgi) == 0)
                continue;
            float w = bv->fact_vertex[pddlISetGet(mgs_vert + mgi, 0)].weight;
            max_weight = PDDL_MAX(max_weight, w);
        }
        for (int mgi = 0; mgi < mgroups->mgroup_size; ++mgi){
            if (mgroups->mgroup[mgi].lifted_mgroup_id != lifted_id)
                continue;
            if (pddlISetSize(mgs_vert + mgi) == 0)
                continue;
            float w = bv->fact_vertex[pddlISetGet(mgs_vert + mgi, 0)].weight;
            if (w < max_weight){
                LOG(err, "Change weight of mutex group (%s), ... to %.2f",
                    strips->fact.fact[pddlISetGet(mgs + mgi, 0)]->name,
                    max_weight);
                int vert_id;
                PDDL_ISET_FOR_EACH(mgs_vert + mgi, vert_id)
                    bv->fact_vertex[vert_id].weight = max_weight;
            }
        }
    }
    pddlISetFree(&lifted_ids);

    if (conflicts != NULL)
        FREE(conflicts);

    for (int i = 0; i < mgroups->mgroup_size; ++i){
        pddlISetFree(mgs + i);
        pddlISetFree(mgs_vert + i);
    }
    FREE(mgs);
    FREE(mgs_vert);
}

static void blackVarsInit(black_vars_t *bv,
                          const pddl_strips_t *strips,
                          const pddl_mgroups_t *mgroups,
                          const pddl_mutex_pairs_t *mutex,
                          const pddl_black_mgroups_config_t *cfg,
                          pddl_err_t *err)
{
    ZEROIZE(bv);
    bv->fact_size = strips->fact.fact_size;

    // Find invertible facts
    pddlRSEInvertibleFacts(strips, mgroups, &bv->invertible_facts, err);
    LOG(err, "Invertible facts: %d/%d",
        pddlISetSize(&bv->invertible_facts), bv->fact_size);

    // Prepare vertices
    bv->fact_to_fact_vertex = CALLOC_ARR(pddl_iset_t, bv->fact_size);
    bv->fact_vertex_size = numFactVertices(mgroups, &bv->invertible_facts);
    LOG(err, "Fact-mgroup pairs: %d", bv->fact_vertex_size);
    bv->fact_vertex = CALLOC_ARR(fact_vertex_t, bv->fact_vertex_size);
    for (int vert_id = 0; vert_id < bv->fact_vertex_size; ++vert_id){
        fact_vertex_t *vert = bv->fact_vertex + vert_id;
        vert->fact = -1;
        vert->mgroup = -1;
        vert->weight = 1.;
    }

    // Assign facts and mgroups to vertices
    PDDL_ISET(facts);
    int vert_id = 0;
    for (int mgi = 0; mgi < mgroups->mgroup_size; ++mgi){
        pddlISetIntersect2(&facts, &bv->invertible_facts,
                           &mgroups->mgroup[mgi].mgroup);
        int fact;
        PDDL_ISET_FOR_EACH(&facts, fact){
            bv->fact_vertex[vert_id].fact = fact;
            bv->fact_vertex[vert_id].mgroup = mgi;
            pddlISetAdd(bv->fact_to_fact_vertex + fact, vert_id);
            ++vert_id;
        }
    }
    pddlISetFree(&facts);

    // Assign the rest of the facts not belonging to any mgroup
    int fact;
    PDDL_ISET_FOR_EACH(&bv->invertible_facts, fact){
        if (pddlISetSize(bv->fact_to_fact_vertex + fact) == 0){
            bv->fact_vertex[vert_id].fact = fact;
            bv->fact_vertex[vert_id].mgroup = -1;
            pddlISetAdd(bv->fact_to_fact_vertex + fact, vert_id);
            ++vert_id;
        }
    }

    // Delete effects that are uncovered by preconditions must be encoded
    // as a single-fact variables
    PDDL_ISET(uncovered_del_effs);
    uncoveredDelEffs(strips, &uncovered_del_effs);
    PDDL_ISET_FOR_EACH(&uncovered_del_effs, fact){
        int vert_id;
        PDDL_ISET_FOR_EACH(bv->fact_to_fact_vertex + fact, vert_id)
            bv->fact_vertex[vert_id].mgroup = -1;
    }
    pddlISetFree(&uncovered_del_effs);

    blackVarsCGInit(bv, &bv->cg, strips, mgroups);

    if (cfg->weight_facts_with_relaxed_plan)
        setWeightWithProjectionsToRelaxedPlan(bv, strips, mgroups, mutex, err);
    if (cfg->weight_facts_with_conflicts)
        setWeightWithConflictsInRelaxedPlan(bv, strips, mgroups, mutex, err);
}

static void blackVarsFree(black_vars_t *bv)
{
    pddlISetFree(&bv->invertible_facts);
    for (int f = 0; f < bv->fact_size; ++f)
        pddlISetFree(bv->fact_to_fact_vertex + f);
    FREE(bv->fact_to_fact_vertex);
    FREE(bv->fact_vertex);
    pddlSCCGraphFree(&bv->cg);
}

static pddl_lp_t *createLP(const black_vars_t *bv)
{
    pddl_lp_config_t cfg = PDDL_LP_CONFIG_INIT;
    cfg.maximize = 1;
    cfg.rows = 0;
    cfg.cols = bv->fact_vertex_size;
    pddl_lp_t *lp = pddlLPNew(&cfg, NULL);
    for (int vi = 0; vi < bv->fact_vertex_size; ++vi){
        pddlLPSetObj(lp, vi, bv->fact_vertex[vi].weight);
        pddlLPSetVarBinary(lp, vi);
    }

    return lp;
}

static void addCycle(pddl_lp_t *lp, const pddl_iarr_t *cycle)
{
    int row = pddlLPNumRows(lp);
    double rhs = pddlIArrSize(cycle) - 1;
    char sense = 'L';
    pddlLPAddRows(lp, 1, &rhs, &sense);
    int var;
    PDDL_IARR_FOR_EACH(cycle, var)
        pddlLPSetCoef(lp, row, var, 1.);
}

static void addCycles2(pddl_lp_t *lp, const black_vars_t *bv, pddl_err_t *err)
{
    int num = 0;
    for (int v1 = 0; v1 < bv->fact_vertex_size; ++v1){
        int v2;
        PDDL_ISET_FOR_EACH(&bv->cg.node[v1], v2){
            // Skip cycles with the same mutex group
            if (bv->fact_vertex[v1].mgroup == bv->fact_vertex[v2].mgroup
                    && bv->fact_vertex[v1].mgroup >= 0){
                continue;
            }

            if (pddlISetIn(v1, &bv->cg.node[v2])){
                PDDL_IARR(path);
                pddlIArrAdd(&path, v1);
                pddlIArrAdd(&path, v2);
                addCycle(lp, &path);
                pddlIArrFree(&path);
                ++num;
            }
        }
    }
    LOG(err, "Added %d 2-cycles", num);
}

static void addCycles3(pddl_lp_t *lp, const black_vars_t *bv, pddl_err_t *err)
{
    int num = 0;
    for (int v1 = 0; v1 < bv->fact_vertex_size; ++v1){
        int v1mgroup = bv->fact_vertex[v1].mgroup;
        int v2;
        PDDL_ISET_FOR_EACH(&bv->cg.node[v1], v2){
            // Skip 2-vertex cycles
            if (pddlISetIn(v1, &bv->cg.node[v2]))
                continue;
            int v2mgroup = bv->fact_vertex[v2].mgroup;
            int v3;
            PDDL_ISET_FOR_EACH(&bv->cg.node[v2], v3){
                int v3mgroup = bv->fact_vertex[v3].mgroup;
                // Skip cycles with the same mutex group
                if (v1mgroup == v2mgroup && v2mgroup == v3mgroup
                        && v1mgroup >= 0){
                    continue;
                }

                if (pddlISetIn(v1, &bv->cg.node[v3])){
                    PDDL_IARR(path);
                    pddlIArrAdd(&path, v1);
                    pddlIArrAdd(&path, v2);
                    pddlIArrAdd(&path, v3);
                    addCycle(lp, &path);
                    pddlIArrFree(&path);
                    ++num;
                }
            }
        }
    }
    LOG(err, "Added %d 3-cycles", num);
}

static void addLeafMGroup(pddl_lp_t *lp, const pddl_iset_t *mg)
{
    int row = pddlLPNumRows(lp);
    double rhs = 0;
    char sense = 'L';
    pddlLPAddRows(lp, 1, &rhs, &sense);
    int var;
    PDDL_ISET_FOR_EACH(mg, var)
        pddlLPSetCoef(lp, row, var, 1.);
}

static void addRedFacts(pddl_lp_t *lp,
                        const black_vars_t *bv,
                        const pddl_iset_t *black_vars)
{
    int row = pddlLPNumRows(lp);
    double rhs = 1;
    char sense = 'G';
    pddlLPAddRows(lp, 1, &rhs, &sense);

    int *black = CALLOC_ARR(int, bv->fact_vertex_size);
    int var;
    PDDL_ISET_FOR_EACH(black_vars, var){
        int fact = bv->fact_vertex[var].fact;
        int var2;
        PDDL_ISET_FOR_EACH(bv->fact_to_fact_vertex + fact, var2)
            black[var2] = 1;

    }
    for (int var = 0; var < bv->fact_vertex_size; ++var){
        if (!black[var])
            pddlLPSetCoef(lp, row, var, 1.);
    }
    FREE(black);
}

static int compIsSingleMGroup(const black_vars_t *bv, const pddl_iset_t *comp)
{
    int mgi = bv->fact_vertex[pddlISetGet(comp, 0)].mgroup;
    int var;
    PDDL_ISET_FOR_EACH(comp, var){
        if (bv->fact_vertex[var].mgroup != mgi)
            return 0;
    }
    return 1;
}

static int findMultiMGroupComponent(const black_vars_t *bv,
                                    const pddl_scc_graph_t *black_graph,
                                    pddl_iset_t *comp)
{
    int found = 0;
    pddl_scc_t scc;
    pddlSCC(&scc, black_graph);
    for (int i = 0; i < scc.comp_size; ++i){
        if (!compIsSingleMGroup(bv, &scc.comp[i])){
            pddlISetEmpty(comp);
            pddlISetUnion(comp, &scc.comp[i]);
            found = 1;
            break;
        }
    }
    pddlSCCFree(&scc);
    return found;
}

struct update_lp {
    pddl_lp_t *lp;
    const black_vars_t *bv;
};

static int updateLPWithCycleFn(const pddl_iarr_t *cycle, void *ud)
{
    int ret = PDDL_GRAPH_SIMPLE_CYCLE_CONT;
    struct update_lp *update = ud;
    PDDL_ISET(mg);
    int vert_id;
    PDDL_IARR_FOR_EACH(cycle, vert_id){
        pddlISetAdd(&mg, update->bv->fact_vertex[vert_id].mgroup);
        if (pddlISetSize(&mg) > 1)
            break;
    }
    if (pddlISetSize(&mg) > 1){
        addCycle(update->lp, cycle);
        ret = PDDL_GRAPH_SIMPLE_CYCLE_STOP;
    }
    pddlISetFree(&mg);
    return ret;
}

static void updateLPWithCycle(pddl_lp_t *lp,
                              const black_vars_t *bv,
                              const pddl_scc_graph_t *black_graph,
                              const pddl_iset_t *comp)
{
    struct update_lp update = { lp, bv };
    pddl_scc_graph_t graph;
    pddlSCCGraphInitInduced(&graph, black_graph, comp);
    pddlGraphSimpleCyclesFn(&graph, updateLPWithCycleFn, &update);
    pddlSCCGraphFree(&graph);
}

static int solveLP(pddl_lp_t *lp,
                   const black_vars_t *bv,
                   pddl_iset_t *black_vars)
{
    double *obj = CALLOC_ARR(double, bv->fact_vertex_size);
    double val;
    if (pddlLPSolve(lp, &val, obj) != 0){
        FREE(obj);
        return -1;
    }

    pddlISetEmpty(black_vars);
    for (int v = 0; v < bv->fact_vertex_size; ++v){
        if (obj[v] >= .5)
            pddlISetAdd(black_vars, v);
    }
    FREE(obj);
    return 0;
}

static pddl_black_mgroup_t *blackMGroupsAdd(pddl_black_mgroups_t *bmgroups,
                                            const pddl_iset_t *m)
{
    if (bmgroups->mgroup_size == bmgroups->mgroup_alloc){
        if (bmgroups->mgroup_alloc == 0)
            bmgroups->mgroup_alloc = 2;
        bmgroups->mgroup_alloc *= 2;
        bmgroups->mgroup = REALLOC_ARR(bmgroups->mgroup,
                                       pddl_black_mgroup_t,
                                       bmgroups->mgroup_alloc);
    }
    pddl_black_mgroup_t *mg = bmgroups->mgroup + bmgroups->mgroup_size++;
    pddlISetInit(&mg->mgroup);
    pddlISetUnion(&mg->mgroup, m);
    pddlISetInit(&mg->mutex_facts);
    return mg;
}

static pddl_black_mgroup_t *blackMGroupsAddSingle(pddl_black_mgroups_t *bmgroups,
                                                  int fact)
{
    pddl_black_mgroup_t *mg;

    PDDL_ISET(m);
    pddlISetAdd(&m, fact);
    mg = blackMGroupsAdd(bmgroups, &m);
    pddlISetFree(&m);

    return mg;
}

static void blackFactsToBlackMGroups(const black_vars_t *bv,
                                     const pddl_iset_t *black_vars,
                                     const pddl_mgroups_t *mgroups,
                                     pddl_black_mgroups_t *bmgroups,
                                     pddl_err_t *err)
{
    pddl_iset_t *mgs = CALLOC_ARR(pddl_iset_t, mgroups->mgroup_size);
    int vert_id;
    PDDL_ISET_FOR_EACH(black_vars, vert_id){
        int fact = bv->fact_vertex[vert_id].fact;
        int mgi = bv->fact_vertex[vert_id].mgroup;
        if (mgi >= 0){
            pddlISetAdd(mgs + mgi, fact);
        }else{
            blackMGroupsAddSingle(bmgroups, fact);
        }
    }

    for (int mgi = 0; mgi < mgroups->mgroup_size; ++mgi){
        if (pddlISetSize(mgs + mgi) == 0)
            continue;
        pddl_black_mgroup_t *mg = blackMGroupsAdd(bmgroups, mgs + mgi);
        pddlISetUnion(&mg->mutex_facts, &mgroups->mgroup[mgi].mgroup);
        pddlISetMinus(&mg->mutex_facts, &mg->mgroup);
    }

    for (int mgi = 0; mgi < mgroups->mgroup_size; ++mgi)
        pddlISetFree(mgs + mgi);
    if (mgs != NULL)
        FREE(mgs);
}

static int mgroupIsLeaf(const pddl_iset_t *mgroup,
                        const pddl_strips_t *strips,
                        const pddl_iset_t *fact_op)
{
    int fact;
    PDDL_ISET_FOR_EACH(mgroup, fact){
        int opi;
        PDDL_ISET_FOR_EACH(fact_op + fact, opi){
            const pddl_strips_op_t *op = strips->op.op[opi];
            if (!pddlISetIsSubset(&op->add_eff, mgroup))
                return 0;
            if (!pddlISetIsSubset(&op->del_eff, mgroup))
                return 0;
        }
    }

    return 1;
}

static int findAndUpdateLeafs(pddl_lp_t *lp,
                              const black_vars_t *bv,
                              const pddl_strips_t *strips,
                              const pddl_iset_t *black_vars,
                              int num_mgroups,
                              pddl_err_t *err)
{
    int updated = 0;
    pddl_strips_fact_cross_ref_t cref;
    pddlStripsFactCrossRefInit(&cref, strips, 0, 0, 1, 1, 1);

    pddl_iset_t *fact_op = CALLOC_ARR(pddl_iset_t, strips->fact.fact_size);
    for (int fact = 0; fact < strips->fact.fact_size; ++fact){
        pddlISetUnion(fact_op + fact, &cref.fact[fact].op_pre);
        pddlISetUnion(fact_op + fact, &cref.fact[fact].op_add);
        pddlISetUnion(fact_op + fact, &cref.fact[fact].op_del);
    }

    pddl_iset_t *bmgroups = CALLOC_ARR(pddl_iset_t, num_mgroups);
    pddl_iset_t *bmgroups_vert = CALLOC_ARR(pddl_iset_t, num_mgroups);
    int vert_id;
    PDDL_ISET_FOR_EACH(black_vars, vert_id){
        if (bv->fact_vertex[vert_id].mgroup >= 0){
            pddlISetAdd(bmgroups + bv->fact_vertex[vert_id].mgroup,
                        bv->fact_vertex[vert_id].fact);
            pddlISetAdd(bmgroups_vert + bv->fact_vertex[vert_id].mgroup,
                        vert_id);
        }
    }

    for (int mgi = 0; mgi < num_mgroups; ++mgi){
        if (pddlISetSize(bmgroups + mgi) == 0)
            continue;

        const pddl_iset_t *mg = bmgroups + mgi;
        if (mgroupIsLeaf(mg, strips, fact_op)){
            addLeafMGroup(lp, bmgroups_vert + mgi);
            ++updated;
        }
    }

    for (int i = 0; i < strips->fact.fact_size; ++i)
        pddlISetFree(fact_op + i);
    FREE(fact_op);
    for (int i = 0; i < num_mgroups; ++i){
        pddlISetFree(bmgroups + i);
        pddlISetFree(bmgroups_vert + i);
    }
    FREE(bmgroups);
    FREE(bmgroups_vert);
    pddlStripsFactCrossRefFree(&cref);

    LOG(err, "Found %d leaf mgroups", updated);
    return updated > 0;
}

static int findBlackVarsUsingLP(pddl_lp_t *lp,
                                const black_vars_t *bv,
                                const pddl_strips_t *strips,
                                const pddl_mgroups_t *mgroups,
                                const pddl_black_mgroups_config_t *cfg,
                                pddl_black_mgroups_t *bmgroups,
                                pddl_err_t *err)
{
    int ret = 0;
    PDDL_ISET(black_vars);
    int cont = 1;
    int solution = 0;
    int num_updates = 0;
    while (cont && (ret = solveLP(lp, bv, &black_vars)) == 0){
        LOG(err, "Solved. Candidate set size: %d", pddlISetSize(&black_vars));

        pddl_scc_graph_t black_graph;
        pddlSCCGraphInitInduced(&black_graph, &bv->cg, &black_vars);
        PDDL_ISET(comp);
        if (findMultiMGroupComponent(bv, &black_graph, &comp)){
            LOG(err, "The solution has a cycle."
                 " Updating LP by adding more cycles...");
            if (num_updates == 5){
                addCycles3(lp, bv, err);
            }else{
                updateLPWithCycle(lp, bv, &black_graph, &comp);
                LOG(err, "Updated. Num constraints: %d", pddlLPNumRows(lp));
            }
            ++num_updates;

        }else if (!findAndUpdateLeafs(lp, bv, strips, &black_vars,
                                      mgroups->mgroup_size, err)){
            if (pddlISetSize(&black_vars) > 0){
                blackFactsToBlackMGroups(bv, &black_vars, mgroups,
                                         bmgroups + solution, err);
                LOG(err, "Found non-empty solution %{solution.id}d with"
                    " %{solution.black_facts}d black facts and"
                    " %{solution.black_mgroups}d black mgroups",
                    solution, pddlISetSize(&black_vars),
                    bmgroups->mgroup_size);
                ++solution;
                if (solution >= cfg->num_solutions){
                    cont = 0;
                }else{
                    LOG(err, "Trying next solution");
                    addRedFacts(lp, bv, &black_vars);
                }
            }else{
                cont = 0;
            }
        }
        pddlISetFree(&comp);
        pddlSCCGraphFree(&black_graph);
        pddlISetEmpty(&black_vars);
    }
    if (ret != 0)
        LOG(err, "No solution exists.");
    pddlISetFree(&black_vars);

    if (bmgroups->mgroup_size > 0 || ret == 0)
        return 0;
    return -1;
}


void pddlBlackMGroupsInfer(pddl_black_mgroups_t *bmgroups,
                           const pddl_strips_t *strips,
                           const pddl_mgroups_t *mgroups_in,
                           const pddl_mutex_pairs_t *mutex,
                           const pddl_black_mgroups_config_t *cfg,
                           pddl_err_t *err)
{
    CTX(err, "black_mg_lp", "Black-mg-LP");
    for (int i = 0; i < cfg->num_solutions; ++i)
        ZEROIZE(bmgroups + i);

    pddl_mgroups_t mgroups;
    pddlMGroupsInitEmpty(&mgroups);
    for (int mgi = 0; mgi < mgroups_in->mgroup_size; ++mgi){
        if (!mgroups_in->mgroup[mgi].is_fam_group)
            continue;

        pddl_mgroup_t *mg;
        mg = pddlMGroupsAdd(&mgroups, &mgroups_in->mgroup[mgi].mgroup);
        mg->is_fam_group = 1;
        mg->lifted_mgroup_id = mgroups_in->mgroup[mgi].lifted_mgroup_id;
    }

    black_vars_t bv;
    blackVarsInit(&bv, strips, &mgroups, mutex, cfg, err);

    pddl_lp_t *lp = createLP(&bv);
    if (cfg->lp_add_2cycles)
        addCycles2(lp, &bv, err);
    if (cfg->lp_add_3cycles)
        addCycles3(lp, &bv, err);
    findBlackVarsUsingLP(lp, &bv, strips, &mgroups, cfg, bmgroups, err);
    pddlLPDel(lp);
    blackVarsFree(&bv);
    pddlMGroupsFree(&mgroups);
    CTXEND(err);
}

void pddlBlackMGroupsFree(pddl_black_mgroups_t *bmgroups)
{
    for (int i = 0; i < bmgroups->mgroup_size; ++i){
        pddlISetFree(&bmgroups->mgroup[i].mgroup);
        pddlISetFree(&bmgroups->mgroup[i].mutex_facts);
    }
    if (bmgroups->mgroup != NULL)
        FREE(bmgroups->mgroup);
}


void pddlBlackMGroupsPrint(const pddl_strips_t *strips,
                           const pddl_black_mgroups_t *bmgroups,
                           FILE *fout)
{
    for (int mgi = 0; mgi < bmgroups->mgroup_size; ++mgi){
        const pddl_black_mgroup_t *bmg = bmgroups->mgroup + mgi;
        int fact;
        fprintf(fout, "black-mgroup:");
        PDDL_ISET_FOR_EACH(&bmg->mgroup, fact)
            fprintf(fout, " %d:(%s)", fact, strips->fact.fact[fact]->name);
        fprintf(fout, "\n");
        fprintf(fout, "  mutex-facts:");
        PDDL_ISET_FOR_EACH(&bmg->mutex_facts, fact)
            fprintf(fout, " %d:(%s)", fact, strips->fact.fact[fact]->name);
        fprintf(fout, "\n");
    }
}
