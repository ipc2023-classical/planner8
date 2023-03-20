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

#ifndef ___PDDL_PROFILE_H__
#define ___PDDL_PROFILE_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void pddlProfileStart(int slot);
void pddlProfileStop(int slot);
void pddlProfilePrint(void);

#define PROF(slot) pddlProfileStart(slot)
#define PROFE(slot) pddlProfileStop(slot)
#define PROF_PRINT pddlProfilePrint();

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* ___PDDL_PROFILE_H__ */
