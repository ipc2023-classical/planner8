/***
 * cpddl
 * -------
 * Copyright (c)2021 Daniel Fiser <danfis@danfis.cz>,
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

#include "pddl/alloc.h"
#include "pddl/irrelevance.h"
#include "pddl/famgroup.h"
#include "pddl/critical_path.h"
#include "pddl/mg_strips.h"
#include "pddl/op_mutex_pair.h"
#include "pddl/op_mutex_infer.h"
#include "pddl/op_mutex_redundant.h"
#include "pddl/endomorphism.h"
#include "process_strips.h"
#include "print_to_file.h"

typedef int (*pddl_process_strips_execute_fn)(pddl_process_strips_t *prune,
                                            pddl_process_strips_step_t *step,
                                            pddl_err_t *err);
typedef void (*pddl_process_strips_free_fn)(pddl_process_strips_step_t *step);

struct pddl_process_strips_step {
    char *name;
    pddl_list_t conn;
    int can_reuse_rm_op_fact;
    int not_unreachable_or_dead_end;
    pddl_process_strips_execute_fn execute;
    pddl_process_strips_free_fn free;
};

struct pddl_process_strips_fixpoint {
    pddl_process_strips_step_t step;
    pddl_process_strips_t ps;
};
typedef struct pddl_process_strips_fixpoint pddl_process_strips_fixpoint_t;


struct pddl_process_strips_step_hm {
    pddl_process_strips_step_t step;
    float time_limit;
    size_t excess_memory;
};
typedef struct pddl_process_strips_step_hm pddl_process_strips_step_hm_t;

struct pddl_process_strips_step_op_mutex {
    pddl_process_strips_step_t step;
    int ts;
    int op_fact;
    int hm_op;
    int no_prune;
    pddl_op_mutex_redundant_config_t prune_cfg;
    int prune_method;
    float prune_time_limit;
    char *out;
};
typedef struct pddl_process_strips_step_op_mutex
            pddl_process_strips_step_op_mutex_t;

struct pddl_process_strips_step_endomorph {
    pddl_process_strips_step_t step;
    pddl_endomorphism_config_t cfg;
    int fdr;
    int mg_strips;
    int ts;
    int fdr_ts;
};
typedef struct pddl_process_strips_step_endomorph
            pddl_process_strips_step_endomorph_t;

void pddlProcessStripsInit(pddl_process_strips_t *prune)
{
    bzero(prune, sizeof(*prune));
    pddlListInit(&prune->steps);
}

void pddlProcessStripsFree(pddl_process_strips_t *prune)
{
    pddl_list_t *item, *tmp;
    PDDL_LIST_FOR_EACH_SAFE(&prune->steps, item, tmp){
        pddl_process_strips_step_t *step;
        step = PDDL_LIST_ENTRY(item, pddl_process_strips_step_t, conn);
        pddlListDel(&step->conn);
        step->free(step);
        if (step->name != NULL)
            PDDL_FREE(step->name);
        PDDL_FREE(step);
    }
    pddlISetFree(&prune->rm_op);
    pddlISetFree(&prune->rm_fact);
}

static int apply(pddl_process_strips_t *prune, pddl_err_t *err)
{
    if (pddlISetSize(&prune->rm_fact) > 0 || pddlISetSize(&prune->rm_op) > 0){
        PDDL_LOG(err, "Removing %{rm_facts}d facts, %{rm_operators}d operators",
                 pddlISetSize(&prune->rm_fact),
                 pddlISetSize(&prune->rm_op));
        pddlStripsReduce(prune->strips, &prune->rm_fact, &prune->rm_op);
        if (prune->mgroups != NULL && pddlISetSize(&prune->rm_fact) > 0){
            pddlMGroupsReduce(prune->mgroups, &prune->rm_fact);
            if (prune->rm_not_unreachable_or_dead_end){
                pddlMGroupsSetExactlyOne(prune->mgroups, prune->strips);
                pddlMGroupsSetGoal(prune->mgroups, prune->strips);
            }
        }
        if (prune->mutex != NULL && pddlISetSize(&prune->rm_fact) > 0)
            pddlMutexPairsReduce(prune->mutex, &prune->rm_fact);
        prune->removed_op += pddlISetSize(&prune->rm_op);
        prune->removed_fact += pddlISetSize(&prune->rm_fact);
        pddlISetEmpty(&prune->rm_op);
        pddlISetEmpty(&prune->rm_fact);
    }
    prune->rm_not_unreachable_or_dead_end = 0;
    return 0;
}

static int step(pddl_process_strips_t *prune,
                pddl_process_strips_step_t *step,
                pddl_err_t *err)
{
    if (!step->can_reuse_rm_op_fact)
        apply(prune, err);

    PDDL_CTX(err, "process_strips", step->name);
    int rm_fact = pddlISetSize(&prune->rm_fact);
    int rm_op = pddlISetSize(&prune->rm_op);
    if (step->execute(prune, step, err) != 0){
        PDDL_CTXEND(err);
        PDDL_TRACE_RET(err, -1);
    }
    PDDL_LOG(err, "Found new redundant: %{new_redundant_facts}d facts,"
             " %{new_redundant_ops}d operators",
             pddlISetSize(&prune->rm_fact) - rm_fact,
             pddlISetSize(&prune->rm_op) - rm_op);
    PDDL_LOG(err, "Found redundant so far: %{redundant_facts_overall}d facts,"
             " %{redundant_ops_overall}d operators",
             prune->removed_fact + pddlISetSize(&prune->rm_fact),
             prune->removed_op + pddlISetSize(&prune->rm_op));
    PDDL_CTXEND(err);

    if (step->not_unreachable_or_dead_end){
        prune->rm_not_unreachable_or_dead_end = 1;
        apply(prune, err);
    }
    return 0;
}

static int execute(pddl_process_strips_t *prune,
                   pddl_strips_t *strips,
                   pddl_mgroups_t *mgroups,
                   pddl_mutex_pairs_t *mutex,
                   pddl_err_t *err)
{
    if (prune->open_fixpoint)
        PDDL_PANIC("process-strips: Fixpoint was not closed!");

    prune->strips = strips;
    prune->mgroups = mgroups;
    prune->mutex = mutex;

    pddl_list_t *item;
    PDDL_LIST_FOR_EACH(&prune->steps, item){
        pddl_process_strips_step_t *s;
        s = PDDL_LIST_ENTRY(item, pddl_process_strips_step_t, conn);
        if (step(prune, s, err) != 0){
            PDDL_CTXEND(err);
            PDDL_TRACE_RET(err, -1);
        }
    }

    apply(prune, err);
    PDDL_LOG(err, "Removed %{removed_facts}d facts, %{removed_ops}d operators",
             prune->removed_fact,
             prune->removed_op);
    pddlStripsLogInfo(strips, err);
    return 0;
}

int pddlProcessStripsExecute(pddl_process_strips_t *prune,
                           pddl_strips_t *strips,
                           pddl_mgroups_t *mgroups,
                           pddl_mutex_pairs_t *mutex,
                           pddl_err_t *err)
{
    PDDL_CTX(err, "process_strips", "STRIPS-P");
    int ret = execute(prune, strips, mgroups, mutex, err);
    PDDL_CTXEND(err);
    return ret;
}

static void stepInit(const char *name,
                     pddl_process_strips_step_t *step,
                     pddl_process_strips_t *ps,
                     pddl_process_strips_execute_fn execute,
                     pddl_process_strips_free_fn free)
{
    if (ps->open_fixpoint != NULL){
        pddl_process_strips_fixpoint_t *fp;
        fp = pddl_container_of(ps->open_fixpoint, pddl_process_strips_fixpoint_t, step);
        stepInit(name, step, &fp->ps, execute, free);
        return;
    }
    bzero(step, sizeof(*step));
    step->name = PDDL_STRDUP(name);
    pddlListInit(&step->conn);
    step->execute = execute;
    step->free = free;
    step->can_reuse_rm_op_fact = 0;
    step->not_unreachable_or_dead_end = 0;
    pddlListAppend(&ps->steps, &step->conn);
}

static pddl_process_strips_step_t *stepNew(const char *name,
                                         pddl_process_strips_t *prune,
                                         pddl_process_strips_execute_fn execute,
                                         pddl_process_strips_free_fn free)
{
    pddl_process_strips_step_t *step = PDDL_ALLOC(pddl_process_strips_step_t);
    stepInit(name, step, prune, execute, free);
    return step;
}

static pddl_process_strips_step_hm_t *
        stepHmNew(const char *name,
                  pddl_process_strips_t *prune,
                  pddl_process_strips_execute_fn execute,
                  pddl_process_strips_free_fn free)
{
    pddl_process_strips_step_hm_t *step;
    step = PDDL_ALLOC(pddl_process_strips_step_hm_t);
    stepInit(name, &step->step, prune, execute, free);
    step->time_limit = 0.f;
    return step;
}

static void emptyFree(pddl_process_strips_step_t *_)
{
}

static int irrelevance(pddl_process_strips_t *prune,
                       pddl_process_strips_step_t *step,
                       pddl_err_t *err)
{
    return pddlIrrelevanceAnalysis(prune->strips,
                                   &prune->rm_fact,
                                   &prune->rm_op,
                                   NULL,
                                   err);
}

static int irrelevanceOps(pddl_process_strips_t *prune,
                          pddl_process_strips_step_t *step,
                          pddl_err_t *err)
{
    return pddlIrrelevanceAnalysis(prune->strips, NULL, &prune->rm_op,
                                   NULL, err);
}

void pddlProcessStripsAddIrrelevance(pddl_process_strips_t *prune)
{
    pddl_process_strips_step_t *step;
    step = stepNew("irrelevance", prune, irrelevance, emptyFree);
    step->can_reuse_rm_op_fact = 0;
    step->not_unreachable_or_dead_end = 1;
}

void pddlProcessStripsAddIrrelevanceOps(pddl_process_strips_t *prune)
{
    pddl_process_strips_step_t *step;
    step = stepNew("irrelevance-ops", prune, irrelevanceOps, emptyFree);
    step->can_reuse_rm_op_fact = 0;
    step->not_unreachable_or_dead_end = 1;
}


static int removeUselessDelEffs(pddl_process_strips_t *prune,
                                pddl_process_strips_step_t *step,
                                pddl_err_t *err)
{
    pddlStripsRemoveUselessDelEffs(prune->strips, prune->mutex, NULL, err);
    return 0;
}

void pddlProcessStripsAddRemoveUselessDelEffs(pddl_process_strips_t *prune)
{
    pddl_process_strips_step_t *step;
    step = stepNew("rm-useless-del-effs", prune, removeUselessDelEffs, emptyFree);
    step->can_reuse_rm_op_fact = 0;
    step->not_unreachable_or_dead_end = 0;
}

static int unreachableOps(pddl_process_strips_t *prune,
                          pddl_process_strips_step_t *step,
                          pddl_err_t *err)
{
    return pddlStripsFindUnreachableOps(prune->strips, prune->mutex,
                                        &prune->rm_op, err);
}

void pddlProcessStripsAddUnreachableOps(pddl_process_strips_t *prune)
{
    pddl_process_strips_step_t *step;
    step = stepNew("unreachable-ops", prune, unreachableOps, emptyFree);
    step->can_reuse_rm_op_fact = 1;
    step->not_unreachable_or_dead_end = 0;
}

static int removeOpsEmptyAddEff(pddl_process_strips_t *prune,
                                pddl_process_strips_step_t *step,
                                pddl_err_t *err)
{
    pddlStripsFindOpsEmptyAddEff(prune->strips, &prune->rm_op);
    return 0;
}

void pddlProcessStripsAddRemoveOpsEmptyAddEff(pddl_process_strips_t *prune)
{
    pddl_process_strips_step_t *step;
    step = stepNew("rm-ops-empty-add-eff", prune, removeOpsEmptyAddEff, emptyFree);
    step->can_reuse_rm_op_fact = 1;
    step->not_unreachable_or_dead_end = 0;
}

static int famgroupsDeadEndOps(pddl_process_strips_t *prune,
                               pddl_process_strips_step_t *step,
                               pddl_err_t *err)
{
    pddlFAMGroupsDeadEndOps(prune->mgroups, prune->strips, &prune->rm_op);
    return 0;
}

void pddlProcessStripsAddFAMGroupsDeadEndOps(pddl_process_strips_t *prune)
{
    pddl_process_strips_step_t *step;
    step = stepNew("fam dead-end", prune, famgroupsDeadEndOps, emptyFree);
    step->can_reuse_rm_op_fact = 0;
}

static int h2fw(pddl_process_strips_t *prune,
                pddl_process_strips_step_t *_step,
                pddl_err_t *err)
{
    pddl_process_strips_step_hm_t *step;
    step = pddl_container_of(_step, pddl_process_strips_step_hm_t, step);
    return pddlH2(prune->strips,
                  prune->mutex,
                  &prune->rm_fact,
                  &prune->rm_op,
                  step->time_limit,
                  err);
}

void pddlProcessStripsAddH2Fw(pddl_process_strips_t *prune,
                              float time_limit_in_s)
{
    pddl_process_strips_step_hm_t *step;
    step = stepHmNew("h^2 fw", prune, h2fw, emptyFree);
    step->step.can_reuse_rm_op_fact = 1;
    step->time_limit = time_limit_in_s;
}

static int h2fwbw(pddl_process_strips_t *prune,
                  pddl_process_strips_step_t *_step,
                  pddl_err_t *err)
{
    pddl_process_strips_step_hm_t *step;
    step = pddl_container_of(_step, pddl_process_strips_step_hm_t, step);
    pddl_mg_strips_t mg_strips;
    pddlMGStripsInit(&mg_strips, prune->strips, prune->mgroups);
    int ret = pddlH2FwBw(&mg_strips.strips,
                         &mg_strips.mg,
                         prune->mutex,
                         &prune->rm_fact,
                         &prune->rm_op,
                         step->time_limit,
                         err);
    pddlMGStripsFree(&mg_strips);
    return ret;
}

void pddlProcessStripsAddH2FwBw(pddl_process_strips_t *prune,
                                float time_limit_in_s)
{
    pddl_process_strips_step_hm_t *step;
    step = stepHmNew("h^2 fw+bw", prune, h2fwbw, emptyFree);
    step->step.can_reuse_rm_op_fact = 1;
    step->time_limit = time_limit_in_s;
}

static int h3fw(pddl_process_strips_t *prune,
                pddl_process_strips_step_t *_step,
                pddl_err_t *err)
{
    pddl_process_strips_step_hm_t *step;
    step = pddl_container_of(_step, pddl_process_strips_step_hm_t, step);
    return pddlH3(prune->strips,
                  prune->mutex,
                  &prune->rm_fact,
                  &prune->rm_op,
                  step->time_limit,
                  step->excess_memory,
                  err);
}

void pddlProcessStripsAddH3Fw(pddl_process_strips_t *prune,
                              float time_limit_in_s,
                              size_t excess_memory)
{
    pddl_process_strips_step_hm_t *step;
    step = stepHmNew("h^3 fw", prune, h3fw, emptyFree);
    step->step.can_reuse_rm_op_fact = 1;
    step->time_limit = time_limit_in_s;
    step->excess_memory = excess_memory;
}

static int deduplicateOps(pddl_process_strips_t *prune,
                          pddl_process_strips_step_t *step,
                          pddl_err_t *err)
{
    pddlStripsOpsDeduplicateSet(&prune->strips->op, &prune->rm_op);
    return 0;
}

void pddlProcessStripsAddDeduplicateOps(pddl_process_strips_t *prune)
{
    pddl_process_strips_step_t *step;
    step = stepNew("deduplicate ops", prune, deduplicateOps, emptyFree);
    step->can_reuse_rm_op_fact = 0;
    step->not_unreachable_or_dead_end = 1;
}

static int sortOps(pddl_process_strips_t *prune,
                   pddl_process_strips_step_t *step,
                   pddl_err_t *err)
{
    pddlStripsOpsSort(&prune->strips->op);
    PDDL_INFO(err, "Operators sorted by name");
    return 0;
}

void pddlProcessStripsAddSortOps(pddl_process_strips_t *prune)
{
    pddl_process_strips_step_t *step;
    step = stepNew("sort ops", prune, sortOps, emptyFree);
    step->can_reuse_rm_op_fact = 0;
}

static int opMutexExecute(pddl_process_strips_t *prune,
                          pddl_process_strips_step_t *_step,
                          pddl_err_t *err)
{
    pddl_process_strips_step_op_mutex_t *step
        = pddl_container_of(_step, pddl_process_strips_step_op_mutex_t, step);

    PDDL_INFO(err, "Operator Mutexes [ts: %d, op-fact: %d, hm-op: %d,"
                   " prune: %d, output: '%s']",
             step->ts,
             step->op_fact,
             step->hm_op,
             !step->no_prune,
             (step->out == NULL ? "" : step->out));
    if (step->ts < 0
            && step->op_fact < 1
            && step->hm_op < 1){
        PDDL_INFO(err, "Nothing to do");
        return 0;
    }

    pddl_mg_strips_t mg_strips;
    pddlMGStripsInit(&mg_strips, prune->strips, prune->mgroups);
    PDDL_INFO(err, "Created MG-Strips with %d facts, %d ops, %d mgroups,"
              " input mgroups: %d",
              mg_strips.strips.fact.fact_size,
              mg_strips.strips.op.op_size,
              mg_strips.mg.mgroup_size,
              prune->mgroups->mgroup_size);

    pddl_mutex_pairs_t mg_mutex;
    if (step->ts || step->hm_op > 1){
        pddlMutexPairsInitStrips(&mg_mutex, &mg_strips.strips);
        pddlMutexPairsAddMGroups(&mg_mutex, &mg_strips.mg);
        pddlH2(&mg_strips.strips, &mg_mutex, NULL, NULL, 0., err);
    }

    pddl_op_mutex_pairs_t opm;
    pddlOpMutexPairsInit(&opm, &mg_strips.strips);
    int ret;
    size_t max_mem = 0;
    if (step->ts){
        ret = pddlOpMutexInferTransSystems(&opm, &mg_strips, &mg_mutex,
                                           step->ts, max_mem, 1, err);
        if (ret < 0)
            PDDL_TRACE_RET(err, ret);
    }

    if (step->op_fact > 1){
        ret = pddlOpMutexInferHmOpFactCompilation(&opm, step->op_fact,
                                                  &mg_strips.strips, err);
        if (ret < 0)
            PDDL_TRACE_RET(err, ret);
    }

    if (step->hm_op > 1){
        ret = pddlOpMutexInferHmFromEachOp(&opm, step->hm_op,
                                           &mg_strips.strips, &mg_mutex,
                                           NULL, err);
        if (ret < 0)
            PDDL_TRACE_RET(err, ret);
    }

    if (step->out != NULL){
        int o1, o2;
        PRINT_TO_FILE(err, step->out, "operator mutexes",
            PDDL_OP_MUTEX_PAIRS_FOR_EACH(&opm, o1, o2)
                fprintf(fout, "%d %d\n", o1, o2)
        );
    }

    if (opm.num_op_mutex_pairs > 0 && !step->no_prune){
        PDDL_INFO(err, "Computing symmetries on PDG");
        pddl_strips_sym_t sym;
        pddlStripsSymInitPDG(&sym, prune->strips);
        PDDL_INFO(err, "  Symmetry generators: %d", sym.gen_size);
        PDDL_ISET(redundant);
        pddl_op_mutex_redundant_config_t cfg = PDDL_OP_MUTEX_REDUNDANT_CONFIG_INIT;
        cfg.method = step->prune_method;
        cfg.lp_time_limit = step->prune_time_limit;
        pddlOpMutexFindRedundant(&opm, &sym, &cfg, &redundant, err);
        pddlISetUnion(&prune->rm_op, &redundant);
        pddlISetFree(&redundant);
        pddlStripsSymFree(&sym);
    }

    pddlOpMutexPairsFree(&opm);
    if (step->ts || step->hm_op > 1)
        pddlMutexPairsFree(&mg_mutex);
    pddlMGStripsFree(&mg_strips);

    return 0;
}

static void opMutexFree(pddl_process_strips_step_t *step)
{
    pddl_process_strips_step_op_mutex_t *s
        = pddl_container_of(step, pddl_process_strips_step_op_mutex_t, step);
    if (s->out != NULL)
        PDDL_FREE(s->out);
}

static pddl_process_strips_step_op_mutex_t *
            stepOpMutexNew(pddl_process_strips_t *prune)
{
    pddl_process_strips_step_op_mutex_t *step;
    step = PDDL_ALLOC(pddl_process_strips_step_op_mutex_t);
    stepInit("op mutex", &step->step, prune, opMutexExecute, opMutexFree);
    step->step.not_unreachable_or_dead_end = 1;
    return step;
}

void pddlProcessStripsAddOpMutex(pddl_process_strips_t *prune,
                                 int ts, int op_fact, int hm_op,
                                 int no_prune, int prune_method,
                                 float prune_time_limit,
                                 const char *out)
{
    pddl_process_strips_step_op_mutex_t *step;
    step = stepOpMutexNew(prune);
    step->ts = ts;
    step->op_fact = op_fact;
    step->hm_op = hm_op;
    step->no_prune = no_prune;
    step->prune_method = prune_method;
    step->prune_time_limit = prune_time_limit;
    step->out = NULL;
    if (out != NULL)
        step->out = PDDL_STRDUP(out);
}



static int pruneEndomorphismFDR(const pddl_process_strips_t *prune,
                                const pddl_endomorphism_config_t *cfg,
                                pddl_iset_t *redundant_op,
                                pddl_err_t *err)
{
    int ret = 0;
    PDDL_INFO(err, "Redundant operators using endomorphism on FDR ...");
    pddl_fdr_t fdr;
    pddlFDRInitFromStrips(&fdr, prune->strips, prune->mgroups, prune->mutex,
                          PDDL_FDR_VARS_LARGEST_FIRST, 0, err);
    ret = pddlEndomorphismFDRRedundantOps(&fdr, cfg, redundant_op, err);
    if (ret >= 0)
        ret = 0;
    pddlFDRFree(&fdr);
    PDDL_INFO(err, "Redundant operators using endomorphism on FDR DONE");
    return ret;
}

static int pruneEndomorphismTS(const pddl_process_strips_t *prune,
                               const pddl_endomorphism_config_t *cfg,
                               pddl_iset_t *redundant_op,
                               pddl_err_t *err)
{
    int ret = 0;
    PDDL_INFO(err, "Redundant operators using endomorphism on TSs ...");
    pddl_mg_strips_t mg_strips;
    pddlMGStripsInit(&mg_strips, prune->strips, prune->mgroups);

    pddl_trans_systems_t tss;
    pddl_mutex_pairs_t mg_mutex;
    pddlMutexPairsInitStrips(&mg_mutex, &mg_strips.strips);
    pddlMutexPairsAddMGroups(&mg_mutex, &mg_strips.mg);
    pddlH2(&mg_strips.strips, &mg_mutex, NULL, NULL, 0., err);
    pddlTransSystemsInit(&tss, &mg_strips, &mg_mutex);
    ret = pddlEndomorphismTransSystemRedundantOps(&tss, cfg, redundant_op, err);
    if (ret >= 0)
        ret = 0;
    pddlTransSystemsFree(&tss);
    pddlMutexPairsFree(&mg_mutex);
    pddlMGStripsFree(&mg_strips);
    PDDL_INFO(err, "Redundant operators using endomorphism on TSs DONE");
    return ret;
}

static int pruneEndomorphismFDRTS(const pddl_process_strips_t *prune,
                                  const pddl_endomorphism_config_t *cfg,
                                  pddl_iset_t *redundant_op,
                                  pddl_err_t *err)
{
    int ret = pruneEndomorphismFDR(prune, cfg, redundant_op, err);

    if (ret == 0){
        PDDL_ISET(redundant2);
        int ret2 = pruneEndomorphismTS(prune, cfg, &redundant2, err);
        if (ret2 == 0
                && pddlISetSize(&redundant2) > pddlISetSize(redundant_op)){
            pddlISetEmpty(redundant_op);
            pddlISetUnion(redundant_op, &redundant2);
        }
        pddlISetFree(&redundant2);
    }else{
        PDDL_INFO(err, "Endomorphism on factored TS skipped, because"
                        " endomorphism on FDR failed");
    }

    return ret;
}

static int endomorphExecute(pddl_process_strips_t *prune,
                            pddl_process_strips_step_t *_step,
                            pddl_err_t *err)
{
    pddl_process_strips_step_endomorph_t *step
        = pddl_container_of(_step, pddl_process_strips_step_endomorph_t, step);

    PDDL_ISET(redundant_op);
    int ret = 0;
    if (step->fdr){
        ret = pruneEndomorphismFDR(prune, &step->cfg, &redundant_op, err);

    }else if (step->ts){
        ret = pruneEndomorphismTS(prune, &step->cfg, &redundant_op, err);

    }else if (step->fdr_ts){
        ret = pruneEndomorphismFDRTS(prune, &step->cfg, &redundant_op, err);
    }

    if (ret != 0){
        pddlISetFree(&redundant_op);
        return -1;
    }

    pddlISetUnion(&prune->rm_op, &redundant_op);
    pddlISetFree(&redundant_op);
    return 0;
}

static pddl_process_strips_step_endomorph_t *
            stepEndomorphNew(pddl_process_strips_t *prune)
{
    pddl_process_strips_step_endomorph_t *step;
    step = PDDL_ALLOC(pddl_process_strips_step_endomorph_t);
    bzero(step, sizeof(*step));
    stepInit("endomorph", &step->step, prune, endomorphExecute, emptyFree);
    step->step.not_unreachable_or_dead_end = 1;
    return step;
}

void pddlProcessStripsAddEndomorphFDR(pddl_process_strips_t *prune,
                                      const pddl_endomorphism_config_t *cfg)
{
    pddl_process_strips_step_endomorph_t *p = stepEndomorphNew(prune);
    p->cfg = *cfg;
    p->fdr = 1;
}

void pddlProcessStripsAddEndomorphMGStrips(pddl_process_strips_t *prune,
                                           const pddl_endomorphism_config_t *cfg)
{
    pddl_process_strips_step_endomorph_t *p = stepEndomorphNew(prune);
    p->cfg = *cfg;
    p->mg_strips = 1;
}

void pddlProcessStripsAddEndomorphTS(pddl_process_strips_t *prune,
                                     const pddl_endomorphism_config_t *cfg)
{
    pddl_process_strips_step_endomorph_t *p = stepEndomorphNew(prune);
    p->cfg = *cfg;
    p->ts = 1;
}

void pddlProcessStripsAddEndomorphFDRTS(pddl_process_strips_t *prune,
                                        const pddl_endomorphism_config_t *cfg)
{
    pddl_process_strips_step_endomorph_t *p = stepEndomorphNew(prune);
    p->cfg = *cfg;
    p->fdr_ts = 1;
}

struct pddl_process_strips_step_print {
    pddl_process_strips_step_t step;
    int print_domain;
    int print_problem;
    char *fn;
};
typedef struct pddl_process_strips_step_print pddl_process_strips_step_print_t;

static int printExecute(pddl_process_strips_t *ps,
                        pddl_process_strips_step_t *_step,
                        pddl_err_t *err)
{
    pddl_process_strips_step_print_t *step;
    step = pddl_container_of(_step, pddl_process_strips_step_print_t, step);

    FILE *fout = fopen(step->fn, "w");
    if (fout == NULL){
        PDDL_INFO(err, "Could not open file %s", step->fn);
        return 0;
    }

    if (step->print_domain){
        pddlStripsPrintPDDLDomain(ps->strips, fout);

    }else if (step->print_problem){
        pddlStripsPrintPDDLProblem(ps->strips, fout);
    }

    fclose(fout);
    return 0;
}

static void printFree(pddl_process_strips_step_t *_step)
{
    pddl_process_strips_step_print_t *step;
    step = pddl_container_of(_step, pddl_process_strips_step_print_t, step);
    if (step->fn != NULL)
        PDDL_FREE(step->fn);
}

void pddlProcessStripsAddPrintPddlDomain(pddl_process_strips_t *ps,
                                         const char *fn)
{
    pddl_process_strips_step_print_t *step;
    step = PDDL_ALLOC(pddl_process_strips_step_print_t);
    bzero(step, sizeof(*step));
    stepInit("print-domain", &step->step, ps, printExecute, printFree);
    step->print_domain = 1;
    step->fn = PDDL_STRDUP(fn);
}

void pddlProcessStripsAddPrintPddlProblem(pddl_process_strips_t *ps,
                                          const char *fn)
{
    pddl_process_strips_step_print_t *step;
    step = PDDL_ALLOC(pddl_process_strips_step_print_t);
    bzero(step, sizeof(*step));
    stepInit("print-problem", &step->step, ps, printExecute, printFree);
    step->print_problem = 1;
    step->fn = PDDL_STRDUP(fn);
}

static int fixpointExecute(pddl_process_strips_t *prune,
                           pddl_process_strips_step_t *step,
                           pddl_err_t *err)
{
    pddl_process_strips_fixpoint_t *fp;
    fp = pddl_container_of(step, pddl_process_strips_fixpoint_t, step);
    int cycle = 0;
    do {
        PDDL_CTX_F(err, "cycle_%d", "Cycle %d", cycle);
        apply(prune, err);
        fp->ps.removed_op = 0;
        fp->ps.removed_fact = 0;
        pddlISetEmpty(&fp->ps.rm_op);
        pddlISetEmpty(&fp->ps.rm_fact);
        execute(&fp->ps, prune->strips, prune->mgroups, prune->mutex, err);
        apply(&fp->ps, err);
        prune->removed_op += fp->ps.removed_op;
        prune->removed_fact += fp->ps.removed_fact;
        PDDL_CTXEND(err);
        ++cycle;
    } while (fp->ps.removed_op > 0 || fp->ps.removed_fact > 0);
    PDDL_LOG(err, "Cycles: %{num_cycles}d", cycle);

    return 0;
}

static void fixpointFree(pddl_process_strips_step_t *step)
{
    pddl_process_strips_fixpoint_t *fp;
    fp = pddl_container_of(step, pddl_process_strips_fixpoint_t, step);
    pddlProcessStripsFree(&fp->ps);
}

void pddlProcessStripsFixpointStart(pddl_process_strips_t *ps)
{
    pddl_process_strips_fixpoint_t *fp;
    fp = PDDL_ALLOC(pddl_process_strips_fixpoint_t);
    stepInit("fixpoint", &fp->step, ps, fixpointExecute, fixpointFree);
    pddlProcessStripsInit(&fp->ps);

    while (ps->open_fixpoint != NULL){
        pddl_process_strips_fixpoint_t *fp_child;
        fp_child = pddl_container_of(ps->open_fixpoint, pddl_process_strips_fixpoint_t, step);
        ps = &fp_child->ps;
    }
    ps->open_fixpoint = &fp->step;
}

void pddlProcessStripsFixpointFinalize(pddl_process_strips_t *ps)
{
    if (ps->open_fixpoint == NULL)
        PDDL_PANIC("process-strips: No opened fixpoint!");

    pddl_process_strips_fixpoint_t *fp;
    fp = pddl_container_of(ps->open_fixpoint, pddl_process_strips_fixpoint_t, step);
    if (fp->ps.open_fixpoint == NULL){
        ps->open_fixpoint = NULL;
    }else{
        pddlProcessStripsFixpointFinalize(&fp->ps);
    }
}
