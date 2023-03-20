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

#include "pddl/hadd.h"
#include "internal.h"
#include "_heur.h"

struct pddl_heur_hadd {
    pddl_heur_t heur;
    pddl_fdr_vars_t fdr_vars;
    pddl_hadd_t hadd;
};
typedef struct pddl_heur_hadd pddl_heur_hadd_t;

static void heurDel(pddl_heur_t *_h)
{
    pddl_heur_hadd_t *h = pddl_container_of(_h, pddl_heur_hadd_t, heur);
    _pddlHeurFree(&h->heur);
    pddlFDRVarsFree(&h->fdr_vars);
    pddlHAddFree(&h->hadd);
    FREE(h);
}

static int heurEstimate(pddl_heur_t *_h,
                        const pddl_fdr_state_space_node_t *node,
                        const pddl_fdr_state_space_t *state_space)
{
    pddl_heur_hadd_t *h = pddl_container_of(_h, pddl_heur_hadd_t, heur);
    return pddlHAdd(&h->hadd, node->state, &h->fdr_vars);
}

pddl_heur_t *pddlHeurHAdd(const pddl_fdr_t *fdr, pddl_err_t *err)
{
    pddl_heur_hadd_t *h = ZALLOC(pddl_heur_hadd_t);
    pddlHAddInit(&h->hadd, fdr);
    pddlFDRVarsInitCopy(&h->fdr_vars, &fdr->var);
    _pddlHeurInit(&h->heur, heurDel, heurEstimate);
    return &h->heur;
}

