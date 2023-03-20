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

#ifndef __PDDL_SEARCH_INTERNAL_H__
#define __PDDL_SEARCH_INTERNAL_H__

#include "pddl/search.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef void (*pddl_search_del_fn)(pddl_search_t *);
typedef int (*pddl_search_init_step_fn)(pddl_search_t *);
typedef int (*pddl_search_step_fn)(pddl_search_t *);
typedef int (*pddl_search_extract_plan_fn)(pddl_search_t *, pddl_plan_t *);
typedef void (*pddl_search_stat_fn)(const pddl_search_t *,
                                    pddl_search_stat_t *stat);
struct pddl_search {
    pddl_search_del_fn fn_del;
    pddl_search_init_step_fn fn_init_step;
    pddl_search_step_fn fn_step;
    pddl_search_extract_plan_fn fn_extract_plan;
    pddl_search_stat_fn fn_stat;
};

void _pddlSearchInit(pddl_search_t *s,
                     pddl_search_del_fn fn_del,
                     pddl_search_init_step_fn fn_init_step,
                     pddl_search_step_fn fn_step,
                     pddl_search_extract_plan_fn fn_extract_plan,
                     pddl_search_stat_fn fn_stat);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_SEARCH_INTERNAL_H__ */
