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

#include "pddl/extarr.h"
#include "pddl/lm_cut.h"
#include "_heur.h"
#include "internal.h"

struct pddl_heur_lmc {
    pddl_heur_t heur;
    pddl_lm_cut_t lmc;
    pddl_fdr_vars_t fdr_vars;
    pddl_extarr_t *cache; // TODO: Refactor and generalize
};
typedef struct pddl_heur_lmc pddl_heur_lmc_t;

static void heurDel(pddl_heur_t *_h)
{
    pddl_heur_lmc_t *h = pddl_container_of(_h, pddl_heur_lmc_t, heur);
    _pddlHeurFree(&h->heur);
    pddlFDRVarsFree(&h->fdr_vars);
    pddlLMCutFree(&h->lmc);
    pddlExtArrDel(h->cache);
    FREE(h);
}

static int heurEstimate(pddl_heur_t *_h,
                        const pddl_fdr_state_space_node_t *node,
                        const pddl_fdr_state_space_t *state_space)
{
    pddl_heur_lmc_t *h = pddl_container_of(_h, pddl_heur_lmc_t, heur);
    int *hval = pddlExtArrGet(h->cache, node->id);
    if (*hval == PDDL_COST_MAX)
        *hval = pddlLMCut(&h->lmc, node->state, &h->fdr_vars, NULL, NULL);
    return *hval;
}

pddl_heur_t *pddlHeurLMCut(const pddl_fdr_t *fdr, pddl_err_t *err)
{
    pddl_heur_lmc_t *h = ZALLOC(pddl_heur_lmc_t);
    pddlLMCutInit(&h->lmc, fdr, 0, 0);
    pddlFDRVarsInitCopy(&h->fdr_vars, &fdr->var);
    _pddlHeurInit(&h->heur, heurDel, heurEstimate);
    int init = PDDL_COST_MAX;
    h->cache = pddlExtArrNew2(sizeof(int), 1024, 1024 * 1024, NULL, &init);
    return &h->heur;
}
