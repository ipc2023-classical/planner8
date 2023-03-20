/***
 * cpddl
 * -------
 * Copyright (c)2018 Daniel Fiser <danfis@danfis.cz>,
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

#ifndef __PDDL_FDR_VAR_H__
#define __PDDL_FDR_VAR_H__

#include <pddl/strips.h>
#include <pddl/mgroup.h>
#include <pddl/mutex_pair.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct pddl_fdr_val {
    char *name;
    int var_id; /*!< ID of the variable this value belongs to */
    int val_id; /*!< Value ID within the variable */
    int global_id; /*!< Global unique ID of this value */
    int strips_id; /*!< ID of the STRIPS fact this value was created from */
};
typedef struct pddl_fdr_val pddl_fdr_val_t;

void pddlFDRValInit(pddl_fdr_val_t *val);
void pddlFDRValFree(pddl_fdr_val_t *val);


struct pddl_fdr_var {
    int var_id; /*!< ID of the variable */
    pddl_fdr_val_t *val; /*!< List of values */
    int val_size; /*!< Number of values, i.e., range of the variable */
    int val_none_of_those; /*!< ID of the "none of those" value or -1 --
                                this value is created during translation from
                                STRIPS */
    int is_black; /*!< True if the variable is painted black in a red-black
                       problem */
};
typedef struct pddl_fdr_var pddl_fdr_var_t;

void pddlFDRVarInit(pddl_fdr_var_t *var);
void pddlFDRVarFree(pddl_fdr_var_t *var);

struct pddl_fdr_vars {
    pddl_fdr_var_t *var; /*!< List of variables */
    int var_size; /*!< Number of variables */

    int global_id_size; /*!< Number of global IDs */
    pddl_fdr_val_t **global_id_to_val; /*!< Mapping from global ID to FDR
                                            value */
    int strips_id_size;
    pddl_iset_t *strips_id_to_val; /*!< If the variables were created from
                                       STRIPS, this maps STRIPS IDs to
                                       global IDs of variable values */
};
typedef struct pddl_fdr_vars pddl_fdr_vars_t;

#define PDDL_FDR_VARS_ESSENTIAL_FIRST 0x00u
#define PDDL_FDR_VARS_LARGEST_FIRST 0x01u
#define PDDL_FDR_VARS_LARGEST_FIRST_MULTI 0x02u
// TODO: Minimazion of bits required for storing the whole state
#define PDDL_FDR_VARS_MIN_BITS
#define PDDL_FDR_VARS_NO_NEGATED_FACTS 0x10u

/**
 * Initialize the set of variables from the strips representation given a
 * set of mutex groups and mutex pairs.
 */
int pddlFDRVarsInitFromStrips(pddl_fdr_vars_t *vars,
                              const pddl_strips_t *strips,
                              const pddl_mgroups_t *mg,
                              const pddl_mutex_pairs_t *mutex,
                              unsigned flags);

/**
 * Initialize dst as a deep copy of src.
 */
void pddlFDRVarsInitCopy(pddl_fdr_vars_t *dst, const pddl_fdr_vars_t *src);

struct pddl_fdr_vars_remap {
    int var_size;
    const pddl_fdr_val_t ***remap;
};
typedef struct pddl_fdr_vars_remap pddl_fdr_vars_remap_t;

void pddlFDRVarsRemapFree(pddl_fdr_vars_remap_t *remap);

/**
 * Remove specified facts and create the remapping structure.
 */
void pddlFDRVarsDelFacts(pddl_fdr_vars_t *vars,
                         const pddl_iset_t *del_facts,
                         pddl_fdr_vars_remap_t *remap);

/**
 * Free allocated memory.
 */
void pddlFDRVarsFree(pddl_fdr_vars_t *vars);

/**
 * Adds a new value with the given name to the specified variable.
 */
pddl_fdr_val_t *pddlFDRVarsAddVal(pddl_fdr_vars_t *vars,
                                  int var,
                                  const char *name);

/**
 * TODO
 */
void pddlFDRVarsRemap(pddl_fdr_vars_t *vars, const int *remap);

void pddlFDRVarsPrintDebug(const pddl_fdr_vars_t *vars, FILE *fout);
void pddlFDRVarsPrintTable(const pddl_fdr_vars_t *vars,
                           int linesize,
                           FILE *fout,
                           pddl_err_t *err);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_FDR_VAR_H__ */
