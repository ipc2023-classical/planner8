/***
 * cpddl
 * -------
 * Copyright (c)2016 Daniel Fiser <danfis@danfis.cz>,
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

#ifndef __PDDL_PARAM_H__
#define __PDDL_PARAM_H__

#include <pddl/type.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/**
 * Typed parameter
 */
struct pddl_param {
    char *name;   /*!< Name of the parameter */
    int type;     /*!< Type ID */
    int is_agent; /*!< True if this is :agent parameter */
    int inherit;  /*!< -1 or ID of the parent parameter of which this is a
                       copy */

    int is_counted_var; /*!< True if it is counted variable -- this is used
                             for inference of lifted mutex groups */
};
typedef struct pddl_param pddl_param_t;

struct pddl_params {
    pddl_param_t *param;
    int param_size;
    int param_alloc;
};
typedef struct pddl_params pddl_params_t;


/**
 * Initialzie empty parameter
 */
void pddlParamInit(pddl_param_t *param);

/**
 * Copies src to dst.
 */
void pddlParamInitCopy(pddl_param_t *dst, const pddl_param_t *src);

/**
 * Initialize list of parameters
 */
void pddlParamsInit(pddl_params_t *params);

/**
 * Free allocated memory.
 */
void pddlParamsFree(pddl_params_t *params);

/**
 * Adds a new empty parameter object at the end of params.
 */
pddl_param_t *pddlParamsAdd(pddl_params_t *params);

/**
 * Copies src to dst.
 */
void pddlParamsInitCopy(pddl_params_t *dst, const pddl_params_t *src);

/**
 * Returns index of the parameter with the specified name or -1 if such a
 * parameter is not stored in the list.
 */
int pddlParamsGetId(const pddl_params_t *param, const char *name);

/**
 * Parse typed parameters from lisp.
 */
int pddlParamsParse(pddl_params_t *params,
                    const pddl_lisp_node_t *root,
                    pddl_types_t *types,
                    pddl_err_t *err);

/**
 * Parse :agent var - type parameters
 */
int pddlParamsParseAgent(pddl_params_t *params,
                         const pddl_lisp_node_t *root,
                         int agent_node_index,
                         pddl_types_t *types,
                         pddl_err_t *err);

/**
 * Remap parameters.
 */
void pddlParamsRemap(pddl_params_t *params, const int *remap);

void pddlParamsPrint(const pddl_params_t *params, FILE *fout);

/**
 * Print parameters in PDDL format (without parenthesis), the inherited
 * parameters are not printed.
 */
void pddlParamsPrintPDDL(const pddl_params_t *params,
                         const pddl_types_t *ts,
                         FILE *fout);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_PARAM_H__ */
