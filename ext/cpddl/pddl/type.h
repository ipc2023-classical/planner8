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

#ifndef __PDDL_TYPE_H__
#define __PDDL_TYPE_H__

#include <pddl/iset.h>
#include <pddl/common.h>
#include <pddl/objset.h>
#include <pddl/lisp.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** Forward declaration */
struct pddl_objs;

struct pddl_type {
    char *name;        /*!< Name of the type */
    int parent;        /*!< ID of the parent type */
    pddl_iset_t child;  /*!< IDs of children types */
    pddl_iset_t either; /*!< type IDs for special (either ...) type */
    pddl_objset_t obj; /*!< Objs of this type */
};
typedef struct pddl_type pddl_type_t;

struct pddl_types {
    pddl_type_t *type;
    int type_size;
    int type_alloc;

    char *obj_type_map;
    size_t obj_type_map_memsize;
};
typedef struct pddl_types pddl_types_t;

/**
 * Parses :types into type array.
 */
int pddlTypesParse(pddl_t *pddl, pddl_err_t *err);

/**
 * Initialize dst as a deep copy of src.
 */
void pddlTypesInitCopy(pddl_types_t *dst, const pddl_types_t *src);

/**
 * Frees allocated resources.
 */
void pddlTypesFree(pddl_types_t *types);

/**
 * Returns ID of the type corresponding to the name.
 */
int pddlTypesGet(const pddl_types_t *t, const char *name);

/**
 * Adds a new type with the given name and the specified parent.
 * If a type with the same name already exists, nothing is added and its ID
 * is returned, otherwise the new type's ID is returned.
 */
int pddlTypesAdd(pddl_types_t *t, const char *name, int parent);

/**
 * Prints list of types to the specified output.
 */
void pddlTypesPrint(const pddl_types_t *t, FILE *fout);

/**
 * Returns true if the specified type is (either ...) type.
 */
int pddlTypesIsEither(const pddl_types_t *ts, int tid);

/**
 * Record the given object as being of the given type.
 */
void pddlTypesAddObj(pddl_types_t *ts, pddl_obj_id_t obj_id, int type_id);

/**
 * Build mapping for fast testing whether an object is of a specified type.
 */
void pddlTypesBuildObjTypeMap(pddl_types_t *ts, int obj_size);

/**
 * Returns list of object IDs of the specified type.
 */
const pddl_obj_id_t *pddlTypesObjsByType(const pddl_types_t *ts, int type_id,
                                         int *size);

/**
 * Returns number of objects of the specified type.
 */
int pddlTypeNumObjs(const pddl_types_t *ts, int type_id);

/**
 * Returns idx's object of the given type.
 */
int pddlTypeGetObj(const pddl_types_t *ts, int type_id, int idx);

/**
 * Returns true if the object compatible with the specified type.
 */
int pddlTypesObjHasType(const pddl_types_t *ts, int type, pddl_obj_id_t obj);

/**
 * Returns type ID from the lisp node or -1 if error occured.
 * (either ...) types are created if necessary.
 */
int pddlTypeFromLispNode(pddl_types_t *ts, const pddl_lisp_node_t *node,
                         pddl_err_t *err);

/**
 * Returns true if parent is a parent type of child type.
 */
int pddlTypesIsParent(const pddl_types_t *ts, int child, int parent);

/**
 * Returns true if t1 and t2 are disjunct types, i.e., there cannot be
 * object of both types at the same time.
 */
int pddlTypesAreDisjunct(const pddl_types_t *ts, int t1, int t2);

/**
 * Returns true if D(t1) \subseteq D(t2)
 */
int pddlTypesIsSubset(const pddl_types_t *ts, int t1, int t2);

/**
 * Returns true if {type} is a minimal type, i.e., it has no sub-types.
 */
int pddlTypesIsMinimal(const pddl_types_t *ts, int type);

/**
 * Returns true if:
 * 1. for every pair of types t1,t2 it holds that D(t1) \subseteq D(t2) or
 *    D(t2) \subseteq D(t1) or D(t1) \cap D(t2) = \emptyset; and
 * 2. union of minimal types (i.e., without subtypes) equals the whole set
 *    of objects.
 */
int pddlTypesHasStrictPartitioning(const pddl_types_t *ts,
                                   const struct pddl_objs *obj);

/**
 * Remap objects
 */
void pddlTypesRemapObjs(pddl_types_t *ts, const pddl_obj_id_t *remap);

/**
 * Returns a type ID that is complement of t with respect to p, or -1 if
 * there is no such type.
 * That is, x is complement of t with respect to p if p is a parent of t
 * and D(x) = D(p) \setminus D(t).
 */
int pddlTypesComplement(const pddl_types_t *ts, int t, int p);

/**
 * Remove empty types, but always keep the top type "object"
 */
void pddlTypesRemoveEmpty(pddl_types_t *ts, int obj_size, int *type_remap);

/**
 * Print requirements in PDDL format.
 */
void pddlTypesPrintPDDL(const pddl_types_t *ts, FILE *fout);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_TYPE_H__ */
