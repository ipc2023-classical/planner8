/***
 * cpddl
 * -------
 * Copyright (c)2021 Daniel Fiser <danfis@danfis.cz>,
 * Saarland University, and
 * Czech Technical University in Prague.
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

// TODO: Native support for types (unify function + var types in pddl_datalog_atom)
// TODO: Native support for (in)equality predicates

#ifndef __PDDL_DATALOG_H__
#define __PDDL_DATALOG_H__

#include <pddl/common.h>
#include <pddl/iset.h>
#include <pddl/err.h>
#include <pddl/cost.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct pddl_datalog_atom {
    int pred;
    unsigned *arg;

    pddl_iset_t var_set;
};
typedef struct pddl_datalog_atom pddl_datalog_atom_t;

struct pddl_datalog_rule {
    pddl_datalog_atom_t head;
    pddl_datalog_atom_t *body;
    int body_size;
    int body_alloc;
    pddl_datalog_atom_t *neg_body;
    int neg_body_size;
    int neg_body_alloc;
    pddl_cost_t weight;

    pddl_iset_t var_set;
    int is_safe;
    pddl_iset_t common_body_var_set;
};
typedef struct pddl_datalog_rule pddl_datalog_rule_t;

typedef struct pddl_datalog pddl_datalog_t;

/**
 * Creates an empty datalog program.
 */
pddl_datalog_t *pddlDatalogNew(void);

/**
 * Deletes allocated memory.
 */
void pddlDatalogDel(pddl_datalog_t *dl);

/**
 * Clear datalog database.
 */
void pddlDatalogClear(pddl_datalog_t *dl);

/**
 * Adds constant to the datalog program.
 */
unsigned pddlDatalogAddConst(pddl_datalog_t *dl, const char *name);

/**
 * Adds predicate to the datalog program.
 * TODO: Native support for types
 */
unsigned pddlDatalogAddPred(pddl_datalog_t *dl, int arity, const char *name);

/**
 * Add 0-arity goal predicate. The fact instantiated from this predicate will
 * cause pddlDatalogWeightedCanonicalModel*() to stop.
 */
unsigned pddlDatalogAddGoalPred(pddl_datalog_t *dl, const char *name);

/**
 * Adds variable to the datalog program.
 */
unsigned pddlDatalogAddVar(pddl_datalog_t *dl, const char *name);

/**
 * Set user-id to const/pred element.
 */
void pddlDatalogSetUserId(pddl_datalog_t *dl, unsigned element, int user_id);

/**
 * Adds rule to the datalog.
 * TODO: Native support for types
 */
int pddlDatalogAddRule(pddl_datalog_t *dl, const pddl_datalog_rule_t *cl);

/**
 * Remove last n added rules.
 */
void pddlDatalogRmLastRules(pddl_datalog_t *dl, int n);

/**
 * Remove the given set of rules.
 */
void pddlDatalogRmRules(pddl_datalog_t *dl, const pddl_iset_t *rm_rules);

/**
 * Returns true if the program is safe, i.e., all variables from head are
 * in body.
 */
int pddlDatalogIsSafe(const pddl_datalog_t *dl);

/**
 * Transforms the datalog to the normal form according to
 * Helmert, M. (2009). Concise finite-domain representations for PDDL
 * planning tasks. Artificial Intelligence, 173, 503–535.
 *
 * That is, each rule can have at most two atoms in the body, all variables
 * in the head are in the body, and all variables not in the head are in
 * both atoms in the body.
 *
 * Return 0 on success, -1 if the normal form could not be created.
 */
int pddlDatalogToNormalForm(pddl_datalog_t *dl, pddl_err_t *err);

/**
 * Computes and stores the canonical model [1] of the datalog.
 * [1] Helmert, M. (2009). Concise finite-domain representations for PDDL
 * planning tasks. Artificial Intelligence, 173, 503–535.
 */
void pddlDatalogCanonicalModel(pddl_datalog_t *dl, pddl_err_t *err);

/**
 * Continue computing canonical model.
 */
void pddlDatalogCanonicalModelCont(pddl_datalog_t *dl, pddl_err_t *err);

/**
 * Computes canonical model of the weighted datalog (either add or max
 * variant). The computation stop once a goal fact is reached and the
 * functions return 0 and goal fact's weight via argument. If no goal fact
 * is reached, -1 is returned.
 * If collect_fact_achievers is set to true, facts from the body of the
 * best achiever rule is collected for each fact. Use
 * pddlDatalogAchieverFactsFromWeightedCannonicalModel() to iterate over the
 * achiever facts.
 *
 * Correa, A. B., Frances, G., Pommerening, F., & Helmert, M. (2021).
 * Delete-Relaxation Heuristics for Lifted Classical Planning. Proceedings
 * of the International Conference on Automated Planning and Scheduling,
 * 31(1), 94-102
 */
int pddlDatalogWeightedCanonicalModelAdd(pddl_datalog_t *dl,
                                         pddl_cost_t *weight,
                                         int collect_fact_achievers,
                                         pddl_err_t *err);
int pddlDatalogWeightedCanonicalModelMax(pddl_datalog_t *dl,
                                         pddl_cost_t *weight,
                                         int collect_fact_achievers,
                                         pddl_err_t *err);

/**
 * Can be called only after pddlDatalogCanonicalModel() function.
 * Iterates over facts of the given predicate from the canonical model, the
 * returned values pred_user_id and arg_user_id are ids previously set by
 * pddlDatalogSetUserId().
 */
void pddlDatalogFactsFromCanonicalModel(
            pddl_datalog_t *dl,
            unsigned pred,
            void (*fn)(int pred_user_id,
                       int arity,
                       const pddl_obj_id_t *arg_user_id,
                       void *user_data),
            void *user_data);

/**
 * Same as pddlDatalogFactsFromCanonicalModel() except it returns also the
 * weight of the fact.
 */
void pddlDatalogFactsFromWeightedCanonicalModel(
            pddl_datalog_t *dl,
            unsigned pred,
            void (*fn)(int pred_user_id,
                       int arity,
                       const pddl_obj_id_t *arg_user_id,
                       const pddl_cost_t *weight,
                       void *user_data),
            void *user_data);

/**
 * Similar to pddlDatalogFactsFromWeightedCanonicalModel(), but it first
 * collect all best achievers backtracking from the goal fact (instance of
 * goal_pred) and then iterates over them using the callback.
 * {goal_pred} is assumed to be ID of the zero arity predicate previously
 * added using *AddGoalPred().
 * See pddlDatalogWeightedCanonicalModel{Add,Max}().
 */
void pddlDatalogAchieverFactsFromWeightedCanonicalModel(
            pddl_datalog_t *dl,
            unsigned goal_pred,
            void (*fn)(int pred_user_id,
                       int arity,
                       const pddl_obj_id_t *arg_user_id,
                       const pddl_cost_t *weight,
                       void *user_data),
            void *user_data);

/**
 * Save the current state of the database.
 * After calling pddlDatalogRollbackDB(), the database will be restored to
 * this state.
 */
void pddlDatalogSaveStateOfDB(pddl_datalog_t *dl);

/**
 * Rollback to the last state saved by pddlDatalogSaveStateOfDB().
 * It is not safe to call this function before pddlDatalogSaveStateOfDB().
 */
void pddlDatalogRollbackDB(pddl_datalog_t *dl);

/**
 * Insert a new fact to the database.
 */
void pddlDatalogAddFactToDB(pddl_datalog_t *dl,
                            unsigned in_pred,
                            const unsigned *in_arg);

/**
 * Initializes atom of the given predicate previously created with
 * pddlDatalogAddPred().
 */
void pddlDatalogAtomInit(pddl_datalog_t *dl,
                         pddl_datalog_atom_t *atom,
                         unsigned pred);

/**
 * Deep copy of the atom
 */
void pddlDatalogAtomCopy(pddl_datalog_t *dl,
                         pddl_datalog_atom_t *dst,
                         const pddl_datalog_atom_t *src);

/**
 * Free atom structure
 */
void pddlDatalogAtomFree(pddl_datalog_t *dl, pddl_datalog_atom_t *atom);

/**
 * Compares two atoms.
 */
int pddlDatalogAtomCmp(const pddl_datalog_t *dl,
                       const pddl_datalog_atom_t *atom1,
                       const pddl_datalog_atom_t *atom2);
int pddlDatalogAtomCmpArgs(const pddl_datalog_t *dl,
                           const pddl_datalog_atom_t *atom1,
                           const pddl_datalog_atom_t *atom2);

/**
 * Set argi'th argument of the atom to the given term which must be created
 * with pddlDatalogAdd{Var,Const}()
 */
void pddlDatalogAtomSetArg(pddl_datalog_t *dl,
                           pddl_datalog_atom_t *atom,
                           int argi,
                           unsigned term);

/**
 * Initializes empty rule.
 */
void pddlDatalogRuleInit(pddl_datalog_t *dl, pddl_datalog_rule_t *rule);

/**
 * Deep copy of the rule.
 */
void pddlDatalogRuleCopy(pddl_datalog_t *dl,
                         pddl_datalog_rule_t *dst,
                         const pddl_datalog_rule_t *src);

/**
 * Free rule structure.
 */
void pddlDatalogRuleFree(pddl_datalog_t *dl, pddl_datalog_rule_t *rule);

/**
 * Compare two rules.
 */
int pddlDatalogRuleCmp(const pddl_datalog_t *dl,
                       const pddl_datalog_rule_t *rule1,
                       const pddl_datalog_rule_t *rule2);
int pddlDatalogRuleCmpBodyFirst(const pddl_datalog_t *dl,
                                const pddl_datalog_rule_t *rule1,
                                const pddl_datalog_rule_t *rule2);
int pddlDatalogRuleCmpBodyAndWeight(const pddl_datalog_t *dl,
                                    const pddl_datalog_rule_t *rule1,
                                    const pddl_datalog_rule_t *rule2);

/**
 * Set head of the rule.
 */
void pddlDatalogRuleSetHead(pddl_datalog_t *dl,
                            pddl_datalog_rule_t *rule,
                            const pddl_datalog_atom_t *head);

/**
 * Adds a body atom to the rule.
 */
void pddlDatalogRuleAddBody(pddl_datalog_t *dl,
                            pddl_datalog_rule_t *rule,
                            const pddl_datalog_atom_t *atom);

/**
 * Adds a negative atom to the body assuming this atom is static, i.e.,
 * it's not in any rule's head.
 */
void pddlDatalogRuleAddNegStaticBody(pddl_datalog_t *dl,
                                     pddl_datalog_rule_t *rule,
                                     const pddl_datalog_atom_t *atom);

/**
 * Removes i'th atom from the body
 */
void pddlDatalogRuleRmBody(pddl_datalog_t *dl,
                           pddl_datalog_rule_t *rule,
                           int i);

/**
 * Set weight of the rule.
 */
void pddlDatalogRuleSetWeight(pddl_datalog_t *dl,
                              pddl_datalog_rule_t *rule,
                              const pddl_cost_t *weight);

/**
 * Returns true if the program is safe, i.e., all variables from head are
 * in body.
 */
int pddlDatalogRuleIsSafe(const pddl_datalog_t *dl,
                          const pddl_datalog_rule_t *rule);


void pddlDatalogPrintRule(const pddl_datalog_t *dl,
                          const pddl_datalog_rule_t *rule,
                          FILE *fout);
void pddlDatalogPrint(const pddl_datalog_t *dl, FILE *fout);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_DATALOG_H__ */
