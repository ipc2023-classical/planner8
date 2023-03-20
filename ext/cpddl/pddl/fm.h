/***
 * cpddl
 * -------
 * Copyright (c)2016 Daniel Fiser <danfis@danfis.cz>,
 * AI Center, Department of Computer Science,
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

#ifndef __PDDL_FM_H__
#define __PDDL_FM_H__

#include <pddl/common.h>
#include <pddl/list.h>
#include <pddl/lisp.h>
#include <pddl/require_flags.h>
#include <pddl/param.h>
#include <pddl/obj.h>
#include <pddl/pred.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * Types of formulas
 */
enum pddl_fm_type {
    /** Conjuction */
    PDDL_FM_AND = 0,
    /** Disjunction */
    PDDL_FM_OR,
    /** Universal quantifier */
    PDDL_FM_FORALL,
    /** Existential quantifier */
    PDDL_FM_EXIST,
    /** Conditional effect */
    PDDL_FM_WHEN,
    /** Atom in a positive or negative form */
    PDDL_FM_ATOM,
    /** Assignement to a function (= ...) */
    PDDL_FM_ASSIGN,
    /** (increase ...) */
    PDDL_FM_INCREASE,
    /** True/False */
    PDDL_FM_BOOL,
    /** (imply X Y) */
    PDDL_FM_IMPLY,
    PDDL_FM_NUM_TYPES,
};
typedef enum pddl_fm_type pddl_fm_type_t;

const char *pddlFmTypeName(pddl_fm_type_t type);

#define PDDL_FM_CAST(C, T) \
    (pddl_container_of((C), pddl_fm_##T##_t, fm))

/**
 * Abstract formula
 */
struct pddl_fm {
    /** Type of the formula */
    pddl_fm_type_t type;
    /** Connection to the parent formula */
    pddl_list_t conn;
};
typedef struct pddl_fm pddl_fm_t;

/**
 * Conjunction / Disjunction
 */
struct pddl_fm_junc {
    pddl_fm_t fm;
    pddl_list_t part; /*!< List of parts */
};
typedef struct pddl_fm_junc pddl_fm_junc_t;

/** Conjunction */
typedef pddl_fm_junc_t pddl_fm_and_t;
/** Disjunction */
typedef pddl_fm_junc_t pddl_fm_or_t;

/**
 * Quantifiers
 */
struct pddl_fm_quant {
    pddl_fm_t fm;
    pddl_params_t param; /*!< List of parameters */
    pddl_fm_t *qfm; /*!< Quantified formula */
};
typedef struct pddl_fm_quant pddl_fm_quant_t;

/** Universal quantifier */
typedef pddl_fm_quant_t pddl_fm_forall_t;
/** Existential quantifier */
typedef pddl_fm_quant_t pddl_fm_exist_t;

/**
 * Conditional effect
 */
struct pddl_fm_when {
    pddl_fm_t fm;
    pddl_fm_t *pre;
    pddl_fm_t *eff;
};
typedef struct pddl_fm_when pddl_fm_when_t;


/**
 * Argument of an atom
 */
struct pddl_fm_atom_arg {
    int param; /*!< -1 or index of parameter */
    pddl_obj_id_t obj; /*!< object ID (constant) or PDDL_OBJ_ID_UNDEF */
};
typedef struct pddl_fm_atom_arg pddl_fm_atom_arg_t;

/**
 * Atom
 */
struct pddl_fm_atom {
    pddl_fm_t fm;
    int pred; /*!< Predicate ID */
    pddl_fm_atom_arg_t *arg; /*!< List of arguments */
    int arg_size; /*!< Number of arguments */
    int neg; /*!< True if negated */
};
typedef struct pddl_fm_atom pddl_fm_atom_t;


/**
 * Assign/Increase
 */
struct pddl_fm_func_op {
    pddl_fm_t fm;
    pddl_fm_atom_t *lvalue; /*!< lvalue for assignement */
    int value;                /*!< Assigned immediate value */
    pddl_fm_atom_t *fvalue; /*!< Assigned value through function symbol */
};
typedef struct pddl_fm_func_op pddl_fm_func_op_t;

typedef pddl_fm_func_op_t pddl_fm_assign_t;
/* TODO: For now only (increase (total-cost) (...)) is supported */
typedef pddl_fm_func_op_t pddl_fm_increase_t;

/**
 * Boolean value
 */
struct pddl_fm_bool {
    pddl_fm_t fm;
    int val;
};
typedef struct pddl_fm_bool pddl_fm_bool_t;

/**
 * Imply: (imply (...) (...))
 */
struct pddl_fm_imply {
    pddl_fm_t fm;
    pddl_fm_t *left;
    pddl_fm_t *right;
};
typedef struct pddl_fm_imply pddl_fm_imply_t;


/**
 * Safe casting functions
 */
pddl_fm_junc_t *pddlFmToJunc(pddl_fm_t *c);
const pddl_fm_junc_t *pddlFmToJuncConst(const pddl_fm_t *c);
pddl_fm_and_t *pddlFmToAnd(pddl_fm_t *c);
pddl_fm_or_t *pddlFmToOr(pddl_fm_t *c);
pddl_fm_bool_t *pddlFmToBool(pddl_fm_t *c);
pddl_fm_atom_t *pddlFmToAtom(pddl_fm_t *c);
const pddl_fm_atom_t *pddlFmToAtomConst(const pddl_fm_t *c);
const pddl_fm_increase_t *pddlFmToIncreaseConst(const pddl_fm_t *c);
const pddl_fm_when_t *pddlFmToWhenConst(const pddl_fm_t *c);

/**
 * Free memory.
 */
void pddlFmDel(pddl_fm_t *fm);

/**
 * Returns true if c is FALSE constant
 */
int pddlFmIsFalse(const pddl_fm_t *c);

/**
 * Returns true if c is TRUE constant
 */
int pddlFmIsTrue(const pddl_fm_t *c);

/**
 * Returns true if c is an atom
 */
int pddlFmIsAtom(const pddl_fm_t *c);

/**
 * Returns true if {c} is a when (conditional effect) node
 */
int pddlFmIsWhen(const pddl_fm_t *c);

/**
 * Returns true if {c} is (increase ...) atom
 */
int pddlFmIsIncrease(const pddl_fm_t *c);

/**
 * Creates an exact copy of the condition.
 */
pddl_fm_t *pddlFmClone(const pddl_fm_t *fm);

/**
 * Returns a negated copy of the condition.
 */
pddl_fm_t *pddlFmNegate(const pddl_fm_t *fm, const pddl_t *pddl);

/**
 * Returns true if the conds match exactly.
 */
int pddlFmEq(const pddl_fm_t *c1, const pddl_fm_t *c2);

/**
 * Returns true if s is implied by c
 */
int pddlFmIsImplied(const pddl_fm_t *s,
                    const pddl_fm_t *c,
                    const pddl_t *pddl,
                    const pddl_params_t *param);
#define pddlFmIsEntailed pddlFmIsImplied

/**
 * Traverse all conditionals in a tree and call in pre/post order callbacks
 * if non-NULL.
 * If pre returns -1 the element is skipped (it is not traversed deeper).
 * If pre returns -2 the whole traversing is terminated.
 * If post returns non-zero value the whole traversing is terminated.
 */
void pddlFmTraverse(pddl_fm_t *c,
                    int (*pre)(pddl_fm_t *, void *),
                    int (*post)(pddl_fm_t *, void *),
                    void *u);

/**
 * Same as pddlFmTraverse() but pddl_fm_t structures are passed so that
 * they can be safely changed within callbacks.
 * The return values of pre and post and treated the same way as in
 * pddlFmTraverse().
 */
void pddlFmRebuild(pddl_fm_t **c,
                   int (*pre)(pddl_fm_t **, void *),
                   int (*post)(pddl_fm_t **, void *),
                   void *userdata);

/**
 * When first (when ...) node, that has non-static preconditions, is found,
 * it is removed and returned.
 * If no (when ...) is found, NULL is returned.
 * The function requires that c is the (and ...) node.
 */
pddl_fm_when_t *pddlFmRemoveFirstNonStaticWhen(pddl_fm_t *c, const pddl_t *pddl);
pddl_fm_when_t *pddlFmRemoveFirstWhen(pddl_fm_t *c, const pddl_t *pddl);

/**
 * Creates a new (and a b) node.
 * The objects a and b should not be used after this call.
 */
pddl_fm_t *pddlFmNewAnd2(pddl_fm_t *a, pddl_fm_t *b);

/**
 * Creates a new empty (and ).
 */
pddl_fm_t *pddlFmNewEmptyAnd(void);

/**
 * Creates a new empty (or ).
 */
pddl_fm_t *pddlFmNewEmptyOr(void);

/**
 * Creates a new empty atom with the specified number of arguments all set
 * as "undefined".
 */
pddl_fm_atom_t *pddlFmNewEmptyAtom(int num_args);

/**
 * Creates false/true constants.
 */
pddl_fm_bool_t *pddlFmNewBool(int is_true);

/**
 * Returns true if the conditional contains any atom.
 */
int pddlFmHasAtom(const pddl_fm_t *c);

/**
 * Parse condition from PDDL lisp.
 */
pddl_fm_t *pddlFmParse(const pddl_lisp_node_t *root,
                       pddl_t *pddl,
                       const pddl_params_t *params,
                       const char *err_prefix,
                       pddl_err_t *err);

/**
 * Parse (:init ...) into a conjuction of atoms.
 */
pddl_fm_and_t *pddlFmParseInit(const pddl_lisp_node_t *root,
                               pddl_t *pddl,
                               pddl_err_t *err);

/**
 * Transforms atom into (and atom).
 */
pddl_fm_t *pddlFmAtomToAnd(pddl_fm_t *atom);

/**
 * Creates a new atom that corresponds to a grounded fact.
 */
pddl_fm_atom_t *pddlFmCreateFactAtom(int pred,
                                     int arg_size, 
                                     const pddl_obj_id_t *arg);

/**
 * Adds {c} to and/or condition.
 */
void pddlFmJuncAdd(pddl_fm_junc_t *part, pddl_fm_t *c);

/**
 * Removes {c} from the and/or condition
 */
void pddlFmJuncRm(pddl_fm_junc_t *part, pddl_fm_t *c);

/**
 * Returns true if the and/or is empty
 */
int pddlFmJuncIsEmpty(const pddl_fm_junc_t *part);

/**
 * Returns 0 if cond is a correct precondition, -1 otherwise.
 */
int pddlFmCheckPre(const pddl_fm_t *fm,
                   const pddl_require_flags_t *require,
                   pddl_err_t *err);

/**
 * Same as pddlFmCheckPre() buf effect is checked.
 */
int pddlFmCheckEff(const pddl_fm_t *fm,
                   const pddl_require_flags_t *require,
                   pddl_err_t *err);


/**
 * Set .read to true for all found atoms.
 */
void pddlFmSetPredRead(const pddl_fm_t *fm, pddl_preds_t *preds);

/**
 * Set .write to true for all found atoms, and set .read to true for all
 * atoms found as precondtions in (when ) statement.
 */
void pddlFmSetPredReadWriteEff(const pddl_fm_t *fm, pddl_preds_t *preds);

/**
 * Normalize conditionals by instantiation qunatifiers and transformation to
 * DNF so that the actions can be split.
 */
pddl_fm_t *pddlFmNormalize(pddl_fm_t *fm,
                           const pddl_t *pddl,
                           const pddl_params_t *params);

/**
 * Remove atom node duplicates.
 */
pddl_fm_t *pddlFmDeduplicateAtoms(pddl_fm_t *fm, const pddl_t *pddl);

/**
 * Remove duplicate formulas
 */
pddl_fm_t *pddlFmDeduplicate(pddl_fm_t *fm, const pddl_t *pddl);

/**
 * If conflicting literals are found
 *   1) in the and node, then the positive literal is kept (following the
 *      rule "first delete then add".
 *   2) in the or node, the error is reported.
 */
pddl_fm_t *pddlFmDeconflictEff(pddl_fm_t *fm,
                               const pddl_t *pddl,
                               const pddl_params_t *params);

/**
 * TODO
 */
pddl_fm_t *pddlFmSimplify(pddl_fm_t *fm,
                          const pddl_t *pddl,
                          const pddl_params_t *params);

/**
 * Returns true if the atom is a grounded fact.
 */
int pddlFmAtomIsGrounded(const pddl_fm_atom_t *atom);

/**
 * Compares two atoms.
 */
int pddlFmAtomCmp(const pddl_fm_atom_t *a1, const pddl_fm_atom_t *a2);

/**
 * Compares two atoms without considering negation (.neg flag).
 */
int pddlFmAtomCmpNoNeg(const pddl_fm_atom_t *a1, const pddl_fm_atom_t *a2);

/**
 * Returns true if a1 and a2 are negations of each other.
 */
int pddlFmAtomInConflict(const pddl_fm_atom_t *a1,
                         const pddl_fm_atom_t *a2,
                         const pddl_t *pddl);

/**
 * Remap objects.
 * It is assumed no object from {c} is deleted.
 */
void pddlFmRemapObjs(pddl_fm_t *c, const pddl_obj_id_t *remap);

/**
 * TODO
 */
pddl_fm_t *pddlFmRemoveInvalidAtoms(pddl_fm_t *c);

/**
 * Remap predicates
 */
int pddlFmRemapPreds(pddl_fm_t *c,
                     const int *pred_remap,
                     const int *func_remap);


/**
 * Print the given formula
 */
void pddlFmPrint(const pddl_t *pddl,
                 const pddl_fm_t *fm,
                 const pddl_params_t *params,
                 FILE *fout);


/**
 * Format given formula and write result in [s]
 */
const char *pddlFmFmt(const pddl_fm_t *fm,
                      const pddl_t *pddl,
                      const pddl_params_t *params,
                      char *s,
                      size_t s_size);

/**
 * Print the given formula in PDDL format.
 */
void pddlFmPrintPDDL(const pddl_fm_t *fm,
                     const pddl_t *pddl,
                     const pddl_params_t *params,
                     FILE *fout);

const char *pddlFmPDDLFmt(const pddl_fm_t *fm,
                          const pddl_t *pddl,
                          const pddl_params_t *params,
                          char *s,
                          size_t s_size);



struct pddl_fm_const_it {
    const pddl_list_t *list;
    const pddl_list_t *cur;
};
typedef struct pddl_fm_const_it pddl_fm_const_it_t;
typedef pddl_fm_const_it_t pddl_fm_const_it_atom_t;
typedef pddl_fm_const_it_t pddl_fm_const_it_when_t;
typedef pddl_fm_const_it_t pddl_fm_const_it_increase_t;

const pddl_fm_t *pddlFmConstItInit(pddl_fm_const_it_atom_t *it,
                                   const pddl_fm_t *fm,
                                   int type);
const pddl_fm_t *pddlFmConstItNext(pddl_fm_const_it_atom_t *it,
                                   int type);

#define PDDL_FM_FOR_EACH(COND, IT, ATOM) \
    for ((ATOM) = pddlFmConstItInit((IT), (COND), -1); \
            (ATOM) != NULL; \
            (ATOM) = pddlFmConstItNext((IT), -1))

#define PDDL_FM_FOR_EACH_CONT(IT, ATOM) \
    for ((ATOM) = pddlFmConstItNext((IT), -1); \
            (ATOM) != NULL; \
            (ATOM) = pddlFmConstItNext((IT), -1))

const pddl_fm_atom_t *pddlFmConstItAtomInit(pddl_fm_const_it_atom_t *it,
                                            const pddl_fm_t *fm);
const pddl_fm_atom_t *pddlFmConstItAtomNext(pddl_fm_const_it_atom_t *it);

#define PDDL_FM_FOR_EACH_ATOM(COND, IT, ATOM) \
    for ((ATOM) = pddlFmConstItAtomInit((IT), (COND)); \
            (ATOM) != NULL; \
            (ATOM) = pddlFmConstItAtomNext((IT)))

#define PDDL_FM_FOR_EACH_ATOM_CONT(IT, ATOM) \
    for ((ATOM) = pddlFmConstItAtomNext((IT)); \
            (ATOM) != NULL; \
            (ATOM) = pddlFmConstItAtomNext((IT)))

const pddl_fm_when_t *pddlFmConstItWhenInit(pddl_fm_const_it_when_t *it,
                                            const pddl_fm_t *fm);
const pddl_fm_when_t *pddlFmConstItWhenNext(pddl_fm_const_it_when_t *it);

#define PDDL_FM_FOR_EACH_WHEN(COND, IT, WHEN) \
    for ((WHEN) = pddlFmConstItWhenInit((IT), (COND)); \
            (WHEN) != NULL; \
            (WHEN) = pddlFmConstItWhenNext((IT)))

#define PDDL_FM_FOR_EACH_WHEN_CONT(IT, WHEN) \
    for ((WHEN) = pddlFmConstItWhenNext((IT)); \
            (WHEN) != NULL; \
            (WHEN) = pddlFmConstItWhenNext((IT)))

struct pddl_fm_const_it_eff {
    const pddl_list_t *list;
    const pddl_list_t *cur;
    const pddl_fm_t *when_pre;
    const pddl_list_t *when_list;
    const pddl_list_t *when_cur;
};
typedef struct pddl_fm_const_it_eff pddl_fm_const_it_eff_t;

const pddl_fm_atom_t *pddlFmConstItEffInit(pddl_fm_const_it_eff_t *it,
                                           const pddl_fm_t *fm,
                                           const pddl_fm_t **pre);
const pddl_fm_atom_t *pddlFmConstItEffNext(pddl_fm_const_it_eff_t *it,
                                           const pddl_fm_t **pre);

#define PDDL_FM_FOR_EACH_EFF(COND, IT, ATOM, PRE) \
    for ((ATOM) = pddlFmConstItEffInit((IT), (COND), &(PRE)); \
            (ATOM) != NULL; \
            (ATOM) = pddlFmConstItEffNext((IT), &(PRE)))
#define PDDL_FM_FOR_EACH_EFF_CONT(IT, ATOM, PRE) \
    for ((ATOM) = pddlFmConstItEffNext((IT), &(PRE)); \
            (ATOM) != NULL; \
            (ATOM) = pddlFmConstItEffNext((IT), &(PRE)))

#define PDDL_FM_FOR_EACH_ADD_EFF(COND, IT, ATOM, PRE) \
    PDDL_FM_FOR_EACH_EFF((COND), (IT), (ATOM), (PRE)) \
        if (!(ATOM)->neg)
#define PDDL_FM_FOR_EACH_ADD_EFF_CONT(IT, ATOM, PRE) \
    PDDL_FM_FOR_EACH_EFF_CONT((IT), (ATOM), (PRE)) \
        if (!(ATOM)->neg)

#define PDDL_FM_FOR_EACH_DEL_EFF(COND, IT, ATOM, PRE) \
    PDDL_FM_FOR_EACH_EFF((COND), (IT), (ATOM), (PRE)) \
        if ((ATOM)->neg)
#define PDDL_FM_FOR_EACH_DEL_EFF_CONT(IT, ATOM, PRE) \
    PDDL_FM_FOR_EACH_EFF_CONT((IT), (ATOM), (PRE)) \
        if ((ATOM)->neg)

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_FM_H__ */
