/***
 * cpddl
 * -------
 * Copyright (c)2021 Daniel Fiser <danfis@danfis.cz>,
 * Saarland University, and
 * Czech Technical University in Prague.
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

#ifndef __PDDL_STRIPS_GROUND_SQL_H__
#define __PDDL_STRIPS_GROUND_SQL_H__

#include <pddl/common.h>
#include <pddl/pddl_struct.h>
#include <pddl/strips.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * Ground PDDL into STRIPS.
 */
int pddlStripsGroundSql(pddl_strips_t *strips,
                        const pddl_t *pddl,
                        const pddl_ground_config_t *cfg,
                        pddl_err_t *err);

int pddlStripsGroundSqlLayered(const pddl_t *pddl,
                               const pddl_ground_config_t *cfg,
                               int max_layers,
                               int max_atoms,
                               pddl_strips_t *strips,
                               pddl_ground_atoms_t *ground_atoms,
                               pddl_err_t *err);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_STRIPS_GROUND_SQL_H__ */
