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

#include "internal.h"
#include "pddl/pddl_struct.h"
#include "datalog_pddl.h"


int pddlDatalogPddlMaxVarSize(const pddl_t *pddl)
{
    int max_var_size = 0;
    for (int i = 0; i < pddl->pred.pred_size; ++i)
        max_var_size = PDDL_MAX(max_var_size, pddl->pred.pred[i].param_size);
    for (int i = 0; i < pddl->action.action_size; ++i)
        max_var_size = PDDL_MAX(max_var_size, pddl->action.action[i].param.param_size);
    return max_var_size;
}

void pddlDatalogPddlAddTypeRules(pddl_datalog_t *dl,
                                 const pddl_t *pddl,
                                 const unsigned *type_to_dlpred,
                                 const unsigned *obj_to_dlconst)
{
    for (int ti = 0; ti < pddl->type.type_size; ++ti){
        if (type_to_dlpred[ti] == UINT_MAX)
            continue;
        int size;
        const pddl_obj_id_t *objs;
        objs = pddlTypesObjsByType(&pddl->type, ti, &size);
        for (int i = 0; i < size; ++i){
            pddl_datalog_atom_t atom;
            pddl_datalog_rule_t rule;
            pddlDatalogRuleInit(dl, &rule);
            pddlDatalogAtomInit(dl, &atom, type_to_dlpred[ti]);
            pddlDatalogAtomSetArg(dl, &atom, 0, obj_to_dlconst[objs[i]]);
            pddlDatalogRuleSetHead(dl, &rule, &atom);
            pddlDatalogAtomFree(dl, &atom);
            pddlDatalogAddRule(dl, &rule);
            pddlDatalogRuleFree(dl, &rule);
        }
    }
}

void pddlDatalogPddlAddEqRules(pddl_datalog_t *dl,
                               const pddl_t *pddl,
                               const unsigned *pred_to_dlpred,
                               const unsigned *obj_to_dlconst)
{
    int eqp = pddl->pred.eq_pred;
    for (int i = 0; i < pddl->obj.obj_size; ++i){
        pddl_datalog_atom_t atom;
        pddl_datalog_rule_t rule;
        pddlDatalogRuleInit(dl, &rule);
        pddlDatalogAtomInit(dl, &atom, pred_to_dlpred[eqp]);
        pddlDatalogAtomSetArg(dl, &atom, 0, obj_to_dlconst[i]);
        pddlDatalogAtomSetArg(dl, &atom, 1, obj_to_dlconst[i]);
        pddlDatalogRuleSetHead(dl, &rule, &atom);
        pddlDatalogAtomFree(dl, &atom);
        pddlDatalogAddRule(dl, &rule);
        pddlDatalogRuleFree(dl, &rule);
    }
}

void pddlDatalogPddlAtomToDLAtom(pddl_datalog_t *dl,
                                 pddl_datalog_atom_t *dlatom,
                                 const pddl_fm_atom_t *atom,
                                 const unsigned *pred_to_dlpred,
                                 const unsigned *obj_to_dlconst,
                                 const unsigned *dlvar)
{
    pddlDatalogAtomInit(dl, dlatom, pred_to_dlpred[atom->pred]);
    for (int i = 0; i < atom->arg_size; ++i){
        if (atom->arg[i].obj >= 0){
            pddlDatalogAtomSetArg(dl, dlatom, i,
                                  obj_to_dlconst[atom->arg[i].obj]);
        }else{
            int param = atom->arg[i].param;
            pddlDatalogAtomSetArg(dl, dlatom, i, dlvar[param]);
        }
    }
}

void pddlDatalogPddlSetActionTypeBody(pddl_datalog_t *dl,
                                      pddl_datalog_rule_t *rule,
                                      const pddl_t *pddl,
                                      const pddl_params_t *params,
                                      const pddl_fm_t *pre,
                                      const pddl_fm_t *pre2,
                                      unsigned *type_to_dlpred,
                                      const unsigned *dlvar)
{
    int param_covered[params->param_size];
    ZEROIZE_ARR(param_covered, params->param_size);

    const pddl_fm_atom_t *catom;
    pddl_fm_const_it_atom_t it;
    PDDL_FM_FOR_EACH_ATOM(pre, &it, catom){
        if (catom->neg)
            continue;
        for (int ai = 0; ai < catom->arg_size; ++ai){
            if (catom->arg[ai].param >= 0){
                int tpred = pddl->pred.pred[catom->pred].param[ai];
                int tparam = params->param[catom->arg[ai].param].type;
                if (pddlTypesIsSubset(&pddl->type, tpred, tparam))
                    param_covered[catom->arg[ai].param] = 1;
            }
        }
    }
    if (pre2 != NULL){
        PDDL_FM_FOR_EACH_ATOM(pre2, &it, catom){
            if (catom->neg)
                continue;
            for (int ai = 0; ai < catom->arg_size; ++ai){
                if (catom->arg[ai].param >= 0){
                    int tpred = pddl->pred.pred[catom->pred].param[ai];
                    int tparam = params->param[catom->arg[ai].param].type;
                    if (pddlTypesIsSubset(&pddl->type, tpred, tparam))
                        param_covered[catom->arg[ai].param] = 1;
                }
            }
        }
    }

    for (int pi = 0; pi < params->param_size; ++pi){
        if (param_covered[pi])
            continue;
        int type = params->param[pi].type;
        if (type_to_dlpred[type] == UINT_MAX)
            type_to_dlpred[type] = pddlDatalogAddPred(dl, 1, pddl->type.type[type].name);
        pddl_datalog_atom_t atom;
        pddlDatalogAtomInit(dl, &atom, type_to_dlpred[type]);
        pddlDatalogAtomSetArg(dl, &atom, 0, dlvar[pi]);
        pddlDatalogRuleAddBody(dl, rule, &atom);
        pddlDatalogAtomFree(dl, &atom);
    }
}
