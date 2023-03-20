/***
 * cpddl
 * -------
 * Copyright (c)2018 Daniel Fiser <danfis@danfis.cz>,
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

#include "pddl/lm_cut.h"
#include "internal.h"


#define FID(heur, f) ((f) - (heur)->fact)
#define FVALUE(fact) ((fact)->value)
#define FVALUE_INIT(fact) \
    do { (fact)->value = (fact)->heap.key = INT_MAX; } while(0)
#define FVALUE_IS_SET(fact) (FVALUE(fact) != INT_MAX)
#define FPUSH(pq, val, fact) \
    do { \
    ASSERT(val != INT_MAX); \
    if ((fact)->heap.key != INT_MAX){ \
        (fact)->value = (val); \
        pddlPQUpdate((pq), (val), &(fact)->heap); \
    }else{ \
        (fact)->value = (val); \
        pddlPQPush((pq), (val), &(fact)->heap); \
    } \
    } while (0)

_pddl_inline pddl_lm_cut_fact_t *FPOP(pddl_pq_t *pq, int *value)
{
    pddl_pq_el_t *el = pddlPQPop(pq, value);
    pddl_lm_cut_fact_t *fact = pddl_container_of(el, pddl_lm_cut_fact_t, heap);
    fact->heap.key = INT_MAX;
    return fact;
}

#define SET_OP_SUPP(h, op, fact_id) \
    do { \
        if ((op)->supp != -1) \
            F_UNSET_SUPP((h)->fact + (op)->supp); \
        (op)->supp = (fact_id); \
        (op)->supp_cost = (h)->fact[(fact_id)].value; \
        F_SET_SUPP((h)->fact + (fact_id)); \
    } while (0)

#define F_INIT_SUPP(fact) ((fact)->supp_cnt = 0)
#define F_SET_SUPP(fact) (++(fact)->supp_cnt)
#define F_UNSET_SUPP(fact) (--(fact)->supp_cnt)
#define F_IS_SUPP(fact) ((fact)->supp_cnt)

#define CUT_UNDEF 0
#define CUT_INIT 1
#define CUT_GOAL 2

static int getCost(int op_cost, int op_unit_cost, int op_cost_plus)
{
    int cost;

    cost = op_cost;
    if (op_unit_cost)
        cost = 1;
    cost += op_cost_plus;
    return cost;
}

static void opFree(pddl_lm_cut_op_t *op)
{
    pddlISetFree(&op->eff);
    pddlISetFree(&op->pre);
}

static void factFree(pddl_lm_cut_fact_t *fact)
{
    pddlISetFree(&fact->pre_op);
    pddlISetFree(&fact->eff_op);
}


void pddlLMCutInit(pddl_lm_cut_t *lmc,
                   const pddl_fdr_t *fdr,
                   int op_unit_cost,
                   int op_cost_plus)
{
    const pddl_fdr_vars_t *vars = &fdr->var;

    ZEROIZE(lmc);

    // Allocate facts and add one for empty-precondition fact and one for
    // goal fact
    lmc->fact_size = vars->global_id_size + 2;
    lmc->fact = CALLOC_ARR(pddl_lm_cut_fact_t, lmc->fact_size);
    lmc->fact_goal = lmc->fact_size - 2;
    lmc->fact_nopre = lmc->fact_size - 1;

    // Allocate operators and add one artificial for goal
    lmc->op_size = fdr->op.op_size + 1;
    lmc->op = CALLOC_ARR(pddl_lm_cut_op_t, lmc->op_size);
    lmc->op_goal = lmc->op_size - 1;

    for (int op_id = 0; op_id < fdr->op.op_size; ++op_id){
        const pddl_fdr_op_t *src = fdr->op.op[op_id];
        pddl_lm_cut_op_t *op = lmc->op + op_id;
        int fact_id;

        op->op_id = op_id;

        pddlFDRPartStateToGlobalIDs(&src->eff, vars, &op->eff);
        PDDL_ISET_FOR_EACH(&op->eff, fact_id)
            pddlISetAdd(&lmc->fact[fact_id].eff_op, op_id);

        if (src->pre.fact_size == 0){
            pddlISetAdd(&op->pre, lmc->fact_nopre);
            pddlISetAdd(&lmc->fact[lmc->fact_nopre].pre_op, op_id);

        }else{
            pddlFDRPartStateToGlobalIDs(&src->pre, vars, &op->pre);
            PDDL_ISET_FOR_EACH(&op->pre, fact_id)
                pddlISetAdd(&lmc->fact[fact_id].pre_op, op_id);
        }

        op->op_cost = getCost(src->cost, op_unit_cost, op_cost_plus);
    }

    // Set up goal operator
    pddl_lm_cut_op_t *op = lmc->op + lmc->op_goal;
    pddlISetAdd(&op->eff, lmc->fact_goal);
    pddlISetAdd(&lmc->fact[lmc->fact_goal].eff_op, lmc->op_goal);
    op->op_cost = 0;

    pddlFDRPartStateToGlobalIDs(&fdr->goal, vars, &op->pre);
    int fid;
    PDDL_ISET_FOR_EACH(&op->pre, fid)
        pddlISetAdd(&lmc->fact[fid].pre_op, lmc->op_goal);

    lmc->fact_state = ALLOC_ARR(int, lmc->fact_size);
    pddlIArrRealloc(&lmc->queue, lmc->fact_size / 2);
    pddlPQInit(&lmc->pq);
}

void pddlLMCutInitStrips(pddl_lm_cut_t *lmc,
                         const pddl_strips_t *strips,
                         int op_unit_cost,
                         int op_cost_plus)
{
    ZEROIZE(lmc);

    // Allocate facts and add one for empty-precondition fact and one for
    // goal fact
    lmc->fact_size = strips->fact.fact_size + 2;
    lmc->fact = CALLOC_ARR(pddl_lm_cut_fact_t, lmc->fact_size);
    lmc->fact_goal = lmc->fact_size - 2;
    lmc->fact_nopre = lmc->fact_size - 1;

    // Allocate operators and add one artificial for goal
    lmc->op_size = strips->op.op_size + 1;
    lmc->op = CALLOC_ARR(pddl_lm_cut_op_t, lmc->op_size);
    lmc->op_goal = lmc->op_size - 1;

    for (int op_id = 0; op_id < strips->op.op_size; ++op_id){
        const pddl_strips_op_t *src = strips->op.op[op_id];
        pddl_lm_cut_op_t *op = lmc->op + op_id;
        int fact_id;

        op->op_id = op_id;

        pddlISetUnion(&op->eff, &src->add_eff);
        PDDL_ISET_FOR_EACH(&op->eff, fact_id)
            pddlISetAdd(&lmc->fact[fact_id].eff_op, op_id);

        if (pddlISetSize(&src->pre) == 0){
            pddlISetAdd(&op->pre, lmc->fact_nopre);
            pddlISetAdd(&lmc->fact[lmc->fact_nopre].pre_op, op_id);

        }else{
            pddlISetUnion(&op->pre, &src->pre);
            PDDL_ISET_FOR_EACH(&op->pre, fact_id)
                pddlISetAdd(&lmc->fact[fact_id].pre_op, op_id);
        }

        op->op_cost = getCost(src->cost, op_unit_cost, op_cost_plus);
    }

    // Set up goal operator
    pddl_lm_cut_op_t *op = lmc->op + lmc->op_goal;
    pddlISetAdd(&op->eff, lmc->fact_goal);
    pddlISetAdd(&lmc->fact[lmc->fact_goal].eff_op, lmc->op_goal);
    op->op_cost = 0;

    pddlISetUnion(&op->pre, &strips->goal);
    int fid;
    PDDL_ISET_FOR_EACH(&op->pre, fid)
        pddlISetAdd(&lmc->fact[fid].pre_op, lmc->op_goal);

    lmc->fact_state = ALLOC_ARR(int, lmc->fact_size);
    pddlIArrRealloc(&lmc->queue, lmc->fact_size / 2);
    pddlPQInit(&lmc->pq);
}

void pddlLMCutFree(pddl_lm_cut_t *lmc)
{
    for (int i = 0; i < lmc->fact_size; ++i)
        factFree(lmc->fact + i);
    if (lmc->fact != NULL)
        FREE(lmc->fact);

    for (int i = 0; i < lmc->op_size; ++i)
        opFree(lmc->op + i);
    if (lmc->op != NULL)
        FREE(lmc->op);

    if (lmc->fact_state)
        FREE(lmc->fact_state);
    pddlIArrFree(&lmc->queue);
    pddlPQFree(&lmc->pq);
    pddlISetFree(&lmc->cut);
    pddlISetFree(&lmc->state);
}


static void initFacts(pddl_lm_cut_t *lmc)
{
    for (int i = 0; i < lmc->fact_size; ++i){
        FVALUE_INIT(lmc->fact + i);
        F_INIT_SUPP(lmc->fact + i);
    }
}

static void initOps(pddl_lm_cut_t *lmc, int init_cost)
{
    int i;

    for (i = 0; i < lmc->op_size; ++i){
        lmc->op[i].unsat = lmc->op[i].pre.size;
        lmc->op[i].supp = -1;
        lmc->op[i].supp_cost = INT_MAX;
        if (init_cost)
            lmc->op[i].cost = lmc->op[i].op_cost;
        lmc->op[i].cut_candidate = 0;
    }
}

static void addFDRInitState(pddl_lm_cut_t *lmc,
                            const int *state,
                            const pddl_fdr_vars_t *vars,
                            pddl_pq_t *pq)
{
    pddlISetEmpty(&lmc->state);
    for (int var = 0; var < vars->var_size; ++var){
        int fact_id = vars->var[var].val[state[var]].global_id;
        FPUSH(pq, 0, lmc->fact + fact_id);
        pddlISetAdd(&lmc->state, fact_id);
    }
    FPUSH(pq, 0, lmc->fact + lmc->fact_nopre);
    pddlISetAdd(&lmc->state, lmc->fact_nopre);
}

static void addStripsInitState(pddl_lm_cut_t *lmc,
                               const pddl_iset_t *state,
                               pddl_pq_t *pq)
{
    pddlISetEmpty(&lmc->state);
    int fact_id;
    PDDL_ISET_FOR_EACH(state, fact_id){
        FPUSH(pq, 0, lmc->fact + fact_id);
        pddlISetAdd(&lmc->state, fact_id);
    }
    FPUSH(pq, 0, lmc->fact + lmc->fact_nopre);
    pddlISetAdd(&lmc->state, lmc->fact_nopre);
}

static void enqueueOpEffects(pddl_lm_cut_t *lmc,
                             const pddl_lm_cut_op_t *op, int fact_value,
                             pddl_pq_t *pq)
{
    int value = op->cost + fact_value;

    int fact_id;
    PDDL_ISET_FOR_EACH(&op->eff, fact_id){
        pddl_lm_cut_fact_t *fact = lmc->fact + fact_id;
        if (FVALUE(fact) > value)
            FPUSH(pq, value, fact);
    }
}

static void hMaxFull(pddl_lm_cut_t *lmc,
                     const int *fdr_state,
                     const pddl_fdr_vars_t *vars,
                     const pddl_iset_t *strips_state,
                     int init_cost)
{
    pddl_pq_t pq;

    pddlPQInit(&pq);
    initFacts(lmc);
    initOps(lmc, init_cost);
    if (fdr_state != NULL){
        addFDRInitState(lmc, fdr_state, vars, &pq);
    }else{
        addStripsInitState(lmc, strips_state, &pq);
    }
    while (!pddlPQEmpty(&pq)){
        int value;
        const pddl_lm_cut_fact_t *fact = FPOP(&pq, &value);
        ASSERT(FVALUE(fact) == value);

        int op_id;
        PDDL_ISET_FOR_EACH(&fact->pre_op, op_id){
            pddl_lm_cut_op_t *op = lmc->op + op_id;
            ASSERT(op->unsat > 0);
            if (--op->unsat == 0){
                // Set as supporter the last fact that enabled this
                // operator (it must be one of those that have maximum
                // value
                SET_OP_SUPP(lmc, op, fact - lmc->fact);
                enqueueOpEffects(lmc, op, value, &pq);
            }
        }
    }
    pddlPQFree(&pq);
}


static void updateSupp(pddl_lm_cut_t *lmc, pddl_lm_cut_op_t *op)
{
    int fact_id, supp = -1, value = -1;

    PDDL_ISET_FOR_EACH(&op->pre, fact_id){
        const pddl_lm_cut_fact_t *fact = lmc->fact + fact_id;
        if (FVALUE_IS_SET(fact) && FVALUE(fact) > value){
            value = FVALUE(fact);
            supp = fact_id;
        }else if (FVALUE_IS_SET(fact) && FVALUE(fact) == value
                    && supp >= 0
                    && !F_IS_SUPP(lmc->fact + supp)
                    && F_IS_SUPP(fact)){
            value = FVALUE(fact);
            supp = fact_id;
        }
    }

    ASSERT(supp != -1);
    SET_OP_SUPP(lmc, op, supp);
}

static void enqueueOpEffectsInc(pddl_lm_cut_t *lmc,
                                const pddl_lm_cut_op_t *op,
                                int fact_value, pddl_pq_t *pq)
{
    int value = op->cost + fact_value;
    int fact_id;

    // Check all base effects
    PDDL_ISET_FOR_EACH(&op->eff, fact_id){
        pddl_lm_cut_fact_t *fact = lmc->fact + fact_id;
        if (FVALUE(fact) > value)
            FPUSH(pq, value, fact);
    }
}

static void hMaxIncUpdateOp(pddl_lm_cut_t *lmc,
                            pddl_lm_cut_op_t *op,
                            int fact_id, int fact_value)
{
    int old_supp_value;

    if (op->supp != fact_id || op->unsat > 0)
        return;

    old_supp_value = op->supp_cost;
    if (old_supp_value <= fact_value)
        return;

    updateSupp(lmc, op);
    if (op->supp_cost != old_supp_value){
        ASSERT(op->supp_cost < old_supp_value);
        enqueueOpEffectsInc(lmc, op, op->supp_cost, &lmc->pq);
    }
}

static void hMaxInc(pddl_lm_cut_t *lmc, const pddl_iset_t *cut)
{
    for (int op_id = 0; op_id < lmc->op_size; ++op_id)
        lmc->op[op_id].cut_candidate = 0;

    int op_id;
    PDDL_ISET_FOR_EACH(cut, op_id){
        const pddl_lm_cut_op_t *op = lmc->op + op_id;
        enqueueOpEffectsInc(lmc, op, op->supp_cost, &lmc->pq);
    }

    while (!pddlPQEmpty(&lmc->pq)){
        int fact_value;
        const pddl_lm_cut_fact_t *fact = FPOP(&lmc->pq, &fact_value);
        int fact_id = FID(lmc, fact);

        PDDL_ISET_FOR_EACH(&fact->pre_op, op_id){
            pddl_lm_cut_op_t *op = lmc->op + op_id;
            hMaxIncUpdateOp(lmc, op, fact_id, fact_value);
        }
    }
}

/** Mark facts connected with the goal with zero cost paths */
static void markGoalZone(pddl_lm_cut_t *lmc)
{
    pddlIArrEmpty(&lmc->queue);
    pddlIArrAdd(&lmc->queue, lmc->fact_goal);
    lmc->fact_state[lmc->fact_goal] = CUT_GOAL;
    while (lmc->queue.size > 0){
        int fact_id = lmc->queue.arr[--lmc->queue.size];
        const pddl_lm_cut_fact_t *fact = lmc->fact + fact_id;

        int op_id;
        PDDL_ISET_FOR_EACH(&fact->eff_op, op_id){
            pddl_lm_cut_op_t *op = lmc->op + op_id;
            if (op->supp >= 0 && lmc->fact_state[op->supp] == CUT_UNDEF){
                if (op->cost == 0){
                    pddlIArrAdd(&lmc->queue, op->supp);
                    lmc->fact_state[op->supp] = CUT_GOAL;
                }else{
                    op->cut_candidate = 1;
                }
            }
        }
    }

    /*
    fprintf(stderr, "gz:");
    for (fact_id = 0; fact_id < lmc->fact_size; ++fact_id){
        if (lmc->fact_state[fact_id] == CUT_GOAL)
            fprintf(stderr, " %d", fact_id);
    }
    fprintf(stderr, "\n");
    */
}

/** Finds cut (and fills lmc->cut) and returns cost of the cut.
 *  Requires marked goal zone. */
static int findCut(pddl_lm_cut_t *lmc)
{
    int min_cost = INT_MAX;

    pddlIArrEmpty(&lmc->queue);
    int fact_id;
    PDDL_ISET_FOR_EACH(&lmc->state, fact_id){
        if (lmc->fact_state[fact_id] == CUT_UNDEF){
            pddlIArrAdd(&lmc->queue, fact_id);
            lmc->fact_state[fact_id] = CUT_INIT;
        }
    }

    pddlISetEmpty(&lmc->cut);
    while (lmc->queue.size > 0){
        int fact_id = lmc->queue.arr[--lmc->queue.size];
        const pddl_lm_cut_fact_t *fact = lmc->fact + fact_id;
        int op_id;
        PDDL_ISET_FOR_EACH(&fact->pre_op, op_id){
            const pddl_lm_cut_op_t *op = lmc->op + op_id;
            if (op->supp != fact_id)
                continue;
            if (op->cut_candidate){
                pddlISetAdd(&lmc->cut, op_id);
                min_cost = PDDL_MIN(min_cost, op->cost);
                continue;
            }

            int next;
            PDDL_ISET_FOR_EACH(&op->eff, next){
                if (lmc->fact_state[next] == CUT_UNDEF){
                    if (F_IS_SUPP(lmc->fact + next)){
                        lmc->fact_state[next] = CUT_INIT;
                        pddlIArrAdd(&lmc->queue, next);
                    }
                }
            }
        }
    }

    /*
    fprintf(stderr, "Cut(%d):", min_cost);
    PDDL_ARR_INT_FOR_EACH(&lmc->cut, op_id)
        fprintf(stderr, " %d", op_id);
    fprintf(stderr, "\n");
    */

    if (lmc->cut.size == 0){
        PANIC("Empty cut!");
    }else if (min_cost <= 0){
        PANIC("Invalid cut cost: %d!", min_cost);
    }

    return min_cost;
}

/** Decrease cost of the operators in the cut */
static void applyCutCost(pddl_lm_cut_t *lmc, int min_cost)
{
    int op_id;
    PDDL_ISET_FOR_EACH(&lmc->cut, op_id)
        lmc->op[op_id].cost -= min_cost;
}

/** Perform cut */
static int cut(pddl_lm_cut_t *lmc)
{
    int cost;

    ZEROIZE_ARR(lmc->fact_state, lmc->fact_size);
    markGoalZone(lmc);
    cost = findCut(lmc);
    applyCutCost(lmc, cost);
    return cost;
}

static int landmarkCost(const pddl_lm_cut_t *lmc,
                        const pddl_iset_t *ldm)
{
    int cost = PDDL_COST_MAX;
    int op_id;

    PDDL_ISET_FOR_EACH(ldm, op_id)
        cost = PDDL_MIN(cost, lmc->op[op_id].cost);

    return cost;
}

static int applyInitLandmarks(pddl_lm_cut_t *lmc,
                              const pddl_set_iset_t *ldms,
                              pddl_set_iset_t *ldms_out)
{
    int heur = 0;
    PDDL_ISET(ldm_ops);

    // Record operators that should be changed as well as value that should
    // be substracted from their cost.
    const pddl_iset_t *ldm;
    PDDL_SET_ISET_FOR_EACH(ldms, ldm){
        if (pddlISetSize(ldm) == 0)
            continue;

        // Determine cost of the landmark and skip zero-cost landmarks
        int cost = landmarkCost(lmc, ldm);
        if (cost <= 0)
            continue;

        // Update initial heuristic value
        heur += cost;

        // Mark each operator as changed and update it by substracting the
        // cost of the landmark.
        int op_id;
        PDDL_ISET_FOR_EACH(ldm, op_id){
            pddlISetAdd(&ldm_ops, op_id);
            lmc->op[op_id].cost -= cost;
        }

        if (ldms_out != NULL)
            pddlSetISetAdd(ldms_out, ldm);
    }

    // Update h^max
    hMaxInc(lmc, &ldm_ops);

    pddlISetFree(&ldm_ops);

    return heur;
}

static int lmCut(pddl_lm_cut_t *lmc,
                 const int *fdr_state,
                 const pddl_fdr_vars_t *vars,
                 const pddl_iset_t *strips_state,
                 const pddl_set_iset_t *ldms_in,
                 pddl_set_iset_t *ldms_out)
{
    ASSERT(fdr_state == NULL || strips_state == NULL);
    ASSERT(fdr_state != NULL || strips_state != NULL);
    ASSERT(fdr_state == NULL || vars != NULL);
    int heur = 0;

    hMaxFull(lmc, fdr_state, vars, strips_state, 1);
    if (!FVALUE_IS_SET(lmc->fact + lmc->fact_goal))
        return PDDL_COST_DEAD_END;

    // If landmarks are given, apply them before continuing with LM-Cut and
    // set up initial heuristic value accordingly.
    if (ldms_in != NULL && pddlSetISetSize(ldms_in) > 0)
        heur += applyInitLandmarks(lmc, ldms_in, ldms_out);

    while (FVALUE(lmc->fact + lmc->fact_goal) > 0){
        heur += cut(lmc);

        // Store landmarks into output structure if requested.
        if (ldms_out != NULL && pddlISetSize(&lmc->cut) > 0)
            pddlSetISetAdd(ldms_out, &lmc->cut);

        //hMaxFull(h, state, 0);
        hMaxInc(lmc, &lmc->cut);
    }

    return heur;
}

int pddlLMCut(pddl_lm_cut_t *lmc,
              const int *fdr_state,
              const pddl_fdr_vars_t *vars,
              const pddl_set_iset_t *ldms_in,
              pddl_set_iset_t *ldms_out)
{
    return lmCut(lmc, fdr_state, vars, NULL, ldms_in, ldms_out);
}

int pddlLMCutStrips(pddl_lm_cut_t *lmc,
                    const pddl_iset_t *state,
                    const pddl_set_iset_t *ldms_in,
                    pddl_set_iset_t *ldms_out)
{
    return lmCut(lmc, NULL, NULL, state, ldms_in, ldms_out);
}
