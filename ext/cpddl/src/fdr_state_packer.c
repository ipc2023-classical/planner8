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

#include "internal.h"
#include "pddl/sort.h"
#include "pddl/fdr_state_packer.h"

struct pddl_fdr_state_packer_var {
    int bitlen; /*!< Number of bits required to store a value */
    pddl_fdr_packer_word_t shift; /*!< Left shift size during packing */
    pddl_fdr_packer_word_t mask; /*!< Mask for packed values */
    pddl_fdr_packer_word_t clear_mask; /*!< Clear mask for packed values
                                            (i.e., ~mask) */
    int pos; /*!< Position of a word in buffer where values are stored. */
};
typedef struct pddl_fdr_state_packer_var pddl_fdr_state_packer_var_t;

/** Returns number of bits needed for storing all values in interval
 * [0,range). */
static int packerBitsNeeded(int range);
/** Constructs mask for variable */
static pddl_fdr_packer_word_t packerVarMask(int bitlen, int shift);
/** Set variable into packed buffer */
static void packerSetVar(const pddl_fdr_state_packer_var_t *var,
                         pddl_fdr_packer_word_t val,
                         void *buffer);
/** Retrieves a value of the variable from packed buffer */
static int packerGetVar(const pddl_fdr_state_packer_var_t *var,
                        const void *buffer);

struct sorted_vars {
    pddl_fdr_state_packer_var_t **var;
    int var_size;
    int var_left;
};
typedef struct sorted_vars sorted_vars_t;

static void sortedVarsInit(sorted_vars_t *sv, const pddl_fdr_vars_t *vars,
                           pddl_fdr_state_packer_var_t *pvar);
static void sortedVarsFree(sorted_vars_t *sv);
static pddl_fdr_state_packer_var_t *sortedVarsNext(sorted_vars_t *sv,
                                                   int filled_bits);


void pddlFDRStatePackerInit(pddl_fdr_state_packer_t *p,
                            const pddl_fdr_vars_t *vars)
{
    int i, is_set, bitlen, wordpos;
    sorted_vars_t sorted_vars;
    pddl_fdr_state_packer_var_t *pvar;

    p->num_vars = vars->var_size;
    p->vars = ALLOC_ARR(pddl_fdr_state_packer_var_t, p->num_vars);
    for (i = 0; i < vars->var_size; ++i){
        p->vars[i].bitlen = packerBitsNeeded(vars->var[i].val_size);
        p->vars[i].pos = -1;
    }

    sortedVarsInit(&sorted_vars, vars, p->vars);

    // This arranges variables according to their bit length into array of
    // bytes (more preciselly array of words as defined by
    // pddl_fdr_packer_word_t). Because optimal packing is NP-hard problem we
    // use simple greedy algorithm: we try to fill one word after other
    // always trying to fill the empty space with the biggest variable
    // possible.
    is_set = 0;
    wordpos = -1;
    while (is_set < vars->var_size){
        ++wordpos;

        bitlen = 0;
        while ((pvar = sortedVarsNext(&sorted_vars, bitlen)) != NULL){
            pvar->pos = wordpos;
            bitlen += pvar->bitlen;
            pvar->shift = PDDL_FDR_PACKER_WORD_BITS - bitlen;
            pvar->mask = packerVarMask(pvar->bitlen, pvar->shift);
            pvar->clear_mask = ~pvar->mask;
            ++is_set;
        }
    }

    p->bufsize = sizeof(pddl_fdr_packer_word_t) * (wordpos + 1);

    sortedVarsFree(&sorted_vars);
}

void pddlFDRStatePackerInitCopy(pddl_fdr_state_packer_t *d,
                                const pddl_fdr_state_packer_t *p)
{
    d->num_vars = p->num_vars;
    d->bufsize = p->bufsize;
    d->vars = ALLOC_ARR(pddl_fdr_state_packer_var_t, p->num_vars);
    memcpy(d->vars, p->vars, sizeof(pddl_fdr_state_packer_var_t) * p->num_vars);
}

void pddlFDRStatePackerFree(pddl_fdr_state_packer_t *p)
{
    if (p->vars)
        FREE(p->vars);
}

void pddlFDRStatePackerPack(const pddl_fdr_state_packer_t *p,
                            const int *state,
                            void *buffer)
{
    ZEROIZE_RAW(buffer, p->bufsize);
    for (int i = 0; i < p->num_vars; ++i)
        packerSetVar(p->vars + i, state[i], buffer);
}

void pddlFDRStatePackerUnpack(const pddl_fdr_state_packer_t *p,
                              const void *buffer,
                              int *state)
{
    for (int i = 0; i < p->num_vars; ++i)
        state[i] = packerGetVar(p->vars + i, buffer);
}


static int packerBitsNeeded(int range)
{
    pddl_fdr_packer_word_t max_val = range - 1;
    int num = PDDL_FDR_PACKER_WORD_BITS;
    max_val = PDDL_MAX(1, max_val);

    for (; !(max_val & PDDL_FDR_PACKER_WORD_SET_HI_BIT); --num, max_val <<= 1);
    return num;
}

static pddl_fdr_packer_word_t packerVarMask(int bitlen, int shift)
{
    pddl_fdr_packer_word_t mask1, mask2;

    mask1 = PDDL_FDR_PACKER_WORD_SET_ALL_BITS << shift;
    mask2 = PDDL_FDR_PACKER_WORD_SET_ALL_BITS
                >> (PDDL_FDR_PACKER_WORD_BITS - shift - bitlen);
    return mask1 & mask2;
}

static void packerSetVar(const pddl_fdr_state_packer_var_t *var,
                         pddl_fdr_packer_word_t val,
                         void *buffer)
{
    pddl_fdr_packer_word_t *buf;
    val = (val << var->shift) & var->mask;
    buf = ((pddl_fdr_packer_word_t *)buffer) + var->pos;
    *buf = (*buf & var->clear_mask) | val;
}

static int packerGetVar(const pddl_fdr_state_packer_var_t *var,
                        const void *buffer)
{
    pddl_fdr_packer_word_t val;
    pddl_fdr_packer_word_t *buf;
    buf = ((pddl_fdr_packer_word_t *)buffer) + var->pos;
    val = *buf;
    val = (val & var->mask) >> var->shift;
    return val;
}

static int cmpByBitlen(const void *a, const void *b, void *_)
{
    const pddl_fdr_state_packer_var_t *va, *vb;
    va = *(const pddl_fdr_state_packer_var_t **)a;
    vb = *(const pddl_fdr_state_packer_var_t **)b;
    int cmp = vb->bitlen - va->bitlen;
    // This stabilizes the sort because va and vb are both in the same
    // array so this sorts them by their respective index in the array.
    if (cmp == 0)
        return va - vb;
    return cmp;
}


static void sortedVarsInit(sorted_vars_t *sv, const pddl_fdr_vars_t *vars,
                           pddl_fdr_state_packer_var_t *pvar)
{
    // Allocate arrays
    sv->var = ALLOC_ARR(pddl_fdr_state_packer_var_t *, vars->var_size);
    sv->var_size = vars->var_size;
    sv->var_left = sv->var_size;

    for (int i = 0; i < vars->var_size; ++i)
        sv->var[i] = pvar + i;

    // Sort array
    pddlSort(sv->var, sv->var_size, sizeof(pddl_fdr_state_packer_var_t *),
             cmpByBitlen, NULL);
}

static void sortedVarsFree(sorted_vars_t *sv)
{
    if (sv->var)
        FREE(sv->var);
}

static pddl_fdr_state_packer_var_t *sortedVarsNext(sorted_vars_t *sv,
                                                   int filled_bits)
{
    for (int i = 0; sv->var_left > 0 && i < sv->var_size; ++i){
        if (sv->var[i]->pos == -1
                && sv->var[i]->bitlen + filled_bits <= PDDL_FDR_PACKER_WORD_BITS){
            --sv->var_left;
            return sv->var[i];
        }
    }

    return NULL;
}
