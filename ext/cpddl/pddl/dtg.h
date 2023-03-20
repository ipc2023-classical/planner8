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

#ifndef __PDDL_DTG_H__
#define __PDDL_DTG_H__

#include <pddl/mgroup.h>
#include <pddl/strips_op.h>
#include <pddl/strips_fact_cross_ref.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * Find unreachable facts and operators in the domain transition graph
 * created for the given mutex group.
 * {cref} must have set .op_pre and .op_add.
 * DTG is not actually constructed.
 */
void pddlUnreachableInMGroupDTG(int init_fact,
                                const pddl_mgroup_t *mgroup,
                                const pddl_strips_ops_t *ops,
                                const pddl_strips_fact_cross_ref_t *cref,
                                pddl_iset_t *unreachable_facts,
                                pddl_iset_t *unreachable_ops);

/**
 * Calls pddlUnreachableInMGroupsDTG() for each mutex group.
 */
void pddlUnreachableInMGroupsDTGs(const pddl_strips_t *strips,
                                  const pddl_mgroups_t *mgroups,
                                  pddl_iset_t *unreachable_facts,
                                  pddl_iset_t *unreachable_ops,
                                  pddl_err_t *err);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_DTG_H__ */
