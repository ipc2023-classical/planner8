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

#ifndef __PDDL__HEUR_H__
#define __PDDL__HEUR_H__

#include "internal.h"
#include "pddl/heur.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef void (*pddl_heur_del_fn)(pddl_heur_t *h);
typedef int (*pddl_heur_estimate_fn)(pddl_heur_t *h,
                                     const pddl_fdr_state_space_node_t *node,
                                     const pddl_fdr_state_space_t *state_space);
struct pddl_heur {
    pddl_heur_del_fn del_fn;
    pddl_heur_estimate_fn estimate_fn;
};

_pddl_inline void _pddlHeurInit(pddl_heur_t *h,
                                pddl_heur_del_fn del_fn,
                                pddl_heur_estimate_fn estimate_fn)
{
    ZEROIZE(h);
    h->del_fn = del_fn;
    h->estimate_fn = estimate_fn;
}

_pddl_inline void _pddlHeurFree(pddl_heur_t *h)
{
}

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL__HEUR_H__ */
