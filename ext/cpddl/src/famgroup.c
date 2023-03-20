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
#include "pddl/lp.h"
#include "pddl/timer.h"
#include "pddl/famgroup.h"
#include "pddl/set.h"

struct fam {
    pddl_famgroup_config_t cfg;
    const pddl_strips_t *strips;
    pddl_mgroups_t *mgroups;
    pddl_err_t *err;

    pddl_lp_t *lp;
    int lp_var_size;
    int row; /*!< ID of the next row */
};
typedef struct fam fam_t;

static int varId(const pddl_strips_t *strips, int fact_id)
{
    return fact_id;
}

static void getPredel(pddl_iset_t *predel, const pddl_strips_op_t *op)
{
    pddlISetEmpty(predel);
    pddlISetUnion(predel, &op->pre);
    pddlISetIntersect(predel, &op->del_eff);
}

static int nextRow(fam_t *fam, double rhs, char sense)
{
    if (fam->row >= pddlLPNumRows(fam->lp)){
        pddlLPAddRows(fam->lp, 1, &rhs, &sense);
    }else{
        pddlLPSetRHS(fam->lp, fam->row, rhs, sense);
    }
    return fam->row++;
}

static void initStateConstr(fam_t *fam)
{
    int fact;
    PDDL_ISET_FOR_EACH(&fam->strips->init, fact)
        pddlLPSetCoef(fam->lp, fam->row, varId(fam->strips, fact), 1.);
    pddlLPSetRHS(fam->lp, fam->row, 1., 'L');
    ++fam->row;
}

static void opConstrs(fam_t *fam)
{
    const pddl_strips_op_t *op;
    int fact;
    PDDL_ISET(predel);

    PDDL_STRIPS_OPS_FOR_EACH(&fam->strips->op, op){
        PDDL_ISET_FOR_EACH(&op->add_eff, fact)
            pddlLPSetCoef(fam->lp, fam->row, varId(fam->strips, fact), 1.);

        getPredel(&predel, op);
        PDDL_ISET_FOR_EACH(&predel, fact)
            pddlLPSetCoef(fam->lp, fam->row, varId(fam->strips, fact), -1.);
        pddlLPSetRHS(fam->lp, fam->row, 0., 'L');
        ++fam->row;
    }
    pddlISetFree(&predel);
}

static void goalConstr(fam_t *fam)
{
    int row = nextRow(fam, 1, 'G');
    int fact;
    PDDL_ISET_FOR_EACH(&fam->strips->goal, fact)
        pddlLPSetCoef(fam->lp, row, varId(fam->strips, fact), 1.);

}

static void skipMGroupAndSubsetsConstr(fam_t *fam, const pddl_iset_t *facts)
{
    int row = nextRow(fam, 1., 'G');

    const int size = pddlISetSize(facts);
    int fi = 0;
    for (int fact_id = 0; fact_id < fam->strips->fact.fact_size; ++fact_id){
        if (fi < size && pddlISetGet(facts, fi) == fact_id){
            ++fi;
        }else{
            pddlLPSetCoef(fam->lp, row, varId(fam->strips, fact_id), 1.);
        }
    }
}

static void skipMGroupExactlyConstr(fam_t *fam, const pddl_iset_t *facts)
{
    int row = nextRow(fam, pddlISetSize(facts) - 1, 'L');
    int fact;

    PDDL_ISET_FOR_EACH(facts, fact)
        pddlLPSetCoef(fam->lp, row, varId(fam->strips, fact), 1.);
}

static void skipMGroup(fam_t *fam, const pddl_iset_t *facts)
{
    if (fam->cfg.maximal){
        skipMGroupAndSubsetsConstr(fam, facts);
    }else{
        skipMGroupExactlyConstr(fam, facts);
    }
}


static void objToFAMGroup(const double *obj,
                          const pddl_strips_t *strips,
                          pddl_iset_t *fam_group)
{
    pddlISetEmpty(fam_group);

    for (int fact_id = 0; fact_id < strips->fact.fact_size; ++fact_id){
        int var_id = varId(strips, fact_id);
        if (obj[var_id] > 0.5)
            pddlISetAdd(fam_group, fact_id);
    }
}

static pddl_mgroup_t *addFAMGroup(pddl_mgroups_t *mgs,
                                  const pddl_iset_t *fset,
                                  const pddl_strips_t *strips)
{
    pddl_mgroup_t *mg;

    mg = pddlMGroupsAdd(mgs, fset);
    mg->is_fam_group = 1;
    mg->is_exactly_one = 0;

    return mg;
}

static void genSymmetricFAMGroups(fam_t *fam, const pddl_iset_t *mgfacts)
{
    pddl_set_iset_t set_of_mgroups;

    pddlSetISetInit(&set_of_mgroups);
    pddlSetISetAdd(&set_of_mgroups, mgfacts);
    pddlStripsSymAllFactSetSymmetries(fam->cfg.sym, &set_of_mgroups);
    const pddl_iset_t *fset;
    PDDL_SET_ISET_FOR_EACH(&set_of_mgroups, fset){
        if (!fam->cfg.keep_only_asymetric)
            addFAMGroup(fam->mgroups, fset, fam->strips);
        skipMGroup(fam, fset);
    }
    pddlSetISetFree(&set_of_mgroups);
}

static void prioritizeUncovered(fam_t *fam)
{
    PDDL_ISET(covered);
    for (int mi = 0; mi < fam->mgroups->mgroup_size; ++mi)
        pddlISetUnion(&covered, &fam->mgroups->mgroup[mi].mgroup);

    for (int fact_id = 0; fact_id < fam->strips->fact.fact_size; ++fact_id){
        int var_id = varId(fam->strips, fact_id);
        if (pddlISetIn(fact_id, &covered)){
            pddlLPSetObj(fam->lp, var_id, 1.);
        }else{
            pddlLPSetObj(fam->lp, var_id, pddlISetSize(&covered));
        }
        pddlLPSetVarBinary(fam->lp, var_id);
    }
    pddlISetFree(&covered);
}

static void famInit(fam_t *fam,
                    pddl_mgroups_t *mgroups,
                    const pddl_strips_t *strips,
                    const pddl_famgroup_config_t *cfg,
                    pddl_err_t *err)
{
    ZEROIZE(fam);
    fam->cfg = *cfg;
    fam->strips = strips;
    fam->mgroups = mgroups;
    fam->err = err;
    fam->row = 0;

    if (fam->cfg.limit <= 0)
        fam->cfg.limit = INT_MAX;

    if (!pddlLPSolverAvailable(PDDL_LP_DEFAULT)){
        PANIC("Missing LP solver! Exiting...");
    }

    fam->lp_var_size = strips->fact.fact_size;
    pddl_lp_config_t lpcfg = PDDL_LP_CONFIG_INIT;
    lpcfg.maximize = 1;
    lpcfg.rows = strips->op.op_size + 1;
    lpcfg.cols = fam->lp_var_size;
    fam->lp = pddlLPNew(&lpcfg, err);

    // Set up coeficients in the objective function and set up binary
    // variables
    for (int fact_id = 0; fact_id < strips->fact.fact_size; ++fact_id){
        int var_id = varId(strips, fact_id);
        pddlLPSetObj(fam->lp, var_id, 1.);
        pddlLPSetVarBinary(fam->lp, var_id);
    }

    // Initial state constraint
    initStateConstr(fam);
    // Operator constraints
    opConstrs(fam);

    if (fam->cfg.goal)
        goalConstr(fam);

    // Skip mutex groups already stored in mgroups
    for (int i = 0; i < mgroups->mgroup_size; ++i)
        skipMGroup(fam, &mgroups->mgroup[i].mgroup);

    LOG(err, "Created LP with %{lp_vars}d variables"
        " and %{lp_constrs}d constraints",
        fam->lp_var_size, fam->row);
}

static void famFree(fam_t *fam)
{
    pddlLPDel(fam->lp);
}

static void famInfer(fam_t *fam)
{
    PDDL_ISET(famgroup);
    double val, *obj;
    pddl_mgroup_t *mg;
    pddl_timer_t timer;
    int last_info = 0;

    pddlTimerStart(&timer);

    obj = ALLOC_ARR(double, pddlLPNumCols(fam->lp));
    for (int i = 0;
            pddlLPSolve(fam->lp, &val, obj) == 0
                && val > 0.5 && i < fam->cfg.limit;
            ++i){
        objToFAMGroup(obj, fam->strips, &famgroup);
        mg = addFAMGroup(fam->mgroups, &famgroup, fam->strips);
        skipMGroup(fam, &mg->mgroup);
        if (fam->cfg.sym != NULL)
            genSymmetricFAMGroups(fam, &mg->mgroup);

        if (fam->cfg.prioritize_uncovered)
            prioritizeUncovered(fam);

        pddlTimerStop(&timer);
        float elapsed = pddlTimerElapsedInSF(&timer);
        if ((int)elapsed > last_info){
            LOG(fam->err, "  Inference of fam-groups: fam-groups: %d", i + 1);
            last_info = elapsed;
        }

        if (fam->cfg.time_limit > 0. && elapsed > fam->cfg.time_limit)
            break;
    }
    FREE(obj);
    pddlISetFree(&famgroup);
}

int pddlFAMGroupsInfer(pddl_mgroups_t *mgs,
                       const pddl_strips_t *strips,
                       const pddl_famgroup_config_t *cfg,
                       pddl_err_t *err)
{
    if (strips->has_cond_eff)
        PANIC("fam-groups does not support conditional effects");

    CTX(err, "mg_fam", "MG-fam");
    fam_t fam;
    int start_num = mgs->mgroup_size;
    // TODO: pddlFAMGroupConfigLog()
    LOG(err, "Inference of fam-groups ["
        "maximal: %d, goal: %d, sym: %d, keep-only-asymetric: %d,"
        " prioritize-uncovered: %d,"
        " limit: %d, time-limit: %.2fs] ...",
        cfg->maximal,
        cfg->goal,
        (cfg->sym == NULL ? 0 : 1),
        cfg->keep_only_asymetric,
        cfg->prioritize_uncovered,
        cfg->limit,
        cfg->time_limit);

    famInit(&fam, mgs, strips, cfg, err);
    famInfer(&fam);
    famFree(&fam);

    LOG(err, "Inference of fam-groups DONE: %{fam_groups}d fam-groups found.",
        mgs->mgroup_size - start_num);
    CTXEND(err);
    return 0;
}



static int isDeadEndOp(const pddl_iset_t *mgroup,
                       const pddl_strips_op_t *op,
                       pddl_iset_t *madd,
                       pddl_iset_t *mpredel)
{
    if (pddlISetSize(&op->pre) < pddlISetSize(&op->del_eff)){
        pddlISetIntersect2(mpredel, mgroup, &op->pre);
        pddlISetIntersect(mpredel, &op->del_eff);
    }else{
        pddlISetIntersect2(mpredel, mgroup, &op->del_eff);
        pddlISetIntersect(mpredel, &op->pre);
    }
    pddlISetIntersect2(madd, mgroup, &op->add_eff);
    return pddlISetSize(mpredel) > pddlISetSize(madd);
}

static void deadEndOps(const pddl_iset_t *mgroup,
                       const pddl_strips_t *strips,
                       pddl_iset_t *madd,
                       pddl_iset_t *mpredel,
                       pddl_iset_t *dead_end)
{
    for (int op_id = 0; op_id < strips->op.op_size; ++op_id){
        const pddl_strips_op_t *op = strips->op.op[op_id];
        // Skip operators with conditional effects
        if (op->cond_eff_size > 0)
            continue;

        if (isDeadEndOp(mgroup, op, madd, mpredel))
            pddlISetAdd(dead_end, op->id);
    }
}

void pddlFAMGroupsDeadEndOps(const pddl_mgroups_t *mgs,
                             const pddl_strips_t *strips,
                             pddl_iset_t *dead_end_ops)
{
    PDDL_ISET(madd);
    PDDL_ISET(mpredel);

    for (int mi = 0; mi < mgs->mgroup_size; ++mi){
        const pddl_mgroup_t *mg = mgs->mgroup + mi;
        if (mg->is_fam_group
                && !pddlISetIsDisjunct(&strips->goal, &mg->mgroup)){
            deadEndOps(&mg->mgroup, strips, &madd, &mpredel, dead_end_ops);
        }
    }

    pddlISetFree(&madd);
    pddlISetFree(&mpredel);
}
