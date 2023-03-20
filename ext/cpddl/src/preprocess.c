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

#include "pddl/mg_strips.h"
#include "pddl/mutex_pair.h"
#include "pddl/critical_path.h"
#include "pddl/irrelevance.h"
#include "pddl/preprocess.h"

static int pddlPruneFDRH2FwBw(pddl_fdr_t *fdr, pddl_err_t *err)
{
    pddl_mg_strips_t mg_strips;
    pddl_mutex_pairs_t mutex;

    pddlMGStripsInitFDR(&mg_strips, fdr);
    pddlMutexPairsInitStrips(&mutex, &mg_strips.strips);

    PDDL_ISET(rm_fact);
    PDDL_ISET(rm_op);
    if (fdr->has_cond_eff){
        PDDL_INFO(err, "Skipping h^2, because FDR has conditional effects.");

    }else if (pddlH2FwBw(&mg_strips.strips, &mg_strips.mg, &mutex,
                         &rm_fact, &rm_op, 0., err) != 0){
        PDDL_TRACE_RET(err, -1);
    }

    if (pddlISetSize(&rm_fact) > 0 || pddlISetSize(&rm_op) > 0)
        pddlFDRReduce(fdr, NULL, &rm_fact, &rm_op);

    pddlISetFree(&rm_op);
    pddlISetFree(&rm_fact);
    pddlMutexPairsFree(&mutex);
    pddlMGStripsFree(&mg_strips);

    return 0;
}

static int pddlPruneFDRIrrelevance(pddl_fdr_t *fdr, pddl_err_t *err)
{
    PDDL_ISET(rm_var);
    PDDL_ISET(rm_op);
    if (fdr->has_cond_eff){
        PDDL_INFO(err, "Skipping irrelevance analysis, because FDR has"
                   " conditional effects.");

    }else if (pddlIrrelevanceAnalysisFDR(fdr, &rm_var, &rm_op, err) != 0){
        PDDL_TRACE_RET(err, -1);
    }

    if (pddlISetSize(&rm_var) > 0 || pddlISetSize(&rm_op) > 0)
        pddlFDRReduce(fdr, &rm_var, NULL, &rm_op);

    pddlISetFree(&rm_op);
    pddlISetFree(&rm_var);

    return 0;
}

int pddlPruneFDR(pddl_fdr_t *fdr, pddl_err_t *err)
{
    PDDL_INFO(err, "Pruning of FDR. ops: %d, facts: %d, vars: %d",
              fdr->op.op_size, fdr->var.global_id_size, fdr->var.var_size);

    if (pddlPruneFDRH2FwBw(fdr, err) != 0)
        PDDL_TRACE_RET(err, -1);
    if (pddlPruneFDRIrrelevance(fdr, err) != 0)
        PDDL_TRACE_RET(err, -1);

    PDDL_INFO(err, "Pruning of FDR DONE. ops: %d, facts: %d, vars: %d",
              fdr->op.op_size, fdr->var.global_id_size, fdr->var.var_size);

    return 0;
}
