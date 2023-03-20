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

#include "internal.h"
#include "pddl/timer.h"
#include "pddl/op_mutex_infer.h"
#include "pddl/trans_system.h"
#include "pddl/trans_system_graph.h"

static void setMemLimit(size_t mem_in_mb)
{
    struct rlimit mem_limit;
    mem_limit.rlim_cur = mem_limit.rlim_max = mem_in_mb * 1024 * 1024;
    setrlimit(RLIMIT_AS, &mem_limit);
}

struct op_mutex_infer_op {
    pddl_iset_t start;
    pddl_iset_t end;
};
typedef struct op_mutex_infer_op op_mutex_infer_op_t;


static op_mutex_infer_op_t *opAlloc(const pddl_trans_system_t *ts, int num_ops)
{
    op_mutex_infer_op_t *ops;

    ops = CALLOC_ARR(op_mutex_infer_op_t, num_ops);
    for (int ltri = 0; ltri < ts->trans.trans_size; ++ltri){
        const pddl_labeled_transitions_t *ltr = ts->trans.trans + ltri;
        for (int tri = 0; tri < ltr->trans.trans_size; ++tri){
            int from = ltr->trans.trans[tri].from;
            int to = ltr->trans.trans[tri].to;
            int op;
            PDDL_ISET_FOR_EACH(&ltr->label->label, op){
                pddlISetAdd(&ops[op].start, from);
                pddlISetAdd(&ops[op].end, to);
            }
        }
    }

    return ops;
}

static void opFree(op_mutex_infer_op_t *ops, int num_ops)
{
    for (int o = 0; o < num_ops; ++o){
        pddlISetFree(&ops[o].start);
        pddlISetFree(&ops[o].end);
    }
    FREE(ops);
}

static int *reachabilityAlloc(const pddl_trans_system_t *ts)
{
    pddl_trans_system_graph_t graph;
    pddlTransSystemGraphInit(&graph, ts);

    pddl_iset_t *reach_state = CALLOC_ARR(pddl_iset_t, ts->num_states);
    pddlTransSystemGraphFwReachability(&graph, reach_state, 1);
    int *reach = CALLOC_ARR(int, ts->num_states * ts->num_states);
    for (int s = 0; s < ts->num_states; ++s){
        int from;
        PDDL_ISET_FOR_EACH(reach_state + s, from){
            reach[from * ts->num_states + s] = 1;
        }
    }
    pddlTransSystemGraphFree(&graph);
    for (int s = 0; s < ts->num_states; ++s)
        pddlISetFree(reach_state + s);
    FREE(reach_state);

    return reach;
}

static void reachabilityFree(int *reach)
{
    FREE(reach);
}

static int isOpMutex(const op_mutex_infer_op_t *ops,
                     int num_states,
                     const int *reach,
                     int o1,
                     int o2)
{
    int start, end;
    PDDL_ISET_FOR_EACH(&ops[o1].end, end){
        PDDL_ISET_FOR_EACH(&ops[o2].start, start){
            if (reach[end * num_states + start])
                return 0;
        }
    }
    PDDL_ISET_FOR_EACH(&ops[o2].end, end){
        PDDL_ISET_FOR_EACH(&ops[o1].start, start){
            if (reach[end * num_states + start])
                return 0;
        }
    }
    return 1;
}

static void addOpMutex(int o1, int o2, int fd, pddl_op_mutex_pairs_t *m)
{
    if (fd >= 0){
        int data[2] = { o1, o2 };
        ssize_t written = write(fd, data, sizeof(int) * 2);
        while (written != sizeof(int) * 2){
            void *buf = ((char *)data) + written;
            written += write(fd, buf, sizeof(int) * 2 - written);
        }
    }

    if (m != NULL){
        pddlOpMutexPairsAdd(m, o1, o2);
    }
}

static int opMutexInfer(const pddl_trans_systems_t *tss,
                        int ts_id,
                        int fd,
                        pddl_op_mutex_pairs_t *m,
                        pddl_err_t *err)
{
    const pddl_trans_system_t *ts = tss->ts[ts_id];
    int num_ops = tss->label.label_size;
    int dead_size = pddlISetSize(&tss->dead_labels);
    const pddl_iset_t *dead = &tss->dead_labels;
    int *reach = reachabilityAlloc(ts);
    op_mutex_infer_op_t *op = opAlloc(ts, num_ops);

    int dead1 = 0;
    for (int o1 = 0; o1 < num_ops; ++o1){
        if (dead1 < dead_size && pddlISetGet(dead, dead1) == o1){
            ++dead1;
            continue;
        }
        int dead2 = dead1;
        for (int o2 = o1 + 1; o2 < num_ops; ++o2){
            if (dead2 < dead_size && pddlISetGet(dead, dead2) == o2){
                ++dead2;
                continue;
            }
            if (isOpMutex(op, ts->num_states, reach, o1, o2))
                addOpMutex(o1, o2, fd, m);
        }
    }

    opFree(op, num_ops);
    reachabilityFree(reach);
    return 0;
}

static int transformTransSystemAndFindOpMutexes(int fd,
                                                pddl_op_mutex_pairs_t *m,
                                                pddl_trans_systems_t *tss,
                                                const pddl_iset_t *ts_ids,
                                                int prune_dead_labels,
                                                pddl_err_t *err)
{
    int ts_last;

    pddl_timer_t timer;
    pddlTimerStart(&timer);
    if (pddlISetSize(ts_ids) == 1){
        ts_last = pddlISetGet(ts_ids, 0);

    }else{
        // Construct the merge
        int t1i = pddlISetGet(ts_ids, 0);
        int t2i = pddlISetGet(ts_ids, 1);
        ts_last = pddlTransSystemsMerge(tss, t1i, t2i);
        for (int i = 2; i < pddlISetSize(ts_ids); ++i){
            t1i = ts_last;
            t2i = pddlISetGet(ts_ids, i);
            int ts_next = pddlTransSystemsMerge(tss, t1i, t2i);
            pddlTransSystemsDelTransSystem(tss, ts_last);
            ts_last = ts_next;
        }
    }

    pddl_trans_system_abstr_map_t map;
    pddlTransSystemAbstrMapInit(&map, tss->ts[ts_last]->num_states);

    pddl_trans_system_graph_t graph;
    pddlTransSystemGraphInit(&graph, tss->ts[ts_last]);

    // Remove unreachable/dead-end states
    if (prune_dead_labels){
        int *dist = ALLOC_ARR(int, graph.num_states);
        pddlTransSystemGraphFwDist(&graph, dist);
        for (int i = 0; i < graph.num_states; ++i){
            if (dist[i] < 0)
                pddlTransSystemAbstrMapPruneState(&map, i);
        }
        pddlTransSystemGraphBwDist(&graph, dist);
        for (int i = 0; i < graph.num_states; ++i){
            if (dist[i] < 0)
                pddlTransSystemAbstrMapPruneState(&map, i);
        }
        FREE(dist);
    }

    // Condense strongly connected components
    pddl_set_iset_t comp;
    pddlSetISetInit(&comp);
    pddlTransSystemGraphFwSCC(&graph, &comp);
    for (int ci = 0; ci < pddlSetISetSize(&comp); ++ci){
        const pddl_iset_t *c = pddlSetISetGet(&comp, ci);
        if (pddlISetSize(c) > 1)
            pddlTransSystemAbstrMapCondense(&map, c);
    }
    pddlSetISetFree(&comp);

    pddlTransSystemAbstrMapFinalize(&map);
    pddlTransSystemsAbstract(tss, ts_last, &map);
    pddlTransSystemAbstrMapFree(&map);
    pddlTransSystemGraphFree(&graph);

    if (prune_dead_labels){
        pddlTransSystemsCollectDeadLabels(tss, ts_last);
        int dead_op;
        PDDL_ISET_FOR_EACH(&tss->dead_labels, dead_op){
            for (int op_id = 0; op_id < tss->label.label_size; ++op_id){
                if (dead_op != op_id)
                    addOpMutex(dead_op, op_id, fd, m);
            }
        }
    }

    if (tss->ts[ts_last]->num_states > 1){
        opMutexInfer(tss, ts_last, fd, m, err);
    }
    if (pddlISetSize(ts_ids) > 1){
        pddlTransSystemsDelTransSystem(tss, ts_last);
        pddlTransSystemsCleanDeletedTransSystems(tss);
    }
    return 0;
}

static void readMutexPairs(pddl_op_mutex_pairs_t *m, int fd_in)
{
    int buf[2];
    ssize_t readlen;

    while ((readlen = read(fd_in, buf, sizeof(int) * 2)) > 0){
        int remain = (sizeof(int) * 2) - readlen;
        while (remain > 0){
            ssize_t r = read(fd_in, buf, remain);
            if (r <= 0)
                return;
            remain -= r;
        }

        ASSERT(readlen == 2 * sizeof(int));
        pddlOpMutexPairsAdd(m, buf[0], buf[1]);
    }
}

static int findOpMutexesWithMemLimit(pddl_op_mutex_pairs_t *m,
                                     pddl_trans_systems_t *tss,
                                     size_t max_mem_in_mb,
                                     const pddl_iset_t *ts_ids,
                                     int prune_dead_labels,
                                     pddl_err_t *err)
{
    int fd[2];

    if (pipe(fd) < 0){
        perror("Error: Could not create a pipe:");
        return -1;
    }

    fflush(stderr);
    fflush(stdout);
    int pid = fork();
    if (pid < 0){
        perror("Error: Could not fork:");
        return -1;

    }else if (pid == 0){
        // child process
        setMemLimit(max_mem_in_mb);
        // close unused read end
        close(fd[0]);
        int r;
        r = transformTransSystemAndFindOpMutexes(fd[1], NULL, tss, ts_ids,
                                                 prune_dead_labels, err);
        close(fd[1]);
        exit(r);

    }else{
        // parent process
        // close unused write end
        close(fd[1]);
        readMutexPairs(m, fd[0]);

        int ret = 0;
        int wstatus;
        waitpid(pid, &wstatus, 0);
        if (WIFEXITED(wstatus)){
            if (WEXITSTATUS(wstatus) != 0){
                PDDL_ERR(err, "Inference of op-mutexes failed.");
                ret = -1;
            }
        }else if (WIFSIGNALED(wstatus)){
            int signum = WTERMSIG(wstatus);
            PDDL_ERR(err, "Inference of op-mutexes failed: received signal"
                     "'%s'", strsignal(signum));
            ret = -1;
        }else{
            PDDL_ERR(err, "Inference of op-mutexes failed for unknown reason");
            // TODO: analyase what happened!
            // TODO: Handle out of memory error printout in child!
            ret = -1;
        }

        close(fd[0]);
        return ret;
    }
}

static int findOpMutexesRec(pddl_op_mutex_pairs_t *m,
                            pddl_trans_systems_t *tss,
                            size_t max_mem_mb,
                            const pddl_iset_t *ts_ids,
                            int size,
                            int prune_dead_labels,
                            pddl_err_t *err)
{
    if (size == 0){
        if (max_mem_mb > 0){
            return findOpMutexesWithMemLimit(m, tss, max_mem_mb, ts_ids,
                                             prune_dead_labels, err);
        }
        return transformTransSystemAndFindOpMutexes(-1, m, tss, ts_ids,
                                                    prune_dead_labels, err);
    }

    int ret = 0;
    int tsi_from;
    if (ts_ids == NULL || pddlISetSize(ts_ids) == 0){
        tsi_from = 0;
    }else{
        tsi_from = pddlISetGet(ts_ids, pddlISetSize(ts_ids) - 1) + 1;
    }

    PDDL_ISET(ts_ids_next);
    if (ts_ids != NULL)
        pddlISetUnion(&ts_ids_next, ts_ids);
    for (int tsi = tsi_from; tsi < tss->ts_size; ++tsi){
        if (pddlISetSize(&tss->ts[tsi]->mgroup_ids) != 1)
            continue;
        pddlISetAdd(&ts_ids_next, tsi);
        ret = findOpMutexesRec(m, tss, max_mem_mb, &ts_ids_next, size - 1,
                               prune_dead_labels, err);
        if (ret != 0)
            break;
        pddlISetRm(&ts_ids_next, tsi);
    }
    pddlISetFree(&ts_ids_next);
    return ret;
}

int pddlOpMutexInferTransSystems(pddl_op_mutex_pairs_t *m,
                                 const pddl_mg_strips_t *mg_strips,
                                 const pddl_mutex_pairs_t *mutex,
                                 int merge_size,
                                 size_t max_mem_in_mb,
                                 int prune_dead_labels,
                                 pddl_err_t *err)
{
    CTX(err, "opm", "OPM");
    PDDL_INFO(err, "Computing op-mutex pairs from abstract transition systems."
              " merge-size: %d", merge_size);
    pddl_trans_systems_t tss;
    pddlTransSystemsInit(&tss, mg_strips, mutex);
    PDDL_INFO(err, "  Created %d atomic abstractions", tss.ts_size);
    if (prune_dead_labels)
        pddlTransSystemsCollectDeadLabelsFromAll(&tss);
    int ret = findOpMutexesRec(m, &tss, max_mem_in_mb, NULL, merge_size,
                               prune_dead_labels, err);
    pddlTransSystemsFree(&tss);
    PDDL_INFO(err, "Computing op-mutex pairs from abstract transition"
              " systems DONE. merge-size: %d, num-op-mutex-pairs: %d",
              merge_size, m->num_op_mutex_pairs);
    CTXEND(err);
    return ret;
}
