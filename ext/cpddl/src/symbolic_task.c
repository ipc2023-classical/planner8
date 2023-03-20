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

#include "pddl/config.h"

#include "pddl/sort.h"
#include "pddl/extarr.h"
#include "pddl/pairheap.h"
#include "pddl/rbtree.h"
#include "pddl/rand.h"
#include "pddl/timer.h"

#include "pddl/fdr.h"
#include "pddl/mg_strips.h"
#include "pddl/cg.h"
#include "pddl/critical_path.h"
#include "pddl/symbolic_vars.h"
#include "pddl/symbolic_constr.h"
#include "pddl/symbolic_trans.h"
#include "pddl/symbolic_state.h"
#include "pddl/symbolic_split_goal.h"
#include "pddl/symbolic_task.h"
#include "pddl/cost.h"
#include "pddl/time_limit.h"
#include "pddl/disambiguation.h"
#include "pddl/scc.h"
#include "pddl/famgroup.h"
#include "pddl/hpot.h"
#include "internal.h"

struct pddl_symbolic_search {
    pddl_symbolic_search_config_t cfg; /*!< Configuration */
    int enabled;
    int fw; /*!< True if this is forward search */
    int use_heur; /*!< True if heuristics should be used */
    pddl_symbolic_trans_set_image_fn image; /*!< constructing image */
    pddl_symbolic_trans_set_image_fn pre_image; /*!< constructing pre-image */
    pddl_symbolic_constr_apply_fn constr_apply; /*!< applying constraints */
    pddl_cost_t heur_init;
    pddl_bdds_costs_t init; /*!< BDDs of the inital state */
    pddl_bdd_t *goal; /*!< BDD describing the goal states */
    pddl_symbolic_trans_sets_t trans; /*!< BDD transitions */
    pddl_symbolic_states_t state; /*!< State space */
    pddl_iarr_t plan; /*!< Extracted plan */
    int plan_goal_id; /*!< This search's state where plan was reached */
    int plan_other_goal_id; /*!< Other search's state where plan was reached*/
    float next_step_estimate; /*!< Estimate of the duration of next step */
    size_t steps; /*!< Number of steps so far */
    pddl_timer_t steps_time; /*!< For measuring time between steps */
    unsigned long num_expanded_bdd_nodes;
    unsigned long num_expanded_states;
    float avg_expanded_bdd_nodes;
    int dirty;
};
typedef struct pddl_symbolic_search pddl_symbolic_search_t;

struct pddl_symbolic_task {
    pddl_symbolic_task_config_t cfg; /*!< Configuration */
    pddl_fdr_t fdr;
    pddl_mg_strips_t mg_strips;
    pddl_mutex_pairs_t mutex;
    pddl_bdd_manager_t *mgr; /*!< Cudd manager */
    pddl_symbolic_vars_t vars; /*!< TODO */
    pddl_symbolic_constr_t constr; /*!< Constraints */
    pddl_bdd_t *init; /*!< Initial state */
    pddl_bdd_t *goal; /*!< Goal states */
    int goal_constr_failed; /*!< True if applying constraints on the goal
                                 failed */

    pddl_symbolic_search_t search_fw;
    pddl_symbolic_search_t search_bw;
};

#define LOG_SEARCH_CFG(N, T, F) \
    PDDL_INFO(err, "cfg.%s." #N " = " F, dir, (T)cfg->N)
#define LOG_SEARCH_CFG_I(N) LOG_SEARCH_CFG(N, int, "%d")
static void logSearchConfig(const pddl_symbolic_search_config_t *cfg,
                            pddl_err_t *err)
{
    LOG_CONFIG_BOOL(cfg, enabled, err);
    LOG_CONFIG_ULONG(cfg, trans_merge_max_nodes, err);
    LOG_CONFIG_DBL(cfg, trans_merge_max_time, err);
    LOG_CONFIG_BOOL(cfg, use_constr, err);
    LOG_CONFIG_BOOL(cfg, use_op_constr, err);
    LOG_CONFIG_BOOL(cfg, use_pot_heur, err);
    LOG_CONFIG_BOOL(cfg, use_pot_heur_inconsistent, err);
    LOG_CONFIG_BOOL(cfg, use_pot_heur_sum_op_cost, err);
    LOG_CONFIG_BOOL(cfg, use_goal_splitting, err);
    LOG_CONFIG_DBL(cfg, step_time_limit, err);

    CTX_NO_TIME(err, "pot", "pot");
    pddlHPotConfigLog(&cfg->pot_heur_config, err);
    CTXEND(err);
}


static void logConfig(const pddl_symbolic_task_config_t *cfg, pddl_err_t *err)
{
    CTX_NO_TIME(err, "cfg", "Cfg");
    LOG_CONFIG_INT(cfg, cache_size, err);
    LOG_CONFIG_ULONG(cfg, constr_max_nodes, err);
    LOG_CONFIG_DBL(cfg, constr_max_time, err);
    LOG_CONFIG_DBL(cfg, goal_constr_max_time, err);
    LOG_CONFIG_BOOL(cfg, fam_groups, err);
    LOG_CONFIG_BOOL(cfg, log_every_step, err);

    CTX_NO_TIME(err, "fw", "fw");
    logSearchConfig(&cfg->fw, err);
    CTXEND(err);
    CTX_NO_TIME(err, "bw", "bw");
    logSearchConfig(&cfg->bw, err);
    CTXEND(err);
    CTXEND(err);
}


static int preparePotHeur(const pddl_fdr_t *fdr,
                          const pddl_symbolic_search_config_t *cfg,
                          double **fpot,
                          pddl_cost_t **op_pot,
                          pddl_cost_t *init_h_value,
                          pddl_err_t *err)
{
    *fpot = NULL;
    *op_pot = NULL;
    pddlCostSetZero(init_h_value);
    if (!cfg->use_pot_heur && !cfg->use_pot_heur_inconsistent)
        return 0;

    pddl_pot_solutions_t pot;
    pddlPotSolutionsInit(&pot);
    if (pddlHPot(&pot, &cfg->pot_heur_config, err) != 0){
        TRACE_RET(err, -1);
    }
    if (pot.sol_size != 1){
        PDDL_ERR_RET(err, -1, "Symbolic search supports only a single"
                     " potential function, got %d",
                     pot.sol_size);
    }

    const pddl_pot_solution_t *sol = pot.sol + 0;
    double hflt = pddlPotSolutionEvalFDRStateFlt(sol, &fdr->var, fdr->init);
    PDDL_INFO(err, "Sum of potentials for the initial state: %.4f", hflt);
    if (hflt < 0.){
        init_h_value->cost = ceil(hflt - 0.001);
    }else{
        init_h_value->cost = pddlPotSolutionEvalFDRState(sol, &fdr->var, fdr->init);
    }
    *op_pot = CALLOC_ARR(pddl_cost_t, fdr->op.op_size);
    for (int i = 0; i < sol->op_pot_size && i < fdr->op.op_size; ++i){
        double change = sol->op_pot[i];
        change = floor(change);

        if (change >= PDDL_COST_DEAD_END){
            (*op_pot)[i].cost = PDDL_COST_DEAD_END;
        }else if (change <= PDDL_COST_MIN){
            (*op_pot)[i].cost = PDDL_COST_MIN;
        }else if (change >= PDDL_COST_MAX){
            (*op_pot)[i].cost = PDDL_COST_MAX;
        }else{
            (*op_pot)[i].cost = change;
        }
    }

    *fpot = CALLOC_ARR(double, fdr->var.global_id_size);
    for (int i = 0; i < fdr->var.global_id_size; ++i)
        (*fpot)[i] = sol->pot[i];

    pddlPotSolutionsFree(&pot);
    return 0;
}

static void applyConstrsOnSplitGoals(pddl_symbolic_task_t *ss,
                                     pddl_symbolic_states_split_by_pot_t *goals,
                                     pddl_err_t *err)
{
    float max_time = ss->cfg.goal_constr_max_time;
    int num_bdd_state_vars = ss->vars.bdd_var_size / 2;
    for (int gi = 0; gi < goals->state_size; ++gi){
        pddl_symbolic_states_split_by_pot_bdd_t *goal = goals->state + gi;
        int success = 0;
        if (ss->goal_constr_failed){
            if (pddlSymbolicConstrApplyBwLimit(
                        &ss->constr, &goal->state, max_time) == 0){
                success = 1;
            }
        }else{
            pddl_time_limit_t tlimit;
            pddlTimeLimitSet(&tlimit, max_time);
            pddl_bdd_t *bdd = pddlBDDAndLimit(ss->mgr, goal->state, ss->goal,
                                              UINT_MAX, &tlimit);
            if (bdd != NULL){
                pddlBDDDel(ss->mgr, goal->state);
                goal->state = bdd;
                success = 1;
            }
        }

        if (success){
            ASSERT(goal->h_int <= 0 || pddlBDDIsFalse(ss->mgr, goal->state));
            LOG(err, "Goal with h-value %.2f (%d) updated with constraints."
                " bdd size: %d, number of states: %.2f",
                goal->h, goal->h_int, pddlBDDSize(goal->state),
                pddlBDDCountMinterm(ss->mgr, goal->state, num_bdd_state_vars));
        }else{
            LOG(err, "Failed to apply constraints on the goal with"
                " h-value %.2f (%d), bdd size: %d, number of states: %.2f",
                goal->h, goal->h_int, pddlBDDSize(goal->state),
                pddlBDDCountMinterm(ss->mgr, goal->state, num_bdd_state_vars));
            max_time *= .5f;
            max_time = PDDL_MAX(max_time, 1.);
            LOG(err, "Setting the time limit to %.2fs", max_time);
        }
    }
}

static int searchInit(pddl_symbolic_task_t *ss,
                      pddl_symbolic_search_t *search,
                      int fw,
                      const pddl_symbolic_search_config_t *_cfg,
                      pddl_bdd_t *init,
                      pddl_bdd_t *goal,
                      pddl_err_t *err)
{
    if (fw){
        CTX(err, "symba_search_fw_init", "fw-init");
    }else{
        CTX(err, "symba_search_bw_init", "bw-init");
    }
    ZEROIZE(search);
    search->cfg = *_cfg;
    pddlHPotConfigInitCopy(&search->cfg.pot_heur_config, &_cfg->pot_heur_config);
    if (search->cfg.use_pot_heur){
        search->cfg.pot_heur_config.fdr = &ss->fdr;
        search->cfg.pot_heur_config.mg_strips = &ss->mg_strips;
        search->cfg.pot_heur_config.mutex = &ss->mutex;
    }
    search->enabled = 1;
    search->fw = fw;
    search->use_heur = search->cfg.use_pot_heur;
    if (fw){
        search->image = pddlSymbolicTransSetImage;
        search->pre_image = pddlSymbolicTransSetPreImage;
        if (search->cfg.use_constr)
            search->constr_apply = pddlSymbolicConstrApplyFw;
    }else{
        search->image = pddlSymbolicTransSetPreImage;
        search->pre_image = pddlSymbolicTransSetImage;
        if (search->cfg.use_constr)
            search->constr_apply = pddlSymbolicConstrApplyBw;
    }


    // TODO: Refactor for cases where fw and bw uses the same heuristic
    double *pot;
    pddl_cost_t *op_pot;
    pddl_cost_t pot_init_h_value;
    if (preparePotHeur(&ss->fdr, &search->cfg,
                       &pot, &op_pot, &pot_init_h_value, err) != 0){
        CTXEND(err);
        PDDL_TRACE_RET(err, -1);
    }
    if (search->use_heur)
        search->heur_init = pot_init_h_value;

    pddlBDDsCostsInit(&search->init);
    if (pot != NULL
            && !fw
            && search->use_heur
            && search->cfg.use_goal_splitting){
        CTX(err, "symba_split_goal", "split-goal");

        ASSERT(ss->mg_strips.strips.fact.fact_size == ss->fdr.var.global_id_size);
        pddl_symbolic_states_split_by_pot_t *goals;
        goals = pddlSymbolicStatesSplitByPot(&ss->mg_strips.strips.goal,
                                             &ss->mg_strips.mg, &ss->mutex,
                                             pot, &ss->vars, ss->mgr,
                                             err);
        applyConstrsOnSplitGoals(ss, goals, err);

        pddl_cost_t h;
        for (int i = 0; i < goals->state_size; ++i){
            pddl_symbolic_states_split_by_pot_bdd_t *s = goals->state + i;
            if (pddlBDDIsFalse(ss->mgr, s->state) || s->h_int > 0)
                continue;
            h = pddl_cost_zero;
            pddlCostSum(&h, &search->heur_init);
            h.cost += -s->h_int;
            pddlBDDsCostsAdd(ss->mgr, &search->init, s->state, &h);
            LOG_IN_CTX(err, "bw_unsorted_init", "Determined unsorted bw init",
                       "h-value: %{init_h_value}s,"
                       " bdd-size: %{init_bdd_size}d",
                       F_COST(&h), pddlBDDSize(s->state));
        }
        pddlSymbolicStatesSplitByPotDel(goals, ss->mgr);

        pddlBDDsCostsSortUniq(ss->mgr, &search->init);
        LOG(err, "Init states sorted.");
        for (int i = 0; i < search->init.bdd_size; ++i){
            LOG_IN_CTX(err, "bw_init", "Added bw init",
                       "h-value: %{init_h_value}s,"
                       " bdd-size: %{init_bdd_size}d",
                       F_COST(&search->init.bdd[i].cost),
                       pddlBDDSize(search->init.bdd[i].bdd));
        }
        CTXEND(err);

    }else if (init != NULL){
        pddlBDDsCostsAdd(ss->mgr, &search->init, init,
                         (search->use_heur ? &search->heur_init : NULL));
    }

    search->goal = goal;
    if (search->goal != NULL)
        search->goal = pddlBDDClone(ss->mgr, search->goal);

    LOG(err, "Creating transitions."
        " merge max nodes: %lu,"
        " merge max time: %.2fs",
        search->cfg.trans_merge_max_nodes,
        search->cfg.trans_merge_max_time);
    LOG(err, "Heuristic value for the initial state: %{init_h_value}s",
        F_COST(&pot_init_h_value));
    pddlSymbolicTransSetsInit(&search->trans, &ss->vars, &ss->constr,
                              &ss->mg_strips.strips,
                              search->cfg.use_op_constr,
                              search->cfg.trans_merge_max_nodes,
                              search->cfg.trans_merge_max_time,
                              op_pot,
                              search->cfg.use_pot_heur_sum_op_cost,
                              err);
    PDDL_INFO(err, "Transitions created.");
    if (op_pot != NULL)
        FREE(op_pot);
    if (pot != NULL)
        FREE(pot);

    pddlSymbolicStatesInit(&search->state, ss->mgr,
                           search->cfg.use_pot_heur_inconsistent, err);
    for (int ii = 0; ii < search->init.bdd_size; ++ii){
        pddlSymbolicStatesAddInit(&search->state, ss->mgr,
                                  search->init.bdd[ii].bdd,
                                  &search->init.bdd[ii].cost);
    }


    search->plan_goal_id = -1;
    search->plan_other_goal_id = -1;

    PDDL_INFO(err, "DONE");
    CTXEND(err);
    return 0;
}

static void searchReinit(pddl_symbolic_task_t *ss,
                         pddl_symbolic_search_t *search,
                         pddl_err_t *err)
{
    if (!search->dirty)
        return;
    PDDL_INFO(err, "Re-init %s search...", (search->fw ? "fw" : "bw"));
    pddlSymbolicStatesFree(&search->state, ss->mgr);
    pddlSymbolicStatesInit(&search->state, ss->mgr,
                           search->cfg.use_pot_heur_inconsistent, err);
    for (int ii = 0; ii < search->init.bdd_size; ++ii){
        pddlSymbolicStatesAddInit(&search->state, ss->mgr,
                                  search->init.bdd[ii].bdd,
                                  &search->init.bdd[ii].cost);
    }
    pddlIArrFree(&search->plan);
    pddlIArrInit(&search->plan);
    search->plan_goal_id = -1;
    search->plan_other_goal_id = -1;
    search->next_step_estimate = 0.f;
    search->steps = 0ul;
    search->num_expanded_bdd_nodes = 0ul;
    search->num_expanded_states = 0ul;
    search->avg_expanded_bdd_nodes = 0.f;
    search->dirty = 0;
    PDDL_INFO(err, "Re-init %s search. DONE", (search->fw ? "fw" : "bw"));
}

static void searchStart(pddl_symbolic_task_t *ss,
                        pddl_symbolic_search_t *search,
                        pddl_err_t *err)
{
    searchReinit(ss, search, err);
    search->dirty = 1;
}

static void searchFree(pddl_symbolic_task_t *ss,
                       pddl_symbolic_search_t *search)
{
    pddlSymbolicTransSetsFree(&search->trans);
    pddlSymbolicStatesFree(&search->state, ss->mgr);
    pddlBDDsCostsFree(ss->mgr, &search->init);
    if (search->goal != NULL)
        pddlBDDDel(ss->mgr, search->goal);
    pddlIArrFree(&search->plan);
}

static pddl_bdd_t *bddStateSelectOne(pddl_symbolic_task_t *ss,
                                     pddl_bdd_t *bdd,
                                     pddl_iset_t *state)
{
    pddlISetEmpty(state);
    char *cube = ALLOC_ARR(char, ss->vars.bdd_var_size);
    pddlBDDPickOneCube(ss->mgr, bdd, cube);
    for (int gi = 0; gi < ss->vars.group_size; ++gi){
        int fact_id = pddlSymbolicVarsFactFromBDDCube(&ss->vars, gi, cube);
        ASSERT(fact_id >= 0);
        pddlISetAdd(state, fact_id);
    }
    FREE(cube);
    return pddlSymbolicVarsCreateState(&ss->vars, state);
}

struct plan {
    int plan_len;
    pddl_iset_t *state;
    pddl_iset_t **tr_op;
};
typedef struct plan plan_t;

static const pddl_symbolic_state_t *
        planNextState(pddl_symbolic_task_t *ss,
                      pddl_symbolic_search_t *search,
                      const pddl_symbolic_state_t *state,
                      pddl_bdd_t *bdd)
{
    if (pddlISetSize(&state->parent_ids) == 0)
        return state;

    int state_id;
    PDDL_ISET_FOR_EACH(&state->parent_ids, state_id){
        const pddl_symbolic_state_t *state;
        state = pddlSymbolicStatesGet(&search->state, state_id);
        ASSERT(state->trans_id >= 0);
        // The state BDD must be already constructed
        ASSERT_RUNTIME(state->bdd != NULL);
        pddl_bdd_t *conj = pddlBDDAnd(ss->mgr, bdd, state->bdd);
        if (!pddlBDDIsFalse(ss->mgr, conj)){
            pddlBDDDel(ss->mgr, conj);
            return state;
        }
        pddlBDDDel(ss->mgr, conj);
    }
    ASSERT_RUNTIME(0);
    return state;
}

static void planInit(pddl_symbolic_task_t *ss,
                     pddl_symbolic_search_t *search,
                     plan_t *plan,
                     const pddl_symbolic_state_t *goal_state,
                     pddl_bdd_t *reached_goal)
{
    ZEROIZE(plan);

    int alloc = 2;
    plan->state = CALLOC_ARR(pddl_iset_t, alloc + 1);
    plan->tr_op = CALLOC_ARR(pddl_iset_t *, alloc);

    // Backtrack from the goal_state and extract one particular state at
    // each step.
    // Select one specific state -- it doesn't matter which one
    pddl_bdd_t *bdd = bddStateSelectOne(ss, reached_goal, plan->state + 0);
    const pddl_symbolic_state_t *state;
    state = planNextState(ss, search, goal_state, bdd);
    while (state->parent_id >= 0){
        ASSERT(pddlISetSize(&state->parent_ids) == 0);
        ASSERT(state->trans_id >= 0);
        ASSERT(state->parent_id >= 0);

        int idx = plan->plan_len++;
        if (idx == alloc){
            int old_alloc = alloc;
            alloc *= 2;
            plan->state = REALLOC_ARR(plan->state, pddl_iset_t, alloc + 1);
            ZEROIZE_ARR(plan->state + old_alloc + 1, alloc - old_alloc);
            plan->tr_op = REALLOC_ARR(plan->tr_op, pddl_iset_t *, alloc);
        }

        plan->tr_op[idx] = &search->trans.trans[state->trans_id].op;
        const pddl_symbolic_state_t *prev_state;
        prev_state = pddlSymbolicStatesGet(&search->state, state->parent_id);

        // This step of the plan goes from prev_state to state.
        // So, compute the conjuction of the preimage of state and
        // state_prev.
        pddl_symbolic_trans_set_t *trset = search->trans.trans + state->trans_id;
        pddl_bdd_t *preimg = search->pre_image(trset, bdd, NULL);
        ASSERT_RUNTIME(!pddlBDDIsFalse(ss->mgr, preimg));
        pddlBDDAndUpdate(ss->mgr, &preimg, prev_state->bdd);

        // Select one of the states -- again, it doesn't matter which one
        pddlBDDDel(ss->mgr, bdd);
        bdd = bddStateSelectOne(ss, preimg, plan->state + idx + 1);
        pddlBDDDel(ss->mgr, preimg);
        state = prev_state;
        state = planNextState(ss, search, prev_state, bdd);
    }
    pddlBDDDel(ss->mgr, bdd);

    // Reverse the order of states and transitions
    for (int i = 0; i < (plan->plan_len + 1) / 2; ++i){
        pddl_iset_t tmp;
        PDDL_SWAP(plan->state[i], plan->state[plan->plan_len - i], tmp);
    }
    for (int i = 0; i < plan->plan_len / 2; ++i){
        pddl_iset_t *tmp;
        PDDL_SWAP(plan->tr_op[i], plan->tr_op[plan->plan_len - i - 1], tmp);
    }
}

static void planFree(plan_t *plan)
{
    for (int i = 0; i < plan->plan_len + 1; ++i)
        pddlISetFree(plan->state + i);
    FREE(plan->state);
    FREE(plan->tr_op);
}

static void planReverse(plan_t *plan)
{
    pddl_iset_t state_tmp;
    int len = (plan->plan_len + 1) / 2;
    for (int i = 0; i < len; ++i){
        PDDL_SWAP(plan->state[i],
                  plan->state[plan->plan_len - i],
                  state_tmp);
    }

    pddl_iset_t *tr_tmp;
    len = plan->plan_len / 2;
    for (int i = 0; i < len; ++i){
        PDDL_SWAP(plan->tr_op[i],
                  plan->tr_op[plan->plan_len - i - 1],
                  tr_tmp);
    }
}

static void planExtractFw(plan_t *plan,
                          const pddl_strips_t *strips,
                          pddl_iarr_t *out)
{
    // Extract plan from the intermediate states
    PDDL_ISET(res_state);
    for (int si = 0; si < plan->plan_len; ++si){
        const pddl_iset_t *from = plan->state + si;
        const pddl_iset_t *to = plan->state + si + 1;

        int op_id;
        int found = 0;
        PDDL_ISET_FOR_EACH(plan->tr_op[si], op_id){
            const pddl_strips_op_t *op = strips->op.op[op_id];
            if (pddlISetIsSubset(&op->pre, from)){
                pddlISetMinus2(&res_state, from, &op->del_eff);
                pddlISetUnion(&res_state, &op->add_eff);
                if (pddlISetEq(&res_state, to)){
                    pddlIArrAdd(out, op_id);
                    found = 1;
                    break;
                }
            }
        }
        ASSERT_RUNTIME(found);
    }
    pddlISetFree(&res_state);
}


static pddl_bdd_t *searchStateBDD(pddl_symbolic_task_t *ss,
                                  pddl_symbolic_search_t *search,
                                  pddl_symbolic_state_t *state,
                                  pddl_time_limit_t *time_limit)
{
    if (state->bdd == NULL){
        const pddl_symbolic_state_t *prev_state;
        prev_state = pddlSymbolicStatesGet(&search->state, state->parent_id);
        state->bdd = search->image(search->trans.trans + state->trans_id,
                                   prev_state->bdd, time_limit);
        if (state->bdd != NULL){
            pddlSymbolicStatesRemoveClosedStates(&search->state, ss->mgr,
                                                 &state->bdd, &state->cost);
            if (search->constr_apply)
                search->constr_apply(&ss->constr, &state->bdd);
        }
    }
    return state->bdd;
}

static int searchNextOpenSize(pddl_symbolic_task_t *ss,
                              pddl_symbolic_search_t *search,
                              pddl_time_limit_t *time_limit)
{
    pddl_symbolic_state_t *state = pddlSymbolicStatesOpenPeek(&search->state);
    if (state == NULL)
        return 0;
    pddl_bdd_t *bdd = searchStateBDD(ss, search, state, time_limit);
    if (bdd == NULL)
        return -1;
    return pddlBDDSize(bdd);
}



static int checkGoal(pddl_symbolic_task_t *ss,
                     pddl_symbolic_search_t *search,
                     const pddl_symbolic_state_t *state,
                     pddl_err_t *err)
{
    pddl_bdd_t *goal = pddlBDDAnd(ss->mgr, state->bdd, search->goal);
    if (!pddlBDDIsFalse(ss->mgr, goal)){
        plan_t plan;
        planInit(ss, search, &plan, state, goal);
        if (!search->fw)
            planReverse(&plan);
        planExtractFw(&plan, &ss->mg_strips.strips, &search->plan);
        planFree(&plan);
        pddlBDDDel(ss->mgr, goal);
        search->plan_goal_id = state->id;
        return 1;
    }
    pddlBDDDel(ss->mgr, goal);
    return 0;
}

static int costStatesIsBetter(const pddl_symbolic_search_t *search,
                              const pddl_symbolic_state_t *s1,
                              const pddl_symbolic_state_t *s2)
{
    return pddlCostCmpSum(&s1->cost, &s2->cost, &search->state.bound) < 0;
}

static void searchSetBestPlan(pddl_symbolic_search_t *search,
                              const pddl_symbolic_state_t *s1,
                              const pddl_symbolic_state_t *s2)
{
    search->state.bound = s1->cost;
    pddlCostSum(&search->state.bound, &s2->cost);
    search->plan_goal_id = s1->id;
    search->plan_other_goal_id = s2->id;
}

static int checkGoal2(pddl_symbolic_task_t *ss,
                      pddl_symbolic_search_t *search,
                      pddl_symbolic_search_t *other_search,
                      pddl_symbolic_state_t *state,
                      pddl_time_limit_t *time_limit,
                      pddl_err_t *err)
{
    int res = 0;
    pddl_bdd_t *state_bdd = searchStateBDD(ss, search, state, time_limit);
    if (state_bdd == NULL){
        LOG(err, "Time limit reached when checking for goal");
        return -1;
    }
    pddl_bdd_t *goal = pddlBDDAnd(ss->mgr, state_bdd,
                                  other_search->state.all_closed);
    if (!pddlBDDIsFalse(ss->mgr, goal)){
        pddl_rbtree_node_t *rbs;
        PDDL_RBTREE_FOR_EACH(other_search->state.closed, rbs){
            const pddl_symbolic_state_t *closed_state;
            closed_state = pddl_container_of(rbs, pddl_symbolic_state_t, rbtree);
            if (!costStatesIsBetter(search, state, closed_state))
                break;

            pddl_bdd_t *goal = pddlBDDAnd(ss->mgr, state_bdd, closed_state->bdd);
            if (!pddlBDDIsFalse(ss->mgr, goal)){
                searchSetBestPlan(search, state, closed_state);
                searchSetBestPlan(other_search, closed_state, state);
                LOG(err, "%{found_plan_dir}s: Found plan,"
                    " steps: %{found_plan_steps}lu,"
                    " cost: %{found_plan_cost}s",
                    (search->fw ? "fw" : "bw"),
                    (unsigned long)search->steps,
                    F_COST(&search->state.bound));
                res = 1;
            }
            pddlBDDDel(ss->mgr, goal);

            if (res)
                break;
        }
    }
    pddlBDDDel(ss->mgr, goal);
    return res;
}

static int searchSetNextStepEstimate(pddl_symbolic_task_t *ss,
                                     pddl_symbolic_search_t *search,
                                     pddl_bdd_t *state_bdd,
                                     float cur_time,
                                     pddl_time_limit_t *time_limit,
                                     pddl_err_t *err)
{
    long bdd_size = pddlBDDSize(state_bdd);
    if (bdd_size == 0){
        search->next_step_estimate = 0.f;
    }else if (cur_time < 1.){
        search->next_step_estimate = cur_time;
    }else{
        int next_size = searchNextOpenSize(ss, search, time_limit);
        if (next_size < 0){
            LOG(err, "Time limit reached when estimating time for the next step.");
            search->next_step_estimate = 1E10;
            return -1;
        }
        float est = ((float)next_size / (float)bdd_size) * cur_time;
        search->next_step_estimate = est;
    }
    return 0;
}

static int searchExpandState(pddl_symbolic_task_t *ss,
                             pddl_symbolic_search_t *search,
                             pddl_symbolic_search_t *other_search,
                             pddl_symbolic_state_t *state_in,
                             pddl_time_limit_t *time_limit,
                             pddl_err_t *err)
{
    pddl_symbolic_states_t *states = &search->state;
    pddl_bdd_t *bdd_in = pddlBDDClone(ss->mgr, state_in->bdd);
    ASSERT(bdd_in != NULL);
    pddlSymbolicStatesRemoveClosedStates(&search->state, ss->mgr,
                                         &bdd_in, &state_in->cost);

    if (pddlBDDIsFalse(ss->mgr, bdd_in)){
        pddlBDDDel(ss->mgr, bdd_in);
        return 0;
    }

    for (int tri = 0; tri < search->trans.trans_size; ++tri){
        const pddl_cost_t *tr_cost = &search->trans.trans[tri].cost;

        pddl_cost_t cost = state_in->cost;
        pddlCostSum(&cost, tr_cost);
        if (pddlCostCmp(&cost, &states->bound) >= 0)
            continue;

        // Increase heuristic estimate by the change incurred by this
        // transition
        pddl_cost_t heur = state_in->heur;
        if (search->use_heur)
            pddlCostSumSat(&heur, &search->trans.trans[tri].heur_change);
        if (pddlCostIsDeadEnd(&heur))
            continue;

        // Set f-value = cost + heur, but consider heur < 0 as zero
        pddl_cost_t f_value = cost;
        if (search->use_heur && pddlCostCmp(&heur, &pddl_cost_zero) > 0)
            pddlCostSumSat(&f_value, &heur);
        if (pddlCostCmp(&f_value, &states->bound) >= 0)
            continue;

        pddl_symbolic_state_t *state = pddlSymbolicStatesAdd(states);
        state->parent_id = state_in->id;
        state->trans_id = tri;
        state->cost = cost;
        state->heur = heur;
        state->f_value = f_value;

        // Deal with blown-up f-values
        // TODO
        if (pddlCostCmp(&state->f_value, &pddl_cost_zero) < 0
                || pddlCostCmp(&state->f_value, &pddl_cost_max) > 0){
            PDDL_INFO(err, "MAX f-value HIT");
            state->f_value = pddl_cost_max;
        }

        pddlSymbolicStatesOpenState(states, state);
        if (other_search != NULL){
            if (checkGoal2(ss, search, other_search, state, time_limit, err) < 0){
                pddlBDDDel(ss->mgr, bdd_in);
                return -1;
            }
        }
    }
    search->num_expanded_bdd_nodes += pddlBDDSize(bdd_in);
    if (search->num_expanded_states == 0){
        search->avg_expanded_bdd_nodes = pddlBDDSize(bdd_in);
    }else{
        float avg = search->avg_expanded_bdd_nodes;
        avg *= search->num_expanded_states;
        avg += pddlBDDSize(bdd_in);
        avg /= search->num_expanded_states + 1;
        search->avg_expanded_bdd_nodes = avg;
    }
    ++search->num_expanded_states;

    pddlBDDDel(ss->mgr, bdd_in);
    return 0;
}

static pddl_symbolic_state_t *searchNextNonEmpty(pddl_symbolic_task_t *ss,
                                                 pddl_symbolic_search_t *search,
                                                 pddl_time_limit_t *time_limit,
                                                 pddl_err_t *err)
{
    pddl_symbolic_state_t *state;
    do {
        state = pddlSymbolicStatesNextOpen(&search->state);
        if (state != NULL){
            pddl_bdd_t *bdd = searchStateBDD(ss, search, state, time_limit);
            if (bdd == NULL){
                LOG(err, "Time limit reached when asking for next open state.");
                return NULL;
            }
            pddlSymbolicStatesRemoveClosedStates(&search->state, ss->mgr,
                                                 &state->bdd, &state->cost);
        }
    } while (state != NULL && pddlBDDIsFalse(ss->mgr, state->bdd));

    return state;
}

static void searchPrepareNext(pddl_symbolic_task_t *ss,
                              pddl_symbolic_search_t *search,
                              pddl_time_limit_t *time_limit,
                              pddl_err_t *err)
{
    pddl_symbolic_state_t *state = searchNextNonEmpty(ss, search, time_limit, err);
    if (state == NULL)
        return;

    PDDL_ISET(parents);
    pddlISetAdd(&parents, state->id);
    pddl_bdd_t *bdd = searchStateBDD(ss, search, state, time_limit);
    if (bdd == NULL){
        pddlISetFree(&parents);
        LOG(err, "Time limit reached when preparing next open state.");
        return;
    }
    bdd = pddlBDDClone(ss->mgr, bdd);

    pddl_symbolic_state_t *next = pddlSymbolicStatesOpenPeek(&search->state);
    while (next != NULL
            && pddlCostCmp(&state->cost, &next->cost) == 0
            && pddlCostCmp(&state->heur, &next->heur) == 0){
        ASSERT(pddlISetSize(&next->parent_ids) == 0);
        ASSERT(next->parent_id >= 0);

        next = pddlSymbolicStatesNextOpen(&search->state);
        pddl_bdd_t *next_bdd = searchStateBDD(ss, search, next, time_limit);
        if (next_bdd == NULL){
            pddlBDDDel(ss->mgr, bdd);
            pddlISetFree(&parents);
            LOG(err, "Time limit reached when merging states with the same"
                 " g and h-value");
            return;
        }
        pddlSymbolicStatesRemoveClosedStates(&search->state, ss->mgr,
                                             &next->bdd, &next->cost);
        if (!pddlBDDIsFalse(ss->mgr, next->bdd)){
            pddlBDDOrUpdate(ss->mgr, &bdd, next->bdd);
            pddlISetAdd(&parents, next->id);
        }

        next = pddlSymbolicStatesOpenPeek(&search->state);
    }

    if (pddlISetSize(&parents) > 1){
        pddl_symbolic_state_t *merged;
        merged = pddlSymbolicStatesAddBDD(&search->state, ss->mgr, bdd);
        merged->parent_id = -2;
        merged->trans_id = -1;
        merged->cost = state->cost;
        merged->f_value = state->f_value;
        merged->heur = state->heur;
        pddlISetUnion(&merged->parent_ids, &parents);
        pddlSymbolicStatesOpenState(&search->state, merged);
    }else{
        pddlSymbolicStatesOpenState(&search->state, state);
    }

    pddlISetFree(&parents);
    pddlBDDDel(ss->mgr, bdd);
}

static void printStepLog(const pddl_symbolic_task_t *ss,
                         pddl_symbolic_search_t *search,
                         const pddl_symbolic_search_t *other_search,
                         const pddl_symbolic_state_t *state,
                         pddl_err_t *err)
{
    pddlTimerStop(&search->steps_time);
#ifdef PDDL_DEBUG
    if (1){
#else /* PDDL_DEBUG */
    if (ss->cfg.log_every_step
            || search->steps == 1
            || search->steps % 1000ul == 0
            || pddlTimerElapsedInSF(&search->steps_time) > 1.){
#endif /* PDDL_DEBUG */
        if (other_search == NULL){
            LOG_IN_CTX(err, "step", "step",
                       "%{dir}s %{step}lu, g: %{g}s, h: %{h}s, f: %{f}s,"
                       " gen: %{generated_states}d,"
                       " closed: %{closed_states}d,"
                       " cur-bdd-size: %{cur_state_bdd_size}d"
                       " cur-bdd-states: %{cur_state_num_states}.1f",
                       (search->fw ? "fw" : "bw"),
                       (unsigned long)search->steps,
                       F_COST(&state->cost),
                       F_COST(&state->heur),
                       F_COST(&state->f_value),
                       search->state.num_states,
                       search->state.num_closed,
                       pddlBDDSize(state->bdd),
                       pddlBDDCountMinterm(ss->mgr, state->bdd, ss->vars.bdd_var_size / 2));
        }else{
            const pddl_symbolic_search_t *sfw = search;
            const pddl_symbolic_search_t *sbw = other_search;
            if (!search->fw){
                sfw = other_search;
                sbw = search;
            }

            LOG_IN_CTX(err, "step", "step",
                       "%{dir}s %{step}lu, g: %{g}s, h: %{h}s, f: %{f}s,"
                       " gen: %{generated_states}d,"
                       " closed: %{closed_states}d,"
                       " bound: %{bound}s,"
                       " fw-est: %{fw_estimate}.2f,"
                       " bw-est: %{bw_estimate}.2f,"
                       " cur-bdd-size: %{cur_state_bdd_size}d,"
                       " cur-bdd-states: %{cur_state_num_states}.1f",
                       (search->fw ? "fw" : "bw"),
                       (unsigned long)search->steps,
                       F_COST(&state->cost),
                       F_COST(&state->heur),
                       F_COST(&state->f_value),
                       search->state.num_states,
                       search->state.num_closed,
                       (pddlCostIsMax(&search->state.bound)
                            ? "unset" : F_COST(&search->state.bound)),
                       sfw->next_step_estimate,
                       sbw->next_step_estimate,
                       pddlBDDSize(state->bdd),
                       pddlBDDCountMinterm(ss->mgr, state->bdd, ss->vars.bdd_var_size / 2));
        }
        pddlTimerStart(&search->steps_time);
    }
}

static int searchStep(pddl_symbolic_task_t *ss,
                      pddl_symbolic_search_t *search,
                      pddl_symbolic_search_t *other_search,
                      pddl_time_limit_t *time_limit,
                      pddl_err_t *err)
{
    ++search->steps;
    pddl_timer_t timer;
    pddlTimerStart(&timer);
    pddl_symbolic_state_t *state = pddlSymbolicStatesNextOpen(&search->state);
    if (state == NULL){
        PDDL_INFO(err, "%s: Plan does not exist, steps: %lu",
                 (search->fw ? "fw" : "bw"), (unsigned long)search->steps);
        pddlTimerStop(&timer);
        return PDDL_SYMBOLIC_PLAN_NOT_EXIST;
    }

    printStepLog(ss, search, other_search, state, err);

    pddl_bdd_t *state_bdd = searchStateBDD(ss, search, state, time_limit);
    if (state_bdd == NULL){
        LOG(err, "Time limit reached when constructing BDD from the top of"
             " open-list");
        return PDDL_SYMBOLIC_ABORT_TIME_LIMIT;
    }

    if (pddlBDDIsFalse(ss->mgr, state_bdd))
        return PDDL_SYMBOLIC_CONT;

    if (pddlCostCmp(&state->f_value, &search->state.bound) <= 0){
        if (checkGoal(ss, search, state, err)){
            LOG(err, "%{found_plan_dir}s: Found plan,"
                " steps: %{found_plan_steps}lu,"
                " cost: %{found_plan_cost}s,"
                " length: %{found_plan_length}d",
                (search->fw ? "fw" : "bw"),
                (unsigned long)search->steps,
                F_COST(&state->cost),
                pddlIArrSize(&search->plan));

            pddlTimerStop(&timer);
            searchSetNextStepEstimate(ss, search, state_bdd,
                                      pddlTimerElapsedInSF(&timer),
                                      time_limit, err);
            return PDDL_SYMBOLIC_PLAN_FOUND;

        }else if (other_search != NULL){
            if (checkGoal2(ss, search, other_search, state, time_limit, err) < 0)
                return PDDL_SYMBOLIC_ABORT_TIME_LIMIT;
        }

        if (searchExpandState(ss, search, other_search, state, time_limit, err) < 0){
            LOG(err, "Time limit reached when expanding the current state.");
            return PDDL_SYMBOLIC_ABORT_TIME_LIMIT;
        }
    }
    pddlSymbolicStatesCloseState(&search->state, ss->mgr, state);
    pddlTimerStop(&timer);
    searchPrepareNext(ss, search, time_limit, err);
    searchSetNextStepEstimate(ss, search, state->bdd,
                              pddlTimerElapsedInSF(&timer), time_limit, err);
    return PDDL_SYMBOLIC_CONT;
}

static double orderComputeCost(const int *order,
                               const int *influence,
                               int size)
{
    double cost = 0.;
    for (int i = 0; i < size; ++i){
        for (int j = i + 1; j < size; ++j){
            if (influence[order[i] * size + order[j]])
                cost += (j - i) * (j - i);
        }
    }
    return cost;
}

static void orderSwap(int *order,
                      const int *influence,
                      int size,
                      double *cost,
                      pddl_rand_t *rnd)
{
    int swap_idx1 = pddlRand(rnd, 0, size);
    int swap_idx2 = pddlRand(rnd, 0, size);
    if (swap_idx1 == swap_idx2)
        return;

    double new_cost = *cost;
    for (int i = 0; i < size; ++i){
        if (i == swap_idx1 || i == swap_idx2)
            continue;

        if (influence[order[i] * size + order[swap_idx1]]){
            new_cost += (-1 * (i - swap_idx1) * (i - swap_idx1)
                            + (i - swap_idx2) * (i - swap_idx2));
        }

        if (influence[order[i] * size + order[swap_idx2]]){
            new_cost += (-1 * (i - swap_idx2) * (i - swap_idx2)
                            + (i - swap_idx1) * (i - swap_idx1));
        }
    }

    if (new_cost < *cost){
        int tmp;
        PDDL_SWAP(order[swap_idx1], order[swap_idx2], tmp);
        *cost = new_cost;
    }
}

static double orderOptimize(int iterations,
                            int *order,
                            const int *influence,
                            int size,
                            pddl_rand_t *rnd)
{
    double cost = orderComputeCost(order, influence, size);
    for (int i = 0; i < iterations; ++i)
        orderSwap(order, influence, size, &cost, rnd);
    return cost;
}

static void orderRandomize(int *order, int size, pddl_rand_t *rnd)
{
    int *order2 = ALLOC_ARR(int, size);
    for (int i = 0; i < size; ++i)
        order2[i] = -1;
    for (int num = 0; num < size; ++num){
        while (1){
            int pos = pddlRand(rnd, 0, size);
            if (order2[pos] == -1){
                order2[pos] = num;
                break;
            }
        }
    }
    memcpy(order, order2, sizeof(int) * size);
    FREE(order2);
}

static void orderCompute(int *order,
                         int size,
                         const pddl_cg_t *cg,
                         pddl_err_t *err)
{
    pddl_rand_t rnd;
    pddlRandInit(&rnd, 1371);

    ASSERT_RUNTIME(cg->node_size == size);

    for (int i = 0; i < size / 2; ++i){
        int tmp;
        PDDL_SWAP(order[i], order[size - i - 1], tmp);
    }

    int *influence = CALLOC_ARR(int, size * size);
    for (int f = 0; f < size; ++f){
        const pddl_cg_node_t *node = cg->node + f;
        for (int i = 0; i < node->fw_size; ++i){
            if (node->fw[i].end == f)
                continue;
            influence[f * size + node->fw[i].end] = 1;
            influence[node->fw[i].end * size + f] = 1;
        }
    }

    int *order2 = ALLOC_ARR(int, size);
    memcpy(order2, order, sizeof(int) * size);
    double cost = orderOptimize(50000, order2, influence, size, &rnd);
    memcpy(order, order2, sizeof(int) * size);
    PDDL_INFO(err, "Init order cost: %.2f", cost);

    for (int i = 0; i < 20; ++i){
        memcpy(order2, order, sizeof(int) * size);
        orderRandomize(order2, size, &rnd);
        double new_cost = orderOptimize(50000, order2, influence, size, &rnd);
        if (new_cost < cost){
            memcpy(order, order2, sizeof(int) * size);
            cost = new_cost;
            PDDL_INFO(err, "New order cost: %.2f", cost);
        }
    }
    FREE(order2);
    FREE(influence);
}

static void prepareTask(pddl_symbolic_task_t *ss,
                        const pddl_fdr_t *fdr,
                        const pddl_symbolic_task_config_t *cfg,
                        pddl_err_t *err)
{
    pddlFDRInitCopy(&ss->fdr, fdr);
    pddlMGStripsInitFDR(&ss->mg_strips, fdr);

    pddlMutexPairsInitStrips(&ss->mutex, &ss->mg_strips.strips);
    pddlMutexPairsAddMGroups(&ss->mutex, &ss->mg_strips.mg);
    pddlH2FwBw(&ss->mg_strips.strips, &ss->mg_strips.mg, &ss->mutex,
               NULL, NULL, 0., err);

    int *var_order = ALLOC_ARR(int, fdr->var.var_size + 1);
    pddl_cg_t cg;
    pddlCGInit(&cg, &fdr->var, &fdr->op, 1);
    pddlCGVarOrdering(&cg, &fdr->goal, var_order);
    orderCompute(var_order, fdr->var.var_size, &cg, err);
    pddlCGFree(&cg);

    ASSERT_RUNTIME(ss->mg_strips.mg.mgroup_size == fdr->var.var_size);
    pddlMGStripsReorderMGroups(&ss->mg_strips, var_order);
    PDDL_INFO(err, "Order computed and applied");

#ifdef PDDL_DEBUG
    for (int i = 0; i < ss->mg_strips.mg.mgroup_size; ++i){
        for (int j = i + 1; j < ss->mg_strips.mg.mgroup_size; ++j){
            ASSERT(pddlISetIsDisjoint(&ss->mg_strips.mg.mgroup[i].mgroup,
                                     &ss->mg_strips.mg.mgroup[j].mgroup));
        }
    }
#endif /* PDDL_DEBUG */

    FREE(var_order);
}

static void initConstr(pddl_symbolic_task_t *ss,
                       const pddl_symbolic_task_config_t *cfg,
                       pddl_err_t *err)
{
    pddl_mgroups_t mgs;
    pddlMGroupsInitEmpty(&mgs);
    for (int i = 0; i < ss->mg_strips.mg.mgroup_size; ++i){
        const pddl_mgroup_t *mgin = ss->mg_strips.mg.mgroup + i;
        pddl_mgroup_t *mg = pddlMGroupsAdd(&mgs, &mgin->mgroup);
        mg->is_exactly_one = mgin->is_exactly_one;
        mg->is_fam_group = mgin->is_fam_group;
        mg->is_goal = mgin->is_goal;
    }
    if (cfg->fam_groups > 0){
        PDDL_INFO(err, "Inferring fam-groups: %d exactly-1 mutex groups in"
                      " the input", mgs.mgroup_size);
        pddl_famgroup_config_t fam_cfg = PDDL_FAMGROUP_CONFIG_INIT;
        fam_cfg.maximal = 0;
        fam_cfg.goal = 1;
        fam_cfg.limit = cfg->fam_groups;
        pddlFAMGroupsInfer(&mgs, &ss->mg_strips.strips, &fam_cfg, err);
    }
    pddlMGroupsRemoveSubsets(&mgs);
    pddlMGroupsRemoveSmall(&mgs, 1);
    pddlMGroupsSetExactlyOne(&mgs, &ss->mg_strips.strips);
    pddlMGroupsSetGoal(&mgs, &ss->mg_strips.strips);
    PDDL_INFO(err, "%d exactly-1 mutex groups overall", mgs.mgroup_size);

    pddlSymbolicConstrInit(&ss->constr, &ss->vars, &ss->mutex, &mgs,
                           ss->cfg.constr_max_nodes,
                           ss->cfg.constr_max_time,
                           err);

    pddlMGroupsFree(&mgs);
}

static void fixSearchConfig(pddl_symbolic_search_config_t *cfg,
                            int is_fw,
                            pddl_err_t *err)
{
    if (cfg->use_op_constr)
        cfg->use_constr = 0;
    if (cfg->use_pot_heur
            || cfg->use_pot_heur_inconsistent
            || cfg->use_pot_heur_sum_op_cost){
        cfg->use_pot_heur = 1;
        cfg->pot_heur_config.op_pot = 1;
    }

    if (cfg->use_goal_splitting && !cfg->use_pot_heur){
        LOG(err, "cfg.use_goal_splitting reset to false, because potential"
             " heuristic is not used");
        cfg->use_goal_splitting = 0;
    }
    if (cfg->enabled
            && cfg->use_pot_heur
            && !cfg->use_pot_heur_inconsistent
            && !cfg->use_goal_splitting
            && !is_fw){
        WARN(err, "Using potential heuristics without goal splitting"
              " may lead to suboptimal solutions even if the potential"
              " heuristic is consistent!!");
    }

    if (is_fw && cfg->step_time_limit > 0.){
        WARN(err, "Time limit for a *forward* step is ignored.");
        cfg->step_time_limit = 0.;
    }
}

static void fixConfig(pddl_symbolic_task_config_t *cfg, pddl_err_t *err)
{
    CTX_NO_TIME(err, "fw", "fw");
    fixSearchConfig(&cfg->fw, 1, err);
    CTXEND(err);
    CTX_NO_TIME(err, "bw", "bw");
    fixSearchConfig(&cfg->bw, 0, err);
    CTXEND(err);
}

pddl_symbolic_task_t *pddlSymbolicTaskNew(const pddl_fdr_t *fdr,
                                          const pddl_symbolic_task_config_t *cfg,
                                          pddl_err_t *err)
{
    if (fdr->has_cond_eff){
        PDDL_ERR_RET(err, NULL, "Symbolic tasks does not support conditional"
                                " effects yet.");
    }

    int fw_use_pot = cfg->fw.use_pot_heur
                        || cfg->fw.use_pot_heur_inconsistent
                        || cfg->fw.use_pot_heur_sum_op_cost;
    int bw_use_pot = cfg->bw.use_pot_heur
                        || cfg->bw.use_pot_heur_inconsistent
                        || cfg->bw.use_pot_heur_sum_op_cost;
    if ((fw_use_pot && pddlHPotConfigIsEnsemble(&cfg->fw.pot_heur_config))
            || (bw_use_pot && pddlHPotConfigIsEnsemble(&cfg->bw.pot_heur_config))){
        PDDL_ERR_RET(err, NULL, "Symbolic tasks can use only a single"
                      " potential heuristic.");
    }

    if ((fw_use_pot && pddlHPotConfigIsEmpty(&cfg->fw.pot_heur_config))
            || (bw_use_pot && pddlHPotConfigIsEmpty(&cfg->bw.pot_heur_config))){
        PDDL_ERR_RET(err, NULL, "Missing optimization criteria for the"
                      " potential heuristic");
    }

    CTX(err, "symba_init", "symba-init");

    pddl_symbolic_task_t *ss;
    LOG(err, "Constructing symbolic task from FDR with"
        " vars: %{fdr_vars}d,"
        " facts: %{fdr_facts}d,"
        " ops: %{fdr_ops}d",
        fdr->var.var_size,
        fdr->var.global_id_size,
        fdr->op.op_size);

    ss = ZALLOC(pddl_symbolic_task_t);
    ss->cfg = *cfg;
    pddlHPotConfigInitCopy(&ss->cfg.fw.pot_heur_config, &cfg->fw.pot_heur_config);
    pddlHPotConfigInitCopy(&ss->cfg.bw.pot_heur_config, &cfg->bw.pot_heur_config);
    fixConfig(&ss->cfg, err);
    logConfig(&ss->cfg, err);

    prepareTask(ss, fdr, &ss->cfg, err);

    pddlSymbolicVarsInit(&ss->vars,
                         ss->mg_strips.strips.fact.fact_size,
                         &ss->mg_strips.mg);
    PDDL_INFO(err, "Prepared %d BDD variables covering %d facts and %d mgroups",
             ss->vars.bdd_var_size,
             ss->mg_strips.strips.fact.fact_size,
             ss->mg_strips.mg.mgroup_size);

    ss->mgr = pddlBDDManagerNew(ss->vars.bdd_var_size, ss->cfg.cache_size);
    if (ss->mgr == NULL){
        pddlSymbolicTaskDel(ss);
        PDDL_ERR_RET(err, NULL, "Initialization of CUDD failed.");
    }
    PDDL_INFO(err, "CUDD initialized.");

    pddlSymbolicVarsInitBDD(ss->mgr, &ss->vars);

    initConstr(ss, &ss->cfg, err);
    PDDL_INFO(err, "Constraints created.");

    ss->init = pddlSymbolicVarsCreateState(&ss->vars,
                                           &ss->mg_strips.strips.init);
    PDDL_INFO(err, "Initial state created.");
    ss->goal = pddlSymbolicVarsCreatePartialState(&ss->vars,
                                                  &ss->mg_strips.strips.goal);
    PDDL_INFO(err, "Goal state created.");

    PDDL_INFO(err, "Applying constraints on the goal ...");
    if (pddlSymbolicConstrApplyBwLimit(&ss->constr, &ss->goal,
                                       ss->cfg.goal_constr_max_time) == 0){
        PDDL_INFO(err, "Goal updated with constraints");
    }else{
        PDDL_INFO(err, "Applying constraints on the goal failed.");
        ss->goal_constr_failed = 1;
    }

    if (ss->cfg.fw.enabled){
        if (searchInit(ss, &ss->search_fw, 1, &ss->cfg.fw, ss->init,
                    ss->goal, err) != 0){
            TRACE_RET(err, NULL);
        }
    }
    if (ss->cfg.bw.enabled){
        if (searchInit(ss, &ss->search_bw, 0, &ss->cfg.bw, ss->goal,
                    ss->init, err) != 0){
            TRACE_RET(err, NULL);
        }
    }



    // TODO
    //ASSERT(Cudd_DebugCheck(ss->mgr) == 0);
    /*
    PDDL_INFO(err, "Symbolic task created."
                  " mem in use: %.2fMB, node count: %ld,"
                  " bdd variables: %d,"
                  " peak node count: %d,"
                  " peak live node count: %d,"
                  " garbage collections: %d",
             Cudd_ReadMemoryInUse(ss->mgr) / (1024. * 1024.),
             Cudd_ReadNodeCount(ss->mgr),
             Cudd_ReadSize(ss->mgr),
             Cudd_ReadPeakNodeCount(ss->mgr),
             Cudd_ReadPeakLiveNodeCount(ss->mgr),
             Cudd_ReadGarbageCollections(ss->mgr));
    */
    //Cudd_PrintInfo(ss->mgr, stderr);

    CTXEND(err);
    return ss;
}

void pddlSymbolicTaskDel(pddl_symbolic_task_t *ss)
{
    pddlFDRFree(&ss->fdr);
    pddlMGStripsFree(&ss->mg_strips);
    pddlMutexPairsFree(&ss->mutex);
    pddlSymbolicConstrFree(&ss->constr);
    if (ss->init != NULL)
        pddlBDDDel(ss->mgr, ss->init);
    if (ss->goal != NULL)
        pddlBDDDel(ss->mgr, ss->goal);
    //Cudd_PrintInfo(ss->mgr, stderr);
    pddlSymbolicVarsFree(&ss->vars);
    if (ss->search_fw.enabled)
        searchFree(ss, &ss->search_fw);
    if (ss->search_bw.enabled)
        searchFree(ss, &ss->search_bw);
    if (ss->mgr != NULL)
        pddlBDDManagerDel(ss->mgr);
    pddlHPotConfigFree(&ss->cfg.fw.pot_heur_config);
    pddlHPotConfigFree(&ss->cfg.bw.pot_heur_config);
    FREE(ss);
}

int pddlSymbolicTaskGoalConstrFailed(const pddl_symbolic_task_t *task)
{
    return task->goal_constr_failed;
}

static int searchOneDir(pddl_symbolic_task_t *ss,
                        pddl_symbolic_search_t *search,
                        pddl_err_t *err)
{
    int res = PDDL_SYMBOLIC_CONT;
    while (res == PDDL_SYMBOLIC_CONT){
        res = searchStep(ss, search, NULL, NULL, err);
    }
    LOG(err, "Expanded BDD Nodes: %{expanded_bdd_nodes}lu",
        search->num_expanded_bdd_nodes);
    LOG(err, "Expanded States: %{expanded_bdds}lu",
        search->num_expanded_states);
    LOG(err, "Avg. Expanded BDD Nodes: %{avg_expanded_bdd_nodes}.2f",
             search->avg_expanded_bdd_nodes);
    return res;
}


int pddlSymbolicTaskSearchFw(pddl_symbolic_task_t *ss,
                             pddl_iarr_t *plan,
                             pddl_err_t *err)
{
    if (!ss->search_fw.enabled)
        PANIC("Symbolic Task wasn't initialzed with fw search!");
    CTX(err, "symba_fw", "symba-fw");
    searchStart(ss, &ss->search_fw, err);
    int res = searchOneDir(ss, &ss->search_fw, err);
    pddlIArrAppendArr(plan, &ss->search_fw.plan);

#ifdef PDDL_DEBUG
    int op_id;
    PDDL_IARR_FOR_EACH(plan, op_id){
        PDDL_INFO(err, "plan: (%s) ;; id=%d, cost %d",
                 ss->mg_strips.strips.op.op[op_id]->name,
                 op_id,
                 ss->mg_strips.strips.op.op[op_id]->cost);
    }
#endif /* PDDL_DEBUG */
    CTXEND(err);
    return res;
}

int pddlSymbolicTaskSearchBw(pddl_symbolic_task_t *ss,
                             pddl_iarr_t *plan,
                             pddl_err_t *err)
{
    if (!ss->search_bw.enabled)
        PANIC("Symbolic Task wasn't initialzed with bw search!");
    CTX(err, "symba_bw", "symba-bw");
    searchStart(ss, &ss->search_bw, err);
    int res = searchOneDir(ss, &ss->search_bw, err);
    pddlIArrAppendArr(plan, &ss->search_bw.plan);

#ifdef PDDL_DEBUG
    int op_id;
    PDDL_IARR_FOR_EACH(plan, op_id){
        PDDL_INFO(err, "plan: (%s) ;; id=%d, cost %d",
                 ss->mg_strips.strips.op.op[op_id]->name,
                 op_id,
                 ss->mg_strips.strips.op.op[op_id]->cost);
    }
#endif /* PDDL_DEBUG */
    CTXEND(err);
    return res;
}

static void fwbwExtractPlan(pddl_symbolic_task_t *ss,
                            pddl_symbolic_search_t *fw_search,
                            pddl_symbolic_search_t *bw_search,
                            pddl_iarr_t *plan,
                            pddl_err_t *err)
{
    const pddl_symbolic_state_t *fw_goal_state, *bw_goal_state;
    fw_goal_state = pddlSymbolicStatesGet(&fw_search->state, fw_search->plan_goal_id);
    bw_goal_state = pddlSymbolicStatesGet(&bw_search->state, bw_search->plan_goal_id);

    pddl_bdd_t *fw_goal_bdd = fw_goal_state->bdd;
    pddl_bdd_t *bw_goal_bdd = bw_goal_state->bdd;

    // Compute cut between forward and backward search frontier
    pddl_bdd_t *cut = pddlBDDAnd(ss->mgr, fw_goal_bdd, bw_goal_bdd);
    ASSERT_RUNTIME(!pddlBDDIsFalse(ss->mgr, cut));

    // We need to choose one particular state before extracting plans from
    // fw and bw searches
    PDDL_ISET(cut_fact_state);
    pddl_bdd_t *cut_state = bddStateSelectOne(ss, cut, &cut_fact_state);
    pddlISetFree(&cut_fact_state);
    pddlBDDDel(ss->mgr, cut);

    // Extract forward plan from init to cut_state
    plan_t fw_plan;
    planInit(ss, fw_search, &fw_plan, fw_goal_state, cut_state);
    planExtractFw(&fw_plan, &ss->mg_strips.strips, &fw_search->plan);
    planFree(&fw_plan);

    // Extract backward plan from cut_state to goal
    plan_t bw_plan;
    planInit(ss, bw_search, &bw_plan, bw_goal_state, cut_state);
    planReverse(&bw_plan);
    planExtractFw(&bw_plan, &ss->mg_strips.strips, &bw_search->plan);
    planFree(&bw_plan);

    pddlBDDDel(ss->mgr, cut_state);

    // Join fw and bw plans
    int op_id;
    PDDL_IARR_FOR_EACH(&fw_search->plan, op_id)
        pddlIArrAdd(plan, op_id);
    PDDL_IARR_FOR_EACH(&bw_search->plan, op_id)
        pddlIArrAdd(plan, op_id);
}

int pddlSymbolicTaskSearchFwBw(pddl_symbolic_task_t *ss,
                               pddl_iarr_t *plan,
                               pddl_err_t *err)
{
    if (!ss->search_fw.enabled)
        PANIC("Symbolic Task wasn't initialzed with fw search!");
    if (!ss->search_bw.enabled)
        PANIC("Symbolic Task wasn't initialzed with bw search!");
    CTX(err, "symba_fwbw", "symba-bi");
    PDDL_INFO(err, "start");
    searchStart(ss, &ss->search_fw, err);
    searchStart(ss, &ss->search_bw, err);

    int res = PDDL_SYMBOLIC_FAIL;
    pddl_cost_t zero_cost;
    pddlCostSetZero(&zero_cost);

    int fw_cont, bw_cont;
    fw_cont = bw_cont = PDDL_SYMBOLIC_CONT;

    while (!pddlPairHeapEmpty(ss->search_fw.state.open)
            && !pddlPairHeapEmpty(ss->search_bw.state.open)){
        if (fw_cont == PDDL_SYMBOLIC_PLAN_FOUND
                || bw_cont == PDDL_SYMBOLIC_PLAN_FOUND
                || fw_cont == PDDL_SYMBOLIC_PLAN_NOT_EXIST
                || bw_cont == PDDL_SYMBOLIC_PLAN_NOT_EXIST
                || (fw_cont != PDDL_SYMBOLIC_CONT
                        && bw_cont != PDDL_SYMBOLIC_CONT)){
            break;
        }

        const pddl_cost_t *min_fw_cost = pddlSymbolicStatesMinOpenCost(&ss->search_fw.state);
        if (min_fw_cost == NULL)
            min_fw_cost = &zero_cost;
        const pddl_cost_t *min_bw_cost = pddlSymbolicStatesMinOpenCost(&ss->search_bw.state);
        if (min_bw_cost == NULL)
            min_bw_cost = &zero_cost;
        const pddl_cost_t *bound = &ss->search_fw.state.bound;
        ASSERT(pddlCostCmp(bound, &ss->search_bw.state.bound) == 0);
        if (pddlCostCmpSum(min_fw_cost, min_bw_cost, bound) >= 0)
            break;

        int fw_step = 0;
        if (bw_cont == PDDL_SYMBOLIC_ABORT_TIME_LIMIT){
            fw_step = 1;

        }else{
            float fw_est = ss->search_fw.next_step_estimate;
            float bw_est = ss->search_bw.next_step_estimate;
            if (fw_cont == PDDL_SYMBOLIC_CONT && fw_est <= bw_est)
                fw_step = 1;
        }

        if (fw_step){
            CTX_NO_TIME(err, "fw_step", "fw-step");
            fw_cont = searchStep(ss, &ss->search_fw, &ss->search_bw, NULL, err);
            CTXEND(err);

        }else{
            CTX_NO_TIME(err, "bw_step", "bw-step");
            pddl_time_limit_t step_time_limit;
            pddlTimeLimitSet(&step_time_limit, ss->search_bw.cfg.step_time_limit);
            bw_cont = searchStep(ss, &ss->search_bw, &ss->search_fw,
                                 &step_time_limit, err);
            if (pddlTimeLimitCheck(&step_time_limit) != 0)
                bw_cont = PDDL_SYMBOLIC_ABORT_TIME_LIMIT;
            if (bw_cont == PDDL_SYMBOLIC_ABORT_TIME_LIMIT){
                LOG(err, "Time limit for the bw step reached.");
                LOG(err, "bw search disabled");
            }
            CTXEND(err);
        }
    }

    if (fw_cont == PDDL_SYMBOLIC_PLAN_FOUND){
        const pddl_symbolic_state_t *goal;
        goal = pddlSymbolicStatesGet(&ss->search_fw.state,
                                     ss->search_fw.plan_goal_id);
        pddlIArrAppendArr(plan, &ss->search_fw.plan);
        LOG(err, "Found plan, cost: %{plan_cost}s,"
            " length: %{plan_length}d,"
            " using %{plan_dir}s direction",
            F_COST(&goal->cost), pddlIArrSize(plan), "fw");
        res = PDDL_SYMBOLIC_PLAN_FOUND;

    }else if (bw_cont == PDDL_SYMBOLIC_PLAN_FOUND){
        const pddl_symbolic_state_t *goal;
        goal = pddlSymbolicStatesGet(&ss->search_bw.state,
                                     ss->search_bw.plan_goal_id);
        pddlIArrAppendArr(plan, &ss->search_bw.plan);
        LOG(err, "Found plan, cost: %{plan_cost}s,"
            " length: %{plan_length}d,"
            " using %{plan_dir}s direction",
            F_COST(&goal->cost), pddlIArrSize(plan), "bw");
        res = PDDL_SYMBOLIC_PLAN_FOUND;

    }else{
        ASSERT(pddlCostCmp(&ss->search_fw.state.bound, &ss->search_bw.state.bound) == 0);
        ASSERT(ss->search_fw.plan_goal_id == ss->search_bw.plan_other_goal_id);
        ASSERT(ss->search_fw.plan_other_goal_id == ss->search_bw.plan_goal_id);

        if (ss->search_fw.plan_goal_id == -1){
            res = PDDL_SYMBOLIC_PLAN_NOT_EXIST;
        }else{
            res = PDDL_SYMBOLIC_PLAN_FOUND;
            fwbwExtractPlan(ss, &ss->search_fw, &ss->search_bw, plan, err);
            LOG(err, "Found plan, cost: %{plan_cost}s,"
                " length: %{plan_length}d,"
                " using %{plan_dir}s direction",
                F_COST(&ss->search_fw.state.bound), pddlIArrSize(plan), "fwbw");
        }
    }

    LOG(err, "Fw Expanded BDD Nodes: %{fw.expanded_bdd_nodes}lu",
        ss->search_fw.num_expanded_bdd_nodes);
    LOG(err, "Fw Expanded States: %{fw.expanded_bdds}lu",
        ss->search_fw.num_expanded_states);
    LOG(err, "Fw Avg. Expanded BDD Nodes: %{fw.avg_expanded_bdd_nodes}.2f",
        ss->search_fw.avg_expanded_bdd_nodes);

    LOG(err, "Bw Expanded BDD Nodes: %{bw.expanded_bdd_nodes}lu",
        ss->search_bw.num_expanded_bdd_nodes);
    LOG(err, "Bw Expanded States: %{bw.expanded_bdds}lu",
        ss->search_bw.num_expanded_states);
    LOG(err, "Bw Avg. Expanded BDD Nodes: %{bw.avg_expanded_bdd_nodes}.2f",
        ss->search_bw.avg_expanded_bdd_nodes);

    LOG(err, "Expanded BDD Nodes: %{expanded_bdd_nodes}lu",
             ss->search_fw.num_expanded_bdd_nodes
                + ss->search_bw.num_expanded_bdd_nodes);
    LOG(err, "Expanded States: %{expanded_bdds}lu",
             ss->search_fw.num_expanded_states + ss->search_bw.num_expanded_states);
    float avg = ss->search_fw.avg_expanded_bdd_nodes
                    * ss->search_fw.num_expanded_states;
    avg += ss->search_bw.avg_expanded_bdd_nodes
                * ss->search_bw.num_expanded_states;
    avg /= ss->search_fw.num_expanded_states + ss->search_bw.num_expanded_states;
    LOG(err, "Avg. Expanded BDD Nodes: %{avg_expanded_bdd_nodes}.2f", avg);

#ifdef PDDL_DEBUG
    int op_id;
    PDDL_IARR_FOR_EACH(plan, op_id){
        PDDL_INFO(err, "plan: (%s) ;; id=%d, cost %d",
                 ss->mg_strips.strips.op.op[op_id]->name,
                 op_id,
                 ss->mg_strips.strips.op.op[op_id]->cost);
    }
#endif /* PDDL_DEBUG */

    const char *res_str = "UNKOWN";
    switch (res){
        case PDDL_SYMBOLIC_CONT:
            res_str = "CONT";
            break;
        case PDDL_SYMBOLIC_PLAN_FOUND:
            res_str = "PLAN FOUND";
            break;
        case PDDL_SYMBOLIC_PLAN_NOT_EXIST:
            res_str = "PLAN NOT EXIST";
            break;
        case PDDL_SYMBOLIC_FAIL:
            res_str = "FAIL";
            break;
    }
    PDDL_INFO(err, "DONE: %s", res_str);
    CTXEND(err);
    return res;
}

int pddlSymbolicTaskSearch(pddl_symbolic_task_t *ss,
                           pddl_iarr_t *plan,
                           pddl_err_t *err)
{
    if (ss->cfg.fw.enabled && ss->cfg.bw.enabled){
        return pddlSymbolicTaskSearchFwBw(ss, plan, err);
    }else if (ss->cfg.fw.enabled){
        return pddlSymbolicTaskSearchFw(ss, plan, err);
    }else if (ss->cfg.bw.enabled){
        return pddlSymbolicTaskSearchBw(ss, plan, err);
    }else{
        PDDL_ERR_RET(err, -1, "Neither of search directions was initialized");
    }
}

static pddl_bdd_t *createFDRState(pddl_symbolic_task_t *ss, const int *state)
{
    PDDL_ISET(st);
    for (int i = 0; i < ss->fdr.var.var_size; ++i)
        pddlISetAdd(&st, ss->fdr.var.var[i].val[state[i]].global_id);
    pddl_bdd_t *bdd_state = pddlSymbolicVarsCreateState(&ss->vars, &st);
    pddlISetFree(&st);
    return bdd_state;
}

int pddlSymbolicTaskCheckApplyFw(pddl_symbolic_task_t *ss,
                                 const int *state,
                                 const int *res_state,
                                 int op_id)
{
    int res = 1;
    pddl_bdd_t *bdd_state = createFDRState(ss, state);
    pddl_bdd_t *bdd_res_state = createFDRState(ss, res_state);
    for (int tri = 0; res && tri < ss->search_fw.trans.trans_size; ++tri){
        pddl_symbolic_trans_set_t *trs = ss->search_fw.trans.trans + tri;
        if (!pddlISetIn(op_id, &trs->op))
            continue;

        pddl_bdd_t *next_states = pddlSymbolicTransSetImage(trs, bdd_state, NULL);
        pddlSymbolicConstrApplyFw(&ss->constr, &next_states);
        pddl_bdd_t *conj = pddlBDDAnd(ss->mgr, next_states, bdd_res_state);
        if (pddlBDDIsFalse(ss->mgr, conj)){
            res = 0;
        }
        pddlBDDDel(ss->mgr, conj);
        pddlBDDDel(ss->mgr, next_states);
    }
    pddlBDDDel(ss->mgr, bdd_state);
    pddlBDDDel(ss->mgr, bdd_res_state);

    return res;
}

int pddlSymbolicTaskCheckApplyBw(pddl_symbolic_task_t *ss,
                                 const int *state,
                                 const int *res_state,
                                 int op_id)
{
    int res = 1;
    pddl_bdd_t *bdd_state = createFDRState(ss, state);
    pddl_bdd_t *bdd_res_state = createFDRState(ss, res_state);
    for (int tri = 0; res && tri < ss->search_bw.trans.trans_size; ++tri){
        pddl_symbolic_trans_set_t *trs = ss->search_bw.trans.trans + tri;
        if (!pddlISetIn(op_id, &trs->op))
            continue;

        pddl_bdd_t *next_states = pddlSymbolicTransSetPreImage(trs, bdd_state, NULL);
        pddlSymbolicConstrApplyBw(&ss->constr, &next_states);
        pddl_bdd_t *conj = pddlBDDAnd(ss->mgr, next_states, bdd_res_state);
        if (pddlBDDIsFalse(ss->mgr, conj)){
            res = 0;
        }
        pddlBDDDel(ss->mgr, conj);
        pddlBDDDel(ss->mgr, next_states);
    }
    pddlBDDDel(ss->mgr, bdd_state);
    pddlBDDDel(ss->mgr, bdd_res_state);

    return res;
}

int pddlSymbolicTaskCheckPlan(pddl_symbolic_task_t *ss,
                              const pddl_iarr_t *op,
                              int plan_size)
{
    int res = 1;
    pddl_bdd_t **fw_node = ALLOC_ARR(pddl_bdd_t *, plan_size + 1);
    pddl_bdd_t **bw_node = ALLOC_ARR(pddl_bdd_t *, plan_size + 1);
    fw_node[0] = pddlBDDClone(ss->mgr, ss->init);
    bw_node[plan_size] = pddlBDDClone(ss->mgr, ss->goal);
    pddl_bdd_t *fw_closed = pddlBDDClone(ss->mgr, fw_node[0]);
    pddl_bdd_t *bw_closed = pddlBDDClone(ss->mgr, bw_node[plan_size]);
    for (int fi = 0; fi < plan_size; ++fi){
        int fw_op_id = pddlIArrGet(op, fi);
        for (int tri = 0; tri < ss->search_fw.trans.trans_size; ++tri){
            pddl_symbolic_trans_set_t *trs = ss->search_fw.trans.trans + tri;
            if (!pddlISetIn(fw_op_id, &trs->op))
                continue;
            fw_node[fi + 1] = pddlSymbolicTransSetImage(trs, fw_node[fi], NULL);
            if (ss->cfg.fw.use_op_constr){
                pddl_bdd_t *tmp = pddlBDDClone(ss->mgr, fw_node[fi + 1]);
                pddlSymbolicConstrApplyFw(&ss->constr, &tmp);

                pddl_bdd_t *fwnot = pddlBDDNot(ss->mgr, fw_node[fi + 1]);
                pddl_bdd_t *diff;
                diff = pddlBDDAnd(ss->mgr, tmp, fwnot);
                //ASSERT(IS_FALSE(ss->mgr, diff));
                pddlBDDDel(ss->mgr, diff);
                pddlBDDDel(ss->mgr, fwnot);

                pddl_bdd_t *tmpnot = pddlBDDNot(ss->mgr, tmp);
                diff = pddlBDDAnd(ss->mgr, tmpnot, fw_node[fi + 1]);
                //ASSERT(IS_FALSE(ss->mgr, diff));
                pddlBDDDel(ss->mgr, diff);
                pddlBDDDel(ss->mgr, tmpnot);

                //if (tmp != fw_node[fi + 1])
                //    res = 0;
                //ASSERT(tmp == fw_node[fi + 1]);
                pddlBDDDel(ss->mgr, tmp);

            }else if (ss->cfg.fw.use_constr){
                pddlSymbolicConstrApplyFw(&ss->constr, &fw_node[fi + 1]);
            }

            pddl_bdd_t *nclosed = pddlBDDNot(ss->mgr, fw_closed);
            pddlBDDAndUpdate(ss->mgr, &fw_node[fi + 1], nclosed);
            pddlBDDDel(ss->mgr, nclosed);
            pddlBDDOrUpdate(ss->mgr, &fw_closed, fw_node[fi + 1]);
        }

        int bw_op_id = pddlIArrGet(op, plan_size - fi - 1);
        for (int tri = 0; tri < ss->search_bw.trans.trans_size; ++tri){
            pddl_symbolic_trans_set_t *trs = ss->search_bw.trans.trans + tri;
            if (!pddlISetIn(bw_op_id, &trs->op))
                continue;
            int fi2 = plan_size - fi;
            bw_node[fi2 - 1] = pddlSymbolicTransSetPreImage(trs, bw_node[fi2], NULL);
            if (ss->cfg.bw.use_op_constr){
                pddl_bdd_t *tmp = pddlBDDClone(ss->mgr, bw_node[fi2 - 1]);
                pddlSymbolicConstrApplyBw(&ss->constr, &tmp);
                //if (tmp != bw_node[fi2 - 1])
                //    res = 0;
                //ASSERT(tmp == bw_node[fi2 - 1]);
                pddlBDDDel(ss->mgr, tmp);

            }else if (ss->cfg.bw.use_constr){
                pddlSymbolicConstrApplyBw(&ss->constr, &bw_node[fi2 - 1]);
            }
            pddl_bdd_t *nclosed = pddlBDDNot(ss->mgr, bw_closed);
            pddlBDDAndUpdate(ss->mgr, &bw_node[fi2 - 1], nclosed);
            pddlBDDDel(ss->mgr, nclosed);
            pddlBDDOrUpdate(ss->mgr, &bw_closed, bw_node[fi2 - 1]);
        }
    }

    for (int fi = 0; fi < plan_size + 1; ++fi){
        pddl_bdd_t *conj = pddlBDDAnd(ss->mgr, fw_node[fi], bw_node[fi]);
        if (pddlBDDIsFalse(ss->mgr, conj)){
            fprintf(stderr, "FAIL1 %d\n", fi);
            fflush(stderr);
            res = 0;
        }
        pddlBDDDel(ss->mgr, conj);

        conj = pddlBDDAnd(ss->mgr, bw_node[fi], fw_closed);
        if (pddlBDDIsFalse(ss->mgr, conj)){
            fprintf(stderr, "FAIL2 %d\n", fi);
            fflush(stderr);
            res = 0;
        }
        pddlBDDDel(ss->mgr, conj);

        conj = pddlBDDAnd(ss->mgr, fw_node[fi], bw_closed);
        if (pddlBDDIsFalse(ss->mgr, conj)){
            fprintf(stderr, "FAIL3 %d\n", fi);
            fflush(stderr);
            res = 0;
        }
        pddlBDDDel(ss->mgr, conj);
    }

    pddlBDDDel(ss->mgr, fw_closed);
    pddlBDDDel(ss->mgr, bw_closed);
    for (int fi = 0; fi < plan_size + 1; ++fi){
        pddlBDDDel(ss->mgr, fw_node[fi]);
        pddlBDDDel(ss->mgr, bw_node[fi]);
    }
    FREE(fw_node);
    FREE(bw_node);

    return res;
}
