/***
 * cpddl
 * -------
 * Copyright (c)2020 Daniel Fiser <danfis@danfis.cz>,
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

#include "pddl/symbolic_vars.h"
#include "internal.h"


void pddlSymbolicVarsInit(pddl_symbolic_vars_t *vars,
                          int fact_size,
                          const pddl_mgroups_t *mgroups)
{
    ZEROIZE(vars);
    vars->group_size = mgroups->mgroup_size;
    vars->group = CALLOC_ARR(pddl_symbolic_fact_group_t, vars->group_size);
    vars->fact_size = fact_size;
    vars->fact = CALLOC_ARR(pddl_symbolic_fact_t, vars->fact_size);
    for (int fact_id = 0; fact_id < fact_size; ++fact_id){
        pddl_symbolic_fact_t *fact = vars->fact + fact_id;
        fact->id = fact_id;
        fact->group_id = -1;
        fact->val = -1;
    }

    vars->ordered_facts = ALLOC_ARR(int, fact_size);

    int fact_ins = 0;
    int var_id = 0;
    vars->bdd_var_size = 0;
    for (int mgi = 0; mgi < vars->group_size; ++mgi){
        const pddl_iset_t *mg = &mgroups->mgroup[mgi].mgroup;
        pddl_symbolic_fact_group_t *group = vars->group + mgi;
        group->id = mgi;
        pddlISetUnion(&group->fact, mg);
        int var_size = ceil(log2(pddlISetSize(&group->fact)));
        for (int i = 0; i < var_size; ++i){
            pddlISetAdd(&group->pre_var, var_id++);
            pddlISetAdd(&group->eff_var, var_id++);
        }
        vars->bdd_var_size += 2 * var_size;

        int val = 0;
        int fact_id;
        PDDL_ISET_FOR_EACH(mg, fact_id){
            pddl_symbolic_fact_t *fact = vars->fact + fact_id;
            ASSERT(fact->group_id < 0);
            ASSERT(fact->val < 0);
            fact->group_id = group->id;
            fact->val = val++;
            vars->ordered_facts[fact_ins++] = fact_id;
        }
    }
}

static pddl_bdd_t *createFactBDD(pddl_bdd_manager_t *mgr,
                                 pddl_symbolic_vars_t *vars,
                                 pddl_symbolic_fact_t *fact,
                                 int is_eff)
{
    pddl_bdd_t *bdd = pddlBDDOne(mgr);

    const pddl_iset_t *bdd_vars = &vars->group[fact->group_id].pre_var;
    if (is_eff)
        bdd_vars = &vars->group[fact->group_id].eff_var;

    int val = fact->val;
    int var_id;
    PDDL_ISET_FOR_EACH(bdd_vars, var_id){
        pddl_bdd_t *var = pddlBDDVar(mgr, var_id);
        if (val & 0x1){
            pddlBDDAndUpdate(mgr, &bdd, var);
        }else{
            pddl_bdd_t *nvar = pddlBDDNot(mgr, var);
            pddlBDDAndUpdate(mgr, &bdd, nvar);
            pddlBDDDel(mgr, nvar);
        }
        pddlBDDDel(mgr, var);

        val >>= 1;
    }
    ASSERT(val == 0);

    return bdd;
}

void pddlSymbolicVarsInitBDD(pddl_bdd_manager_t *mgr,
                             pddl_symbolic_vars_t *vars)
{
    vars->mgr = mgr;
    for (int fact_id = 0; fact_id < vars->fact_size; ++fact_id){
        pddl_symbolic_fact_t *fact = vars->fact + fact_id;
        fact->pre_bdd = createFactBDD(mgr, vars, fact, 0);
        fact->eff_bdd = createFactBDD(mgr, vars, fact, 1);
    }

    vars->valid_states = pddlBDDOne(mgr);
    for (int gi = 0; gi < vars->group_size; ++gi){
        pddl_symbolic_fact_group_t *group = vars->group + gi;

        pddl_bdd_t *bdd = pddlBDDZero(mgr);
        int fact_id;
        PDDL_ISET_FOR_EACH(&group->fact, fact_id){
            pddlBDDOrUpdate(mgr, &bdd, vars->fact[fact_id].pre_bdd);
        }

        pddlBDDAndUpdate(mgr, &vars->valid_states, bdd);
        pddlBDDDel(mgr, bdd);
    }
}

void pddlSymbolicVarsFree(pddl_symbolic_vars_t *vars)
{
    for (int gi = 0; gi < vars->group_size; ++gi){
        pddl_symbolic_fact_group_t *group = vars->group + gi;
        pddlISetFree(&group->fact);
        pddlISetFree(&group->pre_var);
        pddlISetFree(&group->eff_var);
    }
    if (vars->group != NULL)
        FREE(vars->group);

    for (int fact_id = 0; fact_id < vars->fact_size; ++fact_id){
        pddl_symbolic_fact_t *fact = vars->fact + fact_id;
        if (fact->pre_bdd != NULL)
            pddlBDDDel(vars->mgr, fact->pre_bdd);
        if (fact->eff_bdd != NULL)
            pddlBDDDel(vars->mgr, fact->eff_bdd);
    }
    if (vars->fact != NULL)
        FREE(vars->fact);

    if (vars->valid_states != NULL)
        pddlBDDDel(vars->mgr, vars->valid_states);

    if (vars->ordered_facts != NULL)
        FREE(vars->ordered_facts);
}

pddl_bdd_t *pddlSymbolicVarsCreateState(pddl_symbolic_vars_t *vars,
                                        const pddl_iset_t *state)
{
    pddl_bdd_t *bdd = pddlBDDOne(vars->mgr);

    int fact;
    PDDL_ISET_FOR_EACH(state, fact)
        pddlBDDAndUpdate(vars->mgr, &bdd, vars->fact[fact].pre_bdd);
    return bdd;
}

pddl_bdd_t *pddlSymbolicVarsCreatePartialState(pddl_symbolic_vars_t *vars,
                                               const pddl_iset_t *part_state)
{
    pddl_bdd_t *bdd = pddlBDDClone(vars->mgr, vars->valid_states);

    int fact;
    PDDL_ISET_FOR_EACH(part_state, fact)
        pddlBDDAndUpdate(vars->mgr, &bdd, vars->fact[fact].pre_bdd);
    return bdd;
}

pddl_bdd_t *pddlSymbolicVarsCreateBiimp(pddl_symbolic_vars_t *vars,
                                        int group_id)
{
    pddl_bdd_t *res = pddlBDDOne(vars->mgr);

    const pddl_iset_t *pre = &vars->group[group_id].pre_var;
    const pddl_iset_t *eff = &vars->group[group_id].eff_var;
    for (int i = 0; i < pddlISetSize(pre); ++i){
        int var1 = pddlISetGet(pre, i);
        int var2 = pddlISetGet(eff, i);

        pddl_bdd_t *bvar1 = pddlBDDVar(vars->mgr, var1);
        pddl_bdd_t *bvar2 = pddlBDDVar(vars->mgr, var2);
        pddl_bdd_t *bdd = pddlBDDXnor(vars->mgr, bvar1, bvar2);
        pddlBDDAndUpdate(vars->mgr, &res, bdd);
        pddlBDDDel(vars->mgr, bdd);
        pddlBDDDel(vars->mgr, bvar1);
        pddlBDDDel(vars->mgr, bvar2);
    }

    return res;
}

pddl_bdd_t *pddlSymbolicVarsCreateMutexPre(pddl_symbolic_vars_t *vars,
                                           int fact1, int fact2)
{
    pddl_bdd_t *var1 = pddlBDDClone(vars->mgr, vars->fact[fact1].pre_bdd);
    pddl_bdd_t *var2 = pddlBDDClone(vars->mgr, vars->fact[fact2].pre_bdd);
    pddl_bdd_t *bdd = pddlBDDAnd(vars->mgr, var1, var2);
    pddl_bdd_t *nbdd = pddlBDDNot(vars->mgr, bdd);
    pddlBDDDel(vars->mgr, bdd);
    pddlBDDDel(vars->mgr, var1);
    pddlBDDDel(vars->mgr, var2);
    return nbdd;
}

pddl_bdd_t *pddlSymbolicVarsCreateExactlyOneMGroupPre(pddl_symbolic_vars_t *vars,
                                                      const pddl_iset_t *mgroup)
{
    pddl_bdd_t *bdd = pddlBDDZero(vars->mgr);
    int fact_id;
    PDDL_ISET_FOR_EACH(mgroup, fact_id){
        pddl_bdd_t *var1 = pddlBDDClone(vars->mgr, vars->fact[fact_id].pre_bdd);
        pddlBDDOrUpdate(vars->mgr, &bdd, var1);
        pddlBDDDel(vars->mgr, var1);
    }
    return bdd;
}

pddl_bdd_t *pddlSymbolicVarsCreateExactlyOneMGroupEff(pddl_symbolic_vars_t *vars,
                                                      const pddl_iset_t *mgroup)
{
    pddl_bdd_t *bdd = pddlBDDZero(vars->mgr);
    int fact_id;
    PDDL_ISET_FOR_EACH(mgroup, fact_id){
        pddl_bdd_t *var1 = pddlBDDClone(vars->mgr, vars->fact[fact_id].eff_bdd);
        pddlBDDOrUpdate(vars->mgr, &bdd, var1);
        pddlBDDDel(vars->mgr, var1);
    }
    return bdd;
}


int pddlSymbolicVarsFactFromBDDCube(const pddl_symbolic_vars_t *vars,
                                    int group_id,
                                    const char *cube)
{
    const pddl_iset_t *bdd_vars = &vars->group[group_id].pre_var;
    int val = 0;
    int var_id;
    int pos = 0;
    PDDL_ISET_FOR_EACH(bdd_vars, var_id){
        if (cube[var_id] == 1)
            val |= (0x1 << pos);
        ++pos;
    }

    for (int fi = 0; fi < vars->fact_size; ++fi){
        if (vars->fact[fi].group_id == group_id
                && vars->fact[fi].val == val)
            return fi;
    }

    return -1;
}

void pddlSymbolicVarsGroupsBDDVars(pddl_symbolic_vars_t *vars,
                                   const pddl_iset_t *groups,
                                   pddl_bdd_t ***var_pre,
                                   pddl_bdd_t ***var_eff,
                                   int *var_size)
{
    int group_id;

    *var_size = 0;
    PDDL_ISET_FOR_EACH(groups, group_id)
        *var_size += pddlISetSize(&vars->group[group_id].pre_var);

    *var_pre = CALLOC_ARR(pddl_bdd_t *, *var_size);
    *var_eff = CALLOC_ARR(pddl_bdd_t *, *var_size);
    int ins = 0;
    PDDL_ISET_FOR_EACH(groups, group_id){
        const pddl_iset_t *pre_var = &vars->group[group_id].pre_var;
        const pddl_iset_t *eff_var = &vars->group[group_id].eff_var;
        for (int j = 0; j < pddlISetSize(pre_var); ++j){
            pddl_bdd_t *vpre = pddlBDDVar(vars->mgr, pddlISetGet(pre_var, j));
            pddl_bdd_t *veff = pddlBDDVar(vars->mgr, pddlISetGet(eff_var, j));
            (*var_pre)[ins] = vpre;
            (*var_eff)[ins] = veff;
            ++ins;
        }
    }
    ASSERT(ins == *var_size);
}
