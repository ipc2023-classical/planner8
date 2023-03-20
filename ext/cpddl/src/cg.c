/***
 * cpddl
 * -------
 * Copyright (c)2019 Daniel Fiser <danfis@danfis.cz>,
 * Agent Technology Center, Department of Computer Science,
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
#include "pddl/pairheap.h"
#include "pddl/cg.h"
#include "pddl/scc.h"

#define GOAL_BONUS 100000

static void collectEdges(const pddl_iset_t *pre,
                         const pddl_iset_t *eff,
                         const pddl_fdr_vars_t *vars,
                         int eff_eff_edges,
                         int *value)
{
    int eff_var;
    PDDL_ISET_FOR_EACH(eff, eff_var){
        int pre_var;
        PDDL_ISET_FOR_EACH(pre, pre_var){
            if (pre_var == eff_var)
                continue;
            value[pre_var * vars->var_size + eff_var] += 1;
        }
        if (eff_eff_edges){
            int eff_var2;
            PDDL_ISET_FOR_EACH(eff, eff_var2){
                if (eff_var2 == eff_var)
                    continue;
                value[eff_var2 * vars->var_size + eff_var] += 1;
                value[eff_var * vars->var_size + eff_var2] += 1;
            }
        }
    }
}

void pddlCGInit(pddl_cg_t *cg,
                const pddl_fdr_vars_t *vars,
                const pddl_fdr_ops_t *ops,
                int eff_eff_edges)
{
    ZEROIZE(cg);

    int *value = CALLOC_ARR(int, vars->var_size * vars->var_size);
    PDDL_ISET(pre);
    PDDL_ISET(eff);
    for (int oi = 0; oi < ops->op_size; ++oi){
        const pddl_fdr_op_t *op = ops->op[oi];

        pddlISetEmpty(&pre);
        pddlISetEmpty(&eff);

        for (int prei = 0; prei < op->pre.fact_size; ++prei)
            pddlISetAdd(&pre, op->pre.fact[prei].var);
        for (int effi = 0; effi < op->eff.fact_size; ++effi)
            pddlISetAdd(&eff, op->eff.fact[effi].var);
        for (int cei = 0; cei < op->cond_eff_size; ++cei){
            const pddl_fdr_op_cond_eff_t *ce = op->cond_eff + cei;
            for (int prei = 0; prei < ce->pre.fact_size; ++prei)
                pddlISetAdd(&pre, ce->pre.fact[prei].var);
            for (int effi = 0; effi < ce->eff.fact_size; ++effi)
                pddlISetAdd(&eff, ce->eff.fact[effi].var);
        }
        collectEdges(&pre, &eff, vars, eff_eff_edges, value);
    }
    pddlISetFree(&pre);
    pddlISetFree(&eff);

    cg->node_size = vars->var_size;
    cg->node = CALLOC_ARR(pddl_cg_node_t, cg->node_size);
    for (int v1 = 0; v1 < vars->var_size; ++v1){
        pddl_cg_node_t *n1 = cg->node + v1;
        for (int v2 = 0; v2 < vars->var_size; ++v2){
            n1->fw_size += (value[v1 * vars->var_size + v2] > 0 ? 1 : 0);
            n1->bw_size += (value[v2 * vars->var_size + v1] > 0 ? 1 : 0);
        }

        int ins_fw = 0;
        int ins_bw = 0;
        n1->fw = CALLOC_ARR(pddl_cg_edge_t, n1->fw_size);
        n1->bw = CALLOC_ARR(pddl_cg_edge_t, n1->bw_size);
        for (int v2 = 0; v2 < vars->var_size; ++v2){
            if (value[v1 * vars->var_size + v2] > 0){
                n1->fw[ins_fw].value = value[v1 * vars->var_size + v2];
                n1->fw[ins_fw++].end = v2;
            }
            if (value[v2 * vars->var_size + v1] > 0){
                n1->bw[ins_bw].value = value[v2 * vars->var_size + v1];
                n1->bw[ins_bw++].end = v2;
            }
        }
    }

    FREE(value);
}

void pddlCGInitCopy(pddl_cg_t *cg, const pddl_cg_t *cg_in)
{
    ZEROIZE(cg);
    cg->node_size = cg_in->node_size;
    cg->node = CALLOC_ARR(pddl_cg_node_t, cg->node_size);
    for (int node_id = 0; node_id < cg->node_size; ++node_id){
        pddl_cg_node_t *n = cg->node + node_id;
        const pddl_cg_node_t *m = cg_in->node + node_id;
        n->fw_size = m->fw_size;
        n->fw = ALLOC_ARR(pddl_cg_edge_t, n->fw_size);
        memcpy(n->fw, m->fw, sizeof(pddl_cg_edge_t) * n->fw_size);
        n->bw_size = m->bw_size;
        n->bw = ALLOC_ARR(pddl_cg_edge_t, n->bw_size);
        memcpy(n->bw, m->bw, sizeof(pddl_cg_edge_t) * n->bw_size);
    }
}

void pddlCGInitProjectToVars(pddl_cg_t *dst,
                             const pddl_cg_t *src,
                             const pddl_iset_t *vars_set)
{
    pddlCGInitCopy(dst, src);

    int *vars = CALLOC_ARR(int, src->node_size);
    int var;
    PDDL_ISET_FOR_EACH(vars_set, var)
        vars[var] = 1;

    for (int ni = 0; ni < dst->node_size; ++ni){
        pddl_cg_node_t *node = dst->node + ni;
        if (!vars[ni]){
            if (node->fw != NULL)
                FREE(node->fw);
            if (node->bw != NULL)
                FREE(node->bw);
            node->fw_size = 0;
            node->fw = NULL;
            node->bw_size = 0;
            node->bw = NULL;
            continue;
        }

        int ins = 0;
        for (int ei = 0; ei < node->fw_size; ++ei){
            if (vars[node->fw[ei].end])
                node->fw[ins++] = node->fw[ei];
        }
        node->fw_size = ins;

        ins = 0;
        for (int ei = 0; ei < node->bw_size; ++ei){
            if (vars[node->bw[ei].end])
                node->bw[ins++] = node->bw[ei];
        }
        node->bw_size = ins;
    }

    if (vars != NULL)
        FREE(vars);
}

void pddlCGInitProjectToBlackVars(pddl_cg_t *dst,
                                  const pddl_cg_t *src,
                                  const pddl_fdr_vars_t *vars)
{
    PDDL_ISET(rm_vars);
    for (int i = 0; i < vars->var_size; ++i){
        if (vars->var[i].is_black)
            pddlISetAdd(&rm_vars, i);
    }
    pddlCGInitProjectToVars(dst, src, &rm_vars);
    pddlISetFree(&rm_vars);
}

void pddlCGFree(pddl_cg_t *cg)
{
    for (int n = 0; n < cg->node_size; ++n){
        pddl_cg_node_t *node = cg->node + n;
        if (node->fw != NULL)
            FREE(node->fw);
        if (node->bw != NULL)
            FREE(node->bw);
    }
    if (cg->node != NULL)
        FREE(cg->node);
}

static void markBackwardReachableVarsDFS(const pddl_cg_t *cg,
                                         int var,
                                         int *important)
{
    for (int i = 0; i < cg->node[var].bw_size; ++i){
        int w = cg->node[var].bw[i].end;
        if (!important[w]){
            important[w] = 1;
            markBackwardReachableVarsDFS(cg, w, important);
        }
    }
}

void pddlCGMarkBackwardReachableVars(const pddl_cg_t *cg,
                                     const pddl_fdr_part_state_t *goal,
                                     int *important_vars)
{
    ZEROIZE_ARR(important_vars, cg->node_size);

    for (int fi = 0; fi < goal->fact_size; ++fi){
        int var = goal->fact[fi].var;
        if (!important_vars[var]){
            important_vars[var] = 1;
            markBackwardReachableVarsDFS(cg, var, important_vars);
        }
    }
}

struct order_var {
    pddl_pairheap_node_t heap;
    int var; /*!< ID of the variable */
    int w; /*!< Incoming weight */
    int ordered; /*!< True if the variable was already ordered */
    int scc_id; /*!< ID of the strongly connected component */
    int is_goal; /*!< True if the variable is goal variable */
};
typedef struct order_var order_var_t;

static int heapLT(const pddl_pairheap_node_t *a,
                  const pddl_pairheap_node_t *b, void *_)
{
    order_var_t *v1 = pddl_container_of(a, order_var_t, heap);
    order_var_t *v2 = pddl_container_of(b, order_var_t, heap);
    if (v1->scc_id == v2->scc_id){
        if (v1->w == v2->w)
            return v1->var < v2->var;
        return v1->w < v2->w;
    }
    return v1->scc_id > v2->scc_id;
}

static void orderVarInit(order_var_t *order_var,
                         const pddl_cg_t *cg,
                         const pddl_fdr_part_state_t *goal)
{
    for (int var_id = 0; var_id < cg->node_size; ++var_id){
        order_var_t *v = order_var + var_id;
        v->var = var_id;
        v->w = 0;
        v->ordered = 0;
        v->scc_id = -1;
        v->is_goal = 0;
    }

    if (goal != NULL){
        for (int i = 0; i < goal->fact_size; ++i)
            order_var[goal->fact[i].var].is_goal = 1;
    }

    // Compute strongly connected components and mark variables with IDs of
    // the found components
    pddl_scc_graph_t scc_graph;
    pddlSCCGraphInit(&scc_graph, cg->node_size);
    for (int v = 0; v < cg->node_size; ++v){
        for (int ei = 0; ei < cg->node[v].fw_size; ++ei)
            pddlSCCGraphAddEdge(&scc_graph, v, cg->node[v].fw[ei].end);
    }

    pddl_scc_t scc;
    pddlSCC(&scc, &scc_graph);
    for (int ci = 0; ci < scc.comp_size; ++ci){
        int var_id;
        PDDL_ISET_FOR_EACH(scc.comp + ci, var_id)
            order_var[var_id].scc_id = ci;
    }

    pddlSCCFree(&scc);
    pddlSCCGraphFree(&scc_graph);

    // Set weights of variables by summing costs of incoming edges within
    // each components
    for (int var_id = 0; var_id < cg->node_size; ++var_id){
        const pddl_cg_node_t *n = cg->node + var_id;
        for (int ei = 0; ei < n->fw_size; ++ei){
            const pddl_cg_edge_t *e = n->fw + ei;
            if (order_var[var_id].scc_id == order_var[e->end].scc_id){
                order_var[e->end].w += e->value;
                if (order_var[e->end].is_goal){
                    order_var[e->end].w += GOAL_BONUS;
                }
            }
        }
    }
}

static void removeVar(order_var_t *order_var,
                      int var_id,
                      pddl_pairheap_t *heap,
                      const pddl_cg_t *cg)
{
    const pddl_cg_node_t *n = cg->node + var_id;
    for (int ei = 0; ei < n->fw_size; ++ei){
        const pddl_cg_edge_t *e = n->fw + ei;
        if (order_var[var_id].scc_id == order_var[e->end].scc_id
                && !order_var[e->end].ordered){
            order_var[e->end].w -= e->value;
            //order_var[e->end].w -= order_var[var_id].w;
            pddlPairHeapDecreaseKey(heap, &order_var[e->end].heap);
        }
    }
}

/*
static void reverseArr(int *arr, int size)
{
    int len = size / 2;
    for (int i = 0; i < len; ++i){
        int tmp;
        PDDL_SWAP(arr[i], arr[size - 1 - i], tmp);
    }
}
*/

static void moveUnimportantVarsBack(const pddl_cg_t *cg,
                                    const pddl_fdr_part_state_t *goal,
                                    int *var_ordering)
{
    int *old_order = ALLOC_ARR(int, cg->node_size);
    memcpy(old_order, var_ordering, sizeof(int) * cg->node_size);

    int *important = CALLOC_ARR(int, cg->node_size);
    if (goal == NULL){
        for (int v = 0; v < cg->node_size; ++v)
            important[v] = 1;
    }else{
        pddlCGMarkBackwardReachableVars(cg, goal, important);
    }

    int ins = 0;
    for (int i = 0; i < cg->node_size; ++i){
        if (important[old_order[i]])
            var_ordering[ins++] = old_order[i];
    }
    for (int v = 0; v < cg->node_size; ++v){
        if (!important[v])
            var_ordering[ins++] = v;
    }
    FREE(important);
    FREE(old_order);
}

void pddlCGVarOrdering(const pddl_cg_t *cg,
                       const pddl_fdr_part_state_t *goal,
                       int *var_ordering)
{
    if (cg->node_size == 1){
        var_ordering[0] = 0;
        return;
    }

    order_var_t *order_var = ALLOC_ARR(order_var_t, cg->node_size);
    orderVarInit(order_var, cg, goal);

    pddl_pairheap_t *heap = pddlPairHeapNew(heapLT, NULL);
    for (int var_id = 0; var_id < cg->node_size; ++var_id)
        pddlPairHeapAdd(heap, &order_var[var_id].heap);

    int ins = 0;
    for (; !pddlPairHeapEmpty(heap); ++ins){
        pddl_pairheap_node_t *hnode = pddlPairHeapExtractMin(heap);
        order_var_t *minvar = pddl_container_of(hnode, order_var_t, heap);
        minvar->ordered = 1;
        var_ordering[ins] = minvar->var;
        removeVar(order_var, minvar->var, heap, cg);
    }
    ASSERT_RUNTIME(ins == cg->node_size);
    //reverseArr(var_ordering, cg->node_size);
    moveUnimportantVarsBack(cg, goal, var_ordering);

    pddlPairHeapDel(heap);
    FREE(order_var);
}

int pddlCGIsAcyclic(const pddl_cg_t *cg)
{
    int is_acyclic = 0;

    pddl_scc_graph_t scc_graph;
    pddlSCCGraphInit(&scc_graph, cg->node_size);
    for (int v = 0; v < cg->node_size; ++v){
        for (int ei = 0; ei < cg->node[v].fw_size; ++ei)
            pddlSCCGraphAddEdge(&scc_graph, v, cg->node[v].fw[ei].end);
    }

    pddl_scc_t scc;
    pddlSCC(&scc, &scc_graph);
    is_acyclic = scc.comp_size == cg->node_size;
    pddlSCCFree(&scc);
    pddlSCCGraphFree(&scc_graph);

    return is_acyclic;
}

void pddlCGPrintDebug(const pddl_cg_t *cg, FILE *fout)
{
    for (int ni = 0; ni < cg->node_size; ++ni){
        if (cg->node[ni].fw_size == 0)
            continue;
        fprintf(fout, "%d ->", ni);
        for (int ei = 0; ei < cg->node[ni].fw_size; ++ei){
            fprintf(fout, " %d", cg->node[ni].fw[ei].end);
        }
        fprintf(fout, "\n");
    }
}

char *pddlCGAsDot(const pddl_cg_t *cg, size_t *buf_size)
{
    *buf_size = 1024 * 1024;
    char *buf = ALLOC_ARR(char, *buf_size);
    int cur = 0;

    cur += sprintf(buf + cur, "digraph {\n");
    //cur += sprintf(buf + cur, "  rankdir=LR;\n");
    for (int node_i = 0; node_i < cg->node_size; ++node_i){
        const pddl_cg_node_t *node = cg->node + node_i;
        for (int edge_i = 0; edge_i < node->fw_size; ++edge_i){
            const pddl_cg_edge_t *edge = node->fw + edge_i;
            cur += sprintf(buf + cur, "  v%d -> v%d[label=\"%d\"];\n",
                           node_i, edge->end, edge->value);
        }
    }
    cur += sprintf(buf + cur, "}\n");
    buf[cur] = 0x0;

    *buf_size = cur + 1;
    buf = REALLOC_ARR(buf, char, *buf_size);
    return buf;
}

static char *pddlGraphEasy(const char *graph_easy_bin,
                           const char *input,
                           size_t *buflen)
{
    *buflen = 0;
    if (graph_easy_bin == NULL)
        graph_easy_bin = "/usr/bin/graph-easy";

    int pipein[2];
    int pipeout[2];
    if (pipe(pipein) != 0){
        perror("Pipe failed:");
        return NULL;
    }
    if (pipe(pipeout) != 0){
        perror("Pipe failed:");
        return NULL;
    }

    int pid = fork();
    if (pid == -1){
        perror("fork() failed");
        return NULL;

    }else if (pid == 0){
        // child process
        fflush(stdout);
        fflush(stderr);
        close(pipein[1]);
        dup2(pipein[0], STDIN_FILENO);
        close(pipein[0]);
        close(pipeout[0]);
        dup2(pipeout[1], STDOUT_FILENO);
        close(pipeout[1]);
        dup2(STDOUT_FILENO, STDERR_FILENO);
        exit(execl(graph_easy_bin, graph_easy_bin, "--from", "graphviz",
                   "--as", "boxart", NULL));

    }else{
        // parent process
        close(pipein[0]);
        close(pipeout[1]);

        // Write input to graph-easy
        size_t len = strlen(input);
        ssize_t wlen = 0;
        do {
            ssize_t w = write(pipein[1], input + wlen, sizeof(char) * (len - wlen));
            if (w < 0){
                perror("Could not write to pipe");
                close(pipeout[0]);
                close(pipein[1]);
                waitpid(pid, NULL, 0);
                return NULL;
            }
            wlen += w;
        } while (wlen != (ssize_t)len);
        close(pipein[1]);

        // Read output
        *buflen = 1024 * 1024;
        char *buf = ALLOC_ARR(char, *buflen);
        int cur = 0;
        while (1){
            if (*buflen - 1 - cur < 1024){
                *buflen *= 2;
                buf = REALLOC_ARR(buf, char, *buflen);
            }

            ssize_t ret = read(pipeout[0], buf + cur, *buflen - 1 - cur);
            if (ret <= 0)
                break;
            cur += ret;
            buf[cur] = 0x0;
        }
        close(pipeout[0]);
        *buflen = cur + 1;
        buf = REALLOC_ARR(buf, char, *buflen);

        waitpid(pid, NULL, 0);
        return buf;
    }

    return NULL;
}

void pddlCGPrintAsciiGraph(const pddl_cg_t *cg, FILE *out, pddl_err_t *err)
{
    size_t buf_size;
    char *buf = pddlCGAsDot(cg, &buf_size);
    if (buf == NULL){
        PDDL_INFO(err, "Could not print out causal graph");
        return;
    }

    size_t graph_size;
    char *graph = pddlGraphEasy(NULL, buf, &graph_size);

    size_t cur = 0;
    while (cur < graph_size){
        size_t from = cur;
        for (; cur < graph_size && graph[cur] != '\n'; ++cur);
        graph[cur++] = 0x0;
        if (err != NULL)
            PDDL_INFO(err, "CG: %s", graph + from);
        if (out != NULL)
            fprintf(out, "CG: %s\n", graph + from);
    }

    if (buf != NULL)
        FREE(buf);
    if (graph != NULL)
        FREE(graph);
}
