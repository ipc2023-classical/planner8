/***
 * Copyright (c)2022 Daniel Fiser <danfis@danfis.cz>
 *
 *  This file is part of cpddl.
 *
 *  Distributed under the OSI-approved BSD License (the "License");
 *  see accompanying file BDS-LICENSE for details or see
 *  <http://www.opensource.org/licenses/bsd-license.php>.
 *
 *  This software is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the License for more information.
 */

#include "internal.h"
#include "pddl/endomorphism.h"
#include "pddl/cp.h"
#include "pddl/time_limit.h"
#include "pddl/hfunc.h"

struct mg_strips_op {
    int cost;
    pddl_fdr_part_state_t pre;
    pddl_fdr_part_state_t eff;
};
typedef struct mg_strips_op mg_strips_op_t;

struct mg_strips {
    const pddl_strips_t *strips;
    const pddl_mgroups_t *mgroup;
    int fact_size;
    pddl_iset_t *fact_to_mgroup;
    int op_size;
    mg_strips_op_t *op;

    int *fact_to_cvar;
    int *fact_identity;
    int *op_identity;
    int cvar_fact_size;
    int non_identity_cvar_op_size;
};
typedef struct mg_strips mg_strips_t;

struct pre_eff_vars {
    int group_id;
    pddl_iset_t pre;
    pddl_iset_t eff;
    pddl_htable_key_t key;
    pddl_list_t htable;
};
typedef struct pre_eff_vars pre_eff_vars_t;

struct op_groups {
    pddl_iset_t *group;
    int group_size;
    int group_alloc;
    pddl_htable_t *htable;
};
typedef struct op_groups op_groups_t;

static pddl_htable_key_t preEffComputeHash(const pre_eff_vars_t *v)
{
    uint64_t key;
    key = pddlFastHash_32(v->pre.s, v->pre.size, 13);
    key <<= 32;
    key |= (uint64_t)pddlFastHash_32(v->eff.s, v->eff.size, 13);
    return key;
}

static pddl_htable_key_t preEffHash(const pddl_list_t *l, void *_)
{
    const pre_eff_vars_t *v = PDDL_LIST_ENTRY(l, pre_eff_vars_t, htable);
    return v->key;
}

static int preEffEq(const pddl_list_t *l1, const pddl_list_t *l2, void *_)
{
    const pre_eff_vars_t *v1 = PDDL_LIST_ENTRY(l1, pre_eff_vars_t, htable);
    const pre_eff_vars_t *v2 = PDDL_LIST_ENTRY(l2, pre_eff_vars_t, htable);
    return pddlISetEq(&v1->pre, &v2->pre) && pddlISetEq(&v1->eff, &v2->eff);
}

static void assignOpToGroup(op_groups_t *opgs,
                            int op_id,
                            const pddl_fdr_part_state_t *pre,
                            const pddl_fdr_part_state_t *eff)
{
    pre_eff_vars_t *pev = ZALLOC(pre_eff_vars_t);
    for (int fi = 0; fi < pre->fact_size; ++fi)
        pddlISetAdd(&pev->pre, pre->fact[fi].var);
    for (int fi = 0; fi < eff->fact_size; ++fi)
        pddlISetAdd(&pev->eff, eff->fact[fi].var);
    pev->key = preEffComputeHash(pev);
    pddlListInit(&pev->htable);

    pddl_list_t *found;
    if ((found = pddlHTableInsertUnique(opgs->htable, &pev->htable)) == NULL){
        if (opgs->group_size == opgs->group_alloc){
            if (opgs->group_alloc == 0)
                opgs->group_alloc = 2;
            opgs->group_alloc *= 2;
            opgs->group = REALLOC_ARR(opgs->group, pddl_iset_t,
                                      opgs->group_alloc);
        }
        int group_id = opgs->group_size++;
        pddl_iset_t *g = opgs->group + group_id;
        pddlISetInit(g);
        pev->group_id = group_id;
        pddlISetAdd(g, op_id);

    }else{
        pev = PDDL_LIST_ENTRY(found, pre_eff_vars_t, htable);
        pddlISetAdd(opgs->group + pev->group_id, op_id);
    }
}

static void opGroupsInitFDR(op_groups_t *opg, const pddl_fdr_t *fdr)
{
    ZEROIZE(opg);
    opg->htable = pddlHTableNew(preEffHash, preEffEq, NULL);
    for (int oi = 0; oi < fdr->op.op_size; ++oi){
        const pddl_fdr_op_t *op = fdr->op.op[oi];
        assignOpToGroup(opg, op->id, &op->pre, &op->eff);
    }
}

static void opGroupsFree(op_groups_t *opg)
{
    for (int i = 0; i < opg->group_size; ++i)
        pddlISetFree(opg->group + i);
    if (opg->group != NULL)
        FREE(opg->group);

    pddl_list_t list;
    pddlListInit(&list);
    pddlHTableGather(opg->htable, &list);
    while (!pddlListEmpty(&list)){
        pddl_list_t *item = pddlListNext(&list);
        pddlListDel(item);
        pre_eff_vars_t *v = PDDL_LIST_ENTRY(item, pre_eff_vars_t, htable);
        pddlISetFree(&v->pre);
        pddlISetFree(&v->eff);
        FREE(v);
    }
    pddlHTableDel(opg->htable);
}

static int fdrOpConstr(const pddl_fdr_t *fdr,
                       const pddl_endomorphism_config_t *cfg,
                       const pddl_fdr_op_t *op,
                       const pddl_iset_t *group,
                       pddl_cp_t *cp,
                       int op_var_offset,
                       pddl_err_t *err)
{
    int pre_size = op->pre.fact_size + 1;
    int eff_size = op->eff.fact_size + 1;
    int *pre_vals = ALLOC_ARR(int, pddlISetSize(group) * pre_size);
    int pre_vals_size = 0;
    int *eff_vals = ALLOC_ARR(int, pddlISetSize(group) * eff_size);
    int eff_vals_size = 0;

    int other_op_id;
    PDDL_ISET_FOR_EACH(group, other_op_id){
        const pddl_fdr_op_t *other_op = fdr->op.op[other_op_id];
        // TODO: ignore costs
        if (!cfg->ignore_costs && other_op->cost > op->cost)
            continue;
        int idx = pre_vals_size * pre_size;
        pre_vals[idx++] = other_op->id;
        for (int fi = 0; fi < other_op->pre.fact_size; ++fi)
            pre_vals[idx++] = other_op->pre.fact[fi].val;
        ++pre_vals_size;

        idx = eff_vals_size * eff_size;
        eff_vals[idx++] = other_op->id;
        for (int fi = 0; fi < other_op->eff.fact_size; ++fi)
            eff_vals[idx++] = other_op->eff.fact[fi].val;
        ++eff_vals_size;
    }

    int *pre_var = ALLOC_ARR(int, pre_size);
    pre_var[0] = op->id + op_var_offset;
    for (int fi = 0; fi < op->pre.fact_size; ++fi){
        int pvar = op->pre.fact[fi].var;
        int pval = op->pre.fact[fi].val;
        pre_var[fi + 1] = fdr->var.var[pvar].val[pval].global_id;
    }

    int *eff_var = ALLOC_ARR(int, eff_size);
    eff_var[0] = op->id + op_var_offset;
    for (int fi = 0; fi < op->eff.fact_size; ++fi){
        int pvar = op->eff.fact[fi].var;
        int pval = op->eff.fact[fi].val;
        eff_var[fi + 1] = fdr->var.var[pvar].val[pval].global_id;
    }

    pddlCPAddConstrIVarAllowed(cp, pre_size, pre_var, pre_vals_size, pre_vals);
    pddlCPAddConstrIVarAllowed(cp, eff_size, eff_var, eff_vals_size, eff_vals);
    FREE(pre_var);
    FREE(eff_var);
    FREE(pre_vals);
    FREE(eff_vals);
    return 2;
}

static int fdrSetModel(const pddl_fdr_t *fdr,
                       const pddl_endomorphism_config_t *cfg,
                       const op_groups_t *opg,
                       pddl_cp_t *cp,
                       pddl_time_limit_t *time_limit,
                       pddl_err_t *err)
{
    // Create fact variables
    for (int fi = 0; fi < fdr->var.global_id_size; ++fi){
        const pddl_fdr_val_t *val = fdr->var.global_id_to_val[fi];
        const pddl_fdr_var_t *var = fdr->var.var + val->var_id;
        char name[128];
        snprintf(name, 128, "%d:(%s)", fi, val->name);
        int id = pddlCPAddIVar(cp, 0, var->val_size - 1, name);
        ASSERT_RUNTIME(id == fi);
    }
    LOG(err, "Created %{num_fact_vars}d fact variables",
        fdr->var.global_id_size);
    if (pddlTimeLimitCheck(time_limit) != 0)
        PDDL_ERR_RET(err, -1, "Time limit reached.");

    // Create operator variables
    int op_var_offset = fdr->var.global_id_size;
    for (int oi = 0; oi < fdr->op.op_size; ++oi){
        const pddl_fdr_op_t *op = fdr->op.op[oi];
        char name[128];
        snprintf(name, 128, "%d:(%s)", oi, op->name);
        int id = pddlCPAddIVar(cp, 0, fdr->op.op_size - 1, name);
        ASSERT_RUNTIME(op->id + op_var_offset == id);
    }
    LOG(err, "Created %{num_op_vars}d operator variables", fdr->op.op_size);
    if (pddlTimeLimitCheck(time_limit) != 0)
        PDDL_ERR_RET(err, -1, "Time limit reached.");

    // Set init constraint
    for (int vi = 0; vi < fdr->var.var_size; ++vi){
        int fact_id = fdr->var.var[vi].val[fdr->init[vi]].global_id;
        pddlCPAddConstrIVarEq(cp, fact_id, fdr->init[vi]);
    }

    // Set goal constraint
    for (int fi = 0; fi < fdr->goal.fact_size; ++fi){
        int var = fdr->goal.fact[fi].var;
        int val = fdr->goal.fact[fi].val;
        int fact_id = fdr->var.var[var].val[val].global_id;
        pddlCPAddConstrIVarEq(cp, fact_id, val);
    }
    LOG(err, "Added init and goal constraints");
    if (pddlTimeLimitCheck(time_limit) != 0)
        PDDL_ERR_RET(err, -1, "Time limit reached.");

    // Set operator constraints
    int num_op_constr = 0;
    for (int group_id = 0; group_id < opg->group_size; ++group_id){
        if (pddlTimeLimitCheck(time_limit) != 0)
            PDDL_ERR_RET(err, -1, "Time limit reached.");

        int op_id;
        const pddl_iset_t *group = &opg->group[group_id];
        PDDL_ISET_FOR_EACH(group, op_id){
            if (pddlTimeLimitCheck(time_limit) != 0)
                PDDL_ERR_RET(err, -1, "Time limit reached.");

            const pddl_fdr_op_t *op = fdr->op.op[op_id];
            num_op_constr += fdrOpConstr(fdr, cfg, op, group, cp,
                                         op_var_offset, err);
        }
        //PDDL_INFO(err, "  Created operator constraints %d",
        //         pddlISetSize(&opg.group[group_id]));
    }
    LOG(err, "Added %{num_op_constrs}d operator constraints", num_op_constr);

    PDDL_ISET(op_vars);
    for (int oi = 0; oi < fdr->op.op_size; ++oi)
        pddlISetAdd(&op_vars, oi + op_var_offset);
    pddlCPSetObjectiveMinCountDiff(cp, &op_vars);
    pddlISetFree(&op_vars);
    LOG(err, "  Added objective function min(count-diff())");
    return 0;
}

static void extractSol(const int *cpsol,
                       int fact_size,
                       int op_size,
                       pddl_endomorphism_sol_t *sol)
{
    sol->op_size = op_size;
    sol->op_map = ALLOC_ARR(int, op_size);
    memcpy(sol->op_map, cpsol + fact_size, sizeof(int) * op_size);

    int *mapped_to = CALLOC_ARR(int, op_size);
    for (int oi = 0; oi < op_size; ++oi)
        mapped_to[sol->op_map[oi]] = 1;
    for (int oi = 0; oi < op_size; ++oi){
        if (mapped_to[oi]){
            // get rid of symmetries
            sol->op_map[oi] = oi;
        }else{
            pddlISetAdd(&sol->redundant_ops, oi);
        }
    }

    if (mapped_to != NULL)
        FREE(mapped_to);
}

void pddlEndomorphismSolFree(pddl_endomorphism_sol_t *sol)
{
    pddlISetFree(&sol->redundant_ops);
    if (sol->op_map != NULL)
        FREE(sol->op_map);
}


int pddlEndomorphismFDR(const pddl_fdr_t *fdr,
                        const pddl_endomorphism_config_t *cfg,
                        pddl_endomorphism_sol_t *sol,
                        pddl_err_t *err)
{
    CTX(err, "endo_fdr", "Endo-FDR");
    ZEROIZE(sol);

    pddl_time_limit_t time_limit;
    pddlTimeLimitInit(&time_limit);
    if (cfg->max_time > 0.)
        pddlTimeLimitSet(&time_limit, cfg->max_time);

    int ret = 0;
    LOG(err, "Endomorphism on FDR (facts: %d, ops: %d) ...",
        fdr->var.global_id_size, fdr->op.op_size);
    op_groups_t opg;
    opGroupsInitFDR(&opg, fdr);
    LOG(err, "Operators grouped into %d groups", opg.group_size);

    pddl_cp_t cp;
    pddlCPInit(&cp);
    if (fdrSetModel(fdr, cfg, &opg, &cp, &time_limit, err) != 0){
        opGroupsFree(&opg);
        pddlCPFree(&cp);
        CTXEND(err);
        PDDL_TRACE_RET(err, -2);
    }
    LOG(err, "Created model.");

    pddlCPSimplify(&cp);
    LOG(err, "Model simplified.");

    pddl_cp_solve_config_t sol_cfg = PDDL_CP_SOLVE_CONFIG_INIT;
    if (cfg->max_search_time > 0)
        sol_cfg.max_search_time = pddlTimeLimitRemain(&time_limit);
    sol_cfg.run_in_subprocess = cfg->run_in_subprocess;

    pddl_cp_sol_t cpsol;
    int sret = pddlCPSolve(&cp, &sol_cfg, &cpsol, err);
    // There must exist a solution -- at least identity
    ASSERT_RUNTIME(sret == PDDL_CP_FOUND
                    || sret == PDDL_CP_FOUND_SUBOPTIMAL
                    || sret == PDDL_CP_ABORTED);
    if (sret == PDDL_CP_FOUND || sret == PDDL_CP_FOUND_SUBOPTIMAL){
        ASSERT_RUNTIME(cpsol.num_solutions == 1);
        extractSol(cpsol.isol[0], fdr->var.global_id_size, fdr->op.op_size, sol);
        LOG(err, "Found a solution with %d redundant ops",
            pddlISetSize(&sol->redundant_ops));
        if (sret == PDDL_CP_FOUND)
            sol->is_optimal = 1;

    }else if (sret == PDDL_CP_ABORTED){
        LOG(err, "Solver was aborted.");
        ret = -1;
    }
    pddlCPSolFree(&cpsol);

    pddlCPFree(&cp);
    opGroupsFree(&opg);
    CTXEND(err);
    return ret;
}

int pddlEndomorphismFDRRedundantOps(const pddl_fdr_t *fdr,
                                    const pddl_endomorphism_config_t *cfg,
                                    pddl_iset_t *redundant_ops,
                                    pddl_err_t *err)
{
    pddl_endomorphism_sol_t sol;
    int ret = pddlEndomorphismFDR(fdr, cfg, &sol, err);
    if (ret == 0)
        pddlISetUnion(redundant_ops, &sol.redundant_ops);
    pddlEndomorphismSolFree(&sol);
    return ret;
}
