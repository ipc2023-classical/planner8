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

#ifndef __PDDL_CP_H__
#define __PDDL_CP_H__

#include <pddl/err.h>
#include <pddl/iset.h>
#include <pddl/htable.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * All possible solvers.
 */
enum pddl_cp_solver {
    PDDL_CP_SOLVER_DEFAULT = 0,
    PDDL_CP_SOLVER_CPOPTIMIZER,
    PDDL_CP_SOLVER_MINIZINC,
};
typedef enum pddl_cp_solver pddl_cp_solver_t;

#define PDDL_CP_FOUND 0
#define PDDL_CP_FOUND_SUBOPTIMAL 1
#define PDDL_CP_NO_SOLUTION -1
#define PDDL_CP_REACHED_LIMIT -2
#define PDDL_CP_ABORTED -3
#define PDDL_CP_UNKNOWN -10

/** Integer variable */
struct pddl_cp_ivar {
    int id;
    char *name;
    pddl_iset_t domain;
};
typedef struct pddl_cp_ivar pddl_cp_ivar_t;

/** Set of integer variables */
struct pddl_cp_ivars {
    pddl_cp_ivar_t *ivar;
    int ivar_size;
    int ivar_alloc;
};
typedef struct pddl_cp_ivars pddl_cp_ivars_t;

/** Set of tuples of integer values */
struct pddl_cp_ival_tuple {
    int idx;
    int arity;
    int num_tuples;
    int *ival;
    int ref; /* Reference counter */
    pddl_htable_key_t hash; /*!< Hash of the tuples */
    pddl_list_t htable; /*!< Connector to the hash table */
};
typedef struct pddl_cp_ival_tuple pddl_cp_ival_tuple_t;

/** Set of sets of integer tuples */
struct pddl_cp_ival_tuples {
    pddl_cp_ival_tuple_t **tuple;
    int tuple_size;
    int tuple_alloc;
    pddl_htable_t *htable;
};
typedef struct pddl_cp_ival_tuples pddl_cp_ival_tuples_t;

/** "allowed" constraint restricting domain of tuples of integer variables */
struct pddl_cp_constr_ivar_allowed {
    int arity;
    int *ivar;
    pddl_cp_ival_tuple_t *ival;
};
typedef struct pddl_cp_constr_ivar_allowed pddl_cp_constr_ivar_allowed_t;

/** Set of "allowed" constraints over integer variables */
struct pddl_cp_constrs_ivar_allowed {
    pddl_cp_constr_ivar_allowed_t *constr;
    int constr_size;
    int constr_alloc;
};
typedef struct pddl_cp_constrs_ivar_allowed pddl_cp_constrs_ivar_allowed_t;

struct pddl_cp {
    pddl_cp_ivars_t ivar;
    pddl_cp_ival_tuples_t ival_tuple;
    pddl_cp_constrs_ivar_allowed_t c_ivar_allowed;
    int objective;
    pddl_iset_t obj_ivars;
    int unsat;
};
typedef struct pddl_cp pddl_cp_t;

struct pddl_cp_solve_config {
    int num_threads;
    float max_search_time;
    pddl_cp_solver_t solver; /*< Which solver to use, one of PDDL_CP_SOLVER_* */
    const char *minizinc; /*!< Path to minizinc binary in case minizinc
                               solver is used. If set to NULL, default path
                               is used. */
    int run_in_subprocess; /*!< If true the solver runs in a subprocess */
};
typedef struct pddl_cp_solve_config pddl_cp_solve_config_t;

#define PDDL_CP_SOLVE_CONFIG_INIT \
    { \
        1, /* .num_threads */ \
        -1.f, /* .max_search_time */ \
        PDDL_CP_SOLVER_DEFAULT, /*.solver */ \
        NULL, /* .minizinc */ \
        1, /* .run_in_subprocess */ \
    }

struct pddl_cp_sol {
    int ivar_size;
    int num_solutions;
    int **isol;
    int isol_alloc;
};
typedef struct pddl_cp_sol pddl_cp_sol_t;

/**
 * Free memory allocated by *Solve*() functions.
 */
void pddlCPSolFree(pddl_cp_sol_t *sol);

/**
 * Initialize empty constraint problem.
 */
void pddlCPInit(pddl_cp_t *cp);

/**
 * Free allocated memory
 */
void pddlCPFree(pddl_cp_t *cp);

/**
 * Adds integer variable and returns its ID -- it can be assumed IDs start
 * at 0 and are increased by 1 with each call.
 */
int pddlCPAddIVar(pddl_cp_t *cp, int min_val, int max_val, const char *name);
int pddlCPAddIVarDomain(pddl_cp_t *cp, const pddl_iset_t *dom, const char *name);

/**
 * Restrict variable to one specific value.
 */
void pddlCPAddConstrIVarEq(pddl_cp_t *cp, int var_id, int value);

/**
 * Restrict domain of the given variable.
 */
void pddlCPAddConstrIVarDomainArr(pddl_cp_t *cp, int var_id,
                                  int size, const int *val);

/**
 * Restricts the domain of the tuple of integer variables by the given set
 * of tuple of values.
 */
void pddlCPAddConstrIVarAllowed(pddl_cp_t *cp, int arity, const int *var,
                                int num_val_tuples, const int *val_tuples);

/**
 * Simplify the model (in a rather naive way).
 */
void pddlCPSimplify(pddl_cp_t *cp);

/**
 * Set objective function to minimize number of different values over all
 * integer variables.
 */
void pddlCPSetObjectiveMinCountDiffAllIVars(pddl_cp_t *cp);
void pddlCPSetObjectiveMinCountDiff(pddl_cp_t *cp, const pddl_iset_t *ivars);

/**
 * Write the model in the minizinc format.
 */
void pddlCPWriteMinizinc(const pddl_cp_t *cp, FILE *fout);

/**
 * Set default solver globally.
 */
void pddlCPSetDefaultSolver(pddl_cp_solver_t solver_id);

/**
 * Solve the problem..
 */
int pddlCPSolve(const pddl_cp_t *cp,
                const pddl_cp_solve_config_t *cfg,
                pddl_cp_sol_t *sol,
                pddl_err_t *err);


/**
 * Solve with IBM CP Optimizer.
 */
int pddlCPSolve_CPOptimizer(const pddl_cp_t *cp,
                            const pddl_cp_solve_config_t *cfg,
                            pddl_cp_sol_t *sol,
                            pddl_err_t *err);

/**
 * Solve with minizinc solver.
 */
int pddlCPSolve_Minizinc(const pddl_cp_t *cp,
                         const pddl_cp_solve_config_t *cfg,
                         pddl_cp_sol_t *sol,
                         pddl_err_t *err);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __PDDL_CP_H__ */
