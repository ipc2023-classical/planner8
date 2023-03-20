/***
 * cpddl
 * -------
 * Copyright (c)2021 Daniel Fiser <danfis@danfis.cz>,
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

#include "pddl/homomorphism_heur.h"
#include "pddl/lm_cut.h"
#include "pddl/hff.h"
#include "pddl/strips_ground_sql.h"
#include "pddl/prune_strips.h"
#include "internal.h"

#define LM_CUT_TYPE 1
#define HFF_TYPE 2

struct lmcut {
    pddl_homomorphism_heur_t homo;
    pddl_lm_cut_t lmc;
};
typedef struct lmcut lmcut_t;

struct hff {
    pddl_homomorphism_heur_t homo;
    pddl_hff_t hff;
};
typedef struct hff hff_t;

static int pddlHomomorphismHeurInit(pddl_homomorphism_heur_t *h,
                                    const pddl_t *pddl,
                                    const pddl_homomorphism_config_t *cfg,
                                    pddl_err_t *err)
{
    ZEROIZE(h);
    h->obj_map = CALLOC_ARR(pddl_obj_id_t, pddl->obj.obj_size);
    //pddlInitCopy(&h->homo, pddl);
    if (pddlHomomorphism(&h->homo, pddl, cfg, h->obj_map, err) != 0){
        FREE(h->obj_map);
        PDDL_TRACE_RET(err, -1);
    }

    pddl_ground_config_t ground_cfg = PDDL_GROUND_CONFIG_INIT;
    if (pddlStripsGroundSql(&h->strips, &h->homo, &ground_cfg, err) != 0){
        FREE(h->obj_map);
        pddlFree(&h->homo);
        PDDL_TRACE_RET(err, -1);
    }

    pddl_prune_strips_t prune;
    pddlPruneStripsInit(&prune);
    pddlPruneStripsAddIrrelevance(&prune);
    pddlPruneStripsAddH2(&prune, -1);
    pddlPruneStripsExecute(&prune, &h->strips, NULL, err);
    pddlPruneStripsFree(&prune);
    //pddlStripsPrintDebug(&h->strips, stderr);
    return 0;
}

pddl_homomorphism_heur_t *pddlHomomorphismHeurLMCut(
                                const pddl_t *pddl,
                                const pddl_homomorphism_config_t *cfg,
                                pddl_err_t *err)
{
    CTX(err, "homo_lmc", "Homomorph lm-cut");
    lmcut_t *lmc = ALLOC(lmcut_t);
    if (pddlHomomorphismHeurInit(&lmc->homo, pddl, cfg, err) != 0){
        FREE(lmc);
        CTXEND(err);
        PDDL_TRACE_RET(err, NULL);
    }
    lmc->homo._type = LM_CUT_TYPE;

    pddlLMCutInitStrips(&lmc->lmc, &lmc->homo.strips, 0, 0);
    PDDL_INFO(err, "Constructed lm-cut heuristic from the grounded"
               " homomorphic image");
    CTXEND(err);
    return &lmc->homo;
}

pddl_homomorphism_heur_t *pddlHomomorphismHeurHFF(
                                const pddl_t *pddl,
                                const pddl_homomorphism_config_t *cfg,
                                pddl_err_t *err)
{
    CTX(err, "homo_hff", "Homomorph hff");
    hff_t *hff = ALLOC(hff_t);
    if (pddlHomomorphismHeurInit(&hff->homo, pddl, cfg, err) != 0){
        FREE(hff);
        CTXEND(err);
        PDDL_TRACE_RET(err, NULL);
    }
    hff->homo._type = HFF_TYPE;

    pddlHFFInitStrips(&hff->hff, &hff->homo.strips);
    PDDL_INFO(err, "Constructed h^ff heuristic from the grounded"
               " homomorphic image");
    CTXEND(err);
    return &hff->homo;
}

void pddlHomomorphismHeurDel(pddl_homomorphism_heur_t *h)
{

    pddlFree(&h->homo);
    pddlStripsFree(&h->strips);
    if (h->obj_map != NULL)
        FREE(h->obj_map);
    if (h->ground_atom_to_strips_fact != NULL)
        FREE(h->ground_atom_to_strips_fact);

    if (h->_type == LM_CUT_TYPE){
        lmcut_t *lmc = pddl_container_of(h, lmcut_t, homo);
        pddlLMCutFree(&lmc->lmc);
        FREE(lmc);

    }else if (h->_type == HFF_TYPE){
        hff_t *hff = pddl_container_of(h, hff_t, homo);
        pddlHFFFree(&hff->hff);
        FREE(hff);
    }
}


static void allocateGroundAtomToStripsFact(pddl_homomorphism_heur_t *h,
                                           int state_fact)
{
    int init_size = h->ground_atom_to_strips_fact_size;
    while (state_fact >= h->ground_atom_to_strips_fact_size){
        if (h->ground_atom_to_strips_fact_size == 0)
            h->ground_atom_to_strips_fact_size = 1;
        h->ground_atom_to_strips_fact_size *= 2;
    }
    h->ground_atom_to_strips_fact
        = REALLOC_ARR(h->ground_atom_to_strips_fact,
                      int, h->ground_atom_to_strips_fact_size);
    for (int i = init_size; i < h->ground_atom_to_strips_fact_size; ++i)
        h->ground_atom_to_strips_fact[i] = -1;
}

static int findStripsFact(const pddl_homomorphism_heur_t *h,
                          const pddl_ground_atom_t *ga)
{
    for (int fact_id = 0; fact_id < h->strips.fact.fact_size; ++fact_id){
        const pddl_fact_t *fact = h->strips.fact.fact[fact_id];
        const pddl_ground_atom_t *fga = fact->ground_atom;
        ASSERT_RUNTIME(fga != NULL);
        if (fga->pred != ga->pred)
            continue;
        ASSERT(fga->arg_size == ga->arg_size);
        int eq = 1;
        for (int i = 0; i < fga->arg_size; ++i){
            pddl_obj_id_t ga_obj = h->obj_map[ga->arg[i]];
            if (fga->arg[i] != ga_obj){
                eq = 0;
                break;
            }
        }
        if (eq)
            return fact_id;
    }
    return -1;
}

int pddlHomomorphismHeurEvalGroundInit(pddl_homomorphism_heur_t *h)
{
    if (pddlISetIsSubset(&h->strips.goal, &h->strips.init))
        return 0;

    int hval = 0;
    if (h->_type == LM_CUT_TYPE){
        lmcut_t *lmc = pddl_container_of(h, lmcut_t, homo);
        hval = pddlLMCutStrips(&lmc->lmc, &h->strips.init, NULL, NULL);
    }else if (h->_type == HFF_TYPE){
        hff_t *hff = pddl_container_of(h, hff_t, homo);
        hval = pddlHFFStrips(&hff->hff, &h->strips.init);
    }
    return hval;
}

int pddlHomomorphismHeurEval(pddl_homomorphism_heur_t *h,
                             const pddl_iset_t *state,
                             const pddl_ground_atoms_t *gatoms)
{
    if (pddlISetIsSubset(&h->strips.goal, &h->strips.init))
        return 0;

    PDDL_ISET(strips_state);
    int state_fact;
    PDDL_ISET_FOR_EACH(state, state_fact){
        if (state_fact >= h->ground_atom_to_strips_fact_size)
            allocateGroundAtomToStripsFact(h, state_fact);
        int strips_fact = h->ground_atom_to_strips_fact[state_fact];
        if (strips_fact < 0){
            const pddl_ground_atom_t *ga = gatoms->atom[state_fact];
            strips_fact = findStripsFact(h, ga);
            h->ground_atom_to_strips_fact[state_fact] = strips_fact;
        }
        if (strips_fact >= 0)
            pddlISetAdd(&strips_state, strips_fact);
    }

    int hval = 0;
    if (h->_type == LM_CUT_TYPE){
        lmcut_t *lmc = pddl_container_of(h, lmcut_t, homo);
        hval = pddlLMCutStrips(&lmc->lmc, &strips_state, NULL, NULL);
    }else if (h->_type == HFF_TYPE){
        hff_t *hff = pddl_container_of(h, hff_t, homo);
        hval = pddlHFFStrips(&hff->hff, &strips_state);
    }
    pddlISetFree(&strips_state);
    return hval;
}
