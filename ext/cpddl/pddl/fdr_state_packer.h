/***
 * cpddl
 * -------
 * Copyright (c)2015 Daniel Fiser <danfis@danfis.cz>,
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

#ifndef __PDDL_FDR_STATE_PACKER_H__
#define __PDDL_FDR_STATE_PACKER_H__

#include <pddl/fdr_part_state.h>
#include <pddl/fdr_var.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** Forward declaration */
struct pddl_fdr_state_packer_var;

/**
 * Struct implementing packing of states into binary buffers.
 */
struct pddl_fdr_state_packer {
    struct pddl_fdr_state_packer_var *vars;
    int num_vars;
    int bufsize;
};
typedef struct pddl_fdr_state_packer pddl_fdr_state_packer_t;

/**
 * Initializes a new object for packing states into binary buffers.
 */
void pddlFDRStatePackerInit(pddl_fdr_state_packer_t *p,
                            const pddl_fdr_vars_t *vars);

/**
 * Creates an exact copy of the packer object.
 */
void pddlFDRStatePackerInitCopy(pddl_fdr_state_packer_t *d,
                                const pddl_fdr_state_packer_t *p);

/**
 * Frees a state packer object.
 */
void pddlFDRStatePackerFree(pddl_fdr_state_packer_t *p);

/**
 * Returns size of the buffer in bytes required for storing packed state.
 */
_pddl_inline int pddlFDRStatePackerBufSize(const pddl_fdr_state_packer_t *p);

/**
 * Pack the given state into provided buffer that must be at least
 * pddlFDRStatePackerBufSize(p) long.
 */
void pddlFDRStatePackerPack(const pddl_fdr_state_packer_t *p,
                            const int *state,
                            void *buffer);

/**
 * Unpacks the given packed state into pddl_state_t state structure.
 */
void pddlFDRStatePackerUnpack(const pddl_fdr_state_packer_t *p,
                              const void *buffer,
                              int *state);


/**** INLINES ****/
_pddl_inline int pddlFDRStatePackerBufSize(const pddl_fdr_state_packer_t *p)
{
    return p->bufsize;
}

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_FDR_STATE_PACKER_H__ */
