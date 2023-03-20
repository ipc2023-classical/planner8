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

#ifndef __PDDL_PLAN_FILE_H__
#define __PDDL_PLAN_FILE_H__

#include <pddl/iarr.h>
#include <pddl/fdr.h>
#include <pddl/strips.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct pddl_plan_file_fdr {
    pddl_iarr_t op; /*!< Sequence of operators */
    int **state; /*!< Intermediate states */
    int state_size;
    int state_alloc;
    int cost; /*!< Cost of the plan */
};
typedef struct pddl_plan_file_fdr pddl_plan_file_fdr_t;


/**
 * Reads plan file corresponding to the given FDR planning task.
 * Returns 0 on success.
 */
int pddlPlanFileFDRInit(pddl_plan_file_fdr_t *p,
                        const pddl_fdr_t *fdr,
                        const char *filename,
                        pddl_err_t *err);

/**
 * Free allocated memory.
 */
void pddlPlanFileFDRFree(pddl_plan_file_fdr_t *p);


struct pddl_plan_file_strips {
    pddl_iarr_t op; /*!< Sequence of operators */
    pddl_iset_t *state; /*!< Intermediate states */
    int state_size;
    int state_alloc;
    int cost; /*!< Cost of the plan */
};
typedef struct pddl_plan_file_strips pddl_plan_file_strips_t;

int pddlPlanFileStripsInit(pddl_plan_file_strips_t *p,
                           const pddl_strips_t *strips,
                           const char *filename,
                           pddl_err_t *err);
void pddlPlanFileStripsFree(pddl_plan_file_strips_t *p);


int pddlPlanFileParseOptimalCost(const char *filename, pddl_err_t *err);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_PLAN_FILE_H__ */
