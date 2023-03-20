/***
 * Copyright (c)2022 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

#include "internal.h"
#include "_lifted_app_action.h"
#include "pddl/sql_grounder.h"

struct app_action {
    pddl_lifted_app_action_t app_action;
    pddl_err_t *err;
    pddl_sql_grounder_t *grounder;
};
typedef struct app_action app_action_t;

#define AA(S,_S) CONTAINER_OF(S, (_S), app_action_t, app_action)

static void aaDel(pddl_lifted_app_action_t *_aa)
{
    AA(aa, _aa);
    pddlSqlGrounderDel(aa->grounder);
}

static void aaClearState(pddl_lifted_app_action_t *_aa)
{
    AA(aa, _aa);
    pddlSqlGrounderClearNonStatic(aa->grounder, aa->err);
}

static int aaSetStateAtom(pddl_lifted_app_action_t *_aa,
                          const pddl_ground_atom_t *atom)
{
    AA(aa, _aa);
    if (pddlSqlGrounderInsertGroundAtom(aa->grounder, atom, aa->err))
        return 0;
    return -1;
}

static int aaFindAppActions(pddl_lifted_app_action_t *_aa)
{
    AA(aa, _aa);
    int action_size = pddlSqlGrounderPrepActionSize(aa->grounder);
    for (int ai = 0; ai < action_size; ++ai){
        const pddl_prep_action_t *pa;
        pa = pddlSqlGrounderPrepAction(aa->grounder, ai);
        if (pa->parent_action >= 0)
            continue;

        if (pddlSqlGrounderActionStart(aa->grounder, ai, aa->err) != 0)
            continue;
        pddl_obj_id_t row[pa->param_size];
        while (pddlSqlGrounderActionNext(aa->grounder, row, aa->err)){
            pddlLiftedAppActionAdd(_aa, pa->action->id, row, pa->param_size);
        }
    }
    return 0;
}

pddl_lifted_app_action_t *pddlLiftedAppActionNewSql(const pddl_t *pddl,
                                                    pddl_err_t *err)
{
    app_action_t *aa = ZALLOC(app_action_t);
    _pddlLiftedAppActionInit(&aa->app_action, pddl, aaDel, aaClearState,
                             aaSetStateAtom, aaFindAppActions, err);

    aa->err = err;
    aa->grounder = pddlSqlGrounderNew(pddl, err);

    pddl_fm_const_it_t it;
    const pddl_fm_t *fm;
    PDDL_FM_FOR_EACH(&pddl->init->fm, &it, fm){
        if (pddlFmIsAtom(fm)){
            const pddl_fm_atom_t *atom = pddlFmToAtomConst(fm);
            if (pddlPredIsStatic(&pddl->pred.pred[atom->pred])){
                pddlSqlGrounderInsertAtom(aa->grounder, atom, err);
            }
        }
    }

    return &aa->app_action;
}
