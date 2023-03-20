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

#ifndef __PDDL_SET_H__
#define __PDDL_SET_H__

#include <pddl/hashset.h>
#include <pddl/iset.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct pddl_set_iset {
    pddl_hashset_t set;
};
typedef struct pddl_set_iset pddl_set_iset_t;

#define PDDL_SET_ISET_FOR_EACH_ID(SS, ID) \
    for (int ID = 0; ID < (SS)->set.size; ++ID)
#define PDDL_SET_ISET_FOR_EACH_ID_SET(SS, ID, SET) \
    for (int ID = 0; \
            ID < (SS)->set.size \
                && ((SET) = pddlSetISetGet((SS), ID)); \
            ++ID)
#define PDDL_SET_ISET_FOR_EACH(SS, SET) \
    PDDL_SET_ISET_FOR_EACH_ID_SET(SS, __set_iset_i, SET)

_pddl_inline void pddlSetISetInit(pddl_set_iset_t *ss)
{
    pddlHashSetInitISet(&ss->set);
}

_pddl_inline void pddlSetISetFree(pddl_set_iset_t *ss)
{
    pddlHashSetFree(&ss->set);
}

_pddl_inline int pddlSetISetAdd(pddl_set_iset_t *ss, const pddl_iset_t *set)
{
    return pddlHashSetAdd(&ss->set, set);
}

_pddl_inline int pddlSetISetFind(pddl_set_iset_t *ss, const pddl_iset_t *set)
{
    return pddlHashSetFind(&ss->set, set);
}

_pddl_inline const pddl_iset_t *pddlSetISetGet(const pddl_set_iset_t *ss, int id)
{
    return (const pddl_iset_t *)pddlHashSetGet(&ss->set, id);
}

_pddl_inline int pddlSetISetSize(const pddl_set_iset_t *ss)
{
    return ss->set.size;
}

_pddl_inline void pddlSetISetUnion(pddl_set_iset_t *dst,
                                   const pddl_set_iset_t *src)
{
    for (int i = 0; i < pddlSetISetSize(src); ++i)
        pddlSetISetAdd(dst, pddlSetISetGet(src, i));
}

void pddlISetPrintCompressed(const pddl_iset_t *set, FILE *fout);
void pddlISetPrint(const pddl_iset_t *set, FILE *fout);
void pddlISetPrintln(const pddl_iset_t *set, FILE *fout);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_SET_H__ */
