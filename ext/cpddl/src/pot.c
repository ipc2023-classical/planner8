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

#include "pddl/hfunc.h"
#include "pddl/lp.h"
#include "pddl/pot.h"
#include "pddl/disambiguation.h"
#include "pddl/sort.h"
#include "internal.h"

#define LPVAR_UPPER 1E7
#define LPVAR_LOWER -1E20
#define ROUND_EPS 0.001

static int roundOff(double z)
{
    return ceil(z - ROUND_EPS);
}

static int potFltToInt(double pot)
{
    if (pot < 0.)
        return 0;
    if (pot > INT_MAX / 2 || pot > LPVAR_UPPER)
        return PDDL_COST_DEAD_END;
    return roundOff(pot);
}


void pddlPotSolutionInit(pddl_pot_solution_t *sol)
{
    ZEROIZE(sol);
}

void pddlPotSolutionFree(pddl_pot_solution_t *sol)
{
    if (sol->pot != NULL)
        FREE(sol->pot);
    if (sol->op_pot != NULL)
        FREE(sol->op_pot);
}

double pddlPotSolutionEvalFDRStateFlt(const pddl_pot_solution_t *sol,
                                      const pddl_fdr_vars_t *vars,
                                      const int *state)
{
    // Use kahan summation
    double sum = 0.;
    double comp = 0.;
    for (int var = 0; var < vars->var_size; ++var){
        double v = sol->pot[vars->var[var].val[state[var]].global_id];
        double y = v - comp;
        double t = sum + y;
        comp = (t - sum) - y;
        sum = t;
    }
    return sum;
}

int pddlPotSolutionEvalFDRState(const pddl_pot_solution_t *sol,
                                const pddl_fdr_vars_t *vars,
                                const int *state)
{
    return potFltToInt(pddlPotSolutionEvalFDRStateFlt(sol, vars, state));
}

double pddlPotSolutionEvalStripsStateFlt(const pddl_pot_solution_t *sol,
                                         const pddl_iset_t *state)
{
    // Use kahan summation
    double sum = 0.;
    double comp = 0.;
    int fact_id;
    PDDL_ISET_FOR_EACH(state, fact_id){
        double v = sol->pot[fact_id];
        double y = v - comp;
        double t = sum + y;
        comp = (t - sum) - y;
        sum = t;
    }
    return sum;
}

int pddlPotSolutionEvalStripsState(const pddl_pot_solution_t *sol,
                                   const pddl_iset_t *state)
{
    return potFltToInt(pddlPotSolutionEvalStripsStateFlt(sol, state));
}

int pddlPotSolutionRoundHValue(double hvalue)
{
    return potFltToInt(hvalue);
}

void pddlPotSolutionsInit(pddl_pot_solutions_t *sols)
{
    ZEROIZE(sols);
}

void pddlPotSolutionsFree(pddl_pot_solutions_t *sols)
{
    for (int i = 0; i < sols->sol_size; ++i)
        pddlPotSolutionFree(sols->sol + i);
}

void pddlPotSolutionsAdd(pddl_pot_solutions_t *sols,
                         const pddl_pot_solution_t *sol)
{
    if (sols->sol_size >= sols->sol_alloc){
        if (sols->sol_alloc == 0)
            sols->sol_alloc = 1;
        sols->sol_alloc *= 2;
        sols->sol = REALLOC_ARR(sols->sol, pddl_pot_solution_t,
                                sols->sol_alloc);
    }

    pddl_pot_solution_t *s = sols->sol + sols->sol_size++;
    pddlPotSolutionInit(s);
    s->pot_size = sol->pot_size;
    if (s->pot_size > 0){
        s->pot = ALLOC_ARR(double, s->pot_size);
        memcpy(s->pot, sol->pot, sizeof(double) * s->pot_size);
    }
    s->objval = sol->objval;
    s->op_pot_size = sol->op_pot_size;
    if (s->op_pot_size > 0){
        s->op_pot = ALLOC_ARR(double, s->op_pot_size);
        memcpy(s->op_pot, sol->op_pot, sizeof(double) * s->op_pot_size);
    }
}

int pddlPotSolutionsEvalMaxFDRState(const pddl_pot_solutions_t *sols,
                                    const pddl_fdr_vars_t *vars,
                                    const int *fdr_state)
{
    int h = 0;
    for (int i = 0; i < sols->sol_size; ++i){
        int h1 = pddlPotSolutionEvalFDRState(sols->sol + i, vars, fdr_state);
        h = PDDL_MAX(h, h1);
    }
    return h;
}

struct maxpot_var {
    int var_id;
    int count;
};
typedef struct maxpot_var maxpot_var_t;

struct maxpot {
    maxpot_var_t *var;
    int var_size;
    int maxpot_id;
    int id;

    pddl_htable_key_t hkey;
    pddl_list_t htable;
};
typedef struct maxpot maxpot_t;

static pddl_htable_key_t maxpotComputeHash(const maxpot_t *m)
{
    return pddlCityHash_64(m->var, sizeof(maxpot_var_t) * m->var_size);
}

static pddl_htable_key_t htableHash(const pddl_list_t *key, void *_)
{
    const maxpot_t *m = PDDL_LIST_ENTRY(key, maxpot_t, htable);
    return m->hkey;
}

static int htableEq(const pddl_list_t *key1, const pddl_list_t *key2, void *_)
{
    const maxpot_t *m1 = PDDL_LIST_ENTRY(key1, maxpot_t, htable);
    const maxpot_t *m2 = PDDL_LIST_ENTRY(key2, maxpot_t, htable);
    if (m1->var_size != m2->var_size)
        return 0;
    return memcmp(m1->var, m2->var, sizeof(maxpot_var_t) * m1->var_size) == 0;
}

static pddl_pot_constr_t *addConstr(pddl_pot_constrs_t *cs, int op_id)
{
    if (cs->size >= cs->alloc){
        if (cs->alloc == 0)
            cs->alloc = 4;
        cs->alloc *= 2;
        cs->c = REALLOC_ARR(cs->c, pddl_pot_constr_t, cs->alloc);
    }

    pddl_pot_constr_t *c = cs->c + cs->size++;
    ZEROIZE(c);
    c->op_id = op_id;
    return c;
}


static void putBackLastConstr(pddl_pot_constrs_t *cs)
{
    pddlISetFree(&cs->c[cs->size - 1].plus);
    pddlISetFree(&cs->c[cs->size - 1].minus);
    --cs->size;
}

static int getMaxpot(pddl_pot_t *pot,
                     const pddl_iset_t *set,
                     const int *count)
{
    maxpot_t *m = pddlSegmArrGet(pot->maxpot, pot->maxpot_size);

    m->var_size = pddlISetSize(set);
    m->var = CALLOC_ARR(maxpot_var_t, m->var_size);
    for (int i = 0; i < m->var_size; ++i){
        m->var[i].var_id = pddlISetGet(set, i);
        if (count != NULL)
            m->var[i].count = count[m->var[i].var_id];
    }
    m->hkey = maxpotComputeHash(m);
    pddlListInit(&m->htable);

    pddl_list_t *found;
    found = pddlHTableInsertUnique(pot->maxpot_htable, &m->htable);
    if (found == NULL){
        m->id = pot->maxpot_size++;
        m->maxpot_id = pot->var_size++;
        return m->maxpot_id;

    }else{
        if (m->var != NULL)
            FREE(m->var);

        m = PDDL_LIST_ENTRY(found, maxpot_t, htable);
        return m->maxpot_id;
    }
}

static int getFDRMaxpot(pddl_pot_t *pot,
                        int var_id,
                        const pddl_fdr_vars_t *vars)
{
    PDDL_ISET(lp_vars);
    for (int val = 0; val < vars->var[var_id].val_size; ++val)
        pddlISetAdd(&lp_vars, vars->var[var_id].val[val].global_id);
    int lp_var_id = getMaxpot(pot, &lp_vars, NULL);
    pddlISetFree(&lp_vars);
    return lp_var_id;
}

static void addFDROp(pddl_pot_t *pot,
                     const pddl_fdr_vars_t *vars,
                     const pddl_fdr_op_t *op)
{
    pddl_pot_constr_t *c = addConstr(&pot->constr_op, op->id);

    for (int effi = 0; effi < op->eff.fact_size; ++effi){
        const pddl_fdr_fact_t *eff = op->eff.fact + effi;
        int pre = pddlFDRPartStateGet(&op->pre, eff->var);
        if (pre >= 0){
            pddlISetAdd(&c->plus, vars->var[eff->var].val[pre].global_id);
        }else{
            pddlISetAdd(&c->plus, getFDRMaxpot(pot, eff->var, vars));
        }
        pddlISetAdd(&c->minus, vars->var[eff->var].val[eff->val].global_id);
    }
    c->rhs = op->cost;
}

static void addFDRGoal(pddl_pot_t *pot,
                       const pddl_fdr_vars_t *vars,
                       const pddl_fdr_part_state_t *goal)
{
    pddl_pot_constr_t *c = addConstr(&pot->constr_goal, -1);
    for (int var_id = 0; var_id < vars->var_size; ++var_id){
        int eff = pddlFDRPartStateGet(goal, var_id);
        if (eff >= 0){
            pddlISetAdd(&c->plus, vars->var[var_id].val[eff].global_id);
        }else{
            pddlISetAdd(&c->plus, getFDRMaxpot(pot, var_id, vars));
        }
    }
    c->rhs = 0;
}

static void addFDRInit(pddl_pot_t *pot, const pddl_fdr_vars_t *vars, const int *init)
{
    for (int var = 0; var < vars->var_size; ++var)
        pddlISetAdd(&pot->init, vars->var[var].val[init[var]].global_id);
}

static void hsetToVarSet(pddl_pot_t *pot,
                         const pddl_set_iset_t *hset,
                         pddl_iset_t *var_set)
{
    int *count = CALLOC_ARR(int, pot->var_size);
    const pddl_iset_t *set;
    PDDL_SET_ISET_FOR_EACH(hset, set){
        int fact_id;
        PDDL_ISET_FOR_EACH(set, fact_id)
            count[fact_id] += 1;
    }

    PDDL_SET_ISET_FOR_EACH(hset, set){
        if (pddlISetSize(set) == 1){
            int fact_id = pddlISetGet(set, 0);
            ASSERT(count[fact_id] == 1);
            pddlISetAdd(var_set, fact_id);
        }else{
            int maxpot_id = getMaxpot(pot, set, count);
            pddlISetAdd(var_set, maxpot_id);
        }
    }

    if (count != NULL)
        FREE(count);
}

static void addMGStripsOp(pddl_pot_t *pot,
                          pddl_disambiguate_t *dis,
                          const pddl_strips_op_t *op,
                          int single_fact_dis)
{
    pddl_set_iset_t hset;
    pddlSetISetInit(&hset);

    if (pddlDisambiguate(dis, &op->pre, &op->add_eff, 0,
                single_fact_dis, &hset, NULL) < 0){
        // Skip unreachable operators
        pddlSetISetFree(&hset);
        return;
    }

    pddl_pot_constr_t *c = addConstr(&pot->constr_op, op->id);
    hsetToVarSet(pot, &hset, &c->plus);
    pddlISetUnion(&c->minus, &op->add_eff);
    c->rhs = op->cost;

    PDDL_ISET(inter);
    pddlISetIntersect2(&inter, &c->plus, &c->minus);
    pddlISetMinus(&c->minus, &inter);
    pddlISetMinus(&c->plus, &inter);
    pddlISetFree(&inter);

    if (pddlISetSize(&c->plus) == 0 && pddlISetSize(&c->minus) == 0)
        putBackLastConstr(&pot->constr_op);

    pddlSetISetFree(&hset);
}

static int addMGStripsGoal(pddl_pot_t *pot,
                           pddl_disambiguate_t *dis,
                           const pddl_iset_t *goal,
                           int single_fact_dis)
{
    pddl_set_iset_t hset;
    pddlSetISetInit(&hset);

    if (pddlDisambiguate(dis, goal, NULL, 0, single_fact_dis, &hset, NULL) < 0){
        pddlSetISetFree(&hset);
        return -1;
    }

    pddl_pot_constr_t *c = addConstr(&pot->constr_goal, -1);
    hsetToVarSet(pot, &hset, &c->plus);
    c->rhs = 0;

    pddlSetISetFree(&hset);
    return 0;
}

static void init(pddl_pot_t *pot, int maxpot_segm_size)
{
    ZEROIZE(pot);
    pot->constr_lb.set = 0;

    int segm_size = PDDL_MAX(maxpot_segm_size, 8) * sizeof(maxpot_t);
    pot->maxpot_size = 0;
    pot->maxpot = pddlSegmArrNew(sizeof(maxpot_t), segm_size);
    pot->maxpot_htable = pddlHTableNew(htableHash, htableEq, NULL);
}

static int cmpOpConstr(const void *a, const void *b, void *_)
{
    const pddl_pot_constr_t *c1 = a;
    const pddl_pot_constr_t *c2 = b;
    int cmp = pddlISetCmp(&c1->plus, &c2->plus);
    if (cmp == 0)
        cmp = pddlISetCmp(&c2->minus, &c1->minus);
    if (cmp == 0)
        cmp = c1->rhs - c2->rhs;
    if (cmp == 0)
        cmp = c1->op_id - c2->op_id;
    return cmp;
}

static void sortConstrs(pddl_pot_t *pot)
{
    pddlSort(pot->constr_op.c, pot->constr_op.size, sizeof(*pot->constr_op.c),
             cmpOpConstr, NULL);
}

void pddlPotInitFDR(pddl_pot_t *pot, const pddl_fdr_t *fdr)
{
    init(pot, fdr->var.var_size);

    pot->var_size = fdr->var.global_id_size;
    pot->fact_var_size = pot->var_size;
    for (int op_id = 0; op_id < fdr->op.op_size; ++op_id)
        addFDROp(pot, &fdr->var, fdr->op.op[op_id]);
    pot->op_size = fdr->op.op_size;

    addFDRGoal(pot, &fdr->var, &fdr->goal);
    addFDRInit(pot, &fdr->var, fdr->init);

    pot->obj = CALLOC_ARR(double, pot->var_size);

    sortConstrs(pot);
}

static int initMGStrips(pddl_pot_t *pot,
                        const pddl_mg_strips_t *mg_strips,
                        const pddl_mutex_pairs_t *mutex,
                        int single_fact_disamb)
{
    init(pot, mg_strips->mg.mgroup_size);

    pot->var_size = mg_strips->strips.fact.fact_size;
    pot->fact_var_size = pot->var_size;

    pddl_disambiguate_t dis;
    pddlDisambiguateInit(&dis, mg_strips->strips.fact.fact_size,
                         mutex, &mg_strips->mg);

    for (int op_id = 0; op_id < mg_strips->strips.op.op_size; ++op_id){
        addMGStripsOp(pot, &dis, mg_strips->strips.op.op[op_id],
                      single_fact_disamb);
    }
    pot->op_size = mg_strips->strips.op.op_size;
    if (addMGStripsGoal(pot, &dis, &mg_strips->strips.goal,
                single_fact_disamb) != 0){
        pddlDisambiguateFree(&dis);
        pddlPotFree(pot);
        return -1;
    }
    pddlISetUnion(&pot->init, &mg_strips->strips.init);

    pot->obj = CALLOC_ARR(double, pot->var_size);

    pddlDisambiguateFree(&dis);
    sortConstrs(pot);
    return 0;
}

int pddlPotInitMGStrips(pddl_pot_t *pot,
                        const pddl_mg_strips_t *mg_strips,
                        const pddl_mutex_pairs_t *mutex)
{
    return initMGStrips(pot, mg_strips, mutex, 0);
}

int pddlPotInitMGStripsSingleFactDisamb(pddl_pot_t *pot,
                                        const pddl_mg_strips_t *mg_strips,
                                        const pddl_mutex_pairs_t *mutex)
{
    return initMGStrips(pot, mg_strips, mutex, 1);
}

void pddlPotFree(pddl_pot_t *pot)
{
    if (pot->maxpot_htable != NULL)
        pddlHTableDel(pot->maxpot_htable);
    for (int mi = 0; mi < pot->maxpot_size; ++mi){
        maxpot_t *m = pddlSegmArrGet(pot->maxpot, mi);
        if (m->var != NULL)
            FREE(m->var);
    }
    if (pot->maxpot != NULL)
        pddlSegmArrDel(pot->maxpot);

    for (int i = 0; i < pot->constr_op.size; ++i){
        pddlISetFree(&pot->constr_op.c[i].plus);
        pddlISetFree(&pot->constr_op.c[i].minus);
    }
    if (pot->constr_op.c != NULL)
        FREE(pot->constr_op.c);

    for (int i = 0; i < pot->constr_goal.size; ++i){
        pddlISetFree(&pot->constr_goal.c[i].plus);
        pddlISetFree(&pot->constr_goal.c[i].minus);
    }
    if (pot->constr_goal.c != NULL)
        FREE(pot->constr_goal.c);

    pddlISetFree(&pot->constr_lb.vars);

    if (pot->obj != NULL)
        FREE(pot->obj);

    pddlISetFree(&pot->init);
}

void pddlPotSetObj(pddl_pot_t *pot, const double *coef)
{
    memcpy(pot->obj, coef, sizeof(double) * pot->var_size);
}

void pddlPotSetObjFDRState(pddl_pot_t *pot,
                           const pddl_fdr_vars_t *vars,
                           const int *state)
{
    ZEROIZE_ARR(pot->obj, pot->var_size);
    for (int var_id = 0; var_id < vars->var_size; ++var_id)
        pot->obj[vars->var[var_id].val[state[var_id]].global_id] = 1.;
}

void pddlPotSetObjFDRAllSyntacticStates(pddl_pot_t *pot,
                                        const pddl_fdr_vars_t *vars)
{
    ZEROIZE_ARR(pot->obj, pot->var_size);
    for (int var_id = 0; var_id < vars->var_size; ++var_id){
        double c = 1. / vars->var[var_id].val_size;
        for (int val = 0; val < vars->var[var_id].val_size; ++val){
            pot->obj[vars->var[var_id].val[val].global_id] = c;
        }
    }
}

void pddlPotSetObjStripsState(pddl_pot_t *pot, const pddl_iset_t *state)
{
    ZEROIZE_ARR(pot->obj, pot->var_size);
    int fact_id;
    PDDL_ISET_FOR_EACH(state, fact_id)
        pot->obj[fact_id] = 1.;
}

void pddlPotSetLowerBoundConstr(pddl_pot_t *pot,
                                const pddl_iset_t *vars,
                                double rhs)
{
    pot->constr_lb.set = 1;
    pddlISetEmpty(&pot->constr_lb.vars);
    pddlISetUnion(&pot->constr_lb.vars, vars);
    pot->constr_lb.rhs = rhs;
}

void pddlPotResetLowerBoundConstr(pddl_pot_t *pot)
{
    pot->constr_lb.set = 0;
}

double pddlPotSetLowerBoundConstrRHS(const pddl_pot_t *pot)
{
    if (!pot->constr_lb.set)
        return 0.;
    return pot->constr_lb.rhs;
}

void pddlPotDecreaseLowerBoundConstrRHS(pddl_pot_t *pot, double decrease)
{
    if (!pot->constr_lb.set)
        return;
    pot->constr_lb.rhs -= decrease;
}

static void setConstr(pddl_lp_t *lp,
                      int row,
                      const pddl_pot_t *pot,
                      const pddl_pot_constr_t *c)
{
    int var;

    PDDL_ISET_FOR_EACH(&c->plus, var)
        pddlLPSetCoef(lp, row, var, 1);
    PDDL_ISET_FOR_EACH(&c->minus, var)
        pddlLPSetCoef(lp, row, var, -1);
    pddlLPSetRHS(lp, row, c->rhs, 'L');
}

static void setConstrs(pddl_lp_t *lp,
                       const pddl_pot_t *pot,
                       const pddl_pot_constrs_t *cs,
                       int *row)
{
    for (int ci = 0; ci < cs->size; ++ci)
        setConstr(lp, (*row)++, pot, cs->c + ci);
}

static void setMaxpotConstr(pddl_lp_t *lp,
                            const pddl_pot_t *pot,
                            const maxpot_t *maxpot,
                            int *prow)
{
    for (int i = 0; i < maxpot->var_size; ++i){
        double coef = 1.;
        if (maxpot->var[i].count > 1)
            coef = 1. / maxpot->var[i].count;
        int row = (*prow)++;
        pddlLPSetCoef(lp, row, maxpot->var[i].var_id, coef);
        pddlLPSetCoef(lp, row, maxpot->maxpot_id, -1.);
        pddlLPSetRHS(lp, row, 0., 'L');
    }
}

static void setMaxpotConstrs(pddl_lp_t *lp, const pddl_pot_t *pot, int *row)
{
    for (int mi = 0; mi < pot->maxpot_size; ++mi){
        const maxpot_t *m = pddlSegmArrGet(pot->maxpot, mi);
        setMaxpotConstr(lp, pot, m, row);
    }
}

static void setLBConstr(pddl_lp_t *lp, const pddl_pot_t *pot, int *row)
{
    if (!pot->constr_lb.set)
        return;

    if (*row == pddlLPNumRows(lp)){
        char sense = 'G';
        pddlLPAddRows(lp, 1, &pot->constr_lb.rhs, &sense);
    }else{
        pddlLPSetRHS(lp, *row, pot->constr_lb.rhs, 'G');
    }

    int var;
    PDDL_ISET_FOR_EACH(&pot->constr_lb.vars, var)
        pddlLPSetCoef(lp, *row, var, 1.);
    (*row)++;
}

static void enforceIntInit(pddl_lp_t *lp, const pddl_pot_t *pot)
{
    int var = pddlLPNumCols(lp);
    pddlLPAddCols(lp, 1);
    pddlLPSetVarRange(lp, var, -1E20, 1E20);
    pddlLPSetVarInt(lp, var);
    pddlLPSetObj(lp, var, 0);

    double rhs = 0;
    char sense = 'E';
    int row = pddlLPNumRows(lp);
    pddlLPAddRows(lp, 1, &rhs, &sense);
    int fact;
    PDDL_ISET_FOR_EACH(&pot->init, fact)
        pddlLPSetCoef(lp, row, fact, 1);
    pddlLPSetCoef(lp, row, var, 1);
}

static int addOpPotConstrs(pddl_lp_t *lp, const pddl_pot_t *pot)
{
    int var_size = pddlLPNumCols(lp);
    pddlLPAddCols(lp, pot->constr_op.size);
    for (int i = 0; i < pot->constr_op.size; ++i){
        int var = var_size + i;
        //pddlLPSetVarRange(lp, var, LPVAR_LOWER, 10. * LPVAR_UPPER);
        pddlLPSetVarRange(lp, var, -1E20, 1E20);
        pddlLPSetVarInt(lp, var);
        pddlLPSetObj(lp, var, 0);
    }

    int row = pddlLPNumRows(lp);
    for (int i = 0; i < pot->constr_op.size; ++i){
        double rhs = 0;
        char sense = 'E';
        pddlLPAddRows(lp, 1, &rhs, &sense);
        setConstr(lp, row, pot, pot->constr_op.c + i);
        pddlLPSetRHS(lp, row, rhs, sense);
        pddlLPSetCoef(lp, row, var_size + i, 1);
        ++row;
    }

    return var_size;
}

static double constrLHS(const pddl_pot_t *pot,
                        const pddl_pot_constr_t *c,
                        const double *w)
{
    // Use kahan summation
    double sum = 0.;
    double comp = 0.;
    int var;
    PDDL_ISET_FOR_EACH(&c->plus, var){
        double y = w[var] - comp;
        double t = sum + y;
        comp = (t - sum) - y;
        sum = t;
    }
    PDDL_ISET_FOR_EACH(&c->minus, var){
        double y = -w[var] - comp;
        double t = sum + y;
        comp = (t - sum) - y;
        sum = t;
    }

    return sum;
}


static void storeOpPot(pddl_lp_t *lp,
                       const double *obj,
                       int var_offset,
                       const pddl_pot_t *pot,
                       pddl_pot_solution_t *sol)
{
    sol->op_pot_size = pot->constr_op.size;
    sol->op_pot = CALLOC_ARR(double, pot->op_size);
    for (int ci = 0; ci < pot->constr_op.size; ++ci){
        const pddl_pot_constr_t *c = pot->constr_op.c + ci;
        if (c->op_id >= 0){
            if (pot->op_pot_real){
                sol->op_pot[c->op_id] = -constrLHS(pot, c, obj);
            }else{
                double oval = obj[var_offset + ci];
                oval = round(oval);
                if (oval > PDDL_COST_MAX)
                    oval = PDDL_COST_MAX;
                if (oval < PDDL_COST_MIN)
                    oval = PDDL_COST_MIN;
                ASSERT(oval == (int)oval);
                sol->op_pot[c->op_id] = (int)oval;
            }
        }
    }
}

int pddlPotSolve(const pddl_pot_t *pot,
                 pddl_pot_solution_t *sol,
                 pddl_err_t *err)
{
    int ret = 0;

    int rows = pot->constr_op.size;
    rows += pot->constr_goal.size;
    for (int mi = 0; mi < pot->maxpot_size; ++mi){
        const maxpot_t *m = pddlSegmArrGet(pot->maxpot, mi);
        rows += m->var_size;
    }

    pddl_lp_config_t cfg = PDDL_LP_CONFIG_INIT;
    cfg.maximize = 1;
    cfg.rows = rows;
    cfg.cols = pot->var_size;
    if (pot->op_pot && !pot->op_pot_real)
        cfg.tune_int_operator_potential = 1;
    pddl_lp_t *lp = pddlLPNew(&cfg, err);

    for (int i = 0; i < pot->var_size; ++i){
        if (pot->use_ilp)
            pddlLPSetVarInt(lp, i);
        pddlLPSetVarRange(lp, i, LPVAR_LOWER, LPVAR_UPPER);
        pddlLPSetObj(lp, i, pot->obj[i]);
    }

    int row = 0;
    setConstrs(lp, pot, &pot->constr_op, &row);
    setConstrs(lp, pot, &pot->constr_goal, &row);
    setMaxpotConstrs(lp, pot, &row);

    int op_pot_var_offset = 0;
    if (pot->op_pot && !pot->op_pot_real){
        op_pot_var_offset = addOpPotConstrs(lp, pot);
    }
    if (pot->enforce_int_init)
        enforceIntInit(lp, pot);

    row = pddlLPNumRows(lp);
    setLBConstr(lp, pot, &row);

    int var_size = pddlLPNumCols(lp);
    double objval, *obj;
    obj = CALLOC_ARR(double, var_size);

    if (pddlLPSolve(lp, &objval, obj) == 0){
        sol->objval = objval;
        sol->pot_size = pot->var_size;
        sol->pot = ALLOC_ARR(double, sol->pot_size);
        memcpy(sol->pot, obj, sizeof(double) * sol->pot_size);

        if (pot->op_pot)
            storeOpPot(lp, obj, op_pot_var_offset, pot, sol);

    }else{
        ZEROIZE(sol);
        ret = -1;
    }

    FREE(obj);
    pddlLPDel(lp);

    return ret;
}

double pddlPotFDRStateFlt(const pddl_fdr_t *fdr,
                          const int *state,
                          const pddl_pot_solution_t *sol)
{
    // Use kahan summation
    double sum = 0.;
    double comp = 0.;
    for (int var = 0; var < fdr->var.var_size; ++var){
        double v = sol->pot[fdr->var.var[var].val[state[var]].global_id];
        double y = v - comp;
        double t = sum + y;
        comp = (t - sum) - y;
        sum = t;
    }
    return sum;
}

int pddlPotFDRState(const pddl_fdr_t *fdr,
                    const int *state,
                    const pddl_pot_solution_t *sol)
{
    return potFltToInt(pddlPotFDRStateFlt(fdr, state, sol));
}

double pddlPotStripsStateFlt(const pddl_iset_t *state,
                             const pddl_pot_solution_t *sol)
{
    // Use kahan summation
    double sum = 0.;
    double comp = 0.;
    int fact_id;
    PDDL_ISET_FOR_EACH(state, fact_id){
        double v = sol->pot[fact_id];
        double y = v - comp;
        double t = sum + y;
        comp = (t - sum) - y;
        sum = t;
    }
    return sum;
}

int pddlPotStripsState(const pddl_iset_t *state,
                       const pddl_pot_solution_t *sol)
{
    return potFltToInt(pddlPotStripsStateFlt(state, sol));
}

void pddlPotMGStripsPrintLP(const pddl_pot_t *pot,
                            const pddl_mg_strips_t *mg_strips,
                            FILE *fout)
{
    for (int fid = 0; fid < mg_strips->strips.fact.fact_size; ++fid){
        fprintf(fout, "\\Fact[%d] = (%s)\n", fid,
                mg_strips->strips.fact.fact[fid]->name);
    }

    int fact_id;
    fprintf(fout, "\\Init:");
    PDDL_ISET_FOR_EACH(&mg_strips->strips.init, fact_id)
        fprintf(fout, " x%d", fact_id);
    fprintf(fout, "\n");

    fprintf(fout, "\\Goal:");
    PDDL_ISET_FOR_EACH(&mg_strips->strips.goal, fact_id)
        fprintf(fout, " x%d", fact_id);
    fprintf(fout, "\n");

    for (int mi = 0; mi < mg_strips->mg.mgroup_size; ++mi){
        int fact_id;
        fprintf(fout, "\\MG%d:", mi);
        PDDL_ISET_FOR_EACH(&mg_strips->mg.mgroup[mi].mgroup, fact_id)
            fprintf(fout, " x%d", fact_id);
        fprintf(fout, "\n");
    }
    fprintf(fout, "Maximize\n");
    fprintf(fout, "  obj:");
    int first = 1;
    for (int i = 0; i < pot->var_size; ++i){
        if (pot->obj[i] != 0.){
            if (!first)
                fprintf(fout, " +");
            fprintf(fout, " %f x%d", pot->obj[i], i);
            first = 0;
        }
    }
    fprintf(fout, "\n");

    fprintf(fout, "Subject To\n");
    fprintf(fout, "\\Ops:\n");
    for(int ci = 0; ci < pot->constr_op.size; ++ci){
        const pddl_pot_constr_t *c = pot->constr_op.c + ci;
        int var;

        int first = 1;
        PDDL_ISET_FOR_EACH(&c->plus, var){
            if (!first)
                fprintf(fout, " +");
            fprintf(fout, " x%d", var);
            first = 0;
        }
        PDDL_ISET_FOR_EACH(&c->minus, var)
            fprintf(fout, " - x%d", var);
        fprintf(fout, " <= %d\n", c->rhs);
    }

    fprintf(fout, "\\Goals: (");
    PDDL_ISET_FOR_EACH(&mg_strips->strips.goal, fact_id)
        fprintf(fout, " x%d", fact_id);
    fprintf(fout, ")\n");
    for(int ci = 0; ci < pot->constr_goal.size; ++ci){
        const pddl_pot_constr_t *c = pot->constr_goal.c + ci;
        int var;

        int first = 1;
        PDDL_ISET_FOR_EACH(&c->plus, var){
            if (!first)
                fprintf(fout, " +");
            fprintf(fout, " x%d", var);
            first = 0;
        }
        PDDL_ISET_FOR_EACH(&c->minus, var)
            fprintf(fout, " - x%d", var);
        fprintf(fout, " <= %d\n", c->rhs);
    }

    fprintf(fout, "\\Maxpots:\n");

    for (int mxi = 0; mxi < pot->maxpot_size; ++mxi){
        const maxpot_t *m = pddlSegmArrGet(pot->maxpot, mxi);
        for (int i = 0; i < m->var_size; ++i){
            double coef = 1.;
            if (m->var[i].count > 1)
                coef = 1. / m->var[i].count;
            fprintf(fout, "%f x%d - x%d <= 0\n",
                    coef, m->var[i].var_id, m->maxpot_id);
        }
    }

    int mvar = pot->var_size;
    for (int mi = 0; mi < mg_strips->mg.mgroup_size; ++mi){
        int fact_id;
        fprintf(fout, "\\M%d:\n", mi);
        PDDL_ISET_FOR_EACH(&mg_strips->mg.mgroup[mi].mgroup, fact_id)
            fprintf(fout, "x%d - x%d <= 0\n", fact_id, mvar);
        ++mvar;
    }

    fprintf(fout, "Bounds\n");
    for (int i = 0; i < pot->var_size; ++i)
        fprintf(fout, "-inf <= x%d <= %f\n", i, LPVAR_UPPER);
    fprintf(fout, "End\n");
}
