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

#include "internal.h"
#include "pddl/cascading_table.h"

#define PRUNED -1
#define PDDL_CASCADING_TABLE_LEAF 0
#define PDDL_CASCADING_TABLE_MERGE 1

struct pddl_cascading_table {
    int type;
    int size;
    int *lookup_table;
};

struct pddl_cascading_table_leaf {
    pddl_cascading_table_t cascading_table;
    int id;
};
typedef struct pddl_cascading_table_leaf pddl_cascading_table_leaf_t;

struct pddl_cascading_table_merge {
    pddl_cascading_table_t cascading_table;
    pddl_cascading_table_t *left;
    pddl_cascading_table_t *right;
};
typedef struct pddl_cascading_table_merge pddl_cascading_table_merge_t;


#define LEAF(X, T) \
    pddl_cascading_table_leaf_t *X = \
        pddl_container_of((T), pddl_cascading_table_leaf_t, cascading_table)
#define MERGE(X, T) \
    pddl_cascading_table_merge_t *X = \
        pddl_container_of((T), pddl_cascading_table_merge_t, cascading_table)

#define MERGE_IDX(M, LEFT, RIGHT) ((LEFT) * (M)->right->size + (RIGHT))
#define MERGE_LR_FROM_IDX(M, IDX, LEFT, RIGHT) \
    do { \
        *(RIGHT) = (IDX) % (M)->right->size; \
        *(LEFT) = (IDX) / (M)->right->size; \
    } while (0)

static void delLeaf(pddl_cascading_table_t *t);
static void delMerge(pddl_cascading_table_t *t);

static pddl_cascading_table_t *cloneLeaf(const pddl_cascading_table_t *t);
static pddl_cascading_table_t *cloneMerge(const pddl_cascading_table_t *t);

static int valueFromStateLeaf(pddl_cascading_table_t *_t, const int *state);
static int valueFromStateMerge(pddl_cascading_table_t *_t, const int *state);


static void initTable(pddl_cascading_table_t *, int type, int size);
static void copyTable(pddl_cascading_table_t *t,
                      const pddl_cascading_table_t *src);
                      

struct methods {
    void (*delete)(pddl_cascading_table_t *);
    pddl_cascading_table_t *(*clone)(const pddl_cascading_table_t *);
    int (*value_from_state)(pddl_cascading_table_t *, const int *);
};

static struct methods methods[2] = {
    {
        delLeaf, /* .delete */
        cloneLeaf, /* .clone */
        valueFromStateLeaf, /* .value_from_state */
    },
    {
        delMerge, /* .delete */
        cloneMerge, /* .clone */
        valueFromStateMerge, /* .value_from_state */
    }
};

void pddlCascadingTableDel(pddl_cascading_table_t *t)
{
    if (t->lookup_table != NULL)
        FREE(t->lookup_table);
    methods[t->type].delete(t);
}

static void delLeaf(pddl_cascading_table_t *_t)
{
    LEAF(t, _t);
    FREE(t);
}

static void delMerge(pddl_cascading_table_t *_t)
{
    MERGE(t, _t);
    pddlCascadingTableDel(t->left);
    pddlCascadingTableDel(t->right);
    FREE(t);
}


pddl_cascading_table_t *pddlCascadingTableClone(const pddl_cascading_table_t *t)
{
    return methods[t->type].clone(t);
}

static pddl_cascading_table_t *cloneLeaf(const pddl_cascading_table_t *_t)
{
    const LEAF(t, _t);
    pddl_cascading_table_leaf_t *out;
   
    out = ALLOC(pddl_cascading_table_leaf_t);
    ZEROIZE(out);
    copyTable(&out->cascading_table, &t->cascading_table);
    out->id = t->id;

    return &out->cascading_table;
}

static pddl_cascading_table_t *cloneMerge(const pddl_cascading_table_t *_t)
{
    MERGE(t, _t);
    pddl_cascading_table_merge_t *out;
   
    out = ZALLOC(pddl_cascading_table_merge_t);
    copyTable(&out->cascading_table, &t->cascading_table);
    out->left = pddlCascadingTableClone(t->left);
    out->right = pddlCascadingTableClone(t->right);
    return &out->cascading_table;
}

pddl_cascading_table_t *pddlCascadingTableNewLeaf(int id, int size)
{
    pddl_cascading_table_leaf_t *t;
    t = ZALLOC(pddl_cascading_table_leaf_t);
    initTable(&t->cascading_table, PDDL_CASCADING_TABLE_LEAF, size);
    t->id = id;

    return &t->cascading_table;
}

pddl_cascading_table_t *pddlCascadingTableMerge(pddl_cascading_table_t *t1,
                                                pddl_cascading_table_t *t2)
{
    pddl_cascading_table_merge_t *m;
    m = ZALLOC(pddl_cascading_table_merge_t);
    int size = t1->size * t2->size;
    initTable(&m->cascading_table, PDDL_CASCADING_TABLE_MERGE, size);
    m->left = pddlCascadingTableClone(t1);
    m->right = pddlCascadingTableClone(t2);

    return &m->cascading_table;
}

void pddlCascadingTableAbstract(pddl_cascading_table_t *t, const int *abstr)
{
    int new_size = 0;
    for (int i = 0; i < t->size; ++i){
        int new_val = abstr[i];
        if (t->lookup_table[i] != PRUNED){
            if (new_val < 0){
                t->lookup_table[i] = PRUNED;
            }else{
                t->lookup_table[i] = new_val;
                new_size = PDDL_MAX(new_size, t->lookup_table[i] + 1);
            }
        }
    }
    t->size = new_size;
}

int pddlCascadingTableValueFromState(pddl_cascading_table_t *t,
                                     const int *state)
{
    return methods[t->type].value_from_state(t, state);
}

int pddlCascadingTableSize(const pddl_cascading_table_t *t)
{
    return t->size;
}

static int valueFromStateLeaf(pddl_cascading_table_t *_t, const int *state)
{
    LEAF(t, _t);
    return _t->lookup_table[state[t->id]];
}

static int valueFromStateMerge(pddl_cascading_table_t *_t, const int *state)
{
    MERGE(t, _t);
    int vleft = pddlCascadingTableValueFromState(t->left, state);
    int vright = pddlCascadingTableValueFromState(t->right, state);
    return _t->lookup_table[MERGE_IDX(t, vleft, vright)];
}

static void initTable(pddl_cascading_table_t *t, int type, int size)
{
    t->type = type;
    t->size = size;
    t->lookup_table = CALLOC_ARR(int, size);
    for (int i = 0; i < t->size; ++i)
        t->lookup_table[i] = i;
}

static void copyTable(pddl_cascading_table_t *t,
                      const pddl_cascading_table_t *src)
{
    *t = *src;
    t->lookup_table = CALLOC_ARR(int, t->size);
    memcpy(t->lookup_table, src->lookup_table, sizeof(int) * t->size);
}

int pddlCascadingTableLeafValue(const pddl_cascading_table_t *t, int idx)
{
    if (t->type != PDDL_CASCADING_TABLE_LEAF)
        return -2;
    return t->lookup_table[idx];
}

int pddlCascadingTableMergeValue(const pddl_cascading_table_t *_t,
                                 int left_value,
                                 int right_value)
{
    if (_t->type != PDDL_CASCADING_TABLE_MERGE)
        return -2;
    MERGE(t, _t);
    return MERGE_IDX(t, left_value, right_value);
}
