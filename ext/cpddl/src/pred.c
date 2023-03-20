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
#include "pddl/pred.h"
#include "lisp_err.h"
#include "internal.h"

struct _set_t {
    pddl_pred_t *pred;
    pddl_types_t *types;
    const char *owner_var;
};
typedef struct _set_t set_t;

static const char *eq_name = "=";

void pddlPredFree(pddl_pred_t *pred)
{
    if (pred->param != NULL)
        FREE(pred->param);
    if (pred->name != NULL)
        FREE(pred->name);
}

static int setCB(const pddl_lisp_node_t *root,
                 int child_from, int child_to, int child_type, void *ud,
                 pddl_err_t *err)
{
    pddl_pred_t *pred = ((set_t *)ud)->pred;
    pddl_types_t *types = ((set_t *)ud)->types;
    const char *owner_var = ((set_t *)ud)->owner_var;
    int i, j, tid;

    tid = 0;
    if (child_type >= 0){
        const pddl_lisp_node_t *node = root->child + child_type;
        if ((tid = pddlTypeFromLispNode(types, node, err)) < 0)
            return -1;
    }

    j = pred->param_size;
    pred->param_size += child_to - child_from;
    pred->param = REALLOC_ARR(pred->param, int, pred->param_size);
    for (i = child_from; i < child_to; ++i, ++j){
        pred->param[j] = tid;
        if (owner_var != NULL && strcmp(owner_var, root->child[i].value) == 0){
            pred->owner_param = j;
        }
    }
    return 0;
}

static int checkDuplicate(const pddl_preds_t *ps, const char *name)
{
    for (int i = 0; i < ps->pred_size; ++i){
        if (strcmp(ps->pred[i].name, name) == 0)
            return 1;
    }
    return 0;
}

static int parsePred(pddl_t *pddl,
                     const pddl_lisp_node_t *n,
                     const char *owner_var,
                     const char *errname,
                     pddl_preds_t *ps,
                     pddl_err_t *err)
{
    pddl_pred_t *p;
    set_t set;

    if (n->child_size < 1 || n->child[0].value == NULL)
        ERR_LISP_RET(err, -1, n, "Invalid %s", errname);

    if (checkDuplicate(ps, n->child[0].value)){
        // TODO: err/warn
        ERR_LISP_RET(err, -1, n, "Duplicate %s `%s'",
                     errname, n->child[0].value);
    }

    p = pddlPredsAdd(ps);
    set.pred = p;
    set.types = &pddl->type;
    set.owner_var = owner_var;
    if (pddlLispParseTypedList(n, 1, n->child_size, setCB, &set, err) != 0){
        pddlPredsRemoveLast(ps);
        PDDL_TRACE_PREPEND_RET(err, -1, "%s `%s': ", errname, n->child[0].value);
    }

    p->name = STRDUP(n->child[0].value);
    return 0;
}

static int parsePrivatePreds(pddl_t *pddl,
                             const pddl_lisp_node_t *n,
                             pddl_preds_t *ps,
                             pddl_err_t *err)
{
    const char *owner_var;
    int from;

    if (pddl->require.factored_privacy){
        if (n->child_size < 2 || n->child[0].kw != PDDL_KW_PRIVATE){
            ERR_LISP_RET2(err, -1, n,
                          "Invalid definition of :private predicate");
        }

        owner_var = NULL;
        from = 1;

    }else{
        if (n->child_size < 3
                || n->child[0].kw != PDDL_KW_PRIVATE
                || n->child[1].value == NULL
                || n->child[1].value[0] != '?'
                || (n->child[2].value != NULL && n->child_size < 5)){
            ERR_LISP_RET2(err, -1, n,
                          "Invalid definition of :private predicate");
        }

        owner_var = n->child[1].value;

        if (n->child[2].value == NULL){
            from = 2;
        }else{
            from = 4;
        }
    }

    for (int i = from; i < n->child_size; ++i){
        if (parsePred(pddl, n->child + i, owner_var,
                      "private predicate", ps, err) != 0){
            PDDL_TRACE_RET(err, -1);
        }

        ps->pred[ps->pred_size - 1].is_private = 1;
    }

    return 0;
}

static void addEqPredicate(pddl_preds_t *ps)
{
    pddl_pred_t *p;

    p = pddlPredsAdd(ps);
    p->name = STRDUP(eq_name);
    p->param_size = 2;
    p->param = CALLOC_ARR(int, 2);
    ps->eq_pred = ps->pred_size - 1;
}

int pddlPredsParse(pddl_t *pddl, pddl_err_t *err)
{
    const pddl_lisp_node_t *n;
    int  private;

    pddl->pred.eq_pred = -1;
    addEqPredicate(&pddl->pred);

    n = pddlLispFindNode(&pddl->domain_lisp->root, PDDL_KW_PREDICATES);
    if (n == NULL){
        LOG(err, "No predicates found");
        return 0;
    }

    // Determine if we can expect :private definitions
    private = pddl->require.unfactored_privacy || pddl->require.factored_privacy;

    // Fisrt parse non :private predicates
    for (int i = 1; i < n->child_size; ++i){
        if (n->child[i].child_size > 0
                && n->child[i].child[0].kw == PDDL_KW_PRIVATE){
            if (!private){
                ERR_LISP_RET(err, -1, n->child + i,
                             "Private predicates are allowed only"
                             " with :factored-privacy and"
                             " :unfactored-privacy (%s).",
                             pddl->domain_lisp->filename);
            }
            continue;
        }

        if (parsePred(pddl, n->child + i, NULL, "predicate", &pddl->pred,
                    err) != 0)
            PDDL_TRACE_PREPEND_RET(err, -1, "While parsing :predicates in %s: ",
                                   pddl->domain_lisp->filename);
    }

    // And then the private predicates
    if (private){
        for (int i = 1; i < n->child_size; ++i){
            if (n->child[i].child_size == 0
                    || n->child[i].child[0].kw != PDDL_KW_PRIVATE){
                continue;
            }

            if (parsePrivatePreds(pddl, n->child + i, &pddl->pred, err) != 0)
                PDDL_TRACE_PREPEND_RET(err, -1, "While parsing private"
                                       " :predicates in %s: ",
                                       pddl->domain_lisp->filename);
        }
    }

    LOG(err, "Predicates parsed, num predicates: %{parsed_preds}d",
        pddl->pred.pred_size);
    return 0;
}

void pddlPredsInitCopy(pddl_preds_t *dst, const pddl_preds_t *src)
{
    ZEROIZE(dst);
    dst->eq_pred = src->eq_pred;
    dst->pred_size = dst->pred_alloc = src->pred_size;
    dst->pred = CALLOC_ARR(pddl_pred_t, src->pred_size);
    for (int i = 0; i < src->pred_size; ++i){
        dst->pred[i] = src->pred[i];
        if (dst->pred[i].name != NULL)
            dst->pred[i].name = STRDUP(src->pred[i].name);
        if (dst->pred[i].param != NULL){
            dst->pred[i].param_size = src->pred[i].param_size;
            dst->pred[i].param = CALLOC_ARR(int, src->pred[i].param_size);
            memcpy(dst->pred[i].param, src->pred[i].param,
                   sizeof(int) * src->pred[i].param_size);
        }
    }
}

int pddlFuncsParse(pddl_t *pddl, pddl_err_t *err)
{
    const pddl_lisp_node_t *n;

    pddl->func.eq_pred = -1;

    n = pddlLispFindNode(&pddl->domain_lisp->root, PDDL_KW_FUNCTIONS);
    if (n == NULL){
        LOG(err, "No functions found.");
        return 0;
    }

    for (int i = 1; i < n->child_size; ++i){
        if (parsePred(pddl, n->child + i, NULL, "function",
                      &pddl->func, err) != 0){
            PDDL_TRACE_PREPEND_RET(err, -1, "While parsing :functions in %s: ",
                                   pddl->domain_lisp->filename);
        }

        if (i + 2 < n->child_size
                && n->child[i + 1].value != NULL
                && strcmp(n->child[i + 1].value, "-") == 0){
            if (n->child[i + 2].value == NULL
                    || strcmp(n->child[i + 2].value, "number") != 0){
                PDDL_ERR_RET(err, -1, "While parsing :functions in %s: Only number"
                             " functions are supported (line %d).",
                             pddl->domain_lisp->filename, n->child[i + 2].lineno);
            }
            i += 2;
        }
    }
    LOG(err, "Functions parsed, num functions: %{parsed_funcs}d",
        pddl->func.pred_size);
    return 0;
}

void pddlPredsFree(pddl_preds_t *ps)
{
    for (int i = 0; i < ps->pred_size; ++i)
        pddlPredFree(ps->pred + i);
    if (ps->pred != NULL)
        FREE(ps->pred);
}

int pddlPredsGet(const pddl_preds_t *ps, const char *name)
{
    for (int i = 0; i < ps->pred_size; ++i){
        if (strcmp(ps->pred[i].name, name) == 0)
            return i;
    }
    return -1;
}

pddl_pred_t *pddlPredsAdd(pddl_preds_t *ps)
{
    pddl_pred_t *p;

    if (ps->pred_size >= ps->pred_alloc){
        if (ps->pred_alloc == 0){
            ps->pred_alloc = 2;
        }else{
            ps->pred_alloc *= 2;
        }
        ps->pred = REALLOC_ARR(ps->pred, pddl_pred_t,
                               ps->pred_alloc);
    }

    p = ps->pred + ps->pred_size++;
    ZEROIZE(p);
    p->id = ps->pred_size - 1;
    p->owner_param = -1;
    p->neg_of = -1;
    return p;
}

pddl_pred_t *pddlPredsAddCopy(pddl_preds_t *ps, int src_id)
{
    pddl_pred_t *dst = pddlPredsAdd(ps);
    const pddl_pred_t *src = ps->pred + src_id;
    int dst_id = dst->id;

    memcpy(dst, src, sizeof(*src));
    dst->id = dst_id;

    if (src->param != NULL){
        dst->param = ALLOC_ARR(int, src->param_size);
        memcpy(dst->param, src->param, sizeof(int) * src->param_size);
    }

    if (src->name != NULL)
        dst->name = STRDUP(src->name);

    return dst;
}

void pddlPredsRemoveLast(pddl_preds_t *ps)
{
    pddl_pred_t *p;

    p = ps->pred + --ps->pred_size;
    if (p->param != NULL)
        FREE(p->param);
    if (p->name != NULL)
        FREE(p->name);
}

void pddlPredsRemapTypes(pddl_preds_t *ps,
                         const int *type_remap,
                         int *pred_remap)
{
    int ins = 0;
    for (int p = 0; p < ps->pred_size; ++p){
        pddl_pred_t *pred = ps->pred + p;
        int fail = 0;
        for (int parami = 0; parami < pred->param_size; ++parami){
            if (type_remap[pred->param[parami]] == -1){
                fail = 1;
                break;
            }else{
                pred->param[parami] = type_remap[pred->param[parami]];
            }
        }
        if (fail){
            pred_remap[p] = -1;
            pddlPredFree(pred);
        }else{
            ps->pred[ins] = *pred;
            ps->pred[ins].id = ins;
            if (ps->pred[ins].neg_of >= 0)
                ps->pred[ins].neg_of = pred_remap[ps->pred[ins].neg_of];
            pred_remap[p] = ins++;
        }
    }
    ps->pred_size = ins;
}

void pddlPredsPrint(const pddl_preds_t *ps,
                    const char *title, FILE *fout)
{
    fprintf(fout, "%s[%d]:\n", title, ps->pred_size);
    for (int i = 0; i < ps->pred_size; ++i){
        if (ps->pred[i].id != i)
            fprintf(stderr, "%d %d -- %s\n", ps->pred[i].id, i, ps->pred[i].name);
        ASSERT(ps->pred[i].id == i);
        fprintf(fout, "    %s:", ps->pred[i].name);
        for (int j = 0; j < ps->pred[i].param_size; ++j){
            fprintf(fout, " %d", ps->pred[i].param[j]);
        }
        fprintf(fout, " :: is-private: %d, owner-param: %d",
                ps->pred[i].is_private, ps->pred[i].owner_param);
        fprintf(fout, ", read: %d, write: %d",
                ps->pred[i].read, ps->pred[i].write);
        if (ps->pred[i].neg_of >= 0)
            fprintf(fout, ", neg-of: %d", ps->pred[i].neg_of);
        fprintf(fout, "\n");
    }
}

static void printPDDL(const char *t,
                      const pddl_preds_t *ps,
                      const pddl_types_t *ts,
                      FILE *fout)
{
    if (ps->pred_size == 0)
        return;
    fprintf(fout, "(:%s\n", t);
    for (int i = 0; i < ps->pred_size; ++i){
        if (i == ps->eq_pred)
            continue;
        const pddl_pred_t *p = ps->pred + i;
        fprintf(fout, "    (%s", p->name);
        for (int j = 0; j < p->param_size; ++j){
            fprintf(fout, " ?x%d - %s", j, ts->type[p->param[j]].name);
        }
        fprintf(fout, ")\n");
    }
    fprintf(fout, ")\n");
}

void pddlPredsPrintPDDL(const pddl_preds_t *ps,
                        const pddl_types_t *ts,
                        FILE *fout)
{
    printPDDL("predicates", ps, ts, fout);
}

void pddlFuncsPrintPDDL(const pddl_preds_t *ps,
                        const pddl_types_t *ts,
                        FILE *fout)
{
    printPDDL("functions", ps, ts, fout);
}
