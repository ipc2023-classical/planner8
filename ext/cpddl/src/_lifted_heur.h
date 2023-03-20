/***
 * Copyright (c)2022 Daniel Fiser <danfis@danfis.cz>,
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

#ifndef __PDDL__LIFTED_HEUR_H__
#define __PDDL__LIFTED_HEUR_H__

#include <pddl/heur.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef void (*pddl_lifted_heur_del_fn)(pddl_lifted_heur_t *h);
typedef pddl_cost_t (*pddl_lifted_heur_estimate_fn)(
                        pddl_lifted_heur_t *h,
                        const pddl_iset_t *state,
                        const pddl_ground_atoms_t *gatoms);
struct pddl_lifted_heur {
    pddl_lifted_heur_del_fn del_fn;
    pddl_lifted_heur_estimate_fn estimate_fn;
};

_pddl_inline void _pddlLiftedHeurInit(pddl_lifted_heur_t *h,
                                      pddl_lifted_heur_del_fn del_fn,
                                      pddl_lifted_heur_estimate_fn estimate_fn)
{
    PDDL_ZEROIZE(h);
    h->del_fn = del_fn;
    h->estimate_fn = estimate_fn;
}

_pddl_inline void _pddlLiftedHeurFree(pddl_lifted_heur_t *h)
{
}

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL__LIFTED_HEUR_H__ */
