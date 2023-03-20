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

#ifndef __PDDL_TRANS_SYSTEM_ABSTR_MAPPING_H__
#define __PDDL_TRANS_SYSTEM_ABSTR_MAPPING_H__

#include <pddl/iset.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct pddl_trans_system_abstr_map {
    int *map;
    int num_states;
    int map_num_states;
    int is_identity;
};
typedef struct pddl_trans_system_abstr_map pddl_trans_system_abstr_map_t;

/**
 * Initialize abstraction mapping as identity
 */
void pddlTransSystemAbstrMapInit(pddl_trans_system_abstr_map_t *map,
                                 int num_states);

/**
 * Free allocated memory.
 */
void pddlTransSystemAbstrMapFree(pddl_trans_system_abstr_map_t *map);

/**
 * Finalize the mapping.
 * This needs to be called when setting of the mapping is done.
 */
void pddlTransSystemAbstrMapFinalize(pddl_trans_system_abstr_map_t *map);

/**
 * Prune the specified state.
 */
void pddlTransSystemAbstrMapPruneState(pddl_trans_system_abstr_map_t *map,
                                       int state);

/**
 * Condense the specified states into one state
 */
void pddlTransSystemAbstrMapCondense(pddl_trans_system_abstr_map_t *map,
                                     const pddl_iset_t *states);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_TRANS_SYSTEM_ABSTR_MAPPING_H__ */
