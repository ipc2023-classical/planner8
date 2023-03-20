/***
 * cpddl
 * -------
 * Copyright (c)2019 Daniel Fiser <danfis@danfis.cz>,
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

#ifndef __PDDL_POT_H__
#define __PDDL_POT_H__

#include <pddl/segmarr.h>
#include <pddl/fdr.h>
#include <pddl/mg_strips.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct pddl_pot_solution {
    /** Potentials for all facts */
    double *pot;
    int pot_size;
    /** Objective value */
    double objval;
    /** Change of heuristic value for each operator */
    double *op_pot;
    int op_pot_size;
};
typedef struct pddl_pot_solution pddl_pot_solution_t;

void pddlPotSolutionInit(pddl_pot_solution_t *sol);
void pddlPotSolutionFree(pddl_pot_solution_t *sol);
double pddlPotSolutionEvalFDRStateFlt(const pddl_pot_solution_t *sol,
                                      const pddl_fdr_vars_t *vars,
                                      const int *state);
int pddlPotSolutionEvalFDRState(const pddl_pot_solution_t *sol,
                                const pddl_fdr_vars_t *vars,
                                const int *state);
int pddlPotSolutionRoundHValue(double hvalue);
double pddlPotSolutionEvalStripsStateFlt(const pddl_pot_solution_t *sol,
                                         const pddl_iset_t *state);
int pddlPotSolutionEvalStripsState(const pddl_pot_solution_t *sol,
                                   const pddl_iset_t *state);

struct pddl_pot_solutions {
    pddl_pot_solution_t *sol;
    int sol_size;
    int sol_alloc;
    /** True if the input task is proved unsolvable */
    int unsolvable;
};
typedef struct pddl_pot_solutions pddl_pot_solutions_t;

void pddlPotSolutionsInit(pddl_pot_solutions_t *sols);
void pddlPotSolutionsFree(pddl_pot_solutions_t *sols);
void pddlPotSolutionsAdd(pddl_pot_solutions_t *sols,
                         const pddl_pot_solution_t *sol);
int pddlPotSolutionsEvalMaxFDRState(const pddl_pot_solutions_t *sols,
                                    const pddl_fdr_vars_t *vars,
                                    const int *fdr_state);

struct pddl_pot_lb_constr {
    int set;
    pddl_iset_t vars;
    double rhs;
};
typedef struct pddl_pot_lb_constr pddl_pot_lb_constr_t;

struct pddl_pot_constr {
    pddl_iset_t plus;
    pddl_iset_t minus;
    int rhs;
    int op_id;
};
typedef struct pddl_pot_constr pddl_pot_constr_t;

struct pddl_pot_constrs {
    pddl_pot_constr_t *c;
    int size;
    int alloc;
};
typedef struct pddl_pot_constrs pddl_pot_constrs_t;

struct pddl_pot {
    int var_size; /*!< Number of LP variables */
    int fact_var_size;
    int use_ilp; /*!< ILP solver instead of LP */
    int op_pot; /*!< Infer integer operator-potentials */
    int op_pot_real; /*!< Infer real-valued operator-potentials */
    int op_size; /*!< Number of processed operators */
    double *obj; /*!< Objective function coeficients */
    // TODO: Deduplicate constraints using hashtable
    pddl_pot_constrs_t constr_op; /*!< Operator constraints */
    pddl_pot_constrs_t constr_goal; /*!< Goal constraint */
    pddl_pot_lb_constr_t constr_lb;
    pddl_iset_t init;

    pddl_segmarr_t *maxpot;
    int maxpot_size;
    pddl_htable_t *maxpot_htable; /*!< Set of LP variables grouped into maxpot */
    int enforce_int_init; /*!< Enforce integer value for the initial state */
};
typedef struct pddl_pot pddl_pot_t;

/**
 * Initialize potential heuristic with FDR planning task.
 * Global IDs of the facts are the same as IDs of the LP variables.
 */
void pddlPotInitFDR(pddl_pot_t *pot, const pddl_fdr_t *fdr);

/**
 * Initialize potential heuristic with mg-strips task and disambiguation.
 * If the task is detected to be unsolvable, -1 is returned.
 * Fact IDs are the same as IDs of the LP variables.
 * Returns 0 on success.
 */
int pddlPotInitMGStrips(pddl_pot_t *pot,
                        const pddl_mg_strips_t *mg_strips,
                        const pddl_mutex_pairs_t *mutex);

/**
 * Same as pddlPotInitMGStrips() but a single-fact disambiguation is used.
 */
int pddlPotInitMGStripsSingleFactDisamb(pddl_pot_t *pot,
                                        const pddl_mg_strips_t *mg_strips,
                                        const pddl_mutex_pairs_t *mutex);

/**
 * Free allocated memory.
 */
void pddlPotFree(pddl_pot_t *pot);

/**
 * Set full objective function.
 */
void pddlPotSetObj(pddl_pot_t *pot, const double *coef);

/**
 * Set objective function to the given state.
 * This will work only if {pot} was initialized with *InitFDR()
 */
void pddlPotSetObjFDRState(pddl_pot_t *pot,
                           const pddl_fdr_vars_t *vars,
                           const int *state);

/**
 * Set objective function to all syntactic states.
 * This will work only if {pot} was initialized with *InitFDR()
 */
void pddlPotSetObjFDRAllSyntacticStates(pddl_pot_t *pot,
                                        const pddl_fdr_vars_t *vars);

/**
 * Set objective function to the given state.
 * This works only if {pot} was initialized with *InitMGStrips()
 */
void pddlPotSetObjStripsState(pddl_pot_t *pot, const pddl_iset_t *state);

/**
 * Sets lower bound constraint as sum(vars) >= rhs
 */
void pddlPotSetLowerBoundConstr(pddl_pot_t *pot,
                                const pddl_iset_t *vars,
                                double rhs);

/**
 * Removes the lower bound constraints.
 */
void pddlPotResetLowerBoundConstr(pddl_pot_t *pot);

/**
 * Returns RHS of the lower bound constraint
 */
double pddlPotSetLowerBoundConstrRHS(const pddl_pot_t *pot);

/**
 * Decrease the RHS of the previously set lower bound constraint.
 */
void pddlPotDecreaseLowerBoundConstrRHS(pddl_pot_t *pot, double decrease);

/**
 * Turns on/off integer linear program.
 */
_pddl_inline void pddlPotUseILP(pddl_pot_t *pot, int enable)
{
    pot->use_ilp = enable;
}

/**
 * Turns on/off storing of heuristic changes induced by operators.
 */
_pddl_inline void pddlPotEnableOpPot(pddl_pot_t *pot, int enable, int real_valued)
{
    if (enable){
        pot->op_pot = 1;
        pot->op_pot_real = real_valued;
    }else{
        pot->op_pot = pot->op_pot_real = 0;
    }
}

/**
 * Turns on/off enforcing of the integer value for the initial state
 */
_pddl_inline void pddlPotEnforeIntInit(pddl_pot_t *pot, int enable)
{
    pot->enforce_int_init = enable;
}

/**
 * Solve the LP problem and returns the solution via sol.
 * Return 0 on success, -1 if solution was not found.
 *
 * TODO: Add time-limit option
 */
int pddlPotSolve(const pddl_pot_t *pot,
                 pddl_pot_solution_t *sol,
                 pddl_err_t *err);

void pddlPotMGStripsPrintLP(const pddl_pot_t *pot,
                            const pddl_mg_strips_t *mg_strips,
                            FILE *fout);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_POT_H__ */
