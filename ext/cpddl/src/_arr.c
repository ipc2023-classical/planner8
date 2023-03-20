/***
 * Copyright (c)2017 Daniel Fiser <danfis@danfis.cz>
 *
 *  This file is part of cpddl.
 *
 *  Distributed under the OSI-approved BSD License (the "License");
 *  see accompanying file BDS-LICENSE for details or see
 *  <http://www.opensource.org/licenses/bsd-license.php>.
 *
 *  This software is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the License for more information.
 */

#include "internal.h"
#include "pddl/arr.h"

void pddlArrFree(pddl_arr_t *a)
{
    if (a->arr != NULL)
        FREE(a->arr);
}

void pddlArrRealloc(pddl_arr_t *a, int size)
{
    while (size > a->alloc){
        if (a->alloc == 0)
            a->alloc = 1;
        a->alloc *= 2;
    }
    a->arr = REALLOC_ARR(a->arr, TYPE, a->alloc);
}
