/***
 * cpddl
 * -------
 * Copyright (c)2017 Daniel Fiser <danfis@danfis.cz>,
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
#include "pddl/fm_arr.h"

void pddlFmArrInit(pddl_fm_arr_t *ca)
{
    ZEROIZE(ca);
}

void pddlFmArrFree(pddl_fm_arr_t *ca)
{
    if (ca->fm)
        FREE(ca->fm);
}

void pddlFmArrAdd(pddl_fm_arr_t *ca, const pddl_fm_t *c)
{
    if (ca->size >= ca->alloc){
        if (ca->alloc == 0)
            ca->alloc = 1;
        ca->alloc *= 2;
        ca->fm = REALLOC_ARR(ca->fm, const pddl_fm_t *, ca->alloc);
    }
    ca->fm[ca->size++] = c;
}

void pddlFmArrInitCopy(pddl_fm_arr_t *dst, const pddl_fm_arr_t *src)
{
    *dst = *src;
    if (src->fm != NULL){
        dst->fm = ALLOC_ARR(const pddl_fm_t *, dst->alloc);
        memcpy(dst->fm, src->fm, sizeof(pddl_fm_t *) * src->size);
    }
}
