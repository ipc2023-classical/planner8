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

#ifdef PDDL_CPLEX
# include <ilcplex/cplex.h>

struct _lp_t {
    pddl_lp_t cls;
    CPXENVptr env;
    CPXLPptr lp;
    int mip;
    pddl_timer_t log_timer;
};
typedef struct _lp_t lp_t;

#define LP(l) pddl_container_of((l), lp_t, cls)

static void cplexErr(lp_t *lp, int status, const char *s)
{
    char errmsg[1024];
    CPXgeterrorstring(lp->env, status, errmsg);
    PANIC("Error: CPLEX: %s: %s", s, errmsg);
}

static int callback(CPXCALLBACKCONTEXTptr ctx, CPXLONG ctxtid, void *_lp)
{
    lp_t *lp = _lp;

    pddlTimerStop(&lp->log_timer);
    if (pddlTimerElapsedInSF(&lp->log_timer) < 1.)
        return 0;

    double best_sol = 0.;
    CPXcallbackgetinfodbl(ctx, CPXCALLBACKINFO_BEST_SOL, &best_sol);
    if (best_sol < -1E10 || best_sol > 1E10)
        best_sol = NAN;

    double best_bound = 0.;
    CPXcallbackgetinfodbl(ctx, CPXCALLBACKINFO_BEST_BND, &best_bound);
    if (best_bound < -1E10 || best_bound > 1E10)
        best_bound = NAN;

    int feasible = 0;
    CPXcallbackgetinfoint(ctx, CPXCALLBACKINFO_FEASIBLE, &feasible);

    CTX_NO_TIME(lp->cls.err, "cplex", "cplex progress");
    LOG(lp->cls.err, "best solution: %.2f, best bound: %.2f, feasible: %d",
        best_sol, best_bound, feasible);
    CTXEND(lp->cls.err);
    pddlTimerStart(&lp->log_timer);
    return 0;
}

static int callbackLP(CPXCENVptr env,
                      void *cbdata,
                      int wherefrom,
                      void *_lp)
{
    lp_t *lp = _lp;

    pddlTimerStop(&lp->log_timer);
    if (pddlTimerElapsedInSF(&lp->log_timer) < 1.)
        return 0;

    double primal = 0.;
    CPXgetcallbackinfo(env, cbdata, wherefrom,
                       CPX_CALLBACK_INFO_PRIMAL_OBJ, &primal);
    double dual = 0.;
    CPXgetcallbackinfo(env, cbdata, wherefrom,
                       CPX_CALLBACK_INFO_DUAL_OBJ, &dual);

    CTX_NO_TIME(lp->cls.err, "cplex", "cplex progress");
    LOG(lp->cls.err, "primal: %.4f, dual: %.4f", primal, dual);
    CTXEND(lp->cls.err);
    pddlTimerStart(&lp->log_timer);
    return 0;
}

static pddl_lp_t *new(const pddl_lp_config_t *cfg, pddl_err_t *err)
{
    lp_t *lp;
    int st;

    lp = ALLOC(lp_t);
    lp->cls.cls = &pddl_lp_cplex;
    lp->cls.err = err;
    lp->cls.cfg = *cfg;
    lp->mip = 0;

    // Initialize CPLEX structures
    lp->env = CPXopenCPLEX(&st);
    if (lp->env == NULL)
        cplexErr(lp, st, "Could not open CPLEX environment");

    // Set number of processing threads
    int num_threads = PDDL_MAX(1, cfg->num_threads);
    st = CPXsetintparam(lp->env, CPX_PARAM_THREADS, num_threads);
    if (st != 0)
        cplexErr(lp, st, "Could not set number of threads");

    CPXsetintparam(lp->env, CPXPARAM_ScreenOutput, CPX_OFF);

    if (cfg->time_limit > 0.f){
        st = CPXsetdblparam(lp->env, CPXPARAM_TimeLimit, cfg->time_limit);
        if (st != 0)
            cplexErr(lp, st, "Could not set number of threads");
    }

    lp->lp = CPXcreateprob(lp->env, &st, "");
    if (lp->lp == NULL)
        cplexErr(lp, st, "Could not create CPLEX problem");

    if (cfg->maximize){
        CPXchgobjsen(lp->env, lp->lp, CPX_MAX);
    }else{
        CPXchgobjsen(lp->env, lp->lp, CPX_MIN);
    }

    st = CPXnewcols(lp->env, lp->lp, cfg->cols, NULL, NULL, NULL, NULL, NULL);
    if (st != 0)
        cplexErr(lp, st, "Could not initialize variables");

    st = CPXnewrows(lp->env, lp->lp, cfg->rows, NULL, NULL, NULL, NULL);
    if (st != 0)
        cplexErr(lp, st, "Could not initialize constraints");

    if (cfg->tune_int_operator_potential){
        CPXsetintparam(lp->env, CPXPARAM_Preprocessing_Relax, CPX_ON);
        CPXsetintparam(lp->env, CPXPARAM_Preprocessing_Dual, 1);
        //CPXsetintparam(lp->env, CPXPARAM_Preprocessing_CoeffReduce, 2);
        //CPXsetintparam(lp->env, CPXPARAM_Preprocessing_Dependency, 3);
    }

    return &lp->cls;
}

static void del(pddl_lp_t *_lp)
{
    lp_t *lp = LP(_lp);
    if (lp->lp)
        CPXfreeprob(lp->env, &lp->lp);
    if (lp->env)
        CPXcloseCPLEX(&lp->env);
    FREE(lp);
}

static void setObj(pddl_lp_t *_lp, int i, double coef)
{
    lp_t *lp = LP(_lp);
    int st;

    st = CPXchgcoef(lp->env, lp->lp, -1, i, coef);
    if (st != 0)
        cplexErr(lp, st, "Could not set objective coeficient.");
}

static void setVarRange(pddl_lp_t *_lp, int i, double lb, double ub)
{
    lp_t *lp = LP(_lp);
    if (lb <= -1E20)
        lb = -CPX_INFBOUND;
    if (ub >= 1E20)
        ub = CPX_INFBOUND;
    static const char lu[2] = { 'L', 'U' };
    double bd[2] = { lb, ub };
    int ind[2];
    int st;

    ind[0] = ind[1] = i;
    st = CPXchgbds(lp->env, lp->lp, 2, ind, lu, bd);
    if (st != 0)
        cplexErr(lp, st, "Could not set variable as free.");
}

static void setVarFree(pddl_lp_t *_lp, int i)
{
    setVarRange(_lp, i, -CPX_INFBOUND, CPX_INFBOUND);
}

static void setVarInt(pddl_lp_t *_lp, int i)
{
    lp_t *lp = LP(_lp);
    static char type = CPX_INTEGER;
    int st;

    st = CPXchgctype(lp->env, lp->lp, 1, &i, &type);
    if (st != 0)
        cplexErr(lp, st, "Could not set variable as integer.");
    lp->mip = 1;
}

static void setVarBinary(pddl_lp_t *_lp, int i)
{
    lp_t *lp = LP(_lp);
    static char type = CPX_BINARY;
    int st;

    st = CPXchgctype(lp->env, lp->lp, 1, &i, &type);
    if (st != 0)
        cplexErr(lp, st, "Could not set variable as binary.");
    lp->mip = 1;
}

static void setCoef(pddl_lp_t *_lp, int row, int col, double coef)
{
    lp_t *lp = LP(_lp);
    int st;

    st = CPXchgcoef(lp->env, lp->lp, row, col, coef);
    if (st != 0)
        cplexErr(lp, st, "Could not set constraint coeficient.");
}

static void setRHS(pddl_lp_t *_lp, int row, double rhs, char sense)
{
    lp_t *lp = LP(_lp);
    int st;

    st = CPXchgcoef(lp->env, lp->lp, row, -1, rhs);
    if (st != 0)
        cplexErr(lp, st, "Could not set right-hand-side.");

    st = CPXchgsense(lp->env, lp->lp, 1, &row, &sense);
    if (st != 0)
        cplexErr(lp, st, "Could not set right-hand-side sense.");
}

static void addRows(pddl_lp_t *_lp, int cnt, const double *rhs, const char *sense)
{
    lp_t *lp = LP(_lp);
    int st;

    st = CPXnewrows(lp->env, lp->lp, cnt, rhs, sense, NULL, NULL);
    if (st != 0)
        cplexErr(lp, st, "Could not add new rows.");
}

static void delRows(pddl_lp_t *_lp, int begin, int end)
{
    lp_t *lp = LP(_lp);
    int st;

    st = CPXdelrows(lp->env, lp->lp, begin, end);
    if (st != 0)
        cplexErr(lp, st, "Could not delete rows.");
}

static int numRows(const pddl_lp_t *_lp)
{
    lp_t *lp = LP(_lp);
    return CPXgetnumrows(lp->env, lp->lp);
}

static void addCols(pddl_lp_t *_lp, int cnt)
{
    lp_t *lp = LP(_lp);
    int st;

    st = CPXnewcols(lp->env, lp->lp, cnt, NULL, NULL, NULL, NULL, NULL);
    if (st != 0)
        cplexErr(lp, st, "Could not add new columns.");
}

static void delCols(pddl_lp_t *_lp, int begin, int end)
{
    lp_t *lp = LP(_lp);
    int st;

    st = CPXdelcols(lp->env, lp->lp, begin, end);
    if (st != 0)
        cplexErr(lp, st, "Could not delete columns.");
}

static int numCols(const pddl_lp_t *_lp)
{
    lp_t *lp = LP(_lp);
    return CPXgetnumcols(lp->env, lp->lp);
}

static int solve(pddl_lp_t *_lp, double *val, double *obj)
{
    lp_t *lp = LP(_lp);
    int st;

    pddlTimerStart(&lp->log_timer);
    if (lp->mip){
        CPXcallbacksetfunc(lp->env, lp->lp,
                           CPX_CALLBACKCONTEXT_GLOBAL_PROGRESS
                                | CPX_CALLBACKCONTEXT_LOCAL_PROGRESS
                                | CPX_CALLBACKCONTEXT_RELAXATION
                                | CPX_CALLBACKCONTEXT_CANDIDATE,
                           callback, lp);
        if ((st = CPXmipopt(lp->env, lp->lp)) != 0)
            cplexErr(lp, st, "Failed to optimize LP");
        CPXcallbacksetfunc(lp->env, lp->lp, 0, NULL, NULL);

    }else{
        CPXsetlpcallbackfunc(lp->env, callbackLP, lp);
        if ((st = CPXlpopt(lp->env, lp->lp)) != 0)
            cplexErr(lp, st, "Failed to optimize LP");
    }

    st = CPXgetstat(lp->env, lp->lp);
    if (st == CPX_STAT_OPTIMAL
            || st == CPX_STAT_OPTIMAL_INFEAS
            || st == CPXMIP_OPTIMAL
            || st == CPXMIP_OPTIMAL_TOL
            || st == CPXMIP_TIME_LIM_FEAS){
        st = CPXsolution(lp->env, lp->lp, NULL, val, obj, NULL, NULL, NULL);
        if (st != 0)
            cplexErr(lp, st, "Cannot retrieve solution");
    }else{
        if (obj != NULL){
            int cols = CPXgetnumcols(lp->env, lp->lp);
            ZEROIZE_ARR(obj, cols);
        }
        if (val != NULL)
            *val = 0.;
        return -1;
    }
    return 0;
}

static void cpxWrite(pddl_lp_t *_lp, const char *fn)
{
    lp_t *lp = LP(_lp);
    int st;

    st = CPXwriteprob(lp->env, lp->lp, fn, "LP");
    if (st != 0)
        cplexErr(lp, st, "Failed to optimize ILP");
}



#define TOSTR1(x) #x
#define TOSTR(x) TOSTR1(x)
pddl_lp_cls_t pddl_lp_cplex = {
    PDDL_LP_CPLEX,
    "cplex",
    TOSTR(CPX_VERSION_VERSION.CPX_VERSION_RELEASE.CPX_VERSION_MODIFICATION.CPX_VERSION_FIX),
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
#else /* PDDL_CPLEX */
pddl_lp_cls_t pddl_lp_cplex = { 0 };
#endif /* PDDL_CPLEX */
