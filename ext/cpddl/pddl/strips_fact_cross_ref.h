/***
 * cpddl
 * -------
 * Copyright (c)2019 Daniel Fiser <danfis@danfis.cz>,
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

#ifndef __PDDL_STRIPS_FACT_CROSS_REF_H__
#define __PDDL_STRIPS_FACT_CROSS_REF_H__

#include <pddl/strips.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


struct pddl_strips_fact_cross_ref_fact {
    int fact_id; /*!< ID of the fact */
    int is_init; /*!< True if the fact is in the initial state */
    int is_goal; /*!< True if the fact is in the goal */
    pddl_iset_t op_pre; /*!< Operators having this fact in its precondition */
    pddl_iset_t op_add; /*!< Operators having this fact in its add effect */
    pddl_iset_t op_del; /*!< Operators having this fact in its del effect */
};
typedef struct pddl_strips_fact_cross_ref_fact
    pddl_strips_fact_cross_ref_fact_t;

struct pddl_strips_fact_cross_ref {
    pddl_strips_fact_cross_ref_fact_t *fact;
    int fact_size;
};
typedef struct pddl_strips_fact_cross_ref
    pddl_strips_fact_cross_ref_t;


void pddlStripsFactCrossRefInit(pddl_strips_fact_cross_ref_t *cref,
                                const pddl_strips_t *strips,
                                int init,
                                int goal,
                                int op_pre,
                                int op_add,
                                int op_del);

void pddlStripsFactCrossRefFree(pddl_strips_fact_cross_ref_t *cref);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_STRIPS_FACT_CROSS_REF_H__ */
