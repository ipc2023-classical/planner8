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

#ifndef __PDDL_DATALOG_PDDL_H__
#define __PDDL_DATALOG_PDDL_H__

#include <pddl/datalog.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

int pddlDatalogPddlMaxVarSize(const pddl_t *pddl);

void pddlDatalogPddlAddTypeRules(pddl_datalog_t *dl,
                                 const pddl_t *pddl,
                                 const unsigned *type_to_dlpred,
                                 const unsigned *obj_to_dlconst);

void pddlDatalogPddlAddEqRules(pddl_datalog_t *dl,
                               const pddl_t *pddl,
                               const unsigned *pred_to_dlpred,
                               const unsigned *obj_to_dlconst);

void pddlDatalogPddlAtomToDLAtom(pddl_datalog_t *dl,
                                 pddl_datalog_atom_t *dlatom,
                                 const pddl_fm_atom_t *atom,
                                 const unsigned *pred_to_dlpred,
                                 const unsigned *obj_to_dlconst,
                                 const unsigned *dlvar);

void pddlDatalogPddlSetActionTypeBody(pddl_datalog_t *dl,
                                      pddl_datalog_rule_t *rule,
                                      const pddl_t *pddl,
                                      const pddl_params_t *params,
                                      const pddl_fm_t *pre,
                                      const pddl_fm_t *pre2,
                                      unsigned *type_to_dlpred,
                                      const unsigned *dlvar);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_DATALOG_PDDL_H__ */
