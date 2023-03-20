/***
 * Copyright (c)2017 Daniel Fiser <danfis@danfis.cz>
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
#include "pddl/lp.h"
#include "_lp.h"

#if defined(PDDL_CPLEX)
pddl_lp_cls_t *pddl_lp_default = &pddl_lp_cplex;
#elif defined(PDDL_GUROBI)
pddl_lp_cls_t *pddl_lp_default = &pddl_lp_gurobi;
#elif defined(PDDL_HIGHS)
pddl_lp_cls_t *pddl_lp_default = &pddl_lp_highs;
#elif defined(PDDL_GLPK)
pddl_lp_cls_t *pddl_lp_default = &pddl_lp_glpk;
#else
pddl_lp_cls_t *pddl_lp_default = &pddl_lp_not_available;
#endif

static pddl_lp_cls_t *getSolverCls(pddl_lp_solver_t solver)
{
    switch (solver){
        case PDDL_LP_CPLEX:
            return &pddl_lp_cplex;
        case PDDL_LP_GUROBI:
            return &pddl_lp_gurobi;
        case PDDL_LP_HIGHS:
            return &pddl_lp_highs;
        case PDDL_LP_GLPK:
            return &pddl_lp_glpk;
        default:
            return pddl_lp_default;
    }
}

void pddlLPConfigLog(const pddl_lp_config_t *cfg, pddl_err_t *err)
{
    LOG_CONFIG_INT(cfg, rows, err);
    LOG_CONFIG_INT(cfg, cols, err);
    LOG_CONFIG_BOOL(cfg, maximize, err);
    const pddl_lp_cls_t *cls = getSolverCls(cfg->solver);
    LOG(err, "solver = %{solver}s v%{solver_version}s",
        cls->solver_name, cls->solver_version);
    LOG_CONFIG_INT(cfg, num_threads, err);
    LOG_CONFIG_DBL(cfg, time_limit, err);
    LOG_CONFIG_BOOL(cfg, tune_int_operator_potential, err);
}

int pddlLPSolverAvailable(pddl_lp_solver_t solver)
{
    if (solver == PDDL_LP_CPLEX){
        if (pddl_lp_cplex.new != NULL)
            return 1;
        return 0;

    }else if (solver == PDDL_LP_GUROBI){
        if (pddl_lp_gurobi.new != NULL)
            return 1;
        return 0;

    }else if (solver == PDDL_LP_HIGHS){
        if (pddl_lp_highs.new != NULL)
            return 1;
        return 0;

    }else if (solver == PDDL_LP_GLPK){
        if (pddl_lp_glpk.new != NULL)
            return 1;
        return 0;
    }

    return pddlLPSolverAvailable(PDDL_LP_CPLEX)
            || pddlLPSolverAvailable(PDDL_LP_GUROBI)
            || pddlLPSolverAvailable(PDDL_LP_HIGHS)
            || pddlLPSolverAvailable(PDDL_LP_GLPK);
}

int pddlLPSetDefault(pddl_lp_solver_t solver, pddl_err_t *err)
{
    if (!pddlLPSolverAvailable(solver)){
        switch (solver){
            case PDDL_LP_CPLEX:
                WARN(err, "The CPLEX LP solver is not available");
                break;
            case PDDL_LP_GUROBI:
                WARN(err, "The Gurobi LP solver is not available");
                break;
            case PDDL_LP_HIGHS:
                WARN(err, "The HiGHS LP solver is not available");
                break;
            case PDDL_LP_GLPK:
                WARN(err, "The GLPK LP solver is not available");
                break;
            default:
                WARN(err, "Unkown LP solver identifier!");
        }
        return -1;
    }

    pddl_lp_default = getSolverCls(solver);
    return 0;
}

pddl_lp_t *pddlLPNew(const pddl_lp_config_t *cfg, pddl_err_t *err)
{
    pddl_lp_cls_t *cls = getSolverCls(cfg->solver);
    CTX_NO_TIME(err, "lp_init", "LP-Init");
    CTX_NO_TIME(err, "cfg", "Cfg");
    pddlLPConfigLog(cfg, err);
    CTXEND(err);
    pddl_lp_t *lp = cls->new(cfg, err);
    CTXEND(err);
    return lp;
}

void pddlLPDel(pddl_lp_t *lp)
{
    lp->cls->del(lp);
}

const char *pddlLPSolverName(const pddl_lp_t *lp)
{
    return lp->cls->solver_name;
}

int pddlLPSolverID(const pddl_lp_t *lp)
{
    return lp->cls->solver_id;
}

void pddlLPSetObj(pddl_lp_t *lp, int i, double coef)
{
    lp->cls->set_obj(lp, i, coef);
}

void pddlLPSetVarRange(pddl_lp_t *lp, int i, double lb, double ub)
{
    lp->cls->set_var_range(lp, i, lb, ub);
}

void pddlLPSetVarFree(pddl_lp_t *lp, int i)
{
    lp->cls->set_var_free(lp, i);
}

void pddlLPSetVarInt(pddl_lp_t *lp, int i)
{
    lp->cls->set_var_int(lp, i);
}

void pddlLPSetVarBinary(pddl_lp_t *lp, int i)
{
    lp->cls->set_var_binary(lp, i);
}

void pddlLPSetCoef(pddl_lp_t *lp, int row, int col, double coef)
{
    lp->cls->set_coef(lp, row, col, coef);
}

void pddlLPSetRHS(pddl_lp_t *lp, int row, double rhs, char sense)
{
    lp->cls->set_rhs(lp, row, rhs, sense);
}

void pddlLPAddRows(pddl_lp_t *lp, int cnt, const double *rhs, const char *sense)
{
    lp->cls->add_rows(lp, cnt, rhs, sense);
}

void pddlLPDelRows(pddl_lp_t *lp, int begin, int end)
{
    lp->cls->del_rows(lp, begin, end);
}

int pddlLPNumRows(const pddl_lp_t *lp)
{
    return lp->cls->num_rows(lp);
}

void pddlLPAddCols(pddl_lp_t *lp, int cnt)
{
    lp->cls->add_cols(lp, cnt);
}

void pddlLPDelCols(pddl_lp_t *lp, int begin, int end)
{
    lp->cls->del_cols(lp, begin, end);
}

int pddlLPNumCols(const pddl_lp_t *lp)
{
    return lp->cls->num_cols(lp);
}

int pddlLPSolve(pddl_lp_t *lp, double *val, double *obj)
{
    CTX(lp->err, "lp_solve", "LP-Solve");
    LOG(lp->err, "rows: %d, cols: %d", pddlLPNumRows(lp), pddlLPNumCols(lp));
    int ret = lp->cls->solve(lp, val, obj);
    CTXEND(lp->err);
    return ret;
}

void pddlLPWrite(pddl_lp_t *lp, const char *fn)
{
    lp->cls->write(lp, fn);
}




#define noSolverExit() \
    do { \
    fprintf(stderr, "Error: The requested LP solver is not available!\n"); \
    exit(-1); \
    } while (0)
static pddl_lp_t *noNew(const pddl_lp_config_t *cfg, pddl_err_t *err)
{ noSolverExit(); }
static void noDel(pddl_lp_t *lp)
{ noSolverExit(); }
static void noSetObj(pddl_lp_t *lp, int i, double coef)
{ noSolverExit(); }
static void noSetVarRange(pddl_lp_t *lp, int i, double lb, double ub)
{ noSolverExit(); }
static void noSetVarFree(pddl_lp_t *lp, int i)
{ noSolverExit(); }
static void noSetVarInt(pddl_lp_t *lp, int i)
{ noSolverExit(); }
static void noSetVarBinary(pddl_lp_t *lp, int i)
{ noSolverExit(); }
static void noSetCoef(pddl_lp_t *lp, int row, int col, double coef)
{ noSolverExit(); }
static void noSetRHS(pddl_lp_t *lp, int row, double rhs, char sense)
{ noSolverExit(); }
static void noAddRows(pddl_lp_t *lp, int cnt, const double *rhs, const char *sense)
{ noSolverExit(); }
static void noDelRows(pddl_lp_t *lp, int begin, int end)
{ noSolverExit(); }
static int noNumRows(const pddl_lp_t *lp)
{ noSolverExit(); }
static void noAddCols(pddl_lp_t *lp, int cnt)
{ noSolverExit(); }
static void noDelCols(pddl_lp_t *lp, int begin, int end)
{ noSolverExit(); }
static int noNumCols(const pddl_lp_t *lp)
{ noSolverExit(); }
static int noSolve(pddl_lp_t *lp, double *val, double *obj)
{ noSolverExit(); }
static void noWrite(pddl_lp_t *lp, const char *fn)
{ noSolverExit(); }

pddl_lp_cls_t pddl_lp_not_available = {
    0, "", "",
    noNew,
    noDel,
    noSetObj,
    noSetVarRange,
    noSetVarFree,
    noSetVarInt,
    noSetVarBinary,
    noSetCoef,
    noSetRHS,
    noAddRows,
    noDelRows,
    noNumRows,
    noAddCols,
    noDelCols,
    noNumCols,
    noSolve,
    noWrite,
};
