/***
 * Copyright (c)2022 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

#include "pddl/open_list.h"
#include "pddl/strips_state_space.h"
#include "pddl/strips_maker.h"
#include "pddl/lifted_app_action.h"
#include "pddl/lifted_search.h"
#include "internal.h"

typedef void (*search_del_fn)(pddl_lifted_search_t *);
typedef pddl_lifted_search_status_t (*search_init_step_fn)(pddl_lifted_search_t *);
typedef pddl_lifted_search_status_t (*search_step_fn)(pddl_lifted_search_t *);

struct pddl_lifted_search {
    const pddl_t *pddl;
    pddl_err_t *err;
    pddl_lifted_app_action_t *app_action;
    pddl_strips_maker_t strips;
    pddl_strips_state_space_t state_space;

    pddl_strips_state_space_node_t cur_node;
    pddl_strips_state_space_node_t next_node;
    pddl_lifted_search_stat_t _stat;

    pddl_iset_t goal;
    int goal_is_unreachable;
    pddl_lifted_plan_t plan;

    search_del_fn del_fn;
    search_init_step_fn init_step_fn;
    search_step_fn step_fn;

    const char *err_prefix;
};

struct pddl_lifted_search_bfs {
    pddl_lifted_search_t search;
    pddl_lifted_heur_t *heur;
    int g_weight;
    int h_weight;
    int is_lazy;
    pddl_open_list_t *list;
};
typedef struct pddl_lifted_search_bfs pddl_lifted_search_bfs_t;

#define BFS(S) pddl_container_of((S), pddl_lifted_search_bfs_t, search)

static void setGoal(pddl_lifted_search_t *s);
static pddl_state_id_t insertInitState(pddl_lifted_search_t *s);
static int isGoal(const pddl_lifted_search_t *s);
static void applyAction(pddl_lifted_search_t *s,
                        const pddl_action_t *action,
                        const pddl_obj_id_t *args,
                        int *args_id,
                        int *cost);
static void extractPlan(pddl_lifted_search_t *s, pddl_state_id_t goal_state_id);

static int searchInit(pddl_lifted_search_t *s,
                      const pddl_t *pddl,
                      const pddl_lifted_search_config_t *cfg,
                      search_del_fn del_fn,
                      search_init_step_fn init_step_fn,
                      search_step_fn step_fn,
                      const char *err_prefix,
                      pddl_err_t *err)
{
    s->pddl = pddl;
    s->err = err;

    s->app_action = pddlLiftedAppActionNew(pddl, cfg->succ_gen, err);
    pddlStripsMakerInit(&s->strips, pddl);
    pddlStripsStateSpaceInit(&s->state_space, err);

    pddlStripsStateSpaceNodeInit(&s->cur_node, &s->state_space);
    pddlStripsStateSpaceNodeInit(&s->next_node, &s->state_space);
    s->del_fn = del_fn;
    s->init_step_fn = init_step_fn;
    s->step_fn = step_fn;
    s->err_prefix = err_prefix;
    return 0;
}

static void searchFree(pddl_lifted_search_t *s)
{
    pddlStripsStateSpaceNodeFree(&s->cur_node);
    pddlStripsStateSpaceNodeFree(&s->next_node);
    pddlStripsStateSpaceFree(&s->state_space);
    pddlStripsMakerFree(&s->strips);
    pddlLiftedAppActionDel(s->app_action);
    pddlISetFree(&s->goal);
    for (int i = 0; i < s->plan.plan_len; ++i)
        FREE(s->plan.plan[i]);
    if (s->plan.plan != NULL)
        FREE(s->plan.plan);
}

static void bfsDel(pddl_lifted_search_t *bfs);
static pddl_lifted_search_status_t bfsInitStep(pddl_lifted_search_t *bfs);
static pddl_lifted_search_status_t bfsStep(pddl_lifted_search_t *bfs);

static pddl_lifted_search_t *bfsNew(const pddl_lifted_search_config_t *cfg,
                                    int g_weight,
                                    int h_weight,
                                    int is_lazy,
                                    const char *err_prefix,
                                    pddl_err_t *err)
{
    CTX(err, "bfs", err_prefix);
    pddl_lifted_search_bfs_t *bfs;

    bfs = ZALLOC(pddl_lifted_search_bfs_t);
    searchInit(&bfs->search, cfg->pddl, cfg, bfsDel, bfsInitStep, bfsStep,
               err_prefix, err);
    // TODO: Check for conditional effects
    bfs->heur = cfg->heur;
    bfs->g_weight = g_weight;
    bfs->h_weight = h_weight;
    bfs->is_lazy = is_lazy;
    bfs->list = pddlOpenListSplayTree2();

    CTXEND(err);
    return &bfs->search;
}

static void bfsDel(pddl_lifted_search_t *s)
{
    searchFree(s);

    pddl_lifted_search_bfs_t *bfs = BFS(s);
    if (bfs->list)
        pddlOpenListDel(bfs->list);
    FREE(bfs);
}


static void bfsPush(pddl_lifted_search_bfs_t *bfs,
                    pddl_strips_state_space_node_t *node,
                    int h_value)
{
    int cost[2];
    cost[0] = (bfs->g_weight * node->g_value) + (bfs->h_weight * h_value);
    cost[1] = h_value;
    if (node->status == PDDL_STRIPS_STATE_SPACE_STATUS_CLOSED)
        --bfs->search._stat.closed;
    node->status = PDDL_STRIPS_STATE_SPACE_STATUS_OPEN;
    pddlOpenListPush(bfs->list, cost, node->id);
    ++bfs->search._stat.open;
}

static pddl_lifted_search_status_t bfsInitStep(pddl_lifted_search_t *s)
{
    pddl_lifted_search_bfs_t *bfs = BFS(s);
    CTX_NO_TIME(s->err, "bfs", s->err_prefix);
    pddl_lifted_search_status_t ret = PDDL_LIFTED_SEARCH_CONT;

    pddl_state_id_t state_id = insertInitState(s);
    ASSERT_RUNTIME(state_id == 0);

    setGoal(s);
    if (s->goal_is_unreachable)
        ret = PDDL_LIFTED_SEARCH_UNSOLVABLE;

    pddlStripsStateSpaceGet(&s->state_space, state_id, &s->cur_node);
    s->cur_node.parent_id = PDDL_NO_STATE_ID;
    s->cur_node.op_id = -1;
    s->cur_node.g_value = 0;

    int h_value = 0;
    if (bfs->heur != NULL && !bfs->is_lazy){
        pddl_cost_t h = pddlLiftedHeurEstimate(bfs->heur,
                                               &s->cur_node.state,
                                               &s->strips.ground_atom);
        h_value = h.cost;
        ++s->_stat.evaluated;
    }

    LOG(s->err, "Heuristic value for the initial state: %{init_hvalue}d", h_value);
    if (h_value == PDDL_COST_DEAD_END){
        ++s->_stat.dead_end;
        ret = PDDL_LIFTED_SEARCH_UNSOLVABLE;
    }

    ASSERT_RUNTIME(s->cur_node.status == PDDL_STRIPS_STATE_SPACE_STATUS_NEW);
    bfsPush(bfs, &s->cur_node, h_value);
    pddlStripsStateSpaceSet(&s->state_space, &s->cur_node);
    CTXEND(s->err);
    return ret;
}

static void bfsInsertNextState(pddl_lifted_search_bfs_t *bfs,
                               int args_id,
                               int op_cost,
                               int in_h_value)
{
    pddl_lifted_search_t *s = &bfs->search;
    // Compute its g() value
    int next_g_value = s->cur_node.g_value + op_cost;

    // Skip if we have better state already
    if (s->next_node.status != PDDL_STRIPS_STATE_SPACE_STATUS_NEW
            && s->next_node.g_value <= next_g_value){
        return;
    }

    s->next_node.parent_id = s->cur_node.id;
    s->next_node.op_id = args_id;
    s->next_node.g_value = next_g_value;
 
    int h_value = 0;
    if (in_h_value >= 0){
        h_value = in_h_value;
    }else if (bfs->heur != NULL){
        pddl_cost_t h = pddlLiftedHeurEstimate(bfs->heur,
                                               &s->next_node.state,
                                               &s->strips.ground_atom);
        h_value = h.cost;
        ++s->_stat.evaluated;
    }

    if (h_value == PDDL_COST_DEAD_END){
        ++s->_stat.dead_end;
        if (s->next_node.status == PDDL_STRIPS_STATE_SPACE_STATUS_OPEN)
            --s->_stat.open;
        s->next_node.status = PDDL_STRIPS_STATE_SPACE_STATUS_CLOSED;
        ++s->_stat.closed;

    }else if (s->next_node.status == PDDL_STRIPS_STATE_SPACE_STATUS_NEW
                || s->next_node.status == PDDL_STRIPS_STATE_SPACE_STATUS_OPEN){
        bfsPush(bfs, &s->next_node, h_value);

    }else if (s->next_node.status == PDDL_STRIPS_STATE_SPACE_STATUS_CLOSED){
        bfsPush(bfs, &s->next_node, h_value);
        ++s->_stat.reopen;
    }

    pddlStripsStateSpaceSet(&s->state_space, &s->next_node);
}

static pddl_lifted_search_status_t bfsStep(pddl_lifted_search_t *s)
{
    pddl_lifted_search_bfs_t *bfs = BFS(s);
    CTX_NO_TIME(s->err, "bfs", s->err_prefix);

    ++s->_stat.steps;

    // Get next state from open list
    int cur_cost[2];
    pddl_state_id_t cur_state_id;
    if (pddlOpenListPop(bfs->list, &cur_state_id, cur_cost) != 0){
        CTXEND(s->err);
        return PDDL_LIFTED_SEARCH_UNSOLVABLE;
    }

    // Load the current state
    pddlStripsStateSpaceGet(&s->state_space, cur_state_id, &s->cur_node);

    // Skip already closed nodes
    if (s->cur_node.status != PDDL_STRIPS_STATE_SPACE_STATUS_OPEN){
        CTXEND(s->err);
        return PDDL_LIFTED_SEARCH_CONT;
    }

    // Close the current node
    s->cur_node.status = PDDL_STRIPS_STATE_SPACE_STATUS_CLOSED;
    pddlStripsStateSpaceSet(&s->state_space, &s->cur_node);
    --s->_stat.open;
    ++s->_stat.closed;
    int last_f_value = s->_stat.last_f_value;
    s->_stat.last_f_value = cur_cost[0];
    if (last_f_value != s->_stat.last_f_value){
        s->_stat.expanded_before_last_f_layer = s->_stat.expanded;
        s->_stat.dead_end_before_last_f_layer = s->_stat.dead_end;
    }

    // Check whether it is a goal
    if (isGoal(s)){
        extractPlan(s, cur_state_id);
        CTXEND(s->err);
        return PDDL_LIFTED_SEARCH_FOUND;
    }

    // Find all applicable operators
    pddlLiftedAppActionSetStripsState(s->app_action, &s->strips,
                                      &s->cur_node.state);
    pddlLiftedAppActionFindAppActions(s->app_action);
    ++s->_stat.expanded;

    int app_size = pddlLiftedAppActionSize(s->app_action);
    int h_value = -1;
    if (app_size > 0 && bfs->is_lazy){
        h_value = 0;
        if (bfs->heur != NULL){
            pddl_cost_t h = pddlLiftedHeurEstimate(bfs->heur,
                                                   &s->cur_node.state,
                                                   &s->strips.ground_atom);
            h_value = h.cost;
            ++s->_stat.evaluated;
        }
    }

    for (int app_i = 0; app_i < app_size; ++app_i){
        int action_id = pddlLiftedAppActionId(s->app_action, app_i);
        const pddl_action_t *action = s->pddl->action.action + action_id;
        const pddl_obj_id_t *args = pddlLiftedAppActionArgs(s->app_action, app_i);

        int cost, args_id;
        applyAction(s, action, args, &args_id, &cost);
        if (args_id < 0)
            continue;

        // Insert the new state
        pddl_state_id_t next_state_id;
        next_state_id = pddlStripsStateSpaceInsert(&s->state_space,
                                                   &s->next_node.state);
        pddlStripsStateSpaceGetNoState(&s->state_space,
                                       next_state_id, &s->next_node);
        bfsInsertNextState(bfs, args_id, cost, h_value);
    }
    CTXEND(s->err);
    return PDDL_LIFTED_SEARCH_CONT;
}




void pddlLiftedSearchDel(pddl_lifted_search_t *s)
{
    s->del_fn(s);
}

pddl_lifted_search_status_t pddlLiftedSearchInitStep(pddl_lifted_search_t *s)
{
    return s->init_step_fn(s);
}

pddl_lifted_search_status_t pddlLiftedSearchStep(pddl_lifted_search_t *s)
{
    return s->step_fn(s);
}

void pddlLiftedSearchStat(const pddl_lifted_search_t *s,
                          pddl_lifted_search_stat_t *stat)
{
    *stat = s->_stat;
    stat->generated = s->state_space.num_states;
}

void pddlLiftedSearchStatLog(const pddl_lifted_search_t *s, pddl_err_t *err)
{
    pddl_lifted_search_stat_t stat;
    pddlLiftedSearchStat(s, &stat);
    LOG(err, "Search steps: %{stat_steps}lu,"
        " expand: %{stat_expanded}lu,"
        " expand-blfl: %{stat_expanded_blfl}lu,"
        " eval: %{stat_evaluated}lu,"
        " gen: %{stat_generated}lu,"
        " open: %{stat_open}lu,"
        " closed: %{stat_closed}lu,"
        " reopen: %{stat_reopen}lu,"
        " de: %{stat_dead_end}lu,"
        " de-blfl: %{stat_dead_end_blfl}lu,"
        " f: %{stat_fvalue}d",
        stat.steps,
        stat.expanded,
        stat.expanded_before_last_f_layer,
        stat.evaluated,
        stat.generated,
        stat.open,
        stat.closed,
        stat.reopen,
        stat.dead_end,
        stat.dead_end_before_last_f_layer,
        stat.last_f_value);
}





static int _setGoal(pddl_fm_t *c, void *_s)
{
    pddl_lifted_search_t *s = _s;
    const pddl_t *pddl = s->pddl;

    if (c->type == PDDL_FM_ATOM){
        const pddl_fm_atom_t *atom = PDDL_FM_CAST(c, atom);
        if (pddlPredIsStatic(&pddl->pred.pred[atom->pred])){
            const pddl_ground_atom_t *ga;
            ga = pddlGroundAtomsFindAtom(&s->strips.ground_atom_static,
                                         atom, NULL);
            if (ga == NULL){
                s->goal_is_unreachable = 1;
                return -1;
            }

        }else{
            const pddl_ground_atom_t *ga;
            ga = pddlStripsMakerAddAtom(&s->strips, atom, NULL, NULL);
            pddlISetAdd(&s->goal, ga->id);
        }
        if (!pddlFmAtomIsGrounded(atom)){
            s->goal_is_unreachable = 1;
            PDDL_ERR_RET(s->err, -1, "Goal specification cannot contain"
                          " parametrized atoms.");
        }

        return 0;

    }else if (c->type == PDDL_FM_AND){
        return 0;

    }else if (c->type == PDDL_FM_BOOL){
        const pddl_fm_bool_t *b = PDDL_FM_CAST(c, bool);
        if (!b->val)
            s->goal_is_unreachable = 1;
        return 0;

    }else{
        PDDL_ERR(s->err, "Only conjuctive goal specifications are supported."
                 " (Goal contains %s.)", pddlFmTypeName(c->type));
        s->goal_is_unreachable = 1;
        return -2;
    }
}

static void setGoal(pddl_lifted_search_t *s)
{
    pddlISetEmpty(&s->goal);
    pddlFmTraverse(s->pddl->goal, _setGoal, NULL, s);
}

static pddl_state_id_t insertInitState(pddl_lifted_search_t *s)
{
    const pddl_t *pddl = s->pddl;
    pddl_list_t *item;
    PDDL_ISET(init);
    PDDL_LIST_FOR_EACH(&pddl->init->part, item){
        const pddl_fm_t *c = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);
        if (c->type == PDDL_FM_ATOM){
            const pddl_fm_atom_t *a = PDDL_FM_CAST(c, atom);
            const pddl_ground_atom_t *ga;
            if (pddlPredIsStatic(&pddl->pred.pred[a->pred])){
                pddlStripsMakerAddStaticAtom(&s->strips, a, NULL, NULL);
            }else{
                ga = pddlStripsMakerAddAtom(&s->strips, a, NULL, NULL);
                pddlISetAdd(&init, ga->id);
            }

        }else if (c->type == PDDL_FM_ASSIGN){
            const pddl_fm_func_op_t *ass = PDDL_FM_CAST(c, func_op);
            ASSERT(ass->fvalue == NULL);
            ASSERT(ass->lvalue != NULL);
            ASSERT(pddlFmAtomIsGrounded(ass->lvalue));
            pddlStripsMakerAddFunc(&s->strips, ass, NULL, NULL);
        }
    }

    pddl_state_id_t sid;
    sid = pddlStripsStateSpaceInsert(&s->state_space, &init);
    pddlISetFree(&init);

    return sid;
}

static int isGoal(const pddl_lifted_search_t *s)
{
    return pddlISetIsSubset(&s->goal, &s->cur_node.state);
}


static void applyAction(pddl_lifted_search_t *s,
                        const pddl_action_t *action,
                        const pddl_obj_id_t *args,
                        int *args_id,
                        int *cost)
{
    *args_id = -1;
    PDDL_ISET(add_eff);
    PDDL_ISET(del_eff);
    pddlStripsMakerActionEffInState(&s->strips, action, args,
                                    &s->cur_node.state,
                                    &add_eff, &del_eff, cost);
    if (!s->pddl->metric)
        *cost = 1;

    if (pddlISetSize(&add_eff) > 0 || pddlISetSize(&del_eff) > 0){
        const pddl_ground_action_args_t *a;
        a = pddlStripsMakerAddAction(&s->strips, action->id, 0, args, NULL);
        *args_id = a->id;
        pddlISetMinus2(&s->next_node.state, &s->cur_node.state, &del_eff);
        pddlISetUnion(&s->next_node.state, &add_eff);
    }

    pddlISetFree(&add_eff);
    pddlISetFree(&del_eff);
}

static void addPlanOp(pddl_lifted_search_t *s, int op_id)
{
    pddl_lifted_plan_t *plan = &s->plan;
    if (plan->plan_alloc == plan->plan_len){
        if (plan->plan_alloc == 0)
            plan->plan_alloc = 2;
        plan->plan_alloc *= 2;
        plan->plan = REALLOC_ARR(plan->plan, char *, plan->plan_alloc);
    }

    const pddl_ground_action_args_t *aargs;
    aargs = pddlStripsMakerActionArgs(&s->strips, op_id);
    const pddl_action_t *action = s->pddl->action.action + aargs->action_id;

    static int maxlen = 512;
    char name[maxlen + 1];
    int len = snprintf(name, maxlen, "%s", action->name);
    for (int i = 0; i < action->param.param_size; ++i){
        len += snprintf(name + len, maxlen - len, " %s",
                        s->pddl->obj.obj[aargs->arg[i]].name);
    }
    name[maxlen] = 0;
    plan->plan[plan->plan_len++] = STRDUP(name);
}

static void extractPlan(pddl_lifted_search_t *s,
                        pddl_state_id_t goal_state_id)
{
    pddlStripsStateSpaceGetNoState(&s->state_space, goal_state_id,
                                   &s->cur_node);
    s->plan.plan_cost = s->cur_node.g_value;

    pddl_state_id_t state_id = goal_state_id;
    while (state_id != 0){
        pddlStripsStateSpaceGetNoState(&s->state_space, state_id,
                                       &s->cur_node);
        addPlanOp(s, s->cur_node.op_id);
        state_id = s->cur_node.parent_id;
    }

    for (int i = 0; i < s->plan.plan_len / 2; ++i){
        int j = s->plan.plan_len - 1 - i;
        char *tmp;
        PDDL_SWAP(s->plan.plan[i], s->plan.plan[j], tmp);
    }
}

pddl_lifted_search_t *pddlLiftedSearchNew(const pddl_lifted_search_config_t *cfg,
                                          pddl_err_t *err)
{
    switch (cfg->alg){
        case PDDL_LIFTED_SEARCH_ASTAR:
            return bfsNew(cfg, 1, 1, 0, "Lifted A*", err);

        case PDDL_LIFTED_SEARCH_LAZY:
            return bfsNew(cfg, 0, 1, 1, "Lifted Lazy", err);

        case PDDL_LIFTED_SEARCH_GBFS:
            return bfsNew(cfg, 0, 1, 0, "Lifted GBFS", err);

        default:
            ERR_RET(err, NULL, "Unkown algorithm %d", cfg->alg);
    }
}

const pddl_lifted_plan_t *pddlLiftedSearchPlan(const pddl_lifted_search_t *s)
{
    return &s->plan;
}

void pddlLiftedSearchPlanPrint(const pddl_lifted_search_t *s, FILE *fout)
{
    const pddl_lifted_plan_t *plan = &s->plan;
    fprintf(fout, ";; Cost: %d\n", plan->plan_cost);
    fprintf(fout, ";; Length: %d\n", plan->plan_len);
    for (int i = 0; i < plan->plan_len; ++i){
        fprintf(fout, "(%s)\n", plan->plan[i]);
    }
}
