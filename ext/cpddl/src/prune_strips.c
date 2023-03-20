/***
 * cpddl
 * -------
 * Copyright (c)2021 Daniel Fiser <danfis@danfis.cz>,
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

#include "pddl/prune_strips.h"
#include "pddl/irrelevance.h"
#include "pddl/dtg.h"
#include "pddl/famgroup.h"
#include "pddl/critical_path.h"
#include "pddl/mg_strips.h"
#include "internal.h"

#define IRRELEVANCE 1

struct ctx {
    pddl_strips_t *strips;
    pddl_mgroups_t *mgroup;
    pddl_mutex_pairs_t *mutex;
    pddl_iset_t rm_op;
    pddl_iset_t rm_fact;
    pddl_err_t *err;
};
typedef struct ctx ctx_t;

struct prune_strips {
    int (*prune)(struct prune_strips *p, ctx_t *ctx);
    float time_limit_in_s;
    size_t excess_mem;
    pddl_list_t conn;
};
typedef struct prune_strips prune_strips_t;



static void applyPruneStrips(ctx_t *c)
{
    if (pddlISetSize(&c->rm_fact) == 0 && pddlISetSize(&c->rm_op) == 0)
        return;

    pddlStripsReduce(c->strips, &c->rm_fact, &c->rm_op);

    if (pddlISetSize(&c->rm_fact) > 0){
        if (c->mutex != NULL)
            pddlMutexPairsReduce(c->mutex, &c->rm_fact);

        if (c->mgroup != NULL){
            pddlMGroupsReduce(c->mgroup, &c->rm_fact);
            pddlMGroupsSetExactlyOne(c->mgroup, c->strips);
            pddlMGroupsSetGoal(c->mgroup, c->strips);
        }
    }

    pddlISetEmpty(&c->rm_fact);
    pddlISetEmpty(&c->rm_op);
}

static prune_strips_t *addPruneStrips(pddl_prune_strips_t *p,
                                      int (*fn)(prune_strips_t *p, ctx_t *ctx))
{
    prune_strips_t *prune = ALLOC(prune_strips_t);
    prune->prune = fn;
    pddlListInit(&prune->conn);
    pddlListAppend(&p->prune, &prune->conn);
    return prune;
}


void pddlPruneStripsInit(pddl_prune_strips_t *prune)
{
    ZEROIZE(prune);
    pddlListInit(&prune->prune);
    pddlListInit(&prune->conn);
}

void pddlPruneStripsFree(pddl_prune_strips_t *prune)
{
    while (!pddlListEmpty(&prune->prune)){
        pddl_list_t *item = pddlListNext(&prune->prune);
        pddlListDel(item);
        prune_strips_t *p = PDDL_LIST_ENTRY(item, prune_strips_t, conn);
        FREE(p);
    }

    pddlMGroupsFree(&prune->mgroup);
    pddlMutexPairsFree(&prune->mutex);
}

int pddlPruneStripsExecute(pddl_prune_strips_t *prune,
                           pddl_strips_t *strips,
                           pddl_mgroups_t *mgroups,
                           pddl_err_t *err)
{
    CTX(err, "strips_prune", "Prune");
    PDDL_INFO(err, "Start pruning. facts: %d, ops: %d",
              strips->fact.fact_size, strips->op.op_size);
    ctx_t ctx;
    ZEROIZE(&ctx);
    ctx.strips = strips;

    if (mgroups != NULL){
        pddlMGroupsInitCopy(&prune->mgroup, mgroups);
    }else{
        pddlMGroupsInitEmpty(&prune->mgroup);
    }
    ctx.mgroup = &prune->mgroup;

    pddlMutexPairsInitStrips(&prune->mutex, strips);
    ctx.mutex = &prune->mutex;

    ctx.err = err;

    pddl_list_t *item;
    PDDL_LIST_FOR_EACH(&prune->prune, item){
        prune_strips_t *p = PDDL_LIST_ENTRY(item, prune_strips_t, conn);
        if (p->prune(p, &ctx) != 0){
            pddlISetFree(&ctx.rm_fact);
            pddlISetFree(&ctx.rm_op);
            CTXEND(err);
            PDDL_TRACE_RET(err, -1);
        }
    }

    applyPruneStrips(&ctx);

    pddlISetFree(&ctx.rm_fact);
    pddlISetFree(&ctx.rm_op);
    PDDL_INFO(err, "DONE. facts: %d, ops: %d",
              strips->fact.fact_size, strips->op.op_size);
    CTXEND(err);
    return 0;
}


static int pruneIrrelevance(prune_strips_t *p, ctx_t *c)
{
    if (c->strips->has_cond_eff){
        PDDL_INFO(c->err, "irrelevance analysis disabled because the problem"
                   " has conditional effects.");
        return 0;
    }

    PDDL_ISET(irr_fact);
    PDDL_ISET(irr_op);
    PDDL_ISET(static_fact);
    if (pddlIrrelevanceAnalysis(c->strips, &irr_fact, &irr_op, &static_fact, c->err) != 0)
        PDDL_TRACE_RET(c->err, -1);
    pddlISetUnion(&c->rm_fact, &irr_fact);
    pddlISetUnion(&c->rm_op, &irr_op);

    pddlISetFree(&irr_fact);
    pddlISetFree(&irr_op);
    pddlISetFree(&static_fact);
    return 0;
}

void pddlPruneStripsAddIrrelevance(pddl_prune_strips_t *prune)
{
    addPruneStrips(prune, pruneIrrelevance);
}


static int pruneUnreachableInDTGs(prune_strips_t *p, ctx_t *c)
{
    if (c->strips->has_cond_eff){
        PDDL_INFO(c->err, "pruning unreachable facts in DTGs disabled because the problem"
                   " has conditional effects.");
        return 0;
    }

    if (c->mgroup != NULL && c->mgroup->mgroup_size == 0)
        return 0;
    applyPruneStrips(c);
    pddlUnreachableInMGroupsDTGs(c->strips, c->mgroup, &c->rm_fact, &c->rm_op, c->err);
    return 0;
}

void pddlPruneStripsAddUnreachableInDTGs(pddl_prune_strips_t *prune)
{
    addPruneStrips(prune, pruneUnreachableInDTGs);
}


static int pruneFAMGroupDeadEnd(prune_strips_t *p, ctx_t *c)
{
    if (c->mgroup != NULL && c->mgroup->mgroup_size == 0)
        return 0;
    int old_size = pddlISetSize(&c->rm_op);
    PDDL_INFO(c->err, "Pruning dead-end operators ...");
    pddlFAMGroupsDeadEndOps(c->mgroup, c->strips, &c->rm_op);
    PDDL_INFO(c->err, "Pruning dead-end operators done. Dead end ops: %d",
              pddlISetSize(&c->rm_op) - old_size);
    return 0;
}

void pddlPruneStripsAddFAMGroupDeadEnd(pddl_prune_strips_t *prune)
{
    addPruneStrips(prune, pruneFAMGroupDeadEnd);
}


static int pruneH2(prune_strips_t *p, ctx_t *c)
{
    if (c->strips->has_cond_eff){
        PDDL_INFO(c->err, "h^2 disabled because the problem has conditional effects.");
        return 0;
    }

    if (pddlH2(c->strips, c->mutex, &c->rm_fact, &c->rm_op,
               p->time_limit_in_s, c->err) != 0){
        PDDL_INFO(c->err, "h^2 fw failed.");
        PDDL_TRACE_RET(c->err, -1);
    }
    return 0;
}

void pddlPruneStripsAddH2(pddl_prune_strips_t *prune, float time_limit_in_s)
{
    prune_strips_t *p = addPruneStrips(prune, pruneH2);
    p->time_limit_in_s = time_limit_in_s;
}


static int pruneH2FwBw(prune_strips_t *p, ctx_t *c)
{
    if (c->strips->has_cond_eff){
        PDDL_INFO(c->err, "h^2 fw/bw disabled because the problem has"
                   " conditional effects.");
        return 0;
    }

    pddl_mg_strips_t mg_strips;
    pddlMGStripsInit(&mg_strips, c->strips, c->mgroup);
    if (pddlH2FwBw(&mg_strips.strips, &mg_strips.mg, c->mutex, &c->rm_fact, &c->rm_op,
                   p->time_limit_in_s, c->err) != 0){
        PDDL_INFO(c->err, "h^2 fw/bw failed.");
        PDDL_TRACE_RET(c->err, -1);
    }
    pddlMGStripsFree(&mg_strips);
    return 0;
}

void pddlPruneStripsAddH2FwBw(pddl_prune_strips_t *prune, float time_limit_in_s)
{
    prune_strips_t *p = addPruneStrips(prune, pruneH2FwBw);
    p->time_limit_in_s = time_limit_in_s;
}


static int pruneH3(prune_strips_t *p, ctx_t *c)
{
    if (c->strips->has_cond_eff){
        PDDL_INFO(c->err, "h^3 disabled because the problem has conditional effects.");
        return 0;
    }

    if (pddlH3(c->strips, c->mutex, &c->rm_fact, &c->rm_op,
               p->time_limit_in_s, p->excess_mem, c->err) != 0){
        PDDL_INFO(c->err, "h^3 fw failed.");
        PDDL_TRACE_RET(c->err, -1);
    }
    return 0;
}

void pddlPruneStripsAddH3(pddl_prune_strips_t *prune,
                          float time_limit_in_s,
                          size_t excess_mem)
{
    prune_strips_t *p = addPruneStrips(prune, pruneH3);
    p->time_limit_in_s = time_limit_in_s;
    p->excess_mem = excess_mem;
}


static int pruneDeduplicateOps(prune_strips_t *p, ctx_t *c)
{
    int num_ops = c->strips->op.op_size;
    pddlStripsOpsDeduplicate(&c->strips->op);
    PDDL_INFO(c->err, "Deduplication of operators removed %d operators",
              num_ops - c->strips->op.op_size);
    return 0;
}

void pddlPruneStripsAddDeduplicateOps(pddl_prune_strips_t *prune)
{
    addPruneStrips(prune, pruneDeduplicateOps);
}
