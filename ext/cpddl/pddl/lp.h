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

#ifndef __PDDL_LP_H__
#define __PDDL_LP_H__

#include <pddl/err.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// TODO: pddl_lp_status: We need to distinguish at least: no solution
// exists, cannot find a solution, optimal solution, suboptimal solution
/*
enum pddl_lp_status {
    PDDL_LP_STATUS_OPTIMAL = 0,
    PDDL_LP_STATUS_SUBOPTIMAL = 1,
    PDDL_LP_STATUS_INFEASIBLE = 2,
    PDDL_LP_STATUS_NO_SOLUTION_FOUND = -1,
    PDDL_LP_STATUS_ERROR = -2,
};
typedef enum pddl_lp_status pddl_lp_status_t;

#define PDDL_LP_HAS_SOLUTION(S) \
    ((S) == PDDL_LP_STATUS_OPTIMAL || (S) == PDDL_LP_STATUS_SUBOPTIMAL)
*/

/** Forward declaration */
typedef struct pddl_lp pddl_lp_t;

/**
 * All possible solvers.
 */
enum pddl_lp_solver {
    PDDL_LP_DEFAULT = 0,
    PDDL_LP_CPLEX,
    PDDL_LP_GUROBI,
    PDDL_LP_HIGHS,
    PDDL_LP_GLPK,
};
typedef enum pddl_lp_solver pddl_lp_solver_t;

struct pddl_lp_config {
    int rows; /*!< Number of rows (constraints) after initialization */
    int cols; /*!< Number of columns (variables) after initialization */
    int maximize; /*!< 1 for maximize, 0 for minimize */
    pddl_lp_solver_t solver; /*!< One of the solver IDs above */
    int num_threads; /*!< Number of threads. */
    float time_limit; /*!< Time limit for solving the problem. */
    int tune_int_operator_potential; /*!< True for tuning inference of
                                          integer operator potentials */
};
typedef struct pddl_lp_config pddl_lp_config_t;

#define PDDL_LP_CONFIG_INIT \
    { \
        0, /* .rows */ \
        0, /* .cols */ \
        0, /* .maximize */ \
        PDDL_LP_DEFAULT, /* .solver */ \
        1, /* .num_threads */ \
        -1., /* .time_limit */ \
        0, /* .tune_int_operator_potential */ \
    }

/**
 * Returns true if the specified solver is available.
 * For PDDL_LP_DEFAULT returns false if there is no LP solver available.
 */
int pddlLPSolverAvailable(pddl_lp_solver_t solver);

/**
 * Set default solver.
 */
int pddlLPSetDefault(pddl_lp_solver_t solver, pddl_err_t *err);

/**
 * Creates a new LP problem with specified number of rows and columns.
 */
pddl_lp_t *pddlLPNew(const pddl_lp_config_t *cfg, pddl_err_t *err);

/**
 * Deletes the LP object.
 */
void pddlLPDel(pddl_lp_t *lp);

/**
 * Returns name of the current LP solver.
 */
const char *pddlLPSolverName(const pddl_lp_t *lp);

/**
 * Returns one of PDDL_LP_{CPLEX,GUROBI,GLPK,HIGHS} constants according to the
 * current solver.
 */
int pddlLPSolverID(const pddl_lp_t *lp);

/**
 * Sets objective coeficient for i'th variable.
 */
void pddlLPSetObj(pddl_lp_t *lp, int i, double coef);

/**
 * Sets i'th variable's range.
 */
void pddlLPSetVarRange(pddl_lp_t *lp, int i, double lb, double ub);

/**
 * Sets i'th variable as free.
 */
void pddlLPSetVarFree(pddl_lp_t *lp, int i);

/**
 * Sets i'th variable as integer.
 */
void pddlLPSetVarInt(pddl_lp_t *lp, int i);

/**
 * Sets i'th variable as binary.
 */
void pddlLPSetVarBinary(pddl_lp_t *lp, int i);

/**
 * Sets coefficient for row's constraint and col's variable.
 */
void pddlLPSetCoef(pddl_lp_t *lp, int row, int col, double coef);

/**
 * Sets right hand side of the row'th constraint.
 * Also sense of the constraint must be defined:
 *      - 'L' <=
 *      - 'G' >=
 *      - 'E' =
 */
void pddlLPSetRHS(pddl_lp_t *lp, int row, double rhs, char sense);

/**
 * Adds cnt rows to the model.
 */
void pddlLPAddRows(pddl_lp_t *lp, int cnt, const double *rhs, const char *sense);

/**
 * Deletes rows with indexes between begin and end including both limits,
 * i.e., first deleted row has index {begin} the last deleted row has index
 * {end}.
 */
void pddlLPDelRows(pddl_lp_t *lp, int begin, int end);

/**
 * Returns number of rows in model.
 */
int pddlLPNumRows(const pddl_lp_t *lp);

/**
 * Adds cnt columns to the model.
 */
void pddlLPAddCols(pddl_lp_t *lp, int cnt);

/**
 * Deletes columns with indexes between begin and end including both
 * limits, i.e., first deleted column has index {begin} the last deleted
 * column has index {end}.
 */
void pddlLPDelCols(pddl_lp_t *lp, int begin, int end);

/**
 * Returns number of columns in model.
 */
int pddlLPNumCols(const pddl_lp_t *lp);

/**
 * Solves (I)LP problem.
 * Return 0 if problem was solved, -1 if the problem has no solution.
 * Objective value is returned via argument val and values of each variable
 * via argument obj if non-NULL.
 */
// TODO: Status: optimal/suboptimal
int pddlLPSolve(pddl_lp_t *lp, double *val, double *obj);


void pddlLPWrite(pddl_lp_t *lp, const char *fn);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __PDDL_LP_H__ */
