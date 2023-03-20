/***
 * cpddl
 * -------
 * Copyright (c)2016 Daniel Fiser <danfis@danfis.cz>,
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

#ifndef __PDDL_OBJ_H__
#define __PDDL_OBJ_H__

#include <pddl/common.h>
#include <pddl/htable.h>
#include <pddl/type.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct pddl_obj {
    char *name;       /*!< Name of the object */
    int type;         /*!< Type of the object */
    int is_constant;  /*!< True if it is constant (defined in domain) */
    int is_private;   /*!< True if the object is private to an agent */
    pddl_obj_id_t owner; /*!< ID of the object corresponding to an agent in
                              unfactored privacy model or PDDL_OBJ_ID_UNDEF */
    int is_agent;     /*!< True if the object correspondnds to an agent in
                           unfactored privacy model */
};
typedef struct pddl_obj pddl_obj_t;

struct pddl_objs {
    pddl_obj_t *obj;
    int obj_size;
    int obj_alloc;
    pddl_htable_t *htable;
};
typedef struct pddl_objs pddl_objs_t;

/**
 * Parse :constants and :objects from domain and problem PDDLs.
 */
int pddlObjsParse(pddl_t *pddl, pddl_err_t *err);

/**
 * Initialize dst as a deep copy of src.
 */
void pddlObjsInitCopy(pddl_objs_t *dst, const pddl_objs_t *src);

/**
 * Frees allocated resources.
 */
void pddlObjsFree(pddl_objs_t *objs);

/**
 * Returns ID of the object of the specified name.
 */
pddl_obj_id_t pddlObjsGet(const pddl_objs_t *objs, const char *name);

/**
 * Adds a new obj at the end of the array.
 */
pddl_obj_t *pddlObjsAdd(pddl_objs_t *objs, const char *name);

/**
 * Remap object IDs and remove those where remap[id] == -1
 */
void pddlObjsRemap(pddl_objs_t *objs, const pddl_obj_id_t *remap);

/**
 * Remap type IDs assuming all object types are preserved.
 */
void pddlObjsRemapTypes(pddl_objs_t *objs, const int *remap_type);

/**
 * Print formated objects.
 */
void pddlObjsPrint(const pddl_objs_t *objs, FILE *fout);

/**
 * Print objects in PDDL (:constants ) format.
 */
void pddlObjsPrintPDDLConstants(const pddl_objs_t *objs,
                                const pddl_types_t *ts,
                                FILE *fout);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_OBJ_H__ */
