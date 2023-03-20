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

#include "internal.h"
#include "_heur.h"

pddl_heur_t *pot(const pddl_heur_config_t *cfg, pddl_err_t *err)
{
    pddl_hpot_config_t hcfg;
    pddlHPotConfigInitCopy(&hcfg, &cfg->pot);
    hcfg.fdr = cfg->fdr;
    hcfg.mg_strips = cfg->mg_strips;
    hcfg.mutex = cfg->mutex;
    pddl_heur_t *heur = pddlHeurPot(&hcfg, err);
    pddlHPotConfigFree(&hcfg);
    if (heur == NULL)
        TRACE_RET(err, NULL);
    return heur;
}

pddl_heur_t *pddlHeur(const pddl_heur_config_t *cfg, pddl_err_t *err)
{
    if (cfg->fdr == NULL)
        ERR_RET(err, NULL, "Config Error: Missing input task!");

    switch (cfg->heur){
        case PDDL_HEUR_BLIND:
            return pddlHeurBlind();
        case PDDL_HEUR_DEAD_END:
            return pddlHeurDeadEnd();
        case PDDL_HEUR_POT:
            return pot(cfg, err);
        case PDDL_HEUR_FLOW:
            return pddlHeurFlow(cfg->fdr, err);
        case PDDL_HEUR_LM_CUT:
            return pddlHeurLMCut(cfg->fdr, err);
        case PDDL_HEUR_HMAX:
            return pddlHeurHMax(cfg->fdr, err);
        case PDDL_HEUR_HADD:
            return pddlHeurHAdd(cfg->fdr, err);
        case PDDL_HEUR_HFF:
            return pddlHeurHFF(cfg->fdr, err);
        case PDDL_HEUR_OP_MUTEX:
            if (cfg->mutex == NULL)
                ERR_RET(err, NULL, "Config Error: Missing input mutex set!");
            return pddlHeurOpMutex(cfg->fdr, cfg->mutex, &cfg->op_mutex, err);
    }
    return NULL;
}

void pddlHeurDel(pddl_heur_t *h)
{
    h->del_fn(h);
}

int pddlHeurEstimate(pddl_heur_t *h,
                     const pddl_fdr_state_space_node_t *node,
                     const pddl_fdr_state_space_t *state_space)
{
    return h->estimate_fn(h, node, state_space);
}
