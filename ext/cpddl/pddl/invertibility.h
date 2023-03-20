/***
 * cpddl
 * -------
 * Copyright (c)2020 Daniel Fiser <danfis@danfis.cz>,
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

#ifndef __PDDL_INVERTIBILITY_H__
#define __PDDL_INVERTIBILITY_H__

#include <pddl/mg_strips.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

int pddlRSEInvertibleFacts(const pddl_strips_t *strips,
                           const pddl_mgroups_t *fam_groups,
                           pddl_iset_t *invertible_facts,
                           pddl_err_t *err);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_INVERTIBILITY_H__ */
