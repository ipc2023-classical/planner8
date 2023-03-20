/***
 * cpddl
 * -------
 * Copyright (c)2020 Daniel Fiser <danfis@danfis.cz>,
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

#ifndef __PDDL_ENDOMORPHISM_H__
#define __PDDL_ENDOMORPHISM_H__

#include <pddl/pddl_struct.h>
#include <pddl/fdr.h>
#include <pddl/mg_strips.h>
#include <pddl/trans_system.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct pddl_endomorphism_config {
    /** Maximal overall time in seconds. (default: 1 hour) */
    float max_time;
    /** Maximal search time in seconds. (default: 1 hour) */
    float max_search_time;
    /** Maximal number of worker threads. (default: 1) */
    int num_threads;
    /** If set to true, the inference will run in a separate sub-process.
     *  (default: true) */
    int run_in_subprocess;
    /** If set to true, costs of operators are ignored (default: false) */
    int ignore_costs;
    /** True if sub-combinations of lifted mutex groups should be used.
     *  (default: false) */
    int lifted_use_combinations;
};
typedef struct pddl_endomorphism_config pddl_endomorphism_config_t;

#define PDDL_ENDOMORPHISM_CONFIG_INIT \
    { 3600.f, /* .max_time */ \
      3600.f, /* .max_search_time */ \
      1, /* .num_threads */ \
      1, /* .run_in_subprocess */ \
      0, /* .ignore_costs */ \
      0, /* .lifted_use_combinations */ \
    }

struct pddl_endomorphism_sol {
    pddl_iset_t redundant_ops;
    int op_size;
    int *op_map;
    int is_optimal;
};
typedef struct pddl_endomorphism_sol pddl_endomorphism_sol_t;

void pddlEndomorphismSolFree(pddl_endomorphism_sol_t *sol);

int pddlEndomorphismFDR(const pddl_fdr_t *fdr,
                        const pddl_endomorphism_config_t *cfg,
                        pddl_endomorphism_sol_t *sol,
                        pddl_err_t *err);

int pddlEndomorphismTransSystem(const pddl_trans_systems_t *tss,
                                const pddl_endomorphism_config_t *cfg,
                                pddl_endomorphism_sol_t *sol,
                                pddl_err_t *err);

int pddlEndomorphismFDRRedundantOps(const pddl_fdr_t *fdr,
                                    const pddl_endomorphism_config_t *cfg,
                                    pddl_iset_t *redundant_ops,
                                    pddl_err_t *err);

int pddlEndomorphismTransSystemRedundantOps(const pddl_trans_systems_t *tss,
                                            const pddl_endomorphism_config_t *c,
                                            pddl_iset_t *redundant_ops,
                                            pddl_err_t *err);

int pddlEndomorphismLifted(const pddl_t *pddl,
                           const pddl_lifted_mgroups_t *lifted_mgroups,
                           const pddl_endomorphism_config_t *cfg,
                           pddl_iset_t *redundant_objects,
                           pddl_obj_id_t *map,
                           pddl_err_t *err);

int pddlEndomorphismRelaxedLifted(const pddl_t *pddl,
                                  const pddl_endomorphism_config_t *cfg,
                                  pddl_iset_t *redundant_objects,
                                  pddl_obj_id_t *map,
                                  pddl_err_t *err);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_ENDOMORPHISM_H__ */
