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

#include "pddl/hfunc.h"
#include "pddl/pddl.h"
#include "pddl/obj.h"
#include "pddl/require_flags.h"
#include "lisp_err.h"
#include "internal.h"

void pddlObjFree(pddl_obj_t *obj)
{
    if (obj->name != NULL)
        FREE(obj->name);
    obj->name = NULL;
}

struct obj_key {
    pddl_obj_id_t obj_id;
    const char *name;
    uint32_t hash;
    pddl_list_t htable;
};
typedef struct obj_key obj_key_t;

static pddl_htable_key_t objHash(const pddl_list_t *key, void *_)
{
    const obj_key_t *obj = pddl_container_of(key, obj_key_t, htable);
    return obj->hash;
}

static int objEq(const pddl_list_t *key1, const pddl_list_t *key2, void *_)
{
    const obj_key_t *obj1 = pddl_container_of(key1, obj_key_t, htable);
    const obj_key_t *obj2 = pddl_container_of(key2, obj_key_t, htable);
    return strcmp(obj1->name, obj2->name) == 0;
}

struct _set_t {
    pddl_objs_t *objs;
    const pddl_types_t *types;
    int is_const;
};
typedef struct _set_t set_t;

static int setCB(const pddl_lisp_node_t *root,
                 int child_from, int child_to, int child_type, void *ud,
                 pddl_err_t *err)
{
    pddl_objs_t *objs = ((set_t *)ud)->objs;
    pddl_obj_t *o;
    const pddl_types_t *types = ((set_t *)ud)->types;
    int is_const = ((set_t *)ud)->is_const;
    int tid;

    tid = 0;
    if (child_type >= 0){
        if (root->child[child_type].value == NULL){
            ERR_LISP_RET2(err, -1, root->child + child_type,
                          "Expecting type name");
        }

        tid = pddlTypesGet(types, root->child[child_type].value);
        if (tid < 0){
            ERR_LISP_RET(err, -1, root->child + child_type, "Invalid type `%s'",
                         root->child[child_type].value);
        }
    }


    for (int i = child_from; i < child_to; ++i){
        o = pddlObjsAdd(objs, root->child[i].value);
        if (o == NULL){
            // TODO: Configure warn/err
            ERR_LISP_RET(err, -1, root->child + i, "Duplicate object `%s'",
                         root->child[i].value);
        }

        o->type = tid;
        o->is_constant = is_const;
        o->is_private = 0;
        o->owner = PDDL_OBJ_ID_UNDEF;
        o->is_agent = 0;
    }

    return 0;
}

static int parse(pddl_t *pddl, const pddl_lisp_t *lisp, int kw, int is_const,
                 pddl_err_t *err)

{
    if (lisp == NULL)
        return 0;

    const pddl_lisp_node_t *n;
    int to;
    set_t set;

    n = pddlLispFindNode(&lisp->root, kw);
    if (n == NULL)
        return 0;

    // Find out if there are :private objects -- according to BNF the
    // :private definitions must be at the end of the :constants or
    // :objects expression, so we just need to determine the first :private
    // child.
    for (to = 1; to < n->child_size; ++to){
        if (n->child[to].child_size > 0
                && n->child[to].child[0].kw == PDDL_KW_PRIVATE)
            break;
    }

    set.objs = &pddl->obj;
    set.types = &pddl->type;
    set.is_const = is_const;
    if (pddlLispParseTypedList(n, 1, to, setCB, &set, err) != 0){
        if (is_const){
            PDDL_TRACE_PREPEND(err, "Invalid definition of :constants in %s: ",
                               lisp->filename);
        }else{
            PDDL_TRACE_PREPEND(err, "Invalid definition of :objects in %s: ",
                               lisp->filename);
        }
        return -1;
    }

    if (is_const){
        LOG(err, ":constants parsed, num constants: %{parsed_consts}d",
            pddl->obj.obj_size);
    }

    return 0;
}

static int parsePrivate(pddl_t *pddl, const pddl_lisp_t *lisp, int kw,
                        pddl_err_t *err)
{
    if (lisp == NULL)
        return 0;

    const pddl_lisp_node_t *n, *p;
    int i, factor, pi, parse_from;
    pddl_obj_id_t owner;
    set_t set;

    factor = pddl->require.factored_privacy;
    parse_from = 2;
    if (factor)
        parse_from = 1;

    n = pddlLispFindNode(&lisp->root, PDDL_KW_OBJECTS);
    if (n == NULL)
        return 0;

    set.objs = &pddl->obj;
    set.types = &pddl->type;
    set.is_const = 0;
    for (i = 1; i < n->child_size; ++i){
        p = n->child + i;
        if (p->child_size == 0 || p->child[0].kw != PDDL_KW_PRIVATE)
            continue;

        pi = pddl->obj.obj_size;
        if (pddlLispParseTypedList(p, parse_from, p->child_size,
                                   setCB, &set, err) != 0){
            ERR_LISP_RET(err, -1, n->child + i, "Invalid definition of :private"
                         " :objects in %s.", lisp->filename);
        }

        owner = PDDL_OBJ_ID_UNDEF;
        if (!factor){
            owner = pddlObjsGet(&pddl->obj, p->child[1].value);
            if (owner < 0){
                ERR_LISP_RET(err, -1, n->child + i, "Invalid definition of"
                             " :private :objects in %s. Unknown owner `%s'.",
                             lisp->filename, p->child[1].value);
            }
            pddl->obj.obj[owner].is_agent = 1;
        }

        for (; pi < pddl->obj.obj_size; ++pi){
            pddl->obj.obj[pi].is_private = 1;
            pddl->obj.obj[pi].owner = owner;
        }
    }

    return 0;
}

int pddlObjsParse(pddl_t *pddl, pddl_err_t *err)
{
    const pddl_lisp_t *dom_lisp = pddl->domain_lisp;
    const pddl_lisp_t *prob_lisp = pddl->problem_lisp;
    int i;

    ZEROIZE(&pddl->obj);
    pddl->obj.htable = pddlHTableNew(objHash, objEq, NULL);

    if (parse(pddl, dom_lisp, PDDL_KW_CONSTANTS, 1, err) != 0
            || parse(pddl, prob_lisp, PDDL_KW_OBJECTS, 0, err) != 0)
        PDDL_TRACE_RET(err, -1);

    if ((pddl->require.multi_agent && pddl->require.unfactored_privacy)
            || pddl->require.factored_privacy){
        if (parsePrivate(pddl, dom_lisp, PDDL_KW_CONSTANTS, err) != 0
                || parsePrivate(pddl, prob_lisp, PDDL_KW_OBJECTS, err) != 0)
            PDDL_TRACE_RET(err, -1);
    }

    for (i = 0; i < pddl->obj.obj_size; ++i)
        pddlTypesAddObj(&pddl->type, i, pddl->obj.obj[i].type);

    LOG(err, "Objects parsed, num objects: %{parsed_objs}d",
        pddl->obj.obj_size);
    return 0;
}

void pddlObjsInitCopy(pddl_objs_t *dst, const pddl_objs_t *src)
{
    ZEROIZE(dst);

    dst->htable = pddlHTableNew(objHash, objEq, NULL);

    dst->obj_size = dst->obj_alloc = src->obj_size;
    dst->obj = CALLOC_ARR(pddl_obj_t, src->obj_size);
    for (int i = 0; i < src->obj_size; ++i){
        dst->obj[i] = src->obj[i];
        if (dst->obj[i].name != NULL)
            dst->obj[i].name = STRDUP(src->obj[i].name);

        obj_key_t *key;
        key = ALLOC(obj_key_t);
        key->obj_id = i;
        key->name = dst->obj[i].name;
        key->hash = pddlHashSDBM(dst->obj[i].name);
        pddlListInit(&key->htable);
        pddlHTableInsert(dst->htable, &key->htable);
    }
}

void pddlObjsFree(pddl_objs_t *objs)
{
    pddl_list_t list;
    pddl_list_t *item;
    obj_key_t *key;

    for (int i = 0; i < objs->obj_size; ++i)
        pddlObjFree(objs->obj + i);
    if (objs->obj != NULL)
        FREE(objs->obj);

    pddlListInit(&list);
    if (objs->htable != NULL){
        pddlHTableGather(objs->htable, &list);
        while (!pddlListEmpty(&list)){
            item = pddlListNext(&list);
            pddlListDel(item);
            key = PDDL_LIST_ENTRY(item, obj_key_t, htable);
            FREE(key);
        }
        pddlHTableDel(objs->htable);
    }
}

static obj_key_t *findByName(const pddl_objs_t *objs, const char *name)
{
    pddl_list_t *item;
    obj_key_t *key, keyin;

    keyin.name = name;
    keyin.hash = pddlHashSDBM(name);
    item = pddlHTableFind(objs->htable, &keyin.htable);
    if (item == NULL)
        return NULL;

    key = PDDL_LIST_ENTRY(item, obj_key_t, htable);
    return key;
}

pddl_obj_id_t pddlObjsGet(const pddl_objs_t *objs, const char *name)
{
    obj_key_t *key = findByName(objs, name);
    if (key == NULL)
        return PDDL_OBJ_ID_UNDEF;
    return key->obj_id;
}

pddl_obj_t *pddlObjsAdd(pddl_objs_t *objs, const char *name)
{
    pddl_obj_t *o;
    obj_key_t *key;

    if (pddlObjsGet(objs, name) != PDDL_OBJ_ID_UNDEF)
        return NULL;

    if (objs->obj_size >= objs->obj_alloc){
        if (objs->obj_alloc == 0){
            objs->obj_alloc = 2;
        }else{
            objs->obj_alloc *= 2;
        }
        objs->obj = REALLOC_ARR(objs->obj, pddl_obj_t, objs->obj_alloc);
    }

    o = objs->obj + objs->obj_size++;
    ZEROIZE(o);
    o->name = STRDUP(name);
    o->owner = PDDL_OBJ_ID_UNDEF;

    key = ALLOC(obj_key_t);
    key->obj_id = objs->obj_size - 1;
    key->name = name;
    key->hash = pddlHashSDBM(name);
    pddlListInit(&key->htable);
    pddlHTableInsert(objs->htable, &key->htable);

    return o;
}

void pddlObjsRemap(pddl_objs_t *objs, const pddl_obj_id_t *remap)
{
    int new_size = 0;
    for (int i = 0; i < objs->obj_size; ++i)
        new_size = PDDL_MAX(new_size, remap[i] + 1);

    int *isset = CALLOC_ARR(int, new_size);
    pddl_obj_t *nobjs = CALLOC_ARR(pddl_obj_t, new_size);
    for (int i = 0; i < objs->obj_size; ++i){
        if (remap[i] >= 0 && !isset[remap[i]]){
            nobjs[remap[i]] = objs->obj[i];
            isset[remap[i]] = 1;
            if (nobjs[remap[i]].name != NULL){
                obj_key_t *key = findByName(objs, nobjs[remap[i]].name);
                if (key != NULL)
                    key->obj_id = remap[i];
            }

        }else{
            if (objs->obj[i].name != NULL){
                obj_key_t *key = findByName(objs, objs->obj[i].name);
                if (key != NULL){
                    pddlHTableErase(objs->htable, &key->htable);
                    FREE(key);
                }
            }
            pddlObjFree(objs->obj + i);
        }
    }

    FREE(objs->obj);
    objs->obj = nobjs;
    FREE(isset);
    objs->obj_size = new_size;
}

void pddlObjsRemapTypes(pddl_objs_t *objs, const int *remap_type)
{
    for (int i = 0; i < objs->obj_size; ++i)
        objs->obj[i].type = remap_type[objs->obj[i].type];
}

void pddlObjsPrint(const pddl_objs_t *objs, FILE *fout)
{
    fprintf(fout, "Obj[%d]:\n", objs->obj_size);
    for (int i = 0; i < objs->obj_size; ++i){
        fprintf(fout, "    [%d]: %s, type: %d, is-constant: %d,"
                " is-private: %d, owner: %d, is-agent: %d\n", i,
                objs->obj[i].name, objs->obj[i].type,
                objs->obj[i].is_constant, objs->obj[i].is_private,
                (int)objs->obj[i].owner, objs->obj[i].is_agent);
    }
}

void pddlObjsPrintPDDLConstants(const pddl_objs_t *objs,
                                const pddl_types_t *ts,
                                FILE *fout)
{
    fprintf(fout, "(:constants\n");
    for (int i = 0; i < objs->obj_size; ++i){
        const pddl_obj_t *o = objs->obj + i;
        fprintf(fout, "    %s - %s\n", o->name, ts->type[o->type].name);
    }
    fprintf(fout, ")\n");
}
