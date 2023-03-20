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

#include "_heur.h"
#include "internal.h"

static void heurDel(pddl_heur_t *h)
{
    _pddlHeurFree(h);
    FREE(h);
}

static int heurEstimate(pddl_heur_t *h,
                        const pddl_fdr_state_space_node_t *node,
                        const pddl_fdr_state_space_t *state_space)
{
    return 0;
}

pddl_heur_t *pddlHeurBlind(void)
{
    pddl_heur_t *h = ALLOC(pddl_heur_t);
    _pddlHeurInit(h, heurDel, heurEstimate);
    return h;

}

