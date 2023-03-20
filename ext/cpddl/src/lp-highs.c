/***
 * Copyright (c)2022 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

#include "internal.h"
#include "pddl/lp.h"
#include "_lp.h"

#ifdef PDDL_HIGHS
#include <interfaces/highs_c_api.h>

#define TOLERANCE 1E-5
#define MIP_TOLERANCE 1E-5
#define PDDL_LP_MIN_BOUND -1E20
#define PDDL_LP_MAX_BOUND 1E20

enum pddl_lp_col_type {
    PDDL_LP_COL_TYPE_REAL,
    PDDL_LP_COL_TYPE_INT,
    PDDL_LP_COL_TYPE_BINARY,
};
typedef enum pddl_lp_col_type pddl_lp_col_type_t;

struct pddl_lp_col {
    double obj;
    pddl_lp_col_type_t type;
    double lb;
    double ub;
};
typedef struct pddl_lp_col pddl_lp_col_t;

struct pddl_lp_coef {
    int col;
    double coef;
};
typedef struct pddl_lp_coef pddl_lp_coef_t;

struct pddl_lp_row {
    pddl_lp_coef_t *coef;
    int coef_size;
    int coef_alloc;
    double lb;
    double ub;
};
typedef struct pddl_lp_row pddl_lp_row_t;

struct _lp_t {
    pddl_lp_t cls;
    pddl_lp_col_t *col;
    int col_size;
    int col_alloc;
    pddl_lp_row_t *row;
    int row_size;
    int row_alloc;
};
typedef struct _lp_t lp_t;

#define LP(l) pddl_container_of((l), lp_t, cls)

static void addCols(pddl_lp_t *_lp, int cnt);
static void addRow(lp_t *lp, const double rhs, const char sense);

static void freeRow(pddl_lp_row_t *row)
{
    if (row->coef != NULL)
        FREE(row->coef);
}


static pddl_lp_t *new(const pddl_lp_config_t *cfg, pddl_err_t *err)
{
    lp_t *lp = ZALLOC(lp_t);
    lp->cls.cls = &pddl_lp_highs;
    lp->cls.err = err;
    lp->cls.cfg = *cfg;

    if (cfg->cols > 0)
        addCols(&lp->cls, cfg->cols);
    if (cfg->rows > 0){
        for (int i = 0; i < cfg->rows; ++i)
            addRow(lp, 0., 'L');
    }
    return &lp->cls;
}

static void del(pddl_lp_t *_lp)
{
    lp_t *lp = LP(_lp);
    if (lp->col != NULL)
        FREE(lp->col);
    for (int ri = 0; ri < lp->row_size; ++ri)
        freeRow(lp->row + ri);
    if (lp->row != NULL)
        FREE(lp->row);
    FREE(lp);
}

static void setObj(pddl_lp_t *_lp, int i, double coef)
{
    lp_t *lp = LP(_lp);
    PANIC_IF(i < 0 || i >= lp->col_size, "Column %d out of range", i);
    lp->col[i].obj = coef;
}

static void setVarRange(pddl_lp_t *_lp, int i, double lb, double ub)
{
    lp_t *lp = LP(_lp);
    PANIC_IF(i < 0 || i >= lp->col_size, "Column %d out of range", i);
    if (lb <= PDDL_LP_MIN_BOUND)
        lb = PDDL_LP_MIN_BOUND;
    if (ub >= PDDL_LP_MAX_BOUND)
        ub = PDDL_LP_MAX_BOUND;
    lp->col[i].lb = lb;
    lp->col[i].ub = ub;
}

static void setVarFree(pddl_lp_t *_lp, int i)
{
    setVarRange(_lp, i, PDDL_LP_MIN_BOUND, PDDL_LP_MAX_BOUND);
}

static void setVarInt(pddl_lp_t *_lp, int i)
{
    lp_t *lp = LP(_lp);
    PANIC_IF(i < 0 || i >= lp->col_size, "Column %d out of range", i);
    lp->col[i].type = PDDL_LP_COL_TYPE_INT;
}

static void setVarBinary(pddl_lp_t *_lp, int i)
{
    lp_t *lp = LP(_lp);
    PANIC_IF(i < 0 || i >= lp->col_size, "Column %d out of range", i);
    lp->col[i].type = PDDL_LP_COL_TYPE_BINARY;
}

static void setCoef(pddl_lp_t *_lp, int row, int col, double coef)
{
    lp_t *lp = LP(_lp);
    PANIC_IF(row < 0 || row >= lp->row_size, "Row %d out of range", row);
    PANIC_IF(col < 0 || col >= lp->col_size, "Column %d out of range", col);
    pddl_lp_row_t *r = lp->row + row;
    if (r->coef_size == r->coef_alloc){
        if (r->coef_alloc == 0)
            r->coef_alloc = 8;
        r->coef_alloc *= 2;
        r->coef = REALLOC_ARR(r->coef, pddl_lp_coef_t, r->coef_alloc);
    }

    if (r->coef_size == 0 || r->coef[r->coef_size - 1].col < col){
        pddl_lp_coef_t *c = r->coef + r->coef_size++;
        c->col = col;
        c->coef = coef;

    }else{
        int idx;
        for (idx = r->coef_size - 1; idx >= 0; --idx){
            if (r->coef[idx].col <= col)
                break;
        }

        if (idx < 0 || r->coef[idx].col < col){
            for (int i = r->coef_size - 1; i > idx; --i)
                r->coef[i + 1] = r->coef[i];
            r->coef[idx + 1].col = col;
            r->coef[idx + 1].coef = coef;
            ++r->coef_size;

        }else{ // r->coef[idx].col == col
            r->coef[idx].coef = coef;
        }
    }

    // TODO: If coef == 0. delete coef
}

static void setRHS(pddl_lp_t *_lp, int row, double rhs, char sense)
{
    lp_t *lp = LP(_lp);
    PANIC_IF(row < 0 || row >= lp->row_size, "Row %d out of range", row);
    if (sense == 'L'){
        lp->row[row].lb = PDDL_LP_MIN_BOUND;
        lp->row[row].ub = rhs;

    }else if (sense == 'G'){
        lp->row[row].lb = rhs;
        lp->row[row].ub = PDDL_LP_MAX_BOUND;

    }else if (sense == 'E'){
        lp->row[row].lb = rhs;
        lp->row[row].ub = rhs;

    }else{
        PANIC_IF(1, "Unkown sense '%c'", sense);
    }
}

static void addRow(lp_t *lp, const double rhs, const char sense)
{
    if (lp->row_size == lp->row_alloc){
        if (lp->row_alloc == 0)
            lp->row_alloc = 16;
        lp->row_alloc *= 2;
        lp->row = REALLOC_ARR(lp->row, pddl_lp_row_t, lp->row_alloc);
    }
    pddl_lp_row_t *row = lp->row + lp->row_size++;
    ZEROIZE(row);
    setRHS(&lp->cls, lp->row_size - 1, rhs, sense);
}

static void addRows(pddl_lp_t *_lp, int cnt, const double *rhs, const char *sense)
{
    lp_t *lp = LP(_lp);
    for (int i = 0; i < cnt; ++i)
        addRow(lp, rhs[i], sense[i]);
}

static void delRows(pddl_lp_t *_lp, int begin, int end)
{
    lp_t *lp = LP(_lp);
    for (int i = begin; i < end + 1; ++i)
        freeRow(lp->row + i);
    int ins = begin;
    for (int i = end + 1; i < lp->row_size; ++i)
        lp->row[ins++] = lp->row[i];
    lp->row_size = ins;
}

static int numRows(const pddl_lp_t *_lp)
{
    lp_t *lp = LP(_lp);
    return lp->row_size;
}

static void addCol(lp_t *lp)
{
    if (lp->col_size == lp->col_alloc){
        if (lp->col_alloc == 0)
            lp->col_alloc = 16;
        lp->col_alloc *= 2;
        lp->col = REALLOC_ARR(lp->col, pddl_lp_col_t, lp->col_alloc);
    }
    pddl_lp_col_t *col = lp->col + lp->col_size++;
    ZEROIZE(col);
    col->obj = 0.;
    col->type = PDDL_LP_COL_TYPE_REAL;
    col->lb = PDDL_LP_MIN_BOUND;
    col->ub = PDDL_LP_MAX_BOUND;
}

static void addCols(pddl_lp_t *_lp, int cnt)
{
    lp_t *lp = LP(_lp);
    for (int i = 0; i < cnt; ++i)
        addCol(lp);
}

static void delCols(pddl_lp_t *_lp, int begin, int end)
{
    //lp_t *lp = LP(_lp);
    // TODO
    PANIC_IF(1, "Deleting columns not implemented yet.");
}

static int numCols(const pddl_lp_t *_lp)
{
    lp_t *lp = LP(_lp);
    return lp->col_size;
}

static void *createModel(lp_t *lp)
{
    int num_col = lp->col_size;
    int num_row = lp->row_size;
    int num_nz = 0;
    for (int ri = 0; ri < lp->row_size; ++ri)
        num_nz += lp->row[ri].coef_size;

    int sense = kHighsObjSenseMinimize;
    if (lp->cls.cfg.maximize)
        sense = kHighsObjSenseMaximize;

    double offset = 0.;
    double *col_cost = ALLOC_ARR(double, num_col);
    double *col_lower = ALLOC_ARR(double, num_col);
    double *col_upper = ALLOC_ARR(double, num_col);
    int is_mip = 0;
    for (int i = 0; i < lp->col_size; ++i){
        col_cost[i] = lp->col[i].obj;
        col_lower[i] = lp->col[i].lb;
        col_upper[i] = lp->col[i].ub;
        if (lp->col[i].type == PDDL_LP_COL_TYPE_BINARY){
            if (col_lower[i] < 0.){
                col_lower[i] = 0.;
            }else if (col_lower[i] > 1.){
                col_lower[i] = 1.;
            }

            if (col_upper[i] > 1.){
                col_upper[i] = 1.;
            }else if (col_upper[i] < 0.){
                col_upper[i] = 0.;
            }
        }
        if (lp->col[i].type == PDDL_LP_COL_TYPE_BINARY
                || lp->col[i].type == PDDL_LP_COL_TYPE_INT){
            is_mip = 1;
        }
    }

    double *row_lower = ALLOC_ARR(double, num_row);
    double *row_upper = ALLOC_ARR(double, num_row);
    for (int i = 0; i < lp->row_size; ++i){
        row_lower[i] = lp->row[i].lb;
        row_upper[i] = lp->row[i].ub;
    }

    int a_format = kHighsMatrixFormatRowwise;
    HighsInt *a_start = ALLOC_ARR(HighsInt, num_row);
    HighsInt *a_index = ALLOC_ARR(HighsInt, num_nz);
    double *a_value = ALLOC_ARR(double, num_nz);

    int ins = 0;
    for (int ri = 0; ri < num_row; ++ri){
        a_start[ri] = ins;
        for (int ci = 0; ci < lp->row[ri].coef_size; ++ci){
            a_index[ins] = lp->row[ri].coef[ci].col;
            a_value[ins] = lp->row[ri].coef[ci].coef;
            ++ins;
        }
    }

    HighsInt *integrality = NULL;
    if (is_mip){
        integrality = ALLOC_ARR(HighsInt, num_col);
        for (int i = 0; i < num_col; ++i){
            switch (lp->col[i].type){
                case PDDL_LP_COL_TYPE_REAL:
                    integrality[i] = kHighsVarTypeContinuous;
                    break;
                case PDDL_LP_COL_TYPE_INT:
                    integrality[i] = kHighsVarTypeInteger;
                    break;
                case PDDL_LP_COL_TYPE_BINARY:
                    integrality[i] = kHighsVarTypeInteger;
                    break;
            }
        }
    }

    void *model = Highs_create();
    PANIC_IF(model == NULL, "Could not create a HiGHS model.");

    int num_threads = PDDL_MAX(lp->cls.cfg.num_threads, 1);
    Highs_setIntOptionValue(model, "threads", num_threads);
    if (lp->cls.cfg.time_limit > 0.)
        Highs_setDoubleOptionValue(model, "time_limit", lp->cls.cfg.time_limit);
    Highs_setBoolOptionValue(model, "output_flag", 0);
    //Highs_setIntOptionValue(model, "log_dev_level", 2);
    //Highs_setIntOptionValue(model, "highs_debug_level", 2);
    Highs_setDoubleOptionValue(model, "primal_feasibility_tolerance", TOLERANCE);
    Highs_setDoubleOptionValue(model, "dual_feasibility_tolerance", TOLERANCE);
    Highs_setDoubleOptionValue(model, "mip_feasibility_tolerance", MIP_TOLERANCE);

    HighsInt st = 0;
    if (is_mip){
        st = Highs_passMip(model, num_col, num_row, num_nz, a_format,
                           sense, offset, col_cost, col_lower, col_upper,
                           row_lower, row_upper, a_start, a_index, a_value,
                           integrality);
    }else{
        st = Highs_passLp(model, num_col, num_row, num_nz, a_format,
                          sense, offset, col_cost, col_lower, col_upper,
                          row_lower, row_upper, a_start, a_index, a_value);
    }

    FREE(col_cost);
    FREE(col_lower);
    FREE(col_upper);
    FREE(row_lower);
    FREE(row_upper);
    FREE(a_start);
    FREE(a_index);
    FREE(a_value);
    if (integrality != NULL)
        FREE(integrality);

    if (st == kHighsStatusError){
        // TODO: Not sure how to recover from this...
        return NULL;

    }else if (st == kHighsStatusWarning){
        return NULL;
    }

    //Highs_writeModel(model, "model.lp");
    return model;
}

static int solve(pddl_lp_t *_lp, double *val, double *obj)
{
    lp_t *lp = LP(_lp);
    int ret = 0;

    void *model = createModel(lp);
    if (model == NULL){
        LOG(_lp->err, "Something went wrong with the creation of model!");
        return -1;
    }

    HighsInt st = Highs_run(model);
    if (st == kHighsStatusError){
        LOG(_lp->err, "Something went wrong during solving the model!");
        Highs_destroy(model);
        return -1;
    }else if (st == kHighsStatusWarning){
        // TODO
    }

    HighsInt modelst = Highs_getModelStatus(model);
    if (modelst == kHighsModelStatusNotset){
        LOG(_lp->err, "Model status not set");
        ret = -1;

    }else if (modelst == kHighsModelStatusLoadError){
        LOG(_lp->err, "Model load error!");
        ret = -1;

    }else if (modelst == kHighsModelStatusModelError){
        LOG(_lp->err, "Model error!");
        ret = -1;

    }else if (modelst == kHighsModelStatusPresolveError){
        LOG(_lp->err, "Presolve error!");
        ret = -1;

    }else if (modelst == kHighsModelStatusSolveError){
        LOG(_lp->err, "Solve error!");
        ret = -1;

    }else if (modelst == kHighsModelStatusPostsolveError){
        LOG(_lp->err, "Postsolve error!");
        ret = -1;

    }else if (modelst == kHighsModelStatusModelEmpty){
        LOG(_lp->err, "Model is empty!");
        ret = -1;

    }else if (modelst == kHighsModelStatusOptimal){
        //LOG(_lp->err, "Model has optimal solution.");
        ret = 0;

    }else if (modelst == kHighsModelStatusInfeasible){
        LOG(_lp->err, "Solution is infeasible.");
        ret = -1;

    }else if (modelst == kHighsModelStatusUnboundedOrInfeasible){
        LOG(_lp->err, "Solution is unbounded or infeasible.");
        ret = -1;

    }else if (modelst == kHighsModelStatusUnbounded){
        LOG(_lp->err, "Solution is unbounded.");
        ret = -1;

    }else if (modelst == kHighsModelStatusObjectiveBound){
        LOG(_lp->err, "Bound on objective reached.");
        ret = -1;

    }else if (modelst == kHighsModelStatusObjectiveTarget){
        LOG(_lp->err, "Target for objective reached.");
        ret = -1;

    }else if (modelst == kHighsModelStatusTimeLimit){
        //LOG(_lp->err, "Time limit.");
        //ret = -1;
        ret = 0;

    }else if (modelst == kHighsModelStatusIterationLimit){
        //LOG(_lp->err, "Iteration limit.");
        //ret = -1;
        ret = 0;

    }else if (modelst == kHighsModelStatusUnknown){
        LOG(_lp->err, "Unkown solution status");
        ret = -1;

    }else{
        LOG(_lp->err, "Unkown solution status: %d", (int)modelst);
    }

    HighsInt solst;
    Highs_getIntInfoValue(model, "primal_solution_status", &solst);
    if (solst == kHighsSolutionStatusFeasible){
        if (val != NULL)
            *val = Highs_getObjectiveValue(model);
        if (obj != NULL)
            Highs_getSolution(model, obj, NULL, NULL, NULL);
        ret = 0;

    }else{
        ret = -1;
    }

    Highs_destroy(model);

    return ret;
}

static void cpxWrite(pddl_lp_t *_lp, const char *fn)
{
    lp_t *lp = LP(_lp);
    void *model = createModel(lp);
    Highs_writeModel(model, fn);
    Highs_destroy(model);
}



#define TOSTR1(x) #x
#define TOSTR(x) TOSTR1(x)
pddl_lp_cls_t pddl_lp_highs = {
    PDDL_LP_HIGHS,
    "HiGHS",
    TOSTR(HIGHS_VERSION_MAJOR.HIGHS_VERSION_MINOR.HIGHS_VERSION_PATCH),
    new,
    del,
    setObj,
    setVarRange,
    setVarFree,
    setVarInt,
    setVarBinary,
    setCoef,
    setRHS,
    addRows,
    delRows,
    numRows,
    addCols,
    delCols,
    numCols,
    solve,
    cpxWrite,
};
#else /* PDDL_HIGHS */
pddl_lp_cls_t pddl_lp_highs = { 0 };
#endif /* PDDL_HIGHS */
