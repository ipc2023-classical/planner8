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

#include "internal.h"
#include "pddl/bitset.h"

void pddlBitsetInit(pddl_bitset_t *b, int bitsize)
{
    ZEROIZE(b);

    b->bitsize = bitsize;
    b->wordsize = bitsize / PDDL_BITSET_WORD_BITSIZE;
    if (bitsize % PDDL_BITSET_WORD_BITSIZE != 0)
        b->wordsize += 1;

    int last_word_bitsize = bitsize % PDDL_BITSET_WORD_BITSIZE;
    if (last_word_bitsize > 0){
        b->last_word_mask = 1;
        for (int i = 1; i < last_word_bitsize; ++i)
            b->last_word_mask = (b->last_word_mask << (pddl_bitset_word_t)1) | (pddl_bitset_word_t)1;
    }else{
        b->last_word_mask = ~((pddl_bitset_word_t)0);
    }

    //b->bitset = ALLOC_ALIGN_ARR(pddl_bitset_word_t, b->wordsize, 0x100);
    b->bitset = CALLOC_ARR(pddl_bitset_word_t, b->wordsize);
}

void pddlBitsetFree(pddl_bitset_t *b)
{
    if (b->bitset != NULL)
        FREE(b->bitset);
}
