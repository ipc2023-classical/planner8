/***
 * cpddl
 * -------
 * Copyright (c)2019 Daniel Fiser <danfis@danfis.cz>,
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

#ifndef __PDDL_MG_STRIPS_H__
#define __PDDL_MG_STRIPS_H__

#include <pddl/strips.h>
#include <pddl/mutex_pair.h>
#include <pddl/mgroup.h>
#include <pddl/fdr.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct pddl_mg_strips {
    pddl_strips_t strips; /*!< Planning task */
    pddl_mgroups_t mg; /*!< Exactly-one mutex groups covering all facts */
};
typedef struct pddl_mg_strips pddl_mg_strips_t;


void pddlMGStripsInit(pddl_mg_strips_t *mg_strips,
                      const pddl_strips_t *strips,
                      const pddl_mgroups_t *mgroups);
void pddlMGStripsInitCopy(pddl_mg_strips_t *mg_strips,
                          const pddl_mg_strips_t *in);
void pddlMGStripsInitFDR(pddl_mg_strips_t *mg_strips, const pddl_fdr_t *fdr);
void pddlMGStripsFree(pddl_mg_strips_t *mg_strips);

void pddlMGStripsReduce(pddl_mg_strips_t *mg_strips,
                        const pddl_iset_t *del_facts,
                        const pddl_iset_t *del_ops);

void pddlMGStripsReorderMGroups(pddl_mg_strips_t *mg_strips,
                                const int *reorder);

double pddlMGStripsNumStatesApproxMC(const pddl_mg_strips_t *mg_strips,
                                     const pddl_mutex_pairs_t *mutex,
                                     const char *approxmc_bin,
                                     int fix_fact);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_MG_STRIPS_H__ */
