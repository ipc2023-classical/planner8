/***
 * Copyright (c)2022 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

#ifndef __PDDL_FM_ARR_H__
#define __PDDL_FM_ARR_H__

#include <pddl/fm.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct pddl_fm_arr {
    const pddl_fm_t **fm;
    int size;
    int alloc;
};
typedef struct pddl_fm_arr pddl_fm_arr_t;

#define PDDL_FM_ARR_INIT { 0 }

void pddlFmArrInit(pddl_fm_arr_t *ca);
void pddlFmArrFree(pddl_fm_arr_t *ca);
void pddlFmArrAdd(pddl_fm_arr_t *ca, const pddl_fm_t *c);
void pddlFmArrInitCopy(pddl_fm_arr_t *dst, const pddl_fm_arr_t *src);

#define PDDL_FM_ARR_FOR_EACH_ATOM(COND_ARR, ATOM) \
    for (int ___cai = 0; ___cai < (COND_ARR)->size; ++___cai) \
        if (pddlFmIsAtom((COND_ARR)->fm[___cai]) \
                && ((ATOM) = PDDL_FM_CAST((COND_ARR)->fm[___cai], atom)))


#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_FM_ARR_H__ */
