/***
 * cpddl
 * -------
 * Copyright (c)2021 Daniel Fiser <danfis@danfis.cz>,
 * Saarland University, and
 * Czech Technical University in Prague.
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

#include "pddl/strips_ground_sql.h"
#include "pddl/prep_action.h"
#include "pddl/ground_atom.h"
#include "pddl/strips_maker.h"
#include "pddl/sql_grounder.h"
#include "internal.h"


struct sql_ground {
    pddl_sql_grounder_t *grounder;
    const pddl_t *pddl;
    pddl_strips_maker_t strips_maker;
};
typedef struct sql_ground sql_ground_t;

static int sqlGroundInit(sql_ground_t *g,
                         const pddl_t *pddl,
                         const pddl_ground_config_t *cfg,
                         pddl_err_t *err)
{
    ZEROIZE(g);
    g->pddl = pddl;
    g->grounder = pddlSqlGrounderNew(pddl, err);
    if (g->grounder == NULL)
        PDDL_TRACE_RET(err, -1);
    pddlStripsMakerInit(&g->strips_maker, g->pddl);

    // Insert initial state
    pddl_list_t *item;
    PDDL_LIST_FOR_EACH(&g->pddl->init->part, item){
        const pddl_fm_t *c = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);
        if (c->type == PDDL_FM_ATOM){
            const pddl_fm_atom_t *a = PDDL_FM_CAST(c, atom);
            if (pddlPredIsStatic(&pddl->pred.pred[a->pred])){
                pddlStripsMakerAddStaticAtom(&g->strips_maker, a, NULL, NULL);
            }else{
                pddlStripsMakerAddAtom(&g->strips_maker, a, NULL, NULL);
            }
            pddlSqlGrounderInsertAtom(g->grounder, a, err);

        }else if (c->type == PDDL_FM_ASSIGN){
            const pddl_fm_func_op_t *ass = PDDL_FM_CAST(c, func_op);
            ASSERT(ass->fvalue == NULL);
            ASSERT(ass->lvalue != NULL);
            ASSERT(pddlFmAtomIsGrounded(ass->lvalue));
            pddlStripsMakerAddFunc(&g->strips_maker, ass, NULL, NULL);
        }
    }
    PDDL_INFO(err, "Initial state inserted."
                  " %d atoms, %d static atoms, %d functions",
             g->strips_maker.ground_atom.atom_size,
             g->strips_maker.ground_atom_static.atom_size,
             g->strips_maker.ground_func.atom_size);
    return 0;
}

static void sqlGroundFree(sql_ground_t *g)
{
    pddlSqlGrounderDel(g->grounder);
    pddlStripsMakerFree(&g->strips_maker);
}

static int addLayerToGrounder(sql_ground_t *g, int layer, pddl_err_t *err)
{
    int updated = 0;
    for (int i = 0; i < g->strips_maker.ground_atom.atom_size; ++i){
        pddl_ground_atom_t *ga;
        ga = g->strips_maker.ground_atom.atom[i];
        if (ga->layer == layer){
            updated = 1;
            pddlSqlGrounderInsertAtomArgs(g->grounder, ga->pred, ga->arg, err);
        }
    }
    return updated || layer == 0;
}

static int addGroundAction(sql_ground_t *g,
                           int action_id,
                           const pddl_obj_id_t *row)
{
    const pddl_prep_action_t *paction;
    paction = pddlSqlGrounderPrepAction(g->grounder, action_id);
    int is_new = 0;
    int parent_id = action_id;
    if (paction->parent_action >= 0)
        parent_id = paction->parent_action;
    pddlStripsMakerAddAction(&g->strips_maker,
                             parent_id,
                             (parent_id == action_id ? 0 : action_id),
                             row,
                             &is_new);
    return is_new;
}

static int addGroundAtom(sql_ground_t *g,
                         int layer,
                         const pddl_fm_atom_t *atom,
                         const pddl_obj_id_t *row,
                         pddl_err_t *err)
{
    int is_new = 0;
    pddl_ground_atom_t *ga;
    ga = pddlStripsMakerAddAtom(&g->strips_maker, atom, row, &is_new);
    if (is_new && layer >= 0)
        ga->layer = layer;
    if (is_new && layer < 0){
        return pddlSqlGrounderInsertAtomArgs(g->grounder, atom->pred, ga->arg, err);
    }else if (is_new){
        return 1;
    }
    return 0;
}

static int sqlGroundStepActionRow(sql_ground_t *g,
                                  int layer,
                                  int action_id,
                                  pddl_obj_id_t *row,
                                  pddl_err_t *err)
{
    int updated = 0;

    // Try to add a new ground action
    if (!addGroundAction(g, action_id, row))
        return 0;

    const pddl_prep_action_t *paction;
    paction = pddlSqlGrounderPrepAction(g->grounder, action_id);
    for (int i = 0; i < paction->add_eff.size; ++i){
        const pddl_fm_atom_t *atom;
        atom = PDDL_FM_CAST(paction->add_eff.fm[i], atom);

        ASSERT(!pddlPredIsStatic(&g->pddl->pred.pred[atom->pred]));
        updated |= addGroundAtom(g, layer, atom, row, err);
    }

    return updated;
}


static int sqlGroundStepAction(sql_ground_t *g,
                               int layer,
                               int action_id,
                               pddl_err_t *err)
{
    if (pddlSqlGrounderActionStart(g->grounder, action_id, err) != 0)
        return 0;

    const pddl_prep_action_t *paction;
    paction = pddlSqlGrounderPrepAction(g->grounder, action_id);

    pddl_obj_id_t row[paction->param_size];
    int updated = 0;
    while (pddlSqlGrounderActionNext(g->grounder, row, err))
        updated |= sqlGroundStepActionRow(g, layer, action_id, row, err);
    return updated;
}

static int sqlGroundStep(sql_ground_t *g, int layer, pddl_err_t *err)
{
    int action_size = pddlSqlGrounderPrepActionSize(g->grounder);
    int updated = 0;
    for (int ai = 0; ai < action_size; ++ai)
        updated |= sqlGroundStepAction(g, layer, ai, err);
    return updated;
}

int pddlStripsGroundSql(pddl_strips_t *strips,
                        const pddl_t *pddl,
                        const pddl_ground_config_t *cfg,
                        pddl_err_t *err)
{
    CTX(err, "ground_sql", "Ground SQL");
    CTX_NO_TIME(err, "cfg", "Cfg");
    pddlGroundConfigLog(cfg, err);
    CTXEND(err);
    PDDL_INFO(err, "Grounding using sqlite ...");

    sql_ground_t ground;
    if (sqlGroundInit(&ground, pddl, cfg, err) != 0){
        CTXEND(err);
        PDDL_TRACE_RET(err, -1);
    }

    for (int step = 0; 1; ++step){
        PDDL_INFO(err, "Grounding step %d"
                      " (%d (split) actions and %d facts grounded so far) ...",
                 step, ground.strips_maker.num_action_args,
                 ground.strips_maker.ground_atom.atom_size);
        if (!sqlGroundStep(&ground, -1, err))
            break;
    }
    PDDL_INFO(err, "Grounding finished: %d (split) actions, %d facts,"
                  " %d static facts, %d functions",
             ground.strips_maker.num_action_args,
             ground.strips_maker.ground_atom.atom_size,
             ground.strips_maker.ground_atom_static.atom_size,
             ground.strips_maker.ground_func.atom_size);

    int ret = pddlStripsMakerMakeStrips(&ground.strips_maker, ground.pddl, cfg,
                                        strips, err);

    sqlGroundFree(&ground);
    if (ret != 0){
        CTXEND(err);
        PDDL_TRACE_RET(err, ret);
    }

    PDDL_INFO(err, "Grounding finished.");
    pddlStripsLogInfo(strips, err);
    CTXEND(err);
    return 0;
}

int pddlStripsGroundSqlLayered(const pddl_t *pddl,
                               const pddl_ground_config_t *cfg,
                               int max_layers,
                               int max_atoms,
                               pddl_strips_t *strips,
                               pddl_ground_atoms_t *ground_atoms,
                               pddl_err_t *err)
{
    // TODO: max_atoms
    CTX(err, "ground_sql_layer", "Ground SQL Layer");
    CTX_NO_TIME(err, "cfg", "Cfg");
    pddlGroundConfigLog(cfg, err);
    LOG(err, "max_layers = %d", max_layers);
    LOG(err, "max_atoms = %d", max_atoms);
    CTXEND(err);
    LOG(err, "Grounding using sqlite ...");

    sql_ground_t ground;
    if (sqlGroundInit(&ground, pddl, cfg, err) != 0){
        CTXEND(err);
        PDDL_TRACE_RET(err, -1);
    }
    for (int step = 0; step < max_layers; ++step){
        PDDL_INFO(err, "Grounding layer %d"
                      " (%d (split) actions and %d facts grounded so far) ...",
                 step, ground.strips_maker.num_action_args,
                 ground.strips_maker.ground_atom.atom_size);
        if (!addLayerToGrounder(&ground, step, err))
            break;
        sqlGroundStep(&ground, step + 1, err);
    }
    PDDL_INFO(err, "Grounding finished: %d (split) actions, %d facts,"
                  " %d static facts, %d functions",
             ground.strips_maker.num_action_args,
             ground.strips_maker.ground_atom.atom_size,
             ground.strips_maker.ground_atom_static.atom_size,
             ground.strips_maker.ground_func.atom_size);

    int ret = 0;
    if (strips != NULL){
        ret = pddlStripsMakerMakeStrips(&ground.strips_maker, ground.pddl, cfg,
                                        strips, err);
    }

    if (ground_atoms != NULL){
        for (int i = 0; i < ground.strips_maker.ground_atom.atom_size; ++i){
            pddl_ground_atom_t *ga = ground.strips_maker.ground_atom.atom[i];
            pddl_ground_atom_t *n;
            n = pddlGroundAtomsAddPred(ground_atoms, ga->pred, ga->arg, ga->arg_size);
            n->layer = ga->layer;
        }
        for (int i = 0; i < ground.strips_maker.ground_atom_static.atom_size; ++i){
            pddl_ground_atom_t *ga = ground.strips_maker.ground_atom_static.atom[i];
            pddl_ground_atom_t *n;
            n = pddlGroundAtomsAddPred(ground_atoms, ga->pred, ga->arg, ga->arg_size);
            n->layer = ga->layer;
        }
    }

    sqlGroundFree(&ground);
    if (ret != 0){
        CTXEND(err);
        PDDL_TRACE_RET(err, ret);
    }

    PDDL_INFO(err, "Grounding finished.");
    if (strips != NULL)
        pddlStripsLogInfo(strips, err);
    CTXEND(err);
    return 0;
}
