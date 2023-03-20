/***
 * Copyright (c)2022 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

#include "internal.h"
#include "pddl/task.h"
#include "pddl/critical_path.h"

struct pddl_task {
    pddl_err_t *err;
    pddl_fdr_t fdr;
    int set_fdr;
    pddl_mg_strips_t mg_strips;
    int set_mg_strips;
    pddl_mutex_pairs_t mutex;
    int set_mutex;
    int set_mutex_hm;
};

pddl_task_t *pddlTaskNewFDR(const pddl_fdr_t *fdr, pddl_err_t *err)
{
    pddl_task_t *task = ZALLOC(pddl_task_t);
    task->err = err;
    pddlFDRInitCopy(&task->fdr, fdr);
    task->set_fdr = 1;
    return task;
}

pddl_task_t *pddlTaskClone(const pddl_task_t *task, pddl_err_t *err)
{
    pddl_task_t *out = ZALLOC(pddl_task_t);
    out->err = err;

    if (task->set_fdr){
        pddlFDRInitCopy(&out->fdr, &task->fdr);
        out->set_fdr = 1;
    }

    if (task->set_mg_strips){
        pddlMGStripsInitCopy(&out->mg_strips, &task->mg_strips);
        out->set_mg_strips = 1;
    }

    if (task->set_mutex){
        pddlMutexPairsInitCopy(&out->mutex, &task->mutex);
        out->set_mutex = 1;
        out->set_mutex_hm = task->set_mutex_hm;
    }

    return out;
}

void pddlTaskDel(pddl_task_t *task)
{
    if (task->set_mutex)
        pddlMutexPairsFree(&task->mutex);
    if (task->set_mg_strips)
        pddlMGStripsFree(&task->mg_strips);
    if (task->set_fdr)
        pddlFDRFree(&task->fdr);
    ZEROIZE(task);
}

pddl_err_t *pddlTaskErr(pddl_task_t *task)
{
    return task->err;
}

const pddl_fdr_t *pddlTaskFDR(pddl_task_t *task)
{
    if (task->set_fdr)
        return &task->fdr;
    return NULL;
}

const pddl_mg_strips_t *pddlTaskMGStrips(pddl_task_t *task)
{
    if (task->set_mg_strips)
        return &task->mg_strips;
    pddlMGStripsInitFDR(&task->mg_strips, &task->fdr);
    task->set_mg_strips = 1;
    return &task->mg_strips;
}

const pddl_mutex_pairs_t *pddlTaskHmMutex(pddl_task_t *task,
                                          int m,
                                          float time_limit,
                                          size_t excess_memory)
{
    if (task->set_mutex)
        return &task->mutex;

    pddlTaskMGStrips(task);

    pddlMutexPairsInitStrips(&task->mutex, &task->mg_strips.strips);
    pddlMutexPairsAddMGroups(&task->mutex, &task->mg_strips.mg);
    pddlHm(m, &task->mg_strips.strips, &task->mutex, NULL, NULL,
           time_limit, excess_memory, task->err);
    task->set_mutex = 1;
    task->set_mutex_hm = m;
    return &task->mutex;
}
