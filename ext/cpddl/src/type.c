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

#include "pddl/pddl.h"
#include "pddl/type.h"
#include "pddl/obj.h"
#include "lisp_err.h"
#include "internal.h"

static const char *object_name = "object";

static void pddlTypeInit(pddl_type_t *t)
{
    ZEROIZE(t);
}

static void pddlTypeFree(pddl_type_t *t)
{
    if (t->name != NULL)
        FREE(t->name);
    pddlISetFree(&t->child);
    pddlISetFree(&t->either);
    pddlObjSetFree(&t->obj);
}

static void pddlTypeInitCopy(pddl_type_t *dst, const pddl_type_t *src)
{
    if (src->name != NULL)
        dst->name = STRDUP(src->name);
    dst->parent = src->parent;
    pddlISetUnion(&dst->child, &src->child);
    pddlISetUnion(&dst->either, &src->either);
    pddlObjSetInit(&dst->obj);
    pddlObjSetUnion(&dst->obj, &src->obj);
}

int pddlTypesGet(const pddl_types_t *t, const char *name)
{
    for (int i = 0; i < t->type_size; ++i){
        if (strcmp(t->type[i].name, name) == 0)
            return i;
    }

    return -1;
}


int pddlTypesAdd(pddl_types_t *t, const char *name, int parent)
{
    int id;

    if ((id = pddlTypesGet(t, name)) != -1)
        return id;

    if (t->type_size >= t->type_alloc){
        if (t->type_alloc == 0)
            t->type_alloc = 2;
        t->type_alloc *= 2;
        t->type = REALLOC_ARR(t->type, pddl_type_t, t->type_alloc);
    }

    id = t->type_size++;
    pddl_type_t *type = t->type + id;
    pddlTypeInit(type);
    if (name != NULL)
        type->name = STRDUP(name);
    type->parent = parent;
    if (parent >= 0)
        pddlISetAdd(&t->type[parent].child, id);
    return id;
}

static int setCB(const pddl_lisp_node_t *root,
                 int child_from, int child_to, int child_type, void *ud,
                 pddl_err_t *err)
{
    pddl_types_t *t = ud;
    int pid;

    pid = 0;
    if (child_type >= 0){
        if (root->child[child_type].value == NULL){
            ERR_LISP_RET2(err, -1, root->child + child_type,
                          "Invalid typed list. Unexpected expression");
        }
        pid = pddlTypesAdd(t, root->child[child_type].value, 0);
    }

    for (int i = child_from; i < child_to; ++i){
        // This is checked in pddlLispParseTypedList()
        ASSERT(root->child[i].value != NULL);
        if (root->child[i].value == NULL)
            ERR_LISP_RET2(err, -1, root->child + i, "Unexpected expression");

        pddlTypesAdd(t, root->child[i].value, pid);
    }

    return 0;
}

int pddlTypesParse(pddl_t *pddl, pddl_err_t *e)
{
    pddl_types_t *types;
    const pddl_lisp_node_t *n;

    // Create a default "object" type
    types = &pddl->type;
    pddlTypesAdd(types, object_name, -1);

    n = pddlLispFindNode(&pddl->domain_lisp->root, PDDL_KW_TYPES);
    if (n != NULL){
        if (pddlLispParseTypedList(n, 1, n->child_size, setCB, types, e) != 0){
            PDDL_TRACE_PREPEND_RET(e, -1, "Invalid definition of :types in %s: ",
                                   pddl->domain_lisp->filename);
        }
    }

    // TODO: Check circular dependency on types
    return 0;
}

void pddlTypesInitCopy(pddl_types_t *dst, const pddl_types_t *src)
{
    ZEROIZE(dst);
    dst->type_size = src->type_size;
    dst->type_alloc = src->type_alloc;
    dst->type = CALLOC_ARR(pddl_type_t, dst->type_alloc);
    for (int i = 0; i < dst->type_size; ++i)
        pddlTypeInitCopy(dst->type + i, src->type + i);

    if (src->obj_type_map != NULL){
        dst->obj_type_map_memsize = src->obj_type_map_memsize;
        dst->obj_type_map = ALLOC_ARR(char, dst->obj_type_map_memsize);
        memcpy(dst->obj_type_map, src->obj_type_map, dst->obj_type_map_memsize);
    }
}

void pddlTypesFree(pddl_types_t *types)
{
    for (int i = 0; i < types->type_size; ++i)
        pddlTypeFree(types->type + i);
    if (types->type != NULL)
        FREE(types->type);

    if (types->obj_type_map != NULL)
        FREE(types->obj_type_map);
}

void pddlTypesPrint(const pddl_types_t *t, FILE *fout)
{
    fprintf(fout, "Type[%d]:\n", t->type_size);
    for (int i = 0; i < t->type_size; ++i){
        fprintf(fout, "    [%d]: %s, parent: %d", i,
                t->type[i].name, t->type[i].parent);
        fprintf(fout, "\n");
    }

    fprintf(fout, "Obj-by-Type:\n");
    for (int i = 0; i < t->type_size; ++i){
        fprintf(fout, "    [%d]:", i);
        pddl_obj_id_t o;
        PDDL_OBJSET_FOR_EACH(&t->type[i].obj, o)
            fprintf(fout, " %d", (int)o);
        fprintf(fout, "\n");
    }
}

int pddlTypesIsEither(const pddl_types_t *ts, int tid)
{
    return pddlISetSize(&ts->type[tid].either) > 0;
}

void pddlTypesAddObj(pddl_types_t *ts, pddl_obj_id_t obj_id, int type_id)
{
    pddl_type_t *t = ts->type + type_id;
    pddlObjSetAdd(&t->obj, obj_id);
    if (t->parent != -1)
        pddlTypesAddObj(ts, obj_id, t->parent);
}

void pddlTypesBuildObjTypeMap(pddl_types_t *ts, int obj_size)
{
    if (ts->obj_type_map != NULL)
        FREE(ts->obj_type_map);
    ts->obj_type_map = CALLOC_ARR(char, obj_size * ts->type_size);
    ts->obj_type_map_memsize = obj_size * ts->type_size;
    for (int type_id = 0; type_id < ts->type_size; ++type_id){
        const pddl_objset_t *tobj = &ts->type[type_id].obj;
        pddl_obj_id_t obj;
        PDDL_OBJSET_FOR_EACH(tobj, obj){
            ts->obj_type_map[obj * ts->type_size + type_id] = 1;
        }
    }
}

const pddl_obj_id_t *pddlTypesObjsByType(const pddl_types_t *ts, int type_id,
                                         int *size)
{
    if (size != NULL)
        *size = ts->type[type_id].obj.size;
    return ts->type[type_id].obj.s;
}

int pddlTypeNumObjs(const pddl_types_t *ts, int type_id)
{
    return pddlObjSetSize(&ts->type[type_id].obj);
}

int pddlTypeGetObj(const pddl_types_t *ts, int type_id, int idx)
{
    return pddlObjSetGet(&ts->type[type_id].obj, idx);
}

int pddlTypesObjHasType(const pddl_types_t *ts, int type, pddl_obj_id_t obj)
{
    if (ts->obj_type_map != NULL){
        return ts->obj_type_map[obj * ts->type_size + type];

    }else{
        const pddl_obj_id_t *objs;
        int size;

        objs = pddlTypesObjsByType(ts, type, &size);
        for (int i = 0; i < size; ++i){
            if (objs[i] == obj)
                return 1;
        }
        return 0;
    }
}


static int pddlTypesEither(pddl_types_t *ts, const pddl_iset_t *either)
{
    int tid;

    // Try to find already created (either ...) type
    for (int i = 0; i < ts->type_size; ++i){
        if (pddlTypesIsEither(ts, i)
                && pddlISetEq(&ts->type[i].either, either)){
            return i;
        }
    }

    // Construct a name of the (either ...) type
    char *name, *cur;
    int eid;
    int slen = 0;
    PDDL_ISET_FOR_EACH(either, eid)
        slen += 1 + strlen(ts->type[eid].name);
    slen += 2 + 6 + 1;
    name = cur = ALLOC_ARR(char, slen);
    cur += sprintf(cur, "(either");
    PDDL_ISET_FOR_EACH(either, eid)
        cur += sprintf(cur, " %s", ts->type[eid].name);
    sprintf(cur, ")");

    tid = pddlTypesAdd(ts, name, -1);
    if (name != NULL)
        FREE(name);
    pddl_type_t *type = ts->type + tid;
    pddlISetUnion(&type->child, either);
    pddlISetUnion(&type->either, either);

    // Merge obj IDs from all simple types from which this (either ...)
    // type consists of.
    PDDL_ISET_FOR_EACH(either, eid){
        const pddl_type_t *et = ts->type + eid;
        pddl_obj_id_t obj;
        PDDL_OBJSET_FOR_EACH(&et->obj, obj)
            pddlTypesAddObj(ts, obj, tid);
    }

    return tid;
}


int pddlTypeFromLispNode(pddl_types_t *ts, const pddl_lisp_node_t *node,
                         pddl_err_t *err)
{
    int tid;

    if (node->value != NULL){
        tid = pddlTypesGet(ts, node->value);
        if (tid < 0)
            ERR_LISP_RET(err, -1, node, "Unkown type `%s'", node->value);
        return tid;
    }

    if (node->child_size < 2 || node->child[0].kw != PDDL_KW_EITHER)
        ERR_LISP_RET2(err, -1, node, "Unknown expression");

    if (node->child_size == 2 && node->child[1].value != NULL)
        return pddlTypeFromLispNode(ts, node->child + 1, err);

    PDDL_ISET(either);
    for (int i = 1; i < node->child_size; ++i){
        if (node->child[i].value == NULL){
            ERR_LISP_RET2(err, -1, node->child + i,
                          "Invalid (either ...) expression");
        }
        tid = pddlTypesGet(ts, node->child[i].value);
        if (tid < 0){
            ERR_LISP_RET(err, -1, node->child + i, "Unkown type `%s'",
                         node->child[i].value);
        }

        pddlISetAdd(&either, tid);
    }

    tid = pddlTypesEither(ts, &either);
    pddlISetFree(&either);
    return tid;
}

int pddlTypesIsParent(const pddl_types_t *ts, int child, int parent)
{
    const pddl_type_t *tparent = ts->type + parent;
    int eid;

    for (int cur_type = child; cur_type >= 0;){
        if (cur_type == parent)
            return 1;
        PDDL_ISET_FOR_EACH(&tparent->either, eid){
            if (cur_type == eid)
                return 1;
        }
        cur_type = ts->type[cur_type].parent;
    }

    return 0;
}

int pddlTypesAreDisjunct(const pddl_types_t *ts, int t1, int t2)
{
    return !pddlTypesIsParent(ts, t1, t2) && !pddlTypesIsParent(ts, t2, t1);
}

int pddlTypesIsSubset(const pddl_types_t *ts, int t1id, int t2id)
{
    const pddl_type_t *t1 = ts->type + t1id;
    const pddl_type_t *t2 = ts->type + t2id;
    return pddlObjSetIsSubset(&t1->obj, &t2->obj);
}

int pddlTypesIsMinimal(const pddl_types_t *ts, int type)
{
    return pddlISetSize(&ts->type[type].child) == 0;
}

int pddlTypesHasStrictPartitioning(const pddl_types_t *ts,
                                   const pddl_objs_t *obj)
{
    int is_strict = 0;

    for (int t1 = 0; t1 < ts->type_size; ++t1){
        if (pddlISetSize(&ts->type[t1].either) > 0)
            return 0;
        for (int t2 = 0; t2 < ts->type_size; ++t2){
            if (t1 == t2)
                continue;
            if (!pddlTypesAreDisjunct(ts, t1, t2)
                    && !pddlTypesIsSubset(ts, t1, t2)
                    && !pddlTypesIsSubset(ts, t2, t1))
                return 0;
        }
    }

    PDDL_OBJSET(all);
    for (int ti = 0; ti < ts->type_size; ++ti){
        if (pddlISetSize(&ts->type[ti].child) == 0
                && pddlISetSize(&ts->type[ti].either) == 0){
            pddlObjSetUnion(&all, &ts->type[ti].obj);
        }
    }
    if (pddlObjSetSize(&all) == obj->obj_size)
        is_strict = 1;
    pddlObjSetFree(&all);
    return is_strict;
}

void pddlTypesRemapObjs(pddl_types_t *ts,
                        const pddl_obj_id_t *remap)
{
    int num_objs = 0;
    for (int ti = 0; ti < ts->type_size; ++ti){
        pddl_type_t *t = ts->type + ti;
        PDDL_OBJSET(newset);
        pddl_obj_id_t obj;
        PDDL_OBJSET_FOR_EACH(&t->obj, obj){
            if (remap[obj] >= 0){
                pddlObjSetAdd(&newset, remap[obj]);
                num_objs = PDDL_MAX(num_objs, remap[obj] + 1);
            }
        }

        pddlObjSetFree(&t->obj);
        t->obj = newset;
    }

    if (ts->obj_type_map != NULL)
        pddlTypesBuildObjTypeMap(ts, num_objs);
}

int pddlTypesComplement(const pddl_types_t *ts, int t, int p)
{
    int t_size = pddlTypeNumObjs(ts, t);
    int p_size = pddlTypeNumObjs(ts, p);

    for (int c = 0; c < ts->type_size; ++c){
        if (pddlTypesAreDisjunct(ts, t, c)
                && pddlTypesIsSubset(ts, c, p)
                && pddlTypeNumObjs(ts, c) == p_size - t_size)
            return c;
    }
    return -1;
}

void pddlTypesRemoveEmpty(pddl_types_t *ts, int obj_size, int *type_remap)
{
    PDDL_ISET(rm);
    int type_size = 1;
    type_remap[0] = 0;
    for (int t = 1; t < ts->type_size; ++t){
        if (pddlTypeNumObjs(ts, t) == 0){
            type_remap[t] = -1;
            pddlISetAdd(&rm, t);
        }else{
            type_remap[t] = type_size++;
        }
    }
    if (type_size == ts->type_size)
        return;

    for (int t = 0; t < ts->type_size; ++t){
        pddl_type_t *type = ts->type + t;
        if (type_remap[t] >= 0){
            if (type->parent >= 0)
                type->parent = type_remap[type->parent];
            pddlISetMinus(&type->child, &rm);
            pddlISetRemap(&type->child, type_remap);
            pddlISetMinus(&type->either, &rm);
            pddlISetRemap(&type->either, type_remap);
            ts->type[type_remap[t]] = *type;
        }else{
            pddlTypeFree(type);
        }
    }
    pddlISetFree(&rm);
    ts->type_size = type_size;

    if (ts->obj_type_map != NULL)
        pddlTypesBuildObjTypeMap(ts, obj_size);
}

void pddlTypesPrintPDDL(const pddl_types_t *ts, FILE *fout)
{
    int q[ts->type_size];
    int qi = 0, qsize = 0;

    fprintf(fout, "(:types\n");
    for (int i = 0; i < ts->type_size; ++i){
        if (ts->type[i].parent == 0)
            q[qsize++] = i;
    }

    for (qi = 0; qi < qsize; ++qi){
        fprintf(fout, "    %s - %s\n",
                ts->type[q[qi]].name,
                ts->type[ts->type[q[qi]].parent].name);
        for (int i = 0; i < ts->type_size; ++i){
            if (ts->type[i].parent == q[qi] && !pddlTypesIsEither(ts, i))
                q[qsize++] = i;
        }
    }

    fprintf(fout, ")\n");
}
