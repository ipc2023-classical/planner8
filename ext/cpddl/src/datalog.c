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

#include "internal.h"
#include "pddl/sort.h"
#include "pddl/hfunc.h"
#include "pddl/extarr.h"
#include "pddl/htable.h"
#include "pddl/pairheap.h"
#include "pddl/datalog.h"
#include "pddl/iarr.h"

struct pddl_datalog_fact {
    pddl_htable_key_t hash;
    pddl_list_t htable;
    pddl_pairheap_node_t heap;

    int id;
    pddl_cost_t weight;
    pddl_iset_t best_fact_achievers;
    int arity;

    // The following .pred and .arg[] must be kept together
    int pred;
    int arg[];
};
typedef struct pddl_datalog_fact pddl_datalog_fact_t;

struct pddl_datalog_relevant_fact {
    pddl_htable_key_t hash;
    pddl_list_t htable;

    int id;
    pddl_iset_t fact;
    int key_size;
    int rule;
    int key[];
};
typedef struct pddl_datalog_relevant_fact pddl_datalog_relevant_fact_t;

struct pddl_datalog_db_freeze {
    int fact_size;
    int relevant_fact_size[2];
    size_t used_mem;
    pddl_iset_t *pred_to_fact;
    int canonical_model_end;
};
typedef struct pddl_datalog_db_freeze pddl_datalog_db_freeze_t;

struct pddl_datalog_db {
    pddl_htable_t *hfact;
    pddl_htable_t *hrelevant_fact[2];
    size_t used_mem;
    pddl_extarr_t *fact;
    pddl_pairheap_t *fact_queue;
    int fact_size;
    pddl_iset_t *pred_to_fact;
    pddl_extarr_t *relevant_fact[2];
    int relevant_fact_size[2];
    pddl_datalog_db_freeze_t freeze;
    int canonical_model_end;
};
typedef struct pddl_datalog_db pddl_datalog_db_t;

struct pddl_datalog_const {
    unsigned id;
    int idx;
    char *name;
    pddl_obj_id_t user_id;
};
typedef struct pddl_datalog_const pddl_datalog_const_t;

struct pddl_datalog_var {
    unsigned id;
    int idx;
    char *name;
};
typedef struct pddl_datalog_var pddl_datalog_var_t;

struct pddl_datalog_pred {
    unsigned id;
    int idx;
    int arity;
    char *name;
    int is_goal;
    int is_aux;
    int user_id;
    pddl_iset_t relevant_rules;
};
typedef struct pddl_datalog_pred pddl_datalog_pred_t;

struct pddl_datalog {
    pddl_datalog_const_t *c;
    int c_size;
    int c_alloc;

    pddl_datalog_var_t *var;
    int var_size;
    int var_alloc;

    pddl_datalog_pred_t *pred;
    int pred_size;
    int pred_alloc;

    pddl_datalog_rule_t *rule;
    int rule_size;
    int rule_alloc;

    int dirty;
    int max_pred_arity;
    pddl_datalog_db_t db;
};

#define MASK 0x7u
#define MASK_LEN 3u
#define CONST_MASK 0x1u
#define PRED_MASK 0x2u
#define VAR_MASK 0x3u
#define TO_IDX(v) ((v)>>MASK_LEN)
#define IDX_TO_CONST(v) (((v)<<MASK_LEN) | CONST_MASK)
#define IS_CONST(v) (((v) & MASK) == CONST_MASK)
#define IDX_TO_PRED(v) (((v)<<MASK_LEN) | PRED_MASK)
#define IS_PRED(v) (((v) & MASK) == PRED_MASK)
#define IDX_TO_VAR(v) (((v)<<MASK_LEN) | VAR_MASK)
#define IS_VAR(v) (((v) & MASK) == VAR_MASK)

#define NO_WEIGHT 0
#define WEIGHT_MAX 1
#define WEIGHT_ADD 2

#define FACT_NOT_IN_QUEUE(F) ((F)->heap.children.next == NULL)

static pddl_htable_key_t factComputeHash(const pddl_datalog_fact_t *fact)
{
    size_t size = sizeof(int) + fact->arity * sizeof(int);
    return pddlCityHash_64(&fact->pred, size);
}

static pddl_htable_key_t factHash(const pddl_list_t *key, void *_)
{
    return PDDL_LIST_ENTRY(key, pddl_datalog_fact_t, htable)->hash;
}

static int factEq(const pddl_list_t *key1, const pddl_list_t *key2, void *_)
{
    const pddl_datalog_fact_t *f1, *f2;
    f1 = PDDL_LIST_ENTRY(key1, pddl_datalog_fact_t, htable);
    f2 = PDDL_LIST_ENTRY(key2, pddl_datalog_fact_t, htable);
    size_t size = sizeof(int) + f1->arity * sizeof(int);
    return f1->arity == f2->arity && memcmp(&f1->pred, &f2->pred, size) == 0;
}

static int factQueueLT(const pddl_pairheap_node_t *n1,
                       const pddl_pairheap_node_t *n2,
                       void *data)
{
    const pddl_datalog_fact_t *f1, *f2;
    f1 = pddl_container_of(n1, pddl_datalog_fact_t, heap);
    f2 = pddl_container_of(n2, pddl_datalog_fact_t, heap);
    return pddlCostCmp(&f1->weight, &f2->weight) < 0;
}

static pddl_htable_key_t relevantFactComputeHash(
                const pddl_datalog_relevant_fact_t *f)
{
    size_t size = f->key_size * 2 * sizeof(int);
    return pddlCityHash_64(f->key, size);
}

static pddl_htable_key_t relevantFactHash(const pddl_list_t *key, void *_)
{
    return PDDL_LIST_ENTRY(key, pddl_datalog_relevant_fact_t, htable)->hash;
}

static int relevantFactEq(const pddl_list_t *key1,
                          const pddl_list_t *key2,
                          void *_)
{
    const pddl_datalog_relevant_fact_t *f1, *f2;
    f1 = PDDL_LIST_ENTRY(key1, pddl_datalog_relevant_fact_t, htable);
    f2 = PDDL_LIST_ENTRY(key2, pddl_datalog_relevant_fact_t, htable);
    size_t size = sizeof(int) + f1->key_size * 2 * sizeof(int);
    return f1->rule == f2->rule && memcmp(&f1->rule, &f2->rule, size) == 0;
}


static void dbFree(pddl_datalog_t *dl, pddl_datalog_db_t *db);
static void dbInit(pddl_datalog_t *dl, pddl_datalog_db_t *db)
{
    dbFree(dl, db);
    db->hfact = pddlHTableNew(factHash, factEq, NULL);
    db->hrelevant_fact[0] = pddlHTableNew(relevantFactHash,
                                          relevantFactEq, NULL);
    db->hrelevant_fact[1] = pddlHTableNew(relevantFactHash,
                                          relevantFactEq, NULL);

    size_t size = sizeof(pddl_datalog_fact_t);
    size += dl->max_pred_arity * sizeof(int);
    void *init = alloca(size);
    ZEROIZE_RAW(init, size);
    db->fact = pddlExtArrNew(size, NULL, init);
    db->fact_queue = pddlPairHeapNew(factQueueLT, NULL);
    db->pred_to_fact = CALLOC_ARR(pddl_iset_t, dl->pred_size);

    size = sizeof(pddl_datalog_relevant_fact_t);
    size += dl->max_pred_arity * 2 * sizeof(int);
    init = alloca(size);
    ZEROIZE_RAW(init, size);
    db->relevant_fact[0] = pddlExtArrNew(size, NULL, init);
    db->relevant_fact[1] = pddlExtArrNew(size, NULL, init);
    db->freeze.pred_to_fact = CALLOC_ARR(pddl_iset_t, dl->pred_size);
}

static size_t dbUsedMem(const pddl_datalog_db_t *db)
{
    return db->used_mem;
}

static pddl_datalog_fact_t *dbFact(pddl_datalog_db_t *db, int id)
{
    return (pddl_datalog_fact_t *)pddlExtArrGet(db->fact, id);
}

static void dbFree(pddl_datalog_t *dl, pddl_datalog_db_t *db)
{
    if (db->hfact == NULL)
        return;
    pddlHTableDel(db->hfact);
    pddlHTableDel(db->hrelevant_fact[0]);
    pddlHTableDel(db->hrelevant_fact[1]);
    for (int fact_id = 0; fact_id < db->fact_size; ++fact_id){
        pddl_datalog_fact_t *f = dbFact(db, fact_id);
        pddlISetFree(&f->best_fact_achievers);
    }
    pddlExtArrDel(db->fact);
    if (db->fact_queue != NULL)
        pddlPairHeapDel(db->fact_queue);
    for (int i = 0; i < 2; ++i){
        for (int j = 0; j < db->relevant_fact_size[i]; ++j){
            void *x = pddlExtArrGet(db->relevant_fact[i], j);
            pddl_datalog_relevant_fact_t *f = (pddl_datalog_relevant_fact_t *)x;
            pddlISetFree(&f->fact);
        }
        pddlExtArrDel(db->relevant_fact[i]);
    }
    for (int i = 0; i < dl->pred_size; ++i)
        pddlISetFree(db->pred_to_fact + i);
    FREE(db->pred_to_fact);

    for (int i = 0; i < dl->pred_size; ++i)
        pddlISetFree(db->freeze.pred_to_fact + i);
    FREE(db->freeze.pred_to_fact);

    ZEROIZE(db);
}

static void dbFreeze(pddl_datalog_t *dl, pddl_datalog_db_t *db)
{
    db->freeze.fact_size = db->fact_size;
    db->freeze.relevant_fact_size[0] = db->relevant_fact_size[0];
    db->freeze.relevant_fact_size[1] = db->relevant_fact_size[1];
    db->freeze.used_mem = db->used_mem;
    for (int i = 0; i < dl->pred_size; ++i)
        pddlISetSet(db->freeze.pred_to_fact + i, db->pred_to_fact + i);
    db->freeze.canonical_model_end = db->canonical_model_end;
}

static void dbRollback(pddl_datalog_t *dl, pddl_datalog_db_t *db)
{
    for (int fact_id = db->freeze.fact_size; fact_id < db->fact_size; ++fact_id){
        pddl_datalog_fact_t *f = dbFact(db, fact_id);
        pddlISetFree(&f->best_fact_achievers);
        pddlHTableErase(db->hfact, &f->htable);
        if (!FACT_NOT_IN_QUEUE(f))
            pddlPairHeapRemove(db->fact_queue, &f->heap);
    }
    db->fact_size = db->freeze.fact_size;

    for (int side = 0; side < 2; ++side){
        for (int i = db->freeze.relevant_fact_size[side];
                i < db->relevant_fact_size[side]; ++i){
            void *x = pddlExtArrGet(db->relevant_fact[side], i);
            pddl_datalog_relevant_fact_t *f = (pddl_datalog_relevant_fact_t *)x;
            pddlISetFree(&f->fact);
            pddlHTableRemove(db->hrelevant_fact[side], &f->htable);
        }
        db->relevant_fact_size[side] = db->freeze.relevant_fact_size[side];
    }

    for (int i = 0; i < dl->pred_size; ++i)
        pddlISetSet(db->pred_to_fact + i, db->freeze.pred_to_fact + i);
    db->used_mem = db->freeze.used_mem;
    db->canonical_model_end = db->freeze.canonical_model_end;
}

static pddl_datalog_fact_t *dbFindFact(pddl_datalog_t *dl,
                                       pddl_datalog_db_t *db,
                                       int pred,
                                       const int *arg)
{
    int arity = dl->pred[pred].arity;
    size_t size = sizeof(pddl_datalog_fact_t) + arity * sizeof(int);
    pddl_datalog_fact_t *f = alloca(size);

    f->arity = arity;
    f->pred = pred;
    memcpy(f->arg, arg, sizeof(int) * arity);
    f->hash = factComputeHash(f);
    pddl_list_t *ret = pddlHTableFind(db->hfact, &f->htable);
    if (ret == NULL){
        return NULL;

    }else{
        return PDDL_LIST_ENTRY(ret, pddl_datalog_fact_t, htable);
    }
}

static int dbHasFact(pddl_datalog_t *dl,
                     pddl_datalog_db_t *db,
                     int pred,
                     const int *arg)
{
    return dbFindFact(dl, db, pred, arg) != NULL;
}

static int dbAddFact(pddl_datalog_t *dl,
                     pddl_datalog_db_t *db,
                     int pred,
                     const int *arg)
{
    pddl_datalog_fact_t *f;
    f = (pddl_datalog_fact_t *)pddlExtArrGet(db->fact, db->fact_size);

    f->arity = dl->pred[pred].arity;
    f->pred = pred;
    memcpy(f->arg, arg, sizeof(int) * f->arity);
    f->hash = factComputeHash(f);
    pddlListInit(&f->htable);
    pddl_list_t *ret = pddlHTableInsertUnique(db->hfact, &f->htable);
    if (ret == NULL){
        f->id = db->fact_size++;
        db->used_mem += db->fact->arr->el_size;
        pddlISetAdd(&db->pred_to_fact[f->pred], f->id);
        db->used_mem += sizeof(int);

    }else{
        f = PDDL_LIST_ENTRY(ret, pddl_datalog_fact_t, htable);
    }
    return f->id;
}

static void dbSetFactWeight(pddl_datalog_t *dl,
                            pddl_datalog_db_t *db,
                            int fact_id,
                            const pddl_cost_t *weight,
                            const pddl_iset_t *fact_achievers)
{
    pddl_datalog_fact_t *f = dbFact(db, fact_id);
    if (FACT_NOT_IN_QUEUE(f)){
        f->weight = *weight;
        pddlPairHeapAdd(db->fact_queue, &f->heap);
        if (fact_achievers != NULL)
            pddlISetSet(&f->best_fact_achievers, fact_achievers);

    }else if (pddlCostCmp(&f->weight, weight) > 0){
        f->weight = *weight;
        pddlPairHeapDecreaseKey(db->fact_queue, &f->heap);
        if (fact_achievers != NULL)
            pddlISetSet(&f->best_fact_achievers, fact_achievers);
    }
}

static void dbAddRelevantFact(pddl_datalog_t *dl,
                              pddl_datalog_db_t *db,
                              int bid,
                              int rule,
                              const pddl_iset_t *key_vars,
                              const int *var_map,
                              int fact_id)
{
    void *xf = pddlExtArrGet(db->relevant_fact[bid],
                             db->relevant_fact_size[bid]);
    pddl_datalog_relevant_fact_t *f = (pddl_datalog_relevant_fact_t *)xf;

    pddlISetInit(&f->fact);
    f->key_size = pddlISetSize(key_vars);
    f->rule = rule;
    for (int i = 0; i < f->key_size; ++i){
        f->key[2 * i] = pddlISetGet(key_vars, i);
        f->key[2 * i + 1] = var_map[f->key[2 * i]];
    }
    f->hash = relevantFactComputeHash(f);
    pddlListInit(&f->htable);

    pddl_list_t *ret;
    ret = pddlHTableInsertUnique(db->hrelevant_fact[bid], &f->htable);
    if (ret == NULL){
        f->id = db->relevant_fact_size[bid]++;
        db->used_mem += db->relevant_fact[bid]->arr->el_size;

    }else{
        f = PDDL_LIST_ENTRY(ret, pddl_datalog_relevant_fact_t, htable);
    }
    pddlISetAdd(&f->fact, fact_id);
    db->used_mem += sizeof(int);
}

static pddl_datalog_relevant_fact_t *dbFindRelevantFact(
            pddl_datalog_t *dl,
            pddl_datalog_db_t *db,
            int bid,
            int rule,
            const pddl_iset_t *key_vars,
            const int *var_map)
{
    int key_size = pddlISetSize(key_vars);
    size_t size = sizeof(pddl_datalog_relevant_fact_t);
    size += key_size * 2 * sizeof(int);
    pddl_datalog_relevant_fact_t *f = alloca(size);

    f->key_size = key_size;
    f->rule = rule;
    for (int i = 0; i < key_size; ++i){
        f->key[2 * i] = pddlISetGet(key_vars, i);
        f->key[2 * i + 1] = var_map[f->key[2 * i]];
    }
    f->hash = relevantFactComputeHash(f);

    pddl_list_t *ret;
    ret = pddlHTableFind(db->hrelevant_fact[bid], &f->htable);
    if (ret == NULL){
        return NULL;
    }else{
        return PDDL_LIST_ENTRY(ret, pddl_datalog_relevant_fact_t, htable);
    }
}

static void atomSetUp(pddl_datalog_t *dl, pddl_datalog_atom_t *atom)
{
    pddlISetEmpty(&atom->var_set);
    int arity = dl->pred[atom->pred].arity;
    for (int i = 0; i < arity; ++i){
        if (IS_VAR(atom->arg[i])){
            ASSERT(atom->arg[i] > 0);
            pddlISetAdd(&atom->var_set, TO_IDX(atom->arg[i]));
        }
    }
}

static void ruleSetUp(pddl_datalog_t *dl, pddl_datalog_rule_t *rule)
{
    PDDL_ISET(body_vars);
    atomSetUp(dl, &rule->head);
    pddlISetEmpty(&rule->common_body_var_set);
    for (int i = 0; i < rule->body_size; ++i){
        atomSetUp(dl, rule->body + i);
        if (i == 0){
            pddlISetUnion(&rule->common_body_var_set, &rule->body[0].var_set);
        }else{
            pddlISetIntersect(&rule->common_body_var_set,
                              &rule->body[i].var_set);
        }

        pddlISetUnion(&body_vars, &rule->body[i].var_set);
    }
    rule->is_safe = pddlISetIsSubset(&rule->head.var_set, &body_vars);
    pddlISetUnion2(&rule->var_set, &body_vars, &rule->head.var_set);
    pddlISetFree(&body_vars);

    // TODO: Check that the neg_body atom has only variables from
    // rule->var_set
    for (int i = 0; i < rule->neg_body_size; ++i)
        atomSetUp(dl, rule->neg_body + i);
}

static void predsSetUp(pddl_datalog_t *dl)
{
    for (int p = 0; p < dl->pred_size; ++p)
        pddlISetEmpty(&dl->pred[p].relevant_rules);

    for (int r = 0; r < dl->rule_size; ++r){
        const pddl_datalog_rule_t *rule = dl->rule + r;
        if (rule->body_size == 0)
            continue;
        for (int b = 0; b < rule->body_size; ++b){
            int pred = rule->body[b].pred;
            ASSERT(pred < dl->pred_size);
            pddlISetAdd(&dl->pred[pred].relevant_rules, r);
        }
    }
}

static void setUp(pddl_datalog_t *dl, int db, pddl_err_t *err)
{
    if (dl->dirty){
        for (int r = 0; r < dl->rule_size; ++r)
            ruleSetUp(dl, &dl->rule[r]);
        predsSetUp(dl);
        dl->dirty = 0;
        if (db)
            dbInit(dl, &dl->db);

    }else if (db && dl->db.fact == NULL){
        dbInit(dl, &dl->db);
    }
}

pddl_datalog_t *pddlDatalogNew(void)
{
    return ZALLOC(pddl_datalog_t);
}

void pddlDatalogDel(pddl_datalog_t *dl)
{
    for (int i = 0; i < dl->rule_size; ++i)
        pddlDatalogRuleFree(dl, dl->rule + i);
    if (dl->rule != NULL)
        FREE(dl->rule);

    for (int i = 0; i < dl->c_size; ++i){
        if (dl->c[i].name != NULL)
            FREE(dl->c[i].name);
    }
    if (dl->c != NULL)
        FREE(dl->c);

    for (int i = 0; i < dl->var_size; ++i){
        if (dl->var[i].name != NULL)
            FREE(dl->var[i].name);
    }
    if (dl->var != NULL)
        FREE(dl->var);

    for (int i = 0; i < dl->pred_size; ++i){
        if (dl->pred[i].name != NULL)
            FREE(dl->pred[i].name);
        pddlISetFree(&dl->pred[i].relevant_rules);
    }
    if (dl->pred != NULL)
        FREE(dl->pred);
    dbFree(dl, &dl->db);
    FREE(dl);
}

void pddlDatalogClear(pddl_datalog_t *dl)
{
    dbFree(dl, &dl->db);
}

unsigned pddlDatalogAddConst(pddl_datalog_t *dl, const char *name)
{
    if (dl->c_size == dl->c_alloc){
        if (dl->c_alloc == 0)
            dl->c_alloc = 1;
        dl->c_alloc *= 2;
        dl->c = REALLOC_ARR(dl->c, pddl_datalog_const_t, dl->c_alloc);
    }
    pddl_datalog_const_t *c = dl->c + dl->c_size;
    ZEROIZE(c);
    c->idx = dl->c_size++;
    c->id = IDX_TO_CONST(c->idx);
    c->name = NULL;
    if (name != NULL)
        c->name = STRDUP(name);
    c->user_id = -1;
    dl->dirty = 1;
    return c->id;
}

unsigned pddlDatalogAddPred(pddl_datalog_t *dl, int arity, const char *name)
{
    if (dl->pred_size == dl->pred_alloc){
        if (dl->pred_alloc == 0)
            dl->pred_alloc = 1;
        dl->pred_alloc *= 2;
        dl->pred = REALLOC_ARR(dl->pred, pddl_datalog_pred_t,
                               dl->pred_alloc);
    }
    pddl_datalog_pred_t *p = dl->pred + dl->pred_size;
    ZEROIZE(p);
    p->idx = dl->pred_size++;
    p->id = IDX_TO_PRED(p->idx);
    p->arity = arity;
    p->name = NULL;
    if (name != NULL)
        p->name = STRDUP(name);
    p->user_id = -1;
    dl->max_pred_arity = PDDL_MAX(dl->max_pred_arity, arity);
    dl->dirty = 1;
    return p->id;
}

unsigned pddlDatalogAddGoalPred(pddl_datalog_t *dl, const char *name)
{
    unsigned id = pddlDatalogAddPred(dl, 0, name);
    dl->pred[TO_IDX(id)].is_goal = 1;
    return id;
}

static unsigned pddlDatalogAddAuxPred(pddl_datalog_t *dl,
                                      int arity,
                                      const char *name)
{
    unsigned id = pddlDatalogAddPred(dl, arity, name);
    dl->pred[TO_IDX(id)].is_aux = 1;
    return id;
}

unsigned pddlDatalogAddVar(pddl_datalog_t *dl, const char *name)
{
    if (dl->var_size == dl->var_alloc){
        if (dl->var_alloc == 0)
            dl->var_alloc = 1;
        dl->var_alloc *= 2;
        dl->var = REALLOC_ARR(dl->var, pddl_datalog_var_t, dl->var_alloc);
    }
    pddl_datalog_var_t *v = dl->var + dl->var_size;
    ZEROIZE(v);
    v->idx = dl->var_size++;
    v->id = IDX_TO_VAR(v->idx);
    v->name = NULL;
    if (name != NULL)
        v->name = STRDUP(name);
    dl->dirty = 1;
    return v->id;
}

void pddlDatalogSetUserId(pddl_datalog_t *dl, unsigned element, int user_id)
{
    if (IS_PRED(element)){
        dl->pred[TO_IDX(element)].user_id = user_id;
    }else if (IS_CONST(element)){
        dl->c[TO_IDX(element)].user_id = user_id;
    }
}

int pddlDatalogAddRule(pddl_datalog_t *dl, const pddl_datalog_rule_t *cl)
{
    if (dl->rule_size == dl->rule_alloc){
        if (dl->rule_alloc == 0)
            dl->rule_alloc = 1;
        dl->rule_alloc *= 2;
        dl->rule = REALLOC_ARR(dl->rule, pddl_datalog_rule_t,
                               dl->rule_alloc);
    }
    pddl_datalog_rule_t *rule = dl->rule + dl->rule_size++;
    pddlDatalogRuleInit(dl, rule);
    pddlDatalogRuleCopy(dl, rule, cl);
    dl->dirty = 1;
    return dl->rule_size - 1;
}

void pddlDatalogRmLastRules(pddl_datalog_t *dl, int n)
{
    for (int i = 0; i < n && dl->rule_size > 0; ++i)
        pddlDatalogRuleFree(dl, dl->rule + --dl->rule_size);
    dl->dirty = 1;
}

void pddlDatalogRmRules(pddl_datalog_t *dl, const pddl_iset_t *rm_rules)
{
    int rm_size = pddlISetSize(rm_rules);
    int cur = 0;
    int ins = 0;
    for (int ri = 0; ri < dl->rule_size; ++ri){
        if (cur < rm_size && pddlISetGet(rm_rules, cur) == ri){
            pddlDatalogRuleFree(dl, dl->rule + ri);
            ++cur;
        }else{
            dl->rule[ins++] = dl->rule[ri];
        }
    }
    dl->rule_size = ins;
    dl->dirty = 1;
}

static void joinVars(const pddl_datalog_rule_t *rule,
                     const pddl_datalog_atom_t *a1,
                     const pddl_datalog_atom_t *a2,
                     pddl_iset_t *vars)
{
    pddlISetEmpty(vars);
    pddlISetUnion2(vars, &a1->var_set, &a2->var_set);

    PDDL_ISET(cvars);
    pddlISetUnion(&cvars, &rule->head.var_set);
    for (int i = 0; i < rule->body_size; ++i){
        if (rule->body + i == a1 || rule->body + i == a2)
            continue;
        pddlISetUnion(&cvars, &rule->body[i].var_set);
    }
    for (int i = 0; i < rule->neg_body_size; ++i)
        pddlISetUnion(&cvars, &rule->neg_body[i].var_set);
    pddlISetIntersect(vars, &cvars);
    pddlISetFree(&cvars);
}

static void joinCost(const pddl_datalog_rule_t *rule,
                     const pddl_datalog_atom_t *a1,
                     const pddl_datalog_atom_t *a2,
                     pddl_iset_t *join_vars,
                     int *join_cost)
{
    joinVars(rule, a1, a2, join_vars);
    int a1size = pddlISetSize(&a1->var_set);
    int a2size = pddlISetSize(&a2->var_set);
    join_cost[2] = pddlISetSize(join_vars);
    join_cost[0] = join_cost[2] - PDDL_MAX(a1size, a2size);
    join_cost[1] = join_cost[2] - PDDL_MIN(a1size, a2size);
}

static void selectBodyAtoms(const pddl_datalog_t *dl,
                            const pddl_datalog_rule_t *rule,
                            int *a1, int *a2)
{
    PDDL_ISET(join_vars);
    int join_cost_size = 3 * rule->body_size * rule->body_size;
    int *join_cost = ALLOC_ARR(int, join_cost_size);
    for (int a1i = 0; a1i < rule->body_size; ++a1i){
        const pddl_datalog_atom_t *atom1 = rule->body + a1i;
        for (int a2i = a1i + 1; a2i < rule->body_size; ++a2i){
            const pddl_datalog_atom_t *atom2 = rule->body + a2i;
            int *cost = join_cost + 3 * (a1i * rule->body_size + a2i);
            joinCost(rule, atom1, atom2, &join_vars, cost);
        }
    }

    int best_join_cost[3] = { INT_MAX, INT_MAX, INT_MAX };
    for (int a1i = 0; a1i < rule->body_size; ++a1i){
        for (int a2i = a1i + 1; a2i < rule->body_size; ++a2i){
            const int *cost = join_cost + 3 * (a1i * rule->body_size + a2i);
            if (cost[0] < best_join_cost[0]
                    || (cost[0] == best_join_cost[0]
                            && cost[1] < best_join_cost[1])
                    || (cost[0] == best_join_cost[0]
                            && cost[1] == best_join_cost[1]
                            && cost[2] < best_join_cost[2])){
                memcpy(best_join_cost, cost, 3 * sizeof(int));
                *a1 = a1i;
                *a2 = a2i;
            }
        }
    }

    FREE(join_cost);
    pddlISetFree(&join_vars);
}

static void collectVarsFromAtom(const pddl_datalog_t *dl,
                                const pddl_datalog_atom_t *a,
                                pddl_iset_t *var)
{
    int arity = dl->pred[a->pred].arity;
    for (int i = 0; i < arity; ++i){
        if (IS_VAR(a->arg[i]))
            pddlISetAdd(var, TO_IDX(a->arg[i]));
    }
}

static void collectVarsFromRule(const pddl_datalog_t *dl,
                                const pddl_datalog_rule_t *r,
                                pddl_iset_t *var)
{
    if (r->head.arg != NULL)
        collectVarsFromAtom(dl, &r->head, var);
    for (int i = 0; i < r->body_size; ++i)
        collectVarsFromAtom(dl, &r->body[i], var);
}

static void transferNegBody(pddl_datalog_t *dl,
                            pddl_datalog_rule_t *src,
                            pddl_datalog_rule_t *dst)
{
    PDDL_ISET(vars);
    collectVarsFromRule(dl, dst, &vars);
    int ins = 0;
    for (int i = 0; i < src->neg_body_size; ++i){
        pddl_datalog_atom_t *a = src->neg_body + i;
        if (pddlISetIsSubset(&a->var_set, &vars)){
            pddlDatalogRuleAddNegStaticBody(dl, dst, a);
            pddlDatalogAtomFree(dl, a);
        }else{
            src->neg_body[ins++] = *a;
        }
    }
    src->neg_body_size = ins;
    pddlISetFree(&vars);
}

static void toNormalFormStep(pddl_datalog_t *dl, int rule_id)
{
    pddl_datalog_rule_t *rule = dl->rule + rule_id;

    // Select two atoms from the body
    int a1i = 0, a2i = 0;
    selectBodyAtoms(dl, rule, &a1i, &a2i);
    ASSERT(a1i < a2i);
    const pddl_datalog_atom_t *a1 = rule->body + a1i;
    const pddl_datalog_atom_t *a2 = rule->body + a2i;

    // Create a rule with those two atoms in the body and transfer negative
    // atom from rule to newrule if possible
    pddl_datalog_rule_t newrule;
    pddlDatalogRuleInit(dl, &newrule);
    pddlDatalogRuleAddBody(dl, &newrule, a1);
    pddlDatalogRuleAddBody(dl, &newrule, a2);
    transferNegBody(dl, rule, &newrule);

    // Determine variables of the head
    PDDL_ISET(vars);
    joinVars(rule, a1, a2, &vars);

    // Construct a new predicate
    int pred_arity = pddlISetSize(&vars);
    unsigned pred = pddlDatalogAddAuxPred(dl, pred_arity, NULL);

    // Head of the new rule
    pddl_datalog_atom_t head;
    pddlDatalogAtomInit(dl, &head, pred);
    for (int i = 0; i < pred_arity; ++i){
        unsigned v = dl->var[pddlISetGet(&vars, i)].id;
        pddlDatalogAtomSetArg(dl, &head, i, v);
    }
    pddlDatalogRuleSetHead(dl, &newrule, &head);

    // Add the new rule to the datalog database
    pddlDatalogAddRule(dl, &newrule);
    pddlDatalogRuleFree(dl, &newrule);

    // Update rule with the new predicate
    rule = dl->rule + rule_id;
    pddlDatalogRuleRmBody(dl, rule, a1i);
    pddlDatalogRuleRmBody(dl, rule, a2i - 1); // this requires a1i < a2i
    pddlDatalogRuleAddBody(dl, rule, &head);
    ruleSetUp(dl, rule);

    pddlDatalogAtomFree(dl, &head);
    pddlISetFree(&vars);
}

/** Remap predicates in remap to the predicate target */
static void remapPredInAtom(pddl_datalog_t *dl,
                            const pddl_iset_t *remap,
                            int target,
                            pddl_datalog_atom_t *atom)
{
    if (pddlISetIn(atom->pred, remap))
        atom->pred = target;
}

static void remapPredInRule(pddl_datalog_t *dl,
                            const pddl_iset_t *remap,
                            int target,
                            pddl_datalog_rule_t *rule)
{
    remapPredInAtom(dl, remap, target, &rule->head);
    for (int i = 0; i < rule->body_size; ++i)
        remapPredInAtom(dl, remap, target, &rule->body[i]);
    for (int i = 0; i < rule->neg_body_size; ++i)
        remapPredInAtom(dl, remap, target, &rule->neg_body[i]);
}

static void remapPreds(pddl_datalog_t *dl,
                       const pddl_iset_t *remap,
                       int target)
{
    for (int i = 0; i < dl->rule_size; ++i)
        remapPredInRule(dl, remap, target, dl->rule + i);
}

/** Remap variables to the smallest possible IDs */
static void remapVarsInAtom(pddl_datalog_t *dl,
                            const unsigned *remap,
                            pddl_datalog_atom_t *atom)
{
    pddlISetEmpty(&atom->var_set);
    for (int ai = 0; ai < dl->pred[atom->pred].arity; ++ai){
        if (IS_VAR(atom->arg[ai])){
            int old = TO_IDX(atom->arg[ai]);
            atom->arg[ai] = IDX_TO_VAR(remap[old]);
            pddlISetAdd(&atom->var_set, remap[old]);
        }
    }
}

static void renameVarsInRule(pddl_datalog_t *dl, int rule_id)
{
    pddl_datalog_rule_t *rule = dl->rule + rule_id;

    PDDL_ISET(rule_vars);
    collectVarsFromRule(dl, rule, &rule_vars);

    unsigned remap[dl->var_size];
    ZEROIZE_ARR(remap, dl->var_size);
    for (int i = 0; i < pddlISetSize(&rule_vars); ++i)
        remap[pddlISetGet(&rule_vars, i)] = i;

    remapVarsInAtom(dl, remap, &rule->head);
    for (int i = 0; i < rule->body_size; ++i)
        remapVarsInAtom(dl, remap, rule->body + i);
    for (int i = 0; i < rule->neg_body_size; ++i)
        remapVarsInAtom(dl, remap, rule->neg_body + i);
    ruleSetUp(dl, rule);

    pddlISetFree(&rule_vars);
}

static void renameVarsInRules(pddl_datalog_t *dl)
{
    for (int i = 0; i < dl->rule_size; ++i)
        renameVarsInRule(dl, i);
}

static int cmpRuleIds(const void *a, const void *b, void *_d)
{
    pddl_datalog_t *dl = _d;
    int rule_id1 = *(int *)a;
    int rule_id2 = *(int *)b;

    if (rule_id1 == rule_id2)
        return 0;
    if (rule_id1 < 0)
        return 1;
    if (rule_id2 < 0)
        return -1;

    const pddl_datalog_rule_t *rule1 = dl->rule + rule_id1;
    const pddl_datalog_rule_t *rule2 = dl->rule + rule_id2;
    return pddlDatalogRuleCmpBodyFirst(dl, rule1, rule2);
}

/** Returns true if rule1 and rule2 can be merged */
static int canMerge(pddl_datalog_t *dl,
                    const pddl_datalog_rule_t *rule1,
                    const pddl_datalog_rule_t *rule2,
                    const int *pred_num_achievers)
{
    if (pred_num_achievers[rule1->head.pred] != 1)
        return 0;
    if (pred_num_achievers[rule2->head.pred] != 1)
        return 0;
    if (dl->pred[rule1->head.pred].arity != dl->pred[rule2->head.pred].arity)
        return 0;
    if (pddlDatalogAtomCmpArgs(dl, &rule1->head, &rule2->head) != 0)
        return 0;
    if (pddlDatalogRuleCmpBodyAndWeight(dl, rule1, rule2) != 0)
        return 0;
    return 1;
}

static int reduceRuleSet(pddl_datalog_t *dl, pddl_err_t *err)
{
    if (dl->rule_size == 0)
        return 0;

    CTX(err, "reduce_rule_set", "reduce-rule-set");
    int pred_num_achievers[dl->pred_size];
    ZEROIZE_ARR(pred_num_achievers, dl->pred_size);
    for (int ri = 0; ri < dl->rule_size; ++ri)
        pred_num_achievers[dl->rule[ri].head.pred]++;

    int *rule_ids = ALLOC_ARR(int, dl->rule_size);
    int num_rules = 0;
    for (int i = 0; i < dl->rule_size; ++i){
        // Consider only rules that are the only possible achievers of
        // their heads and the head atom was created as auxiliary
        // (otherwise we could loose some facts important for the caller,
        // especially if the caller changes the datalog program after later)
        if (pred_num_achievers[dl->rule[i].head.pred] == 1
                && dl->pred[dl->rule[i].head.pred].is_aux){
            rule_ids[num_rules++] = i;
        }
    }
    pddlSort(rule_ids, num_rules, sizeof(int), cmpRuleIds, dl);

    for (int i = 0; i < num_rules - 1; ++i){
        const pddl_datalog_rule_t *base = dl->rule + rule_ids[i];
        if (pred_num_achievers[base->head.pred] > 1)
            continue;

        // Fill merge_preds with predicates that can be merged into
        // base->head.pred and rm_rules with rules that can be removed
        // after merging because they'll become identical to base
        PDDL_ISET(merge_preds);
        PDDL_ISET(rm_rules);
        int next_id = i + 1;
        const pddl_datalog_rule_t *next = dl->rule + rule_ids[next_id];
        while (canMerge(dl, base, next, pred_num_achievers)){
            pddlISetAdd(&merge_preds, next->head.pred);
            pddlISetAdd(&rm_rules, rule_ids[next_id]);

            if (++next_id == num_rules)
                break;
            next = dl->rule + rule_ids[next_id];
        }

        pddlISetRm(&merge_preds, base->head.pred);
        if (pddlISetSize(&merge_preds) > 0){
            LOG(err, "Remapping %d predicates and removing %d rules",
                pddlISetSize(&merge_preds), pddlISetSize(&rm_rules));

            // Apply changes to the datalog program
            remapPreds(dl, &merge_preds, base->head.pred);
            pddlDatalogRmRules(dl, &rm_rules);

            pddlISetFree(&merge_preds);
            pddlISetFree(&rm_rules);
            FREE(rule_ids);
            CTXEND(err);
            return 1;
        }
        pddlISetFree(&merge_preds);
        pddlISetFree(&rm_rules);
    }
    FREE(rule_ids);
    LOG(err, "Nothing to reduce");
    CTXEND(err);
    return 0;
}

int pddlDatalogIsSafe(const pddl_datalog_t *dl)
{
    for (int i = 0; i < dl->rule_size; ++i){
        if (!pddlDatalogRuleIsSafe(dl, dl->rule + i))
            return 0;
    }
    return 1;
}

int pddlDatalogToNormalForm(pddl_datalog_t *dl, pddl_err_t *err)
{
    CTX(err, "dl_normal_form", "DL Normal form");
    LOG(err, "Normal form of the datalog program start"
        " (consts: %{in.consts}d, vars: %{in.vars}d,"
        " predicates: %{in.predicates}d, rules: %{in.rules}d)",
        dl->c_size, dl->var_size, dl->pred_size, dl->rule_size);
    setUp(dl, 0, err);
    if (!pddlDatalogIsSafe(dl)){
        ERR_RET(err, -1, "Cannot create normal form because the"
                 "datalog program is not safe");
    }

    int rule_size = dl->rule_size;
    for (int ci = 0; ci < rule_size; ++ci){
        while (dl->rule[ci].body_size > 2)
            toNormalFormStep(dl, ci);
    }

    renameVarsInRules(dl);
    while (reduceRuleSet(dl, err))
        ;

    LOG(err, "Normal form of the datalog program DONE"
        " (consts: %{out.consts}d, vars: %{out.vars}d,"
        " predicates: %{out.predicates}d, rules: %{out.rules}d)",
        dl->c_size, dl->var_size, dl->pred_size, dl->rule_size);
    dl->dirty = 1;
    CTXEND(err);
    return 0;
}

static void insertInitialFacts(pddl_datalog_t *dl,
                               int use_weight,
                               pddl_err_t *err)
{
    int args[dl->max_pred_arity];
    for (int ri = 0; ri < dl->rule_size; ++ri){
        const pddl_datalog_rule_t *rule = dl->rule + ri;
        if (rule->body_size > 0)
            continue;
        const pddl_datalog_atom_t *atom = &rule->head;
        int arity = dl->pred[atom->pred].arity;
        int abort = 0;
        for (int ai = 0; ai < arity; ++ai){
            if (IS_CONST(atom->arg[ai])){
                args[ai] = TO_IDX(atom->arg[ai]);
            }else{
                abort = 1;
                break;
            }
        }
        if (abort)
            continue;
        int fact_id = dbAddFact(dl, &dl->db, atom->pred, args);
        if (use_weight)
            dbSetFactWeight(dl, &dl->db, fact_id, &rule->weight, NULL);
    }
}

static int unify(pddl_datalog_t *dl,
                 int fact_pred,
                 const int *fact_arg,
                 const pddl_datalog_atom_t *atom,
                 int *var_map)
{
    if (fact_pred != atom->pred)
        return -1;

    int var;
    PDDL_ISET_FOR_EACH(&atom->var_set, var)
        var_map[var] = -1;

    int arity = dl->pred[fact_pred].arity;
    for (int i = 0; i < arity; ++i){
        if (IS_VAR(atom->arg[i])){
            int var = TO_IDX(atom->arg[i]);
            if (var_map[var] < 0){
                var_map[var] = fact_arg[i];
            }else if (var_map[var] != fact_arg[i]){
                return -1;
            }

        }else{
            int c = TO_IDX(atom->arg[i]);
            if (c != fact_arg[i])
                return -1;
        }
    }

    return 0;
}

static int ruleNegBodySatisfied(pddl_datalog_t *dl,
                                const pddl_datalog_rule_t *rule,
                                const int *var_map)
{
    for (int i = 0; i < rule->neg_body_size; ++i){
        const pddl_datalog_atom_t *a = rule->neg_body + i;
        int arity = dl->pred[a->pred].arity;
        int arg[arity];
        for (int ai = 0; ai < arity; ++ai){
            if (IS_CONST(a->arg[ai])){
                arg[ai] = TO_IDX(a->arg[ai]);
            }else{
                arg[ai] = var_map[TO_IDX(a->arg[ai])];
            }
        }
        if (dbHasFact(dl, &dl->db, a->pred, arg))
            return 0;
    }
    return 1;
}

static void ruleBodyWeight(pddl_datalog_t *dl,
                           int weight_type,
                           const pddl_datalog_rule_t *rule,
                           const int *var_map,
                           pddl_cost_t *w,
                           pddl_iset_t *fact_achievers)
{
    pddlCostSetZero(w);

    for (int bi = 0; bi < rule->body_size; ++bi){
        const pddl_datalog_atom_t *b = &rule->body[bi];
        int arity = dl->pred[b->pred].arity;
        int arg[arity];
        for (int i = 0; i < arity; ++i){
            if (IS_VAR(b->arg[i])){
                arg[i] = var_map[TO_IDX(b->arg[i])];
            }else{
                arg[i] = TO_IDX(b->arg[i]);
            }
        }
        const pddl_datalog_fact_t *f = dbFindFact(dl, &dl->db, b->pred, arg);
        ASSERT(f != NULL);
        if (fact_achievers != NULL)
            pddlISetAdd(fact_achievers, f->id);
        if (weight_type == WEIGHT_ADD){
            pddlCostSum(w, &f->weight);

        }else if (weight_type == WEIGHT_MAX){
            if (pddlCostCmp(w, &f->weight) < 0)
                *w = f->weight;

        }else{
            ASSERT_RUNTIME(0);
        }
    }
}

static void ruleToFact(pddl_datalog_t *dl,
                       int use_weight,
                       int use_fact_achievers,
                       const pddl_datalog_rule_t *rule,
                       const int *var_map)
{
    if (!ruleNegBodySatisfied(dl, rule, var_map))
        return;

    const pddl_datalog_atom_t *head = &rule->head;
    int arity = dl->pred[head->pred].arity;
    int arg[arity];
    for (int i = 0; i < arity; ++i){
        if (IS_VAR(head->arg[i])){
            arg[i] = var_map[TO_IDX(head->arg[i])];
        }else{
            arg[i] = TO_IDX(head->arg[i]);
        }
    }
    int fact_id = dbAddFact(dl, &dl->db, head->pred, arg);
    if (use_weight){
        pddl_cost_t w;
        if (use_fact_achievers){
            PDDL_ISET(fact_achievers);
            ruleBodyWeight(dl, use_weight, rule, var_map, &w, &fact_achievers);
            pddlCostSum(&w, &rule->weight);
            dbSetFactWeight(dl, &dl->db, fact_id, &w, &fact_achievers);
            pddlISetFree(&fact_achievers);

        }else{
            ruleBodyWeight(dl, use_weight, rule, var_map, &w, NULL);
            pddlCostSum(&w, &rule->weight);
            dbSetFactWeight(dl, &dl->db, fact_id, &w, NULL);
        }
    }
}

static void applyFactOnJoinRule(pddl_datalog_t *dl,
                                int use_weight,
                                int use_fact_achievers,
                                int atom_idx,
                                int rule_id,
                                int fact_id,
                                int *var_map,
                                pddl_err_t *err)
{
    const pddl_datalog_rule_t *rule = dl->rule + rule_id;
    int other_atom_idx = (atom_idx + 1) % 2;
    const pddl_datalog_atom_t *b1 = &rule->body[other_atom_idx];
    const pddl_iset_t *key_var = &rule->common_body_var_set;

    dbAddRelevantFact(dl, &dl->db, atom_idx, rule_id,
                      key_var, var_map, fact_id);
    const pddl_datalog_relevant_fact_t *rel;
    rel = dbFindRelevantFact(dl, &dl->db, other_atom_idx, rule_id,
                             key_var, var_map);
    if (rel == NULL)
        return;

    int b1_arity = dl->pred[b1->pred].arity;
    int fid;
    PDDL_ISET_FOR_EACH(&rel->fact, fid){
        const pddl_datalog_fact_t *f = dbFact(&dl->db, fid);
        ASSERT(f->arity == b1_arity);
        for (int i = 0; i < b1_arity; ++i){
            if (IS_VAR(b1->arg[i]))
                var_map[TO_IDX(b1->arg[i])] = f->arg[i];
        }
        ruleToFact(dl, use_weight, use_fact_achievers, rule, var_map);
    }
}

static void applyFactOnRule(pddl_datalog_t *dl,
                            int use_weight,
                            int use_fact_achievers,
                            const pddl_datalog_fact_t *f,
                            int rule_id,
                            pddl_err_t *err)
{
    const pddl_datalog_rule_t *rule = dl->rule + rule_id;
    int var_map[dl->var_size];
    if (rule->body_size == 1
            && unify(dl, f->pred, f->arg, &rule->body[0], var_map) == 0){
        ruleToFact(dl, use_weight, use_fact_achievers, rule, var_map);
    }

    if (rule->body_size == 2
            && unify(dl, f->pred, f->arg, &rule->body[0], var_map) == 0){
        applyFactOnJoinRule(dl, use_weight, use_fact_achievers, 0, rule_id,
                            f->id, var_map, err);
    }

    if (rule->body_size == 2
            && unify(dl, f->pred, f->arg, &rule->body[1], var_map) == 0){
        applyFactOnJoinRule(dl, use_weight, use_fact_achievers, 1, rule_id,
                            f->id, var_map, err);
    }
}

void pddlDatalogCanonicalModel(pddl_datalog_t *dl, pddl_err_t *err)
{
    CTX(err, "dl_canonical_model", "DL Canonical Model");
    LOG(err, "start (consts: %{in.consts}d, vars: %{in.vars}d,"
        " predicates: %{in.predicates}d, rules: %{in.rules}d)",
        dl->c_size, dl->var_size, dl->pred_size, dl->rule_size);
    setUp(dl, 1, err);

    insertInitialFacts(dl, NO_WEIGHT, err);
    LOG(err, "Added initial facts: %{initial_facts}d", dl->db.fact_size);

    int cur_id = 0;
    while (cur_id < dl->db.fact_size){
        const pddl_datalog_fact_t *f = dbFact(&dl->db, cur_id);
        int rule_id;
        PDDL_ISET_FOR_EACH(&dl->pred[f->pred].relevant_rules, rule_id)
            applyFactOnRule(dl, NO_WEIGHT, 0, f, rule_id, err);
        ++cur_id;
        if (cur_id % 100000 == 0){
            LOG(err, "progress (facts processed: %d, overall: %d,"
                " db-mem: %luMB)",
                cur_id, dl->db.fact_size,
                dbUsedMem(&dl->db) / (1024lu * 1024lu));
        }
    }
    dl->db.canonical_model_end = dl->db.fact_size;
    LOG(err, "DONE (facts: %{out.facts}d, db-mem: %luMB)",
        dl->db.fact_size, dbUsedMem(&dl->db) / (1024lu * 1024lu));
    CTXEND(err);
}

void pddlDatalogCanonicalModelCont(pddl_datalog_t *dl, pddl_err_t *err)
{
    // TODO: Refactor with pddlDatalogCanonicalModel()
    int cur_id = dl->db.canonical_model_end;
    while (cur_id < dl->db.fact_size){
        const pddl_datalog_fact_t *f = dbFact(&dl->db, cur_id);
        int rule_id;
        PDDL_ISET_FOR_EACH(&dl->pred[f->pred].relevant_rules, rule_id)
            applyFactOnRule(dl, NO_WEIGHT, 0, f, rule_id, err);
        ++cur_id;
    }
    dl->db.canonical_model_end = dl->db.fact_size;
}

static int weightedCanonicalModel(pddl_datalog_t *dl,
                                  int weight_type,
                                  int use_fact_achievers,
                                  pddl_cost_t *weight,
                                  pddl_err_t *err)
{
    int ret = -1;
    CTX(err, "dl_weighted_canonical_model", "DL Weighted Canonical Model");
    LOG(err, "start (consts: %{in.consts}d, vars: %{in.vars}d,"
        " predicates: %{in.predicates}d, rules: %{in.rules}d,"
        " weight_type: %{weight_type}s)",
        dl->c_size, dl->var_size, dl->pred_size, dl->rule_size,
        (weight_type == WEIGHT_ADD ? "add" : "max"));
    setUp(dl, 1, err);

    insertInitialFacts(dl, weight_type, err);
    LOG(err, "Added initial facts: %{initial_facts}d", dl->db.fact_size);

    int cur_id = 0;
    while (!pddlPairHeapEmpty(dl->db.fact_queue)){
        pddl_pairheap_node_t *qnode = pddlPairHeapExtractMin(dl->db.fact_queue);
        const pddl_datalog_fact_t *f;
        f = pddl_container_of(qnode, pddl_datalog_fact_t, heap);
        if (dl->pred[f->pred].is_goal){
            *weight = f->weight;
            ret = 0;
            break;
        }

        int rule_id;
        PDDL_ISET_FOR_EACH(&dl->pred[f->pred].relevant_rules, rule_id)
            applyFactOnRule(dl, weight_type, use_fact_achievers, f, rule_id, err);
        ++cur_id;
        if (cur_id % 100000 == 0){
            LOG(err, "progress (facts processed: %d, overall: %d,"
                " db-mem: %luMB)",
                cur_id, dl->db.fact_size,
                dbUsedMem(&dl->db) / (1024lu * 1024lu));
        }
    }
    LOG(err, "DONE (facts: %{out.facts}d, db-mem: %luMB)",
        dl->db.fact_size, dbUsedMem(&dl->db) / (1024lu * 1024lu));
    CTXEND(err);
    return ret;
}

int pddlDatalogWeightedCanonicalModelAdd(pddl_datalog_t *dl,
                                         pddl_cost_t *weight,
                                         int collect_fact_achievers,
                                         pddl_err_t *err)
{
    return weightedCanonicalModel(dl, WEIGHT_ADD, collect_fact_achievers,
                                  weight, err);
}

int pddlDatalogWeightedCanonicalModelMax(pddl_datalog_t *dl,
                                         pddl_cost_t *weight,
                                         int collect_fact_achievers,
                                         pddl_err_t *err)
{
    return weightedCanonicalModel(dl, WEIGHT_MAX, collect_fact_achievers,
                                  weight, err);
}

void pddlDatalogFactsFromCanonicalModel(
            pddl_datalog_t *dl,
            unsigned pred,
            void (*fn)(int pred_user_id,
                       int arity,
                       const pddl_obj_id_t *arg_user_id,
                       void *user_data),
            void *user_data)
{
    int arity = dl->pred[TO_IDX(pred)].arity;
    pddl_obj_id_t arg[arity];
    int fact_id;
    PDDL_ISET_FOR_EACH(&dl->db.pred_to_fact[TO_IDX(pred)], fact_id){
        pddl_datalog_fact_t *f = dbFact(&dl->db, fact_id);
        int p = dl->pred[f->pred].user_id;
        for (int i = 0; i < arity; ++i)
            arg[i] = dl->c[f->arg[i]].user_id;
        fn(p, arity, arg, user_data);
    }
}

void pddlDatalogFactsFromWeightedCanonicalModel(
            pddl_datalog_t *dl,
            unsigned pred,
            void (*fn)(int pred_user_id,
                       int arity,
                       const pddl_obj_id_t *arg_user_id,
                       const pddl_cost_t *weight,
                       void *user_data),
            void *user_data)
{
    int arity = dl->pred[TO_IDX(pred)].arity;
    pddl_obj_id_t arg[arity];
    int fact_id;
    PDDL_ISET_FOR_EACH(&dl->db.pred_to_fact[TO_IDX(pred)], fact_id){
        pddl_datalog_fact_t *f = dbFact(&dl->db, fact_id);
        int p = dl->pred[f->pred].user_id;
        for (int i = 0; i < arity; ++i)
            arg[i] = dl->c[f->arg[i]].user_id;
        fn(p, arity, arg, &f->weight, user_data);
    }
}

void pddlDatalogAchieverFactsFromWeightedCanonicalModel(
            pddl_datalog_t *dl,
            unsigned _goal_pred,
            void (*fn)(int pred_user_id,
                       int arity,
                       const pddl_obj_id_t *arg_user_id,
                       const pddl_cost_t *weight,
                       void *user_data),
            void *user_data)
{
    int goal_pred = TO_IDX(_goal_pred);
    ASSERT(dl->pred[goal_pred].arity == 0);
    ASSERT(dl->pred[goal_pred].is_goal);

    int *in_queue = CALLOC_ARR(int, dl->db.fact_size);
    PDDL_IARR(queue);
    pddl_datalog_fact_t *f = dbFindFact(dl, &dl->db, goal_pred, NULL);
    if (f != NULL){
        pddlIArrAdd(&queue, f->id);
        in_queue[f->id] = 1;
    }

    PDDL_ISET(achievers);
    for (int i = 0; i < pddlIArrSize(&queue); ++i){
        int fact_id = pddlIArrGet(&queue, i);
        pddl_datalog_fact_t *f = dbFact(&dl->db, fact_id);
        pddlISetUnion(&achievers, &f->best_fact_achievers);

        int achiever;
        PDDL_ISET_FOR_EACH(&f->best_fact_achievers, achiever){
            if (!in_queue[achiever]){
                pddlIArrAdd(&queue, achiever);
                in_queue[achiever] = 1;
            }
        }
    }

    int fact_id;
    PDDL_ISET_FOR_EACH(&achievers, fact_id){
        pddl_datalog_fact_t *f = dbFact(&dl->db, fact_id);
        int p = dl->pred[f->pred].user_id;
        int arity = dl->pred[f->pred].arity;
        pddl_obj_id_t arg[arity];
        for (int i = 0; i < arity; ++i)
            arg[i] = dl->c[f->arg[i]].user_id;
        fn(p, arity, arg, &f->weight, user_data);
    }

    pddlISetFree(&achievers);
    pddlIArrFree(&queue);
    FREE(in_queue);
}

void pddlDatalogSaveStateOfDB(pddl_datalog_t *dl)
{
    dbFreeze(dl, &dl->db);
}

void pddlDatalogRollbackDB(pddl_datalog_t *dl)
{
    dbRollback(dl, &dl->db);
}

void pddlDatalogAddFactToDB(pddl_datalog_t *dl,
                            unsigned in_pred,
                            const unsigned *in_arg)
{
    ASSERT_RUNTIME(IS_PRED(in_pred));
    int pred = TO_IDX(in_pred);
    int arg_size = dl->pred[pred].arity;
    int arg[arg_size];
    for (int i = 0; i < arg_size; ++i){
        ASSERT_RUNTIME(IS_CONST(in_arg[i]));
        arg[i] = TO_IDX(in_arg[i]);
    }
    dbAddFact(dl, &dl->db, pred, arg);
}

void pddlDatalogAtomInit(pddl_datalog_t *dl,
                         pddl_datalog_atom_t *atom,
                         unsigned pred)
{
    ZEROIZE(atom);
    int p = TO_IDX(pred);
    atom->pred = p;
    atom->arg = CALLOC_ARR(unsigned, dl->pred[p].arity);
}

void pddlDatalogAtomCopy(pddl_datalog_t *dl,
                         pddl_datalog_atom_t *dst,
                         const pddl_datalog_atom_t *src)
{
    ZEROIZE(dst);
    dst->pred = src->pred;
    dst->arg = CALLOC_ARR(unsigned, dl->pred[dst->pred].arity);
    memcpy(dst->arg, src->arg, sizeof(unsigned) * dl->pred[dst->pred].arity);
}

void pddlDatalogAtomFree(pddl_datalog_t *dl, pddl_datalog_atom_t *atom)
{
    if (atom->arg != NULL)
        FREE(atom->arg);
    pddlISetFree(&atom->var_set);
}

int pddlDatalogAtomCmpArgs(const pddl_datalog_t *dl,
                           const pddl_datalog_atom_t *atom1,
                           const pddl_datalog_atom_t *atom2)
{
    ASSERT_RUNTIME(dl->pred[atom1->pred].arity == dl->pred[atom2->pred].arity);
    return memcmp(atom1->arg, atom2->arg,
                  sizeof(unsigned) * dl->pred[atom1->pred].arity);
}

int pddlDatalogAtomCmp(const pddl_datalog_t *dl,
                       const pddl_datalog_atom_t *atom1,
                       const pddl_datalog_atom_t *atom2)
{
    int cmp = atom1->pred - atom2->pred;
    if (cmp == 0){
        cmp = memcmp(atom1->arg, atom2->arg,
                     sizeof(unsigned) * dl->pred[atom1->pred].arity);
    }
    return cmp;
}

void pddlDatalogAtomSetArg(pddl_datalog_t *dl,
                           pddl_datalog_atom_t *atom,
                           int argi,
                           unsigned term)
{
    ASSERT(argi < dl->pred[atom->pred].arity);
    atom->arg[argi] = term;
}

void pddlDatalogRuleInit(pddl_datalog_t *dl, pddl_datalog_rule_t *rule)
{
    ZEROIZE(rule);
}

void pddlDatalogRuleCopy(pddl_datalog_t *dl,
                         pddl_datalog_rule_t *dst,
                         const pddl_datalog_rule_t *src)
{
    ZEROIZE(dst);
    pddlDatalogAtomCopy(dl, &dst->head, &src->head);
    dst->body_alloc = src->body_alloc;
    dst->body_size = src->body_size;
    dst->body = ALLOC_ARR(pddl_datalog_atom_t, dst->body_alloc);
    for (int i = 0; i < dst->body_size; ++i)
        pddlDatalogAtomCopy(dl, dst->body + i, src->body + i);

    dst->neg_body_alloc = src->neg_body_alloc;
    dst->neg_body_size = src->neg_body_size;
    dst->neg_body = ALLOC_ARR(pddl_datalog_atom_t, dst->neg_body_alloc);
    for (int i = 0; i < dst->neg_body_size; ++i)
        pddlDatalogAtomCopy(dl, dst->neg_body + i, src->neg_body + i);

    dst->weight = src->weight;
}

void pddlDatalogRuleFree(pddl_datalog_t *dl, pddl_datalog_rule_t *rule)
{
    pddlDatalogAtomFree(dl, &rule->head);
    for (int i = 0; i < rule->body_size; ++i)
        pddlDatalogAtomFree(dl, &rule->body[i]);
    if (rule->body != NULL)
        FREE(rule->body);
    for (int i = 0; i < rule->neg_body_size; ++i)
        pddlDatalogAtomFree(dl, &rule->neg_body[i]);
    if (rule->neg_body != NULL)
        FREE(rule->neg_body);
    pddlISetFree(&rule->var_set);
    pddlISetFree(&rule->common_body_var_set);
}

int pddlDatalogRuleCmp(const pddl_datalog_t *dl,
                       const pddl_datalog_rule_t *rule1,
                       const pddl_datalog_rule_t *rule2)
{
    int cmp = pddlDatalogAtomCmp(dl, &rule1->head, &rule2->head);
    if (cmp == 0)
        cmp = pddlDatalogRuleCmpBodyAndWeight(dl, rule1, rule2);
    return cmp;
}

int pddlDatalogRuleCmpBodyFirst(const pddl_datalog_t *dl,
                                const pddl_datalog_rule_t *rule1,
                                const pddl_datalog_rule_t *rule2)
{
    int cmp = pddlDatalogRuleCmpBodyAndWeight(dl, rule1, rule2);
    if (cmp == 0)
        cmp = pddlDatalogAtomCmp(dl, &rule1->head, &rule2->head);
    return cmp;
}

int pddlDatalogRuleCmpBodyAndWeight(const pddl_datalog_t *dl,
                                    const pddl_datalog_rule_t *rule1,
                                    const pddl_datalog_rule_t *rule2)
{
    int cmp = rule1->body_size - rule2->body_size;
    for (int i = 0; cmp == 0 && i < rule1->body_size; ++i)
        cmp = pddlDatalogAtomCmp(dl, rule1->body + i, rule2->body + i);
    if (cmp == 0)
        cmp = rule1->neg_body_size - rule2->neg_body_size;
    for (int i = 0; cmp == 0 && i < rule1->neg_body_size; ++i)
        cmp = pddlDatalogAtomCmp(dl, rule1->neg_body + i, rule2->neg_body + i);
    if (cmp == 0)
        cmp = pddlCostCmp(&rule1->weight, &rule2->weight);
    return cmp;
}

void pddlDatalogRuleSetHead(pddl_datalog_t *dl,
                            pddl_datalog_rule_t *rule,
                            const pddl_datalog_atom_t *head)
{
    pddlDatalogAtomFree(dl, &rule->head);
    pddlDatalogAtomCopy(dl, &rule->head, head);
}

void pddlDatalogRuleAddBody(pddl_datalog_t *dl,
                            pddl_datalog_rule_t *rule,
                            const pddl_datalog_atom_t *atom)
{
    if (rule->body_size == rule->body_alloc){
        if (rule->body_alloc == 0)
            rule->body_alloc = 1;
        rule->body_alloc *= 2;
        rule->body = REALLOC_ARR(rule->body, pddl_datalog_atom_t,
                                 rule->body_alloc);
    }
    pddl_datalog_atom_t *a = rule->body + rule->body_size++;
    pddlDatalogAtomCopy(dl, a, atom);
}

void pddlDatalogRuleAddNegStaticBody(pddl_datalog_t *dl,
                                     pddl_datalog_rule_t *rule,
                                     const pddl_datalog_atom_t *atom)
{
    if (rule->neg_body_size == rule->neg_body_alloc){
        if (rule->neg_body_alloc == 0)
            rule->neg_body_alloc = 1;
        rule->neg_body_alloc *= 2;
        rule->neg_body = REALLOC_ARR(rule->neg_body, pddl_datalog_atom_t,
                                     rule->neg_body_alloc);
    }
    pddl_datalog_atom_t *a = rule->neg_body + rule->neg_body_size++;
    pddlDatalogAtomCopy(dl, a, atom);
}

void pddlDatalogRuleRmBody(pddl_datalog_t *dl,
                           pddl_datalog_rule_t *rule,
                           int i)
{
    pddlDatalogAtomFree(dl, rule->body + i);
    for (int j = i + 1; j < rule->body_size; ++j)
        rule->body[j - 1] = rule->body[j];
    --rule->body_size;
}

void pddlDatalogRuleSetWeight(pddl_datalog_t *dl,
                              pddl_datalog_rule_t *rule,
                              const pddl_cost_t *weight)
{
    rule->weight = *weight;
}

int pddlDatalogRuleIsSafe(const pddl_datalog_t *dl,
                          const pddl_datalog_rule_t *rule)
{
    PDDL_ISET(body_vars);
    for (int i = 0; i < rule->body_size; ++i){
        int arity = dl->pred[rule->body[i].pred].arity;
        for (int j = 0; j < arity; ++j){
            if (IS_VAR(rule->body[i].arg[j]))
                pddlISetAdd(&body_vars, TO_IDX(rule->body[i].arg[j]));
        }
    }
    int arity = dl->pred[rule->head.pred].arity;
    for (int j = 0; j < arity; ++j){
        if (IS_VAR(rule->head.arg[j])){
            if (!pddlISetIn(TO_IDX(rule->head.arg[j]), &body_vars))
                return 0;
        }
    }
    pddlISetFree(&body_vars);

    return 1;
}


static void printEl(const pddl_datalog_t *dl, unsigned id, FILE *fout)
{
    int idx = TO_IDX(id);
    if (IS_CONST(id)){
        if (dl->c[idx].name != NULL){
            fprintf(fout, "%s", dl->c[idx].name);
        }else{
            fprintf(fout, "_c%d", idx);
        }

    }else if (IS_VAR(id)){
        if (dl->var[idx].name != NULL){
            fprintf(fout, "%s", dl->var[idx].name);
        }else{
            fprintf(fout, "_X%d", idx);
        }

    }else if (IS_PRED(id)){
        if (dl->pred[idx].name != NULL){
            fprintf(fout, "%s", dl->pred[idx].name);
        }else{
            fprintf(fout, "_p%d", idx);
        }
    }
}

static void printAtom(const pddl_datalog_t *dl,
                      const pddl_datalog_atom_t *atom,
                      FILE *fout)
{
    printEl(dl, IDX_TO_PRED(atom->pred), fout);
    int p = atom->pred;
    fprintf(fout, "(");
    for (int i = 0; i < dl->pred[p].arity; ++i){
        if (i > 0)
            fprintf(fout, ", ");
        printEl(dl, atom->arg[i], fout);
    }
    fprintf(fout, ")");
}

void pddlDatalogPrintRule(const pddl_datalog_t *dl,
                          const pddl_datalog_rule_t *c,
                          FILE *fout)
{
    printAtom(dl, &c->head, fout);
    if (c->body_size > 0 || c->neg_body_size > 0)
        fprintf(fout, " :- ");
    if (c->body_size > 0){
        printAtom(dl, c->body + 0, fout);
        for (int i = 1; i < c->body_size; ++i){
            fprintf(fout, ", ");
            printAtom(dl, c->body + i, fout);
        }
    }
    if (c->neg_body_size > 0){
        if (c->body_size > 0)
            fprintf(fout, ", ");
        fprintf(fout, "!");
        printAtom(dl, c->neg_body + 0, fout);
        for (int i = 1; i < c->neg_body_size; ++i){
            fprintf(fout, ", !");
            printAtom(dl, c->neg_body + i, fout);
        }
    }
    fprintf(fout, ".");
    if (pddlCostCmp(&c->weight, &pddl_cost_zero) != 0)
        fprintf(fout, " ; w = %s", F_COST(&c->weight));
    fprintf(fout, "\n");
}

void pddlDatalogPrint(const pddl_datalog_t *dl, FILE *fout)
{
    for (int ci = 0; ci < dl->rule_size; ++ci){
        pddlDatalogPrintRule(dl, dl->rule + ci, fout);
    }
}
