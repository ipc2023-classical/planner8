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

#ifndef __PDDL_PRED_H__
#define __PDDL_PRED_H__

#include <pddl/common.h>
#include <pddl/require_flags.h>
#include <pddl/type.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct pddl_pred {
    int id;           /*!< ID of the predicate */
    char *name;       /*!< Name of the predicate */
    int *param;       /*!< IDs of types of parameters */
    int param_size;   /*!< Number of parameters */
    int is_private;   /*!< True if the predicate is private */
    int owner_param;  /*!< Index of the parameter that corresponds to the
                           owner object */
    int read;         /*!< True if the predicate appears in some precondition */
    int write;        /*!< True if the predicate appreas in some effect */
    int in_init;      /*!< True if the predicate appear in the initial state */
    int neg_of;       /*!< ID of the predicate this predicate is negation of */
};
typedef struct pddl_pred pddl_pred_t;

_pddl_inline int pddlPredIsStatic(const pddl_pred_t *pred);


struct pddl_preds {
    pddl_pred_t *pred;
    int pred_size;
    int pred_alloc;
    int eq_pred; /*!< index of the predicate that corresponds to
                      (= .  .) predicate */
};
typedef struct pddl_preds pddl_preds_t;

/**
 * Parse :predicates from domain PDDL.
 */
int pddlPredsParse(pddl_t *pddl, pddl_err_t *err);

/**
 * Initialize dst as a deep copy of src.
 */
void pddlPredsInitCopy(pddl_preds_t *dst, const pddl_preds_t *src);

/**
 * Parse :functions from domain PDDL.
 */
int pddlFuncsParse(pddl_t *pddl, pddl_err_t *err);

/**
 * Frees allocated resources.
 */
void pddlPredsFree(pddl_preds_t *ps);

/**
 * Returns ID of the predicate with the specified name.
 */
int pddlPredsGet(const pddl_preds_t *ps, const char *name);

/**
 * Adds a new predicate to the end.
 */
pddl_pred_t *pddlPredsAdd(pddl_preds_t *ps);

/**
 * Adds a new predicate that is exact copy (except its ID) of the one
 * specified by its ID.
 */
pddl_pred_t *pddlPredsAddCopy(pddl_preds_t *ps, int src_id);

/**
 * Removes last predicate from the array.
 */
void pddlPredsRemoveLast(pddl_preds_t *ps);

/**
 * Remap type IDs and remove predicates with removed types.
 */
void pddlPredsRemapTypes(pddl_preds_t *ps,
                         const int *type_remap,
                         int *pred_remap);

void pddlPredsPrint(const pddl_preds_t *ps,
                    const char *title, FILE *fout);

/**
 * Print predicates in PDDL format.
 */
void pddlPredsPrintPDDL(const pddl_preds_t *ps,
                        const pddl_types_t *ts,
                        FILE *fout);
void pddlFuncsPrintPDDL(const pddl_preds_t *ps,
                        const pddl_types_t *ts,
                        FILE *fout);


/**** INLINES: ****/
_pddl_inline int pddlPredIsStatic(const pddl_pred_t *pred)
{
    return !pred->write;
}

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_PRED_H__ */
