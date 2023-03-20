/***
 * Copyright (c)2022 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

#include "internal.h"
#include "_heur.h"
#include "pddl/heur.h"
#include "pddl/hmax.h"
#include "pddl/hfunc.h"

typedef struct heur heur_t;

struct task {
    int id;
    heur_t *heur;
    pddl_iset_t pruned_op;
    pddl_heur_t *h;

    pddl_htable_key_t hkey;
    pddl_list_t htable;
};
typedef struct task task_t;

struct heur {
    pddl_heur_t heur;
    pddl_heur_op_mutex_config_t cfg;
    pddl_fdr_t fdr; /*!< Input (original) FDR task */
    pddl_mutex_pairs_t mutex; /*!< Mutexes in the input task */
    pddl_err_t *err;
    pddl_iset_t *op_mutex; /*!< Map from op o to ops that form op-mutex with o */
    task_t **task;
    int task_size;
    int task_alloc;
    pddl_htable_t *htable; /*!< Hash table of tasks */
    pddl_extarr_t *state_to_task;
};

static pddl_htable_key_t prunedOpHash(const pddl_iset_t *pruned_op)
{
    if (pddlISetSize(pruned_op) == 0)
        return 0;
    return pddlCityHash_64(pruned_op->s, sizeof(int) * pruned_op->size);
}

static pddl_htable_key_t taskHKey(const pddl_list_t *k, void *_)
{
    const task_t *task;
    task = PDDL_LIST_ENTRY(k, task_t, htable);
    return task->hkey;
}

static int taskEq(const pddl_list_t *k1, const pddl_list_t *k2, void *_)
{
    const task_t *t1, *t2;
    t1 = PDDL_LIST_ENTRY(k1, task_t, htable);
    t2 = PDDL_LIST_ENTRY(k2, task_t, htable);
    return pddlISetEq(&t1->pruned_op, &t2->pruned_op);
}
    
static task_t *_taskNew(heur_t *heur,
                        const pddl_iset_t *pruned_op,
                        pddl_htable_key_t hkey)
{
    LOG(heur->err, "Creating new task pruned with %d operators",
        pddlISetSize(pruned_op));
    task_t *t;
    t = ZALLOC(task_t);
    t->heur = heur;
    pddlListInit(&t->htable);
    pddlISetUnion(&t->pruned_op, pruned_op);
    t->hkey = hkey;

    if (heur->task_size == heur->task_alloc){
        heur->task_alloc *= 2;
        heur->task = REALLOC_ARR(heur->task, task_t *, heur->task_alloc);
    }
    heur->task[heur->task_size] = t;
    t->id = heur->task_size++;
    return t;
}

static task_t *taskNew(heur_t *heur, const pddl_iset_t *pruned_op)
{
    if (heur->htable == NULL)
        return _taskNew(heur, pruned_op, prunedOpHash(pruned_op));

    task_t task;
    ZEROIZE(&task);
    task.pruned_op = *pruned_op;
    task.hkey = prunedOpHash(pruned_op);

    task_t *out = NULL;
    pddl_list_t *found;
    if ((found = pddlHTableFind(heur->htable, &task.htable)) == NULL){
        out = _taskNew(heur, pruned_op, task.hkey);
        pddlHTableInsertUnique(heur->htable, &out->htable);

    }else{
        out = PDDL_LIST_ENTRY(found, task_t, htable);
    }
    return out;
}

static void taskDel(task_t *task)
{
    pddlISetFree(&task->pruned_op);
    if (task->h != NULL)
        pddlHeurDel(task->h);
    FREE(task);
}

static int taskHeurEstimate(task_t *task,
                            const pddl_fdr_state_space_node_t *node,
                            const pddl_fdr_state_space_t *state_space)
{
    if (task->h == NULL){
        pddl_fdr_t fdr;
        pddlFDRInitCopy(&fdr, &task->heur->fdr);
        if (pddlISetSize(&task->pruned_op) > 0)
            pddlFDRReduce(&fdr, NULL, NULL, &task->pruned_op);

        pddl_hmax_t hmax;
        pddlHMaxInit(&hmax, &fdr);
        if (pddlHMax(&hmax, node->state, &fdr.var) == PDDL_COST_DEAD_END){
            pddlHMaxFree(&hmax);
            pddlFDRFree(&fdr);
            return PDDL_COST_DEAD_END;
        }
        pddlHMaxFree(&hmax);

        // TODO: pddlFDRSetInit()
        memcpy(fdr.init, node->state, sizeof(int) * fdr.var.var_size);

        pddl_mg_strips_t mg_strips;
        pddlMGStripsInitFDR(&mg_strips, &fdr);

        pddl_heur_config_t hcfg = *task->heur->cfg.cfg;
        hcfg.fdr = &fdr;
        hcfg.mg_strips = &mg_strips;
        hcfg.mutex = &task->heur->mutex;
        task->h = pddlHeur(&hcfg, task->heur->err);

        /*
        //pddlFDRStatePoolGet(&state_space->state_pool, node->id, fdr.init);
        // TODO: Generalize for any heuristic
        pddl_hpot_config_t hcfg = PDDL_HPOT_CONFIG_INIT;
        hcfg.fdr = &fdr;
        pddl_hpot_config_opt_state_t hcfg_state = PDDL_HPOT_CONFIG_OPT_STATE_INIT;
        hcfg_state.fdr_state = node->state;
        PDDL_HPOT_CONFIG_ADD(&hcfg, &hcfg_state);
        //task->h = pddlHeurPot(&fdr, &hcfg, task->heur->err);
        task->h = pddlHeurPot(&hcfg, NULL);
        //task->h = pddlHeurLMCut(&fdr, NULL);
        //task->h = pddlHeurHFF(&fdr, NULL);
        */

        pddlMGStripsFree(&mg_strips);
        pddlFDRFree(&fdr);
    }

    if (task->h == NULL)
        return PDDL_COST_DEAD_END;
    return pddlHeurEstimate(task->h, node, state_space);
}

static void heurDel(pddl_heur_t *_h)
{
    heur_t *h = pddl_container_of(_h, heur_t, heur);
    for (int op_id = 0; op_id < h->fdr.op.op_size; ++op_id)
        pddlISetFree(h->op_mutex + op_id);
    if (h->op_mutex != NULL)
        FREE(h->op_mutex);

    if (h->htable != NULL)
        pddlHTableDel(h->htable);
    pddlExtArrDel(h->state_to_task);

    for (int ti = 0; ti < h->task_size; ++ti)
        taskDel(h->task[ti]);
    if (h->task != NULL)
        FREE(h->task);

    pddlFDRFree(&h->fdr);
    pddlMutexPairsFree(&h->mutex);
    _pddlHeurFree(&h->heur);
    FREE(h);
}

static int heurEstimate(pddl_heur_t *_h,
                        const pddl_fdr_state_space_node_t *node,
                        const pddl_fdr_state_space_t *state_space)
{
    heur_t *h = pddl_container_of(_h, heur_t, heur);
    task_t **t = (task_t **)pddlExtArrGet(h->state_to_task, node->id);
    if (*t == NULL){
        if (node->parent_id == PDDL_NO_STATE_ID){
            PDDL_ISET(empty);
            *t = taskNew(h, &empty);
        }else{
            ASSERT(node->op_id >= 0);
            const pddl_iset_t *opm = h->op_mutex + node->op_id;
            task_t **tpar = (task_t **)pddlExtArrGet(h->state_to_task,
                                                     node->parent_id);
            if (pddlISetIsSubset(opm, &(*tpar)->pruned_op)){
                *t = *tpar;

            }else{
                PDDL_ISET(prune);
                pddlISetUnion2(&prune, &(*tpar)->pruned_op, opm);
                ASSERT(pddlISetSize(&prune) > pddlISetSize(&(*tpar)->pruned_op));
                float ratio;
                if (pddlISetSize(&(*tpar)->pruned_op) == 0){
                    ratio = 1E10;
                }else{
                    ratio = (float)pddlISetSize(&prune)
                                / (float)pddlISetSize(&(*tpar)->pruned_op);
                }
                if (ratio < 2.){
                    *t = *tpar;
                }else{
                    *t = taskNew(h, &prune);
                }
                pddlISetFree(&prune);
            }
        }
    }

    return taskHeurEstimate(*t, node, state_space);
}

pddl_heur_t *pddlHeurOpMutex(const pddl_fdr_t *fdr,
                             const pddl_mutex_pairs_t *mutex,
                             const pddl_heur_op_mutex_config_t *cfg,
                             pddl_err_t *err)
{
    CTX(err, "heur_op_mutex", "hOPM");
    heur_t *h = ZALLOC(heur_t);
    h->cfg = *cfg;
    pddlFDRInitCopy(&h->fdr, fdr);
    LOG(err, "FDR task copied");
    pddlMutexPairsInitCopy(&h->mutex, mutex);
    h->err = err;

    h->op_mutex = CALLOC_ARR(pddl_iset_t, h->fdr.op.op_size);
    ASSERT(cfg->op_mutex != NULL);
    pddlOpMutexPairsGenMapOpToOpSet(cfg->op_mutex, NULL, h->op_mutex);
    LOG(err, "Op-mutexes stored");

    h->task_size = 0;
    h->task_alloc = 8;
    h->task = ALLOC_ARR(task_t *, h->task_alloc);

    h->htable = pddlHTableNew(taskHKey, taskEq, NULL);

    task_t *arrinit = NULL;
    h->state_to_task = pddlExtArrNew(sizeof(arrinit), NULL, &arrinit);

    _pddlHeurInit(&h->heur, heurDel, heurEstimate);

    CTXEND(err);
    return &h->heur;
}
