/***
 * Copyright (c)2022 Daniel Fiser <danfis@danfis.cz>
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

#ifndef __PDDL__CP_H__
#define __PDDL__CP_H__

#define OBJ_SAT 0
#define OBJ_MIN_COUNT_DIFF 1

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

int *pddlCPSolGet(pddl_cp_sol_t *sol, int sol_id);
void pddlCPSolAdd(const pddl_cp_t *cp, pddl_cp_sol_t *sol, const int *isol);
int *pddlCPSolAddEmpty(const pddl_cp_t *cp, pddl_cp_sol_t *sol);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __PDDL__CP_H__ */
