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

#ifndef __PDDL_SYM_H__
#define __PDDL_SYM_H__

#include <pddl/strips.h>
#include <pddl/set.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * Symmetry generator for STRIPS planning task
 */
struct pddl_strips_sym_gen {
    int *fact; /*!< Mapping from facts to facts */
    int *op; /*!< Mapping from operators to operators */
    int *fact_inv; /*!< Inverse mapping of facts */
    int *op_inv; /*!< Inverse mapping of operators */
    pddl_iset_t *op_cycle; /*!< Sets of operators that are permuted between
                               ecah other */
    int op_cycle_size;
    int op_cycle_alloc;
};
typedef struct pddl_strips_sym_gen pddl_strips_sym_gen_t;

/**
 * Symmetry of STRIPS planning task as a set of generators.
 */
struct pddl_strips_sym {
    pddl_strips_sym_gen_t *gen; /*!< A list of generators */
    int gen_size;
    int gen_alloc;

    int fact_size;
    int op_size;
};
typedef struct pddl_strips_sym pddl_strips_sym_t;


/**
 * Find symmetries based on Problem Description Graph.
 */
void pddlStripsSymInitPDG(pddl_strips_sym_t *sym, const pddl_strips_t *strips);

/**
 * Free allocated memory.
 */
void pddlStripsSymFree(pddl_strips_sym_t *sym);

/**
 * Computes all sets that are symmetric to the given set of sets of facts.
 * It takes sym_set and updates it with everything that is symmetric to all
 * sets that were to stored in it.
 * {sym_set} must be a set of isets (pddl_iset_t).
 */
void pddlStripsSymAllFactSetSymmetries(const pddl_strips_sym_t *sym,
                                       pddl_set_iset_t *sym_set);
void pddlStripsSymAllOpSetSymmetries(const pddl_strips_sym_t *sym,
                                     pddl_set_iset_t *sym_set);

/**
 * Computes a transitive closure over all symmetries of the given operator.
 */
void pddlStripsSymOpTransitiveClosure(const pddl_strips_sym_t *sym,
                                      int op_id,
                                      pddl_iset_t *transitive_closure);

/**
 * Add to outset symmetric operators to operators in inset according to
 * generator with gen_id ID.
 */
void pddlStripsSymOpSet(const pddl_strips_sym_t *sym,
                        int gen_id,
                        const pddl_iset_t *inset,
                        pddl_iset_t *outset);

void pddlStripsSymPrintDebug(const pddl_strips_sym_t *sym, FILE *fout);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_SYM_H__ */
