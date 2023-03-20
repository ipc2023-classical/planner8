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

#include "internal.h"
#include "lisp_err.h"
#include "pddl/sort.h"
#include "pddl/pddl.h"
#include "pddl/fm.h"

static char *type_names[PDDL_FM_NUM_TYPES] = {
    "and",      /* PDDL_FM_AND */
    "or",       /* PDDL_FM_OR */
    "forall",   /* PDDL_FM_FORALL */
    "exist",    /* PDDL_FM_EXIST */
    "when",     /* PDDL_FM_WHEN */
    "atom",     /* PDDL_FM_ATOM */
    "assign",   /* PDDL_FM_ASSIGN */
    "increase", /* PDDL_FM_INCREASE */
    "bool",     /* PDDL_FM_BOOL */
    "imply",    /* PDDL_FM_IMPLY */
};

const char *pddlFmTypeName(pddl_fm_type_t type)
{
    return type_names[type];
}

typedef void (*pddl_fm_method_del_fn)(pddl_fm_t *);
typedef pddl_fm_t *(*pddl_fm_method_clone_fn)(const pddl_fm_t *);
typedef pddl_fm_t *(*pddl_fm_method_negate_fn)(const pddl_fm_t *,
                                               const pddl_t *);
typedef int (*pddl_fm_method_eq_fn)(const pddl_fm_t *,
                                    const pddl_fm_t *);
typedef int (*pddl_fm_method_traverse_fn)(pddl_fm_t *,
                                          int (*pre)(pddl_fm_t *, void *),
                                          int (*post)(pddl_fm_t *, void *),
                                          void *userdata);
typedef int (*pddl_fm_method_rebuild_fn)(
                                         pddl_fm_t **c,
                                         int (*pre)(pddl_fm_t **, void *),
                                         int (*post)(pddl_fm_t **, void *),
                                         void *userdata);
typedef void (*pddl_fm_method_print_pddl_fn)(
                                             const pddl_fm_t *c,
                                             const pddl_t *pddl,
                                             const pddl_params_t *params,
                                             FILE *fout);

static int fmEq(const pddl_fm_t *, const pddl_fm_t *);
static int fmTraverse(pddl_fm_t *c,
                      int (*pre)(pddl_fm_t *, void *),
                      int (*post)(pddl_fm_t *, void *),
                      void *u);
static int fmRebuild(pddl_fm_t **c,
                     int (*pre)(pddl_fm_t **, void *),
                     int (*post)(pddl_fm_t **, void *),
                     void *u);

struct pddl_fm_cls {
    pddl_fm_method_del_fn del;
    pddl_fm_method_clone_fn clone;
    pddl_fm_method_negate_fn negate;
    pddl_fm_method_eq_fn eq;
    pddl_fm_method_traverse_fn traverse;
    pddl_fm_method_rebuild_fn rebuild;
    pddl_fm_method_print_pddl_fn print_pddl;
};
typedef struct pddl_fm_cls pddl_fm_cls_t;

#define METHOD(X, NAME) ((pddl_fm_method_##NAME##_fn)(X))
#define MCLS(NAME) \
{ \
    .del = METHOD(fm##NAME##Del, del), \
    .clone = METHOD(fm##NAME##Clone, clone), \
    .negate = METHOD(fm##NAME##Negate, negate), \
    .eq = METHOD(fm##NAME##Eq, eq), \
    .traverse = METHOD(fm##NAME##Traverse, traverse), \
    .rebuild = METHOD(fm##NAME##Rebuild, rebuild), \
    .print_pddl = METHOD(fm##NAME##PrintPDDL, print_pddl), \
}


struct parse_ctx {
    pddl_types_t *types;
    const pddl_objs_t *objs;
    const pddl_preds_t *preds;
    const pddl_preds_t *funcs;
    const pddl_params_t *params;
    const char *err_prefix;
    pddl_err_t *err;
};
typedef struct parse_ctx parse_ctx_t;

#define OBJ(C, T) PDDL_FM_CAST(C, T)

static void fmPartDel(pddl_fm_junc_t *);
static pddl_fm_junc_t *fmPartClone(const pddl_fm_junc_t *p);
static pddl_fm_junc_t *fmPartNegate(const pddl_fm_junc_t *p, const pddl_t *pddl);
static int fmPartEq(const pddl_fm_junc_t *c1, const pddl_fm_junc_t *c2);
static void fmPartAdd(pddl_fm_junc_t *p, pddl_fm_t *add);
static int fmPartTraverse(pddl_fm_junc_t *,
                          int (*pre)(pddl_fm_t *, void *),
                          int (*post)(pddl_fm_t *, void *),
                          void *userdata);
static int fmPartRebuild(pddl_fm_junc_t **p,
                         int (*pre)(pddl_fm_t **, void *),
                         int (*post)(pddl_fm_t **, void *),
                         void *userdata);
static void fmPartPrintPDDL(const pddl_fm_junc_t *p,
                            const pddl_t *pddl,
                            const pddl_params_t *params,
                            FILE *fout);

static void fmQuantDel(pddl_fm_quant_t *);
static pddl_fm_quant_t *fmQuantClone(const pddl_fm_quant_t *q);
static pddl_fm_quant_t *fmQuantNegate(const pddl_fm_quant_t *q,
                                      const pddl_t *pddl);
static int fmQuantEq(const pddl_fm_quant_t *c1,
                     const pddl_fm_quant_t *c2);
static int fmQuantTraverse(pddl_fm_quant_t *,
                           int (*pre)(pddl_fm_t *, void *),
                           int (*post)(pddl_fm_t *, void *),
                           void *userdata);
static int fmQuantRebuild(pddl_fm_quant_t **q,
                          int (*pre)(pddl_fm_t **, void *),
                          int (*post)(pddl_fm_t **, void *),
                          void *userdata);
static void fmQuantPrintPDDL(const pddl_fm_quant_t *q,
                             const pddl_t *pddl,
                             const pddl_params_t *params,
                             FILE *fout);

static void fmWhenDel(pddl_fm_when_t *);
static pddl_fm_when_t *fmWhenClone(const pddl_fm_when_t *w);
static pddl_fm_when_t *fmWhenNegate(const pddl_fm_when_t *w,
                                    const pddl_t *pddl);
static int fmWhenEq(const pddl_fm_when_t *c1, const pddl_fm_when_t *c2);
static int fmWhenTraverse(pddl_fm_when_t *w,
                          int (*pre)(pddl_fm_t *, void *),
                          int (*post)(pddl_fm_t *, void *),
                          void *userdata);
static int fmWhenRebuild(pddl_fm_when_t **w,
                         int (*pre)(pddl_fm_t **, void *),
                         int (*post)(pddl_fm_t **, void *),
                         void *userdata);
static void fmWhenPrintPDDL(const pddl_fm_when_t *w,
                            const pddl_t *pddl,
                            const pddl_params_t *params,
                            FILE *fout);

static void fmAtomDel(pddl_fm_atom_t *);
static pddl_fm_atom_t *fmAtomClone(const pddl_fm_atom_t *a);
static pddl_fm_atom_t *fmAtomNegate(const pddl_fm_atom_t *a,
                                    const pddl_t *pddl);
static int fmAtomEq(const pddl_fm_atom_t *c1, const pddl_fm_atom_t *c2);
static int fmAtomTraverse(pddl_fm_atom_t *,
                          int (*pre)(pddl_fm_t *, void *),
                          int (*post)(pddl_fm_t *, void *),
                          void *userdata);
static int fmAtomRebuild(pddl_fm_atom_t **a,
                         int (*pre)(pddl_fm_t **, void *),
                         int (*post)(pddl_fm_t **, void *),
                         void *userdata);
static void fmAtomPrintPDDL(const pddl_fm_atom_t *a,
                            const pddl_t *pddl,
                            const pddl_params_t *params,
                            FILE *fout);

static void fmFuncOpDel(pddl_fm_func_op_t *);
static pddl_fm_func_op_t *fmFuncOpClone(const pddl_fm_func_op_t *);
static pddl_fm_func_op_t *fmFuncOpNegate(const pddl_fm_func_op_t *,
                                         const pddl_t *pddl);
static int fmFuncOpEq(const pddl_fm_func_op_t *c1,
                      const pddl_fm_func_op_t *c2);
static int fmFuncOpTraverse(pddl_fm_func_op_t *,
                            int (*pre)(pddl_fm_t *, void *),
                            int (*post)(pddl_fm_t *, void *),
                            void *userdata);
static int fmFuncOpRebuild(pddl_fm_func_op_t **,
                           int (*pre)(pddl_fm_t **, void *),
                           int (*post)(pddl_fm_t **, void *),
                           void *userdata);
static void fmFuncOpPrintPDDL(const pddl_fm_func_op_t *,
                              const pddl_t *pddl,
                              const pddl_params_t *params,
                              FILE *fout);

static void fmBoolDel(pddl_fm_bool_t *);
static pddl_fm_bool_t *fmBoolClone(const pddl_fm_bool_t *a);
static pddl_fm_bool_t *fmBoolNegate(const pddl_fm_bool_t *a,
                                    const pddl_t *pddl);
static int fmBoolEq(const pddl_fm_bool_t *c1, const pddl_fm_bool_t *c2);
static int fmBoolTraverse(pddl_fm_bool_t *,
                          int (*pre)(pddl_fm_t *, void *),
                          int (*post)(pddl_fm_t *, void *),
                          void *userdata);
static int fmBoolRebuild(pddl_fm_bool_t **a,
                         int (*pre)(pddl_fm_t **, void *),
                         int (*post)(pddl_fm_t **, void *),
                         void *userdata);
static void fmBoolPrintPDDL(const pddl_fm_bool_t *b,
                            const pddl_t *pddl,
                            const pddl_params_t *params,
                            FILE *fout);

static void fmImplyDel(pddl_fm_imply_t *);
static pddl_fm_imply_t *fmImplyClone(const pddl_fm_imply_t *a);
static pddl_fm_t *fmImplyNegate(const pddl_fm_imply_t *a,
                                const pddl_t *pddl);
static int fmImplyEq(const pddl_fm_imply_t *c1,
                     const pddl_fm_imply_t *c2);
static int fmImplyTraverse(pddl_fm_imply_t *,
                           int (*pre)(pddl_fm_t *, void *),
                           int (*post)(pddl_fm_t *, void *),
                           void *userdata);
static int fmImplyRebuild(pddl_fm_imply_t **a,
                          int (*pre)(pddl_fm_t **, void *),
                          int (*post)(pddl_fm_t **, void *),
                          void *userdata);
static void fmImplyPrintPDDL(const pddl_fm_imply_t *b,
                             const pddl_t *pddl,
                             const pddl_params_t *params,
                             FILE *fout);


static pddl_fm_cls_t cond_cls[PDDL_FM_NUM_TYPES] = {
    MCLS(Part),   // PDDL_FM_AND
    MCLS(Part),   // PDDL_FM_OR
    MCLS(Quant),  // PDDL_FM_FORALL
    MCLS(Quant),  // PDDL_FM_EXIST
    MCLS(When),   // PDDL_FM_WHEN
    MCLS(Atom),   // PDDL_FM_ATOM
    MCLS(FuncOp), // PDDL_FM_ASSIGN
    MCLS(FuncOp), // PDDL_FM_INCREASE
    MCLS(Bool),   // PDDL_FM_BOOL
    MCLS(Imply),  // PDDL_FM_IMPLY
};

static pddl_fm_t *parse(const pddl_lisp_node_t *root,
                        const parse_ctx_t *ctx,
                        int negated);

#define fmNew(CTYPE, TYPE) \
    (CTYPE *)_fmNew(sizeof(CTYPE), TYPE)

static pddl_fm_t *_fmNew(int size, unsigned type)
{
    pddl_fm_t *c = ZMALLOC(size);
    c->type = type;
    pddlListInit(&c->conn);
    return c;
}


/*** PART ***/
static pddl_fm_junc_t *fmPartNew(int type)
{
    pddl_fm_junc_t *p = fmNew(pddl_fm_junc_t, type);
    pddlListInit(&p->part);
    return p;
}

static void fmPartDel(pddl_fm_junc_t *p)
{
    pddl_list_t *item, *tmp;
    PDDL_LIST_FOR_EACH_SAFE(&p->part, item, tmp){
        pddl_fm_t *fm = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);
        pddlFmDel(fm);
    }

    FREE(p);
}

static pddl_fm_junc_t *fmPartClone(const pddl_fm_junc_t *p)
{
    pddl_fm_junc_t *n;
    pddl_fm_t *c, *nc;
    pddl_list_t *item;

    n = fmPartNew(p->fm.type);
    PDDL_LIST_FOR_EACH(&p->part, item){
        c = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);
        nc = pddlFmClone(c);
        pddlListAppend(&n->part, &nc->conn);
    }
    return n;
}

static pddl_fm_junc_t *fmPartNegate(const pddl_fm_junc_t *p,
                                    const pddl_t *pddl)
{
    pddl_fm_junc_t *n;
    pddl_fm_t *c, *nc;
    pddl_list_t *item;

    if (p->fm.type == PDDL_FM_AND){
        n = fmPartNew(PDDL_FM_OR);
    }else{
        n = fmPartNew(PDDL_FM_AND);
    }
    PDDL_LIST_FOR_EACH(&p->part, item){
        c = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);
        nc = pddlFmNegate(c, pddl);
        pddlListAppend(&n->part, &nc->conn);
    }
    return n;
}

static int fmPartEq(const pddl_fm_junc_t *p1,
                    const pddl_fm_junc_t *p2)
{
    pddl_fm_t *c1, *c2;
    pddl_list_t *item1, *item2;
    PDDL_LIST_FOR_EACH(&p1->part, item1){
        c1 = PDDL_LIST_ENTRY(item1, pddl_fm_t, conn);
        int found = 0;
        PDDL_LIST_FOR_EACH(&p2->part, item2){
            c2 = PDDL_LIST_ENTRY(item2, pddl_fm_t, conn);
            if (fmEq(c1, c2)){
                found = 1;
                break;
            }
        }
        if (!found)
            return 0;
    }

    return 1;
}

static void fmPartAdd(pddl_fm_junc_t *p, pddl_fm_t *add)
{
    pddlListInit(&add->conn);
    pddlListAppend(&p->part, &add->conn);
}

static int fmPartTraverse(pddl_fm_junc_t *p,
                          int (*pre)(pddl_fm_t *, void *),
                          int (*post)(pddl_fm_t *, void *),
                          void *u)
{
    pddl_fm_t *c;
    pddl_list_t *item, *tmp;

    PDDL_LIST_FOR_EACH_SAFE(&p->part, item, tmp){
        c = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);
        if (fmTraverse(c, pre, post, u) != 0)
            return -1;
    }

    return 0;
}

static int fmPartRebuild(pddl_fm_junc_t **p,
                         int (*pre)(pddl_fm_t **, void *),
                         int (*post)(pddl_fm_t **, void *),
                         void *u)
{
    pddl_fm_t *c;
    pddl_list_t *item, *last;

    if (pddlListEmpty(&(*p)->part))
        return 0;

    last = pddlListPrev(&(*p)->part);
    do {
        item = pddlListNext(&(*p)->part);
        pddlListDel(item);
        c = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);
        if (fmRebuild(&c, pre, post, u) != 0)
            return -1;
        if (c != NULL)
            pddlListAppend(&(*p)->part, &c->conn);
    } while (item != last);

    return 0;
}

/** Moves all parts of src to dst */
static void fmPartStealPart(pddl_fm_junc_t *dst,
                            pddl_fm_junc_t *src)
{
    pddl_list_t *item;

    while (!pddlListEmpty(&src->part)){
        item = pddlListNext(&src->part);
        pddlListDel(item);
        pddlListAppend(&dst->part, item);
    }
}

static void fmPartPrintPDDL(const pddl_fm_junc_t *p,
                            const pddl_t *pddl,
                            const pddl_params_t *params,
                            FILE *fout)
{
    pddl_list_t *item;
    const pddl_fm_t *child;


    fprintf(fout, "(");
    if (p->fm.type == PDDL_FM_AND){
        fprintf(fout, "and");
    }else if (p->fm.type == PDDL_FM_OR){
        fprintf(fout, "or");
    }
    PDDL_LIST_FOR_EACH(&p->part, item){
        child = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);
        //fprintf(fout, "\n        ");
        fprintf(fout, " ");
        pddlFmPrintPDDL(child, pddl, params, fout);
    }
    fprintf(fout, ")");
}


/*** QUANT ***/
static pddl_fm_quant_t *fmQuantNew(int type)
{
    return fmNew(pddl_fm_quant_t, type);
}

static void fmQuantDel(pddl_fm_quant_t *q)
{
    pddlParamsFree(&q->param);
    if (q->qfm != NULL)
        pddlFmDel(q->qfm);
    FREE(q);
}

static pddl_fm_quant_t *fmQuantClone(const pddl_fm_quant_t *q)
{
    pddl_fm_quant_t *n;

    n = fmQuantNew(q->fm.type);
    pddlParamsInitCopy(&n->param, &q->param);
    n->qfm = pddlFmClone(q->qfm);
    return n;
}

static pddl_fm_quant_t *fmQuantNegate(const pddl_fm_quant_t *q,
                                      const pddl_t *pddl)
{
    pddl_fm_quant_t *n;

    if (q->fm.type == PDDL_FM_FORALL){
        n = fmQuantNew(PDDL_FM_EXIST);
    }else{
        n = fmQuantNew(PDDL_FM_FORALL);
    }
    pddlParamsInitCopy(&n->param, &q->param);
    n->qfm = pddlFmNegate(q->qfm, pddl);
    return n;
}

static int fmQuantEq(const pddl_fm_quant_t *q1,
                     const pddl_fm_quant_t *q2)
{
    if (q1->param.param_size != q2->param.param_size)
        return 0;
    for (int i = 0; i < q1->param.param_size; ++i){
        if (q1->param.param[i].type != q2->param.param[i].type
                || q1->param.param[i].is_agent != q2->param.param[i].is_agent
                || q1->param.param[i].inherit != q2->param.param[i].inherit)
            return 0;
    }
    return fmEq(q1->qfm, q2->qfm);
}

static int fmQuantTraverse(pddl_fm_quant_t *q,
                           int (*pre)(pddl_fm_t *, void *),
                           int (*post)(pddl_fm_t *, void *),
                           void *u)
{
    if (q->qfm)
        return fmTraverse(q->qfm, pre, post, u);
    return 0;
}

static int fmQuantRebuild(pddl_fm_quant_t **q,
                          int (*pre)(pddl_fm_t **, void *),
                          int (*post)(pddl_fm_t **, void *),
                          void *userdata)
{
    if ((*q)->qfm)
        return fmRebuild(&(*q)->qfm, pre, post, userdata);
    return 0;
}

static void fmQuantPrintPDDL(const pddl_fm_quant_t *q,
                             const pddl_t *pddl,
                             const pddl_params_t *params,
                             FILE *fout)
{
    fprintf(fout, "(");
    if (q->fm.type == PDDL_FM_FORALL){
        fprintf(fout, "forall");
    }else if (q->fm.type == PDDL_FM_EXIST){
        fprintf(fout, "exists");
    }

    fprintf(fout, " (");
    pddlParamsPrintPDDL(&q->param, &pddl->type, fout);
    fprintf(fout, ") ");

    pddlFmPrintPDDL(q->qfm, pddl, &q->param, fout);

    fprintf(fout, ")");
}




/*** WHEN ***/
static pddl_fm_when_t *fmWhenNew(void)
{
    return fmNew(pddl_fm_when_t, PDDL_FM_WHEN);
}

static void fmWhenDel(pddl_fm_when_t *w)
{
    if (w->pre)
        pddlFmDel(w->pre);
    if (w->eff)
        pddlFmDel(w->eff);
    FREE(w);
}

static pddl_fm_when_t *fmWhenClone(const pddl_fm_when_t *w)
{
    pddl_fm_when_t *n;

    n = fmWhenNew();
    if (w->pre)
        n->pre = pddlFmClone(w->pre);
    if (w->eff)
        n->eff = pddlFmClone(w->eff);
    return n;
}

static pddl_fm_when_t *fmWhenNegate(const pddl_fm_when_t *w,
                                    const pddl_t *pddl)
{
    PANIC("Cannot negate (when ...)");
    return NULL;
}

static int fmWhenEq(const pddl_fm_when_t *w1,
                    const pddl_fm_when_t *w2)
{
    return fmEq(w1->pre, w2->pre) && fmEq(w1->eff, w2->eff);
}

static int fmWhenTraverse(pddl_fm_when_t *w,
                          int (*pre)(pddl_fm_t *, void *),
                          int (*post)(pddl_fm_t *, void *),
                          void *u)
{
    if (w->pre != NULL && fmTraverse(w->pre, pre, post, u) != 0)
        return -1;
    if (w->eff != NULL && fmTraverse(w->eff, pre, post, u) != 0)
        return -1;
    return 0;
}

static int fmWhenRebuild(pddl_fm_when_t **w,
                         int (*pre)(pddl_fm_t **, void *),
                         int (*post)(pddl_fm_t **, void *),
                         void *u)
{
    if ((*w)->pre){
        if (fmRebuild(&(*w)->pre, pre, post, u) != 0)
            return -1;
    }
    if ((*w)->eff){
        if (fmRebuild(&(*w)->eff, pre, post, u) != 0)
            return -1;
    }
    return 0;
}

static void fmWhenPrintPDDL(const pddl_fm_when_t *w,
                            const pddl_t *pddl,
                            const pddl_params_t *params,
                            FILE *fout)
{
    fprintf(fout, "(when ");
    pddlFmPrintPDDL(w->pre, pddl, params, fout);
    fprintf(fout, " ");
    pddlFmPrintPDDL(w->eff, pddl, params, fout);
    fprintf(fout, ")");
}



/*** ATOM ***/
static pddl_fm_atom_t *fmAtomNew(void)
{
    return fmNew(pddl_fm_atom_t, PDDL_FM_ATOM);
}

static void fmAtomDel(pddl_fm_atom_t *a)
{
    if (a->arg != NULL)
        FREE(a->arg);
    FREE(a);
}

static pddl_fm_atom_t *fmAtomClone(const pddl_fm_atom_t *a)
{
    pddl_fm_atom_t *n;

    n = fmAtomNew();
    n->pred = a->pred;
    n->arg_size = a->arg_size;
    n->arg = ALLOC_ARR(pddl_fm_atom_arg_t, n->arg_size);
    memcpy(n->arg, a->arg, sizeof(pddl_fm_atom_arg_t) * n->arg_size);
    n->neg = a->neg;

    return n;
}

static pddl_fm_atom_t *fmAtomNegate(const pddl_fm_atom_t *a,
                                    const pddl_t *pddl)
{
    pddl_fm_atom_t *n;

    n = fmAtomClone(a);
    if (pddl->pred.pred[a->pred].neg_of >= 0){
        n->pred = pddl->pred.pred[a->pred].neg_of;
    }else{
        n->neg = !a->neg;
    }
    return n;
}

static int fmAtomEq(const pddl_fm_atom_t *a1,
                    const pddl_fm_atom_t *a2)
{
    if (a1->pred != a2->pred
            || a1->neg != a2->neg
            || a1->arg_size != a2->arg_size)
        return 0;
    for (int i = 0; i < a1->arg_size; ++i){
        if (a1->arg[i].param != a2->arg[i].param
                || a1->arg[i].obj != a2->arg[i].obj){
            return 0;
        }
    }
    return 1;
}

static int fmAtomEqNoNeg(const pddl_fm_atom_t *a1,
                         const pddl_fm_atom_t *a2)
{
    if (a1->pred != a2->pred
            || a1->arg_size != a2->arg_size)
        return 0;
    for (int i = 0; i < a1->arg_size; ++i){
        if (a1->arg[i].param != a2->arg[i].param
                || a1->arg[i].obj != a2->arg[i].obj){
            return 0;
        }
    }
    return 1;
}

static int fmAtomTraverse(pddl_fm_atom_t *a,
                          int (*pre)(pddl_fm_t *, void *),
                          int (*post)(pddl_fm_t *, void *),
                          void *u)
{
    return 0;
}

static int fmAtomRebuild(pddl_fm_atom_t **a,
                         int (*pre)(pddl_fm_t **, void *),
                         int (*post)(pddl_fm_t **, void *),
                         void *u)
{
    return 0;
}

static void atomPrintPDDL(const pddl_fm_atom_t *a,
                          const pddl_t *pddl,
                          const pddl_params_t *params,
                          int is_func,
                          FILE *fout)
{
    if (a->neg)
        fprintf(fout, "(not ");
    if (is_func){
        fprintf(fout, "(%s", pddl->func.pred[a->pred].name);
    }else{
        fprintf(fout, "(%s", pddl->pred.pred[a->pred].name);
    }
    for (int i = 0; i < a->arg_size; ++i){
        pddl_fm_atom_arg_t *arg = a->arg + i;
        if (arg->param >= 0){
            if (params->param[arg->param].name != NULL){
                fprintf(fout, " %s", params->param[arg->param].name);
            }else{
                if (params->param[arg->param].is_counted_var){
                    fprintf(fout, " c%d", arg->param);
                }else{
                    fprintf(fout, " x%d", arg->param);
                }
            }
        }else{
            fprintf(fout, " %s", pddl->obj.obj[arg->obj].name);
        }
    }
    fprintf(fout, ")");
    if (a->neg)
        fprintf(fout, ")");
}

static void fmAtomPrintPDDL(const pddl_fm_atom_t *a,
                            const pddl_t *pddl,
                            const pddl_params_t *params,
                            FILE *fout)
{
    atomPrintPDDL(a, pddl, params, 0, fout);
}



/*** FUNC_OP ***/
static pddl_fm_func_op_t *fmFuncOpNew(int type)
{
    return fmNew(pddl_fm_func_op_t, type);
}

static void fmFuncOpDel(pddl_fm_func_op_t *op)
{
    if (op->lvalue)
        fmAtomDel(op->lvalue);
    if (op->fvalue)
        fmAtomDel(op->fvalue);
    FREE(op);
}

static pddl_fm_func_op_t *fmFuncOpClone(const pddl_fm_func_op_t *op)
{
    pddl_fm_func_op_t *n;
    n = fmFuncOpNew(op->fm.type);
    n->value = op->value;
    if (op->lvalue)
        n->lvalue = fmAtomClone(op->lvalue);
    if (op->fvalue)
        n->fvalue = fmAtomClone(op->fvalue);
    return n;
}

static pddl_fm_func_op_t *fmFuncOpNegate(const pddl_fm_func_op_t *op,
                                         const pddl_t *pddl)
{
    PANIC("Cannot negate function!");
    return NULL;
}

static int fmFuncOpEq(const pddl_fm_func_op_t *f1,
                      const pddl_fm_func_op_t *f2)
{
    if (f1->fvalue == NULL && f2->fvalue != NULL)
        return 0;
    if (f1->fvalue != NULL && f2->fvalue == NULL)
        return 0;
    if (f1->fvalue != NULL){
        return fmAtomEq(f1->lvalue, f2->lvalue)
            && fmAtomEq(f1->fvalue, f2->fvalue);
    }else{
        return f1->value == f2->value && fmAtomEq(f1->lvalue, f2->lvalue);
    }
}

static int fmFuncOpTraverse(pddl_fm_func_op_t *op,
                            int (*pre)(pddl_fm_t *, void *),
                            int (*post)(pddl_fm_t *, void *),
                            void *u)
{
    return 0;
}

static int fmFuncOpRebuild(pddl_fm_func_op_t **op,
                           int (*pre)(pddl_fm_t **, void *),
                           int (*post)(pddl_fm_t **, void *),
                           void *userdata)
{
    return 0;
}

static void fmFuncOpPrintPDDL(const pddl_fm_func_op_t *op,
                              const pddl_t *pddl,
                              const pddl_params_t *params,
                              FILE *fout)
{
    if (op->fm.type == PDDL_FM_ASSIGN){
        fprintf(fout, "(= ");
    }else if (op->fm.type == PDDL_FM_INCREASE){
        fprintf(fout, "(increase ");
    }

    if (op->lvalue == NULL){
        fprintf(fout, "(total-cost)");
    }else{
        atomPrintPDDL(op->lvalue, pddl, params, 1, fout);
    }
    fprintf(fout, " ");
    if (op->fvalue == NULL){
        fprintf(fout, "%d", op->value);
    }else{
        atomPrintPDDL(op->fvalue, pddl, params, 1, fout);
    }
    fprintf(fout, ")");
}


/*** BOOL ***/
static pddl_fm_bool_t *fmBoolNew(int val)
{
    pddl_fm_bool_t *b;
    b = fmNew(pddl_fm_bool_t, PDDL_FM_BOOL);
    b->val = val;
    return b;
}

static void fmBoolDel(pddl_fm_bool_t *a)
{
    FREE(a);
}

static pddl_fm_bool_t *fmBoolClone(const pddl_fm_bool_t *a)
{
    return fmBoolNew(a->val);
}

static pddl_fm_bool_t *fmBoolNegate(const pddl_fm_bool_t *a,
                                    const pddl_t *pddl)
{
    pddl_fm_bool_t *b = fmBoolClone(a);
    b->val = !a->val;
    return b;
}

static int fmBoolEq(const pddl_fm_bool_t *b1,
                    const pddl_fm_bool_t *b2)
{
    return b1->val == b2->val;
}


static int fmBoolTraverse(pddl_fm_bool_t *a,
                          int (*pre)(pddl_fm_t *, void *),
                          int (*post)(pddl_fm_t *, void *),
                          void *u)
{
    return 0;
}

static int fmBoolRebuild(pddl_fm_bool_t **a,
                         int (*pre)(pddl_fm_t **, void *),
                         int (*post)(pddl_fm_t **, void *),
                         void *userdata)
{
    return 0;
}

static void fmBoolPrintPDDL(const pddl_fm_bool_t *b,
                            const pddl_t *pddl,
                            const pddl_params_t *params,
                            FILE *fout)
{
    if (b->val){
        fprintf(fout, "(TRUE)");
    }else{
        fprintf(fout, "(FALSE)");
    }
}


/*** IMPLY ***/
static pddl_fm_imply_t *fmImplyNew(void)
{
    return fmNew(pddl_fm_imply_t, PDDL_FM_IMPLY);
}

static void fmImplyDel(pddl_fm_imply_t *a)
{
    if (a->left != NULL)
        pddlFmDel(a->left);
    if (a->right != NULL)
        pddlFmDel(a->right);
    FREE(a);
}

static pddl_fm_imply_t *fmImplyClone(const pddl_fm_imply_t *a)
{
    pddl_fm_imply_t *n = fmImplyNew();
    if (a->left != NULL)
        n->left = pddlFmClone(a->left);
    if (a->right != NULL)
        n->right = pddlFmClone(a->right);
    return n;
}

static pddl_fm_t *fmImplyNegate(const pddl_fm_imply_t *a,
                                const pddl_t *pddl)
{
    pddl_fm_junc_t *or;
    pddl_fm_t *left, *right;

    or = fmPartNew(PDDL_FM_AND);
    left = pddlFmClone(a->left);
    right = pddlFmNegate(a->right, pddl);
    pddlFmJuncAdd(or, left);
    pddlFmJuncAdd(or, right);
    return &or->fm;
}

static int fmImplyEq(const pddl_fm_imply_t *i1,
                     const pddl_fm_imply_t *i2)
{
    return fmEq(i1->left, i2->left) && fmEq(i1->right, i2->right);
}

static int fmImplyTraverse(pddl_fm_imply_t *imp,
                           int (*pre)(pddl_fm_t *, void *),
                           int (*post)(pddl_fm_t *, void *),
                           void *u)
{
    if (imp->left != NULL){
        if (fmTraverse(imp->left, pre, post, u) != 0)
            return -1;
    }
    if (imp->right != NULL){
        if (fmTraverse(imp->right, pre, post, u) != 0)
            return -1;
    }
    return 0;
}

static int fmImplyRebuild(pddl_fm_imply_t **imp,
                          int (*pre)(pddl_fm_t **, void *),
                          int (*post)(pddl_fm_t **, void *),
                          void *u)
{
    if ((*imp)->left != NULL){
        if (fmRebuild(&(*imp)->left, pre, post, u) != 0)
            return -1;
    }
    if ((*imp)->right != NULL){
        if (fmRebuild(&(*imp)->right, pre, post, u) != 0)
            return -1;
    }
    return 0;
}

static void fmImplyPrintPDDL(const pddl_fm_imply_t *imp,
                             const pddl_t *pddl,
                             const pddl_params_t *params,
                             FILE *fout)
{
    fprintf(fout, "(imply ");
    if (imp->left == NULL){
        fprintf(fout, "()");
    }else{
        pddlFmPrintPDDL(imp->left, pddl, params, fout);
    }
    fprintf(fout, " ");
    if (imp->right == NULL){
        fprintf(fout, "()");
    }else{
        pddlFmPrintPDDL(imp->right, pddl, params, fout);
    }
    fprintf(fout, ")");
}



pddl_fm_junc_t *pddlFmToJunc(pddl_fm_t *c)
{
    ASSERT(c->type == PDDL_FM_AND || c->type == PDDL_FM_OR);
    return PDDL_FM_CAST(c, junc);
}

const pddl_fm_junc_t *pddlFmToJuncConst(const pddl_fm_t *c)
{
    ASSERT(c->type == PDDL_FM_AND || c->type == PDDL_FM_OR);
    return PDDL_FM_CAST(c, junc);
}

pddl_fm_and_t *pddlFmToAnd(pddl_fm_t *c)
{
    ASSERT(c->type == PDDL_FM_AND);
    return PDDL_FM_CAST(c, and);
}

pddl_fm_or_t *pddlFmToOr(pddl_fm_t *c)
{
    ASSERT(c->type == PDDL_FM_OR);
    return PDDL_FM_CAST(c, or);
}

pddl_fm_bool_t *pddlFmToBool(pddl_fm_t *c)
{
    ASSERT(c->type == PDDL_FM_BOOL);
    return PDDL_FM_CAST(c, bool);
}

pddl_fm_atom_t *pddlFmToAtom(pddl_fm_t *c)
{
    ASSERT(c->type == PDDL_FM_ATOM);
    return PDDL_FM_CAST(c, atom);
}

const pddl_fm_atom_t *pddlFmToAtomConst(const pddl_fm_t *c)
{
    ASSERT(c->type == PDDL_FM_ATOM);
    return PDDL_FM_CAST(c, atom);
}

const pddl_fm_increase_t *pddlFmToIncreaseConst(const pddl_fm_t *c)
{
    ASSERT(c->type == PDDL_FM_INCREASE);
    return PDDL_FM_CAST(c, increase);
}

const pddl_fm_when_t *pddlFmToWhenConst(const pddl_fm_t *c)
{
    ASSERT(c->type == PDDL_FM_WHEN);
    return PDDL_FM_CAST(c, when);
}

void pddlFmDel(pddl_fm_t *fm)
{
    cond_cls[fm->type].del(fm);
}

pddl_fm_t *pddlFmClone(const pddl_fm_t *fm)
{
    return cond_cls[fm->type].clone(fm);
}

int pddlFmIsFalse(const pddl_fm_t *c)
{
    if (c->type == PDDL_FM_BOOL){
        const pddl_fm_bool_t *b = PDDL_FM_CAST(c, bool);
        return !b->val;
    }
    return 0;
}

int pddlFmIsTrue(const pddl_fm_t *c)
{
    if (c->type == PDDL_FM_BOOL){
        const pddl_fm_bool_t *b = PDDL_FM_CAST(c, bool);
        return b->val;
    }
    return 0;
}

int pddlFmIsAtom(const pddl_fm_t *c)
{
    return c->type == PDDL_FM_ATOM;
}

int pddlFmIsWhen(const pddl_fm_t *c)
{
    return c->type == PDDL_FM_WHEN;
}

int pddlFmIsIncrease(const pddl_fm_t *c)
{
    return c->type == PDDL_FM_INCREASE;
}

pddl_fm_t *pddlFmNegate(const pddl_fm_t *fm, const pddl_t *pddl)
{
    return cond_cls[fm->type].negate(fm, pddl);
}

static int fmEq(const pddl_fm_t *c1, const pddl_fm_t *c2)
{
    if (c1 == c2)
        return 1;
    if ((c1 == NULL && c2 != NULL)
            || (c1 != NULL && c2 == NULL))
        return 0;
    if (c1->type != c2->type)
        return 0;
    return cond_cls[c1->type].eq(c1, c2);
}

int pddlFmEq(const pddl_fm_t *c1, const pddl_fm_t *c2)
{
    return fmEq(c1, c2);
}

int pddlFmIsImplied(const pddl_fm_t *s,
                    const pddl_fm_t *c,
                    const pddl_t *pddl,
                    const pddl_params_t *param)
{
    ASSERT_RUNTIME(s->type == PDDL_FM_BOOL
                   || s->type == PDDL_FM_ATOM
                   || s->type == PDDL_FM_AND
                   || s->type == PDDL_FM_OR);
    ASSERT_RUNTIME(c->type == PDDL_FM_BOOL
                   || c->type == PDDL_FM_ATOM
                   || c->type == PDDL_FM_AND
                   || c->type == PDDL_FM_OR);
    if (pddlFmEq(s, c))
        return 1;

    if (s->type == PDDL_FM_BOOL && c->type == PDDL_FM_BOOL){
        if (pddlFmEq(s, c))
            return 1;

    }else if (s->type == PDDL_FM_BOOL){
        if (pddlFmIsTrue(s))
            return 1;
        return 0;

    }else if (c->type == PDDL_FM_BOOL){
        if (pddlFmIsFalse(c))
            return 1;
        return 0;

    }else if (s->type == PDDL_FM_ATOM && c->type == PDDL_FM_ATOM){
        return pddlFmEq(s, c);

        if (pddl == NULL || param == NULL)
            return 0;
        pddl_fm_atom_t *sa = PDDL_FM_CAST(s, atom);
        pddl_fm_atom_t *ca = PDDL_FM_CAST(c, atom);
        if (sa->pred != ca->pred)
            return 0;
        for (int argi = 0; argi < sa->arg_size; ++argi){
            if (sa->arg[argi].param >= 0 && ca->arg[argi].param >= 0){
                int stype = param->param[sa->arg[argi].param].type;
                int ctype = param->param[ca->arg[argi].param].type;
                if (!pddlTypesIsSubset(&pddl->type, stype, ctype))
                    return 0;

            }else if (sa->arg[argi].param >= 0){
                int type = param->param[sa->arg[argi].param].type;
                pddl_obj_id_t cobj = ca->arg[argi].obj;
                if (pddlTypeNumObjs(&pddl->type, type) != 1
                        || pddlTypeGetObj(&pddl->type, type, 0) != cobj){
                    return 0;
                }

            }else if (ca->arg[argi].param >= 0){
                int type = param->param[ca->arg[argi].param].type;
                if (!pddlTypesObjHasType(&pddl->type, type, sa->arg[argi].obj))
                    return 0;

            }else{
                if (sa->arg[argi].obj != ca->arg[argi].obj)
                    return 0;
            }
        }
        return 1;

    }else if (s->type == PDDL_FM_OR){
        pddl_fm_junc_t *p = OBJ(s, junc);
        pddl_list_t *item;
        PDDL_LIST_FOR_EACH(&p->part, item){
            pddl_fm_t *e = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);
            if (pddlFmIsImplied(e, c, pddl, param))
                return 1;
        }
        return 0;

    }else if (s->type == PDDL_FM_AND){
        pddl_fm_junc_t *p = OBJ(s, junc);
        pddl_list_t *item;
        PDDL_LIST_FOR_EACH(&p->part, item){
            pddl_fm_t *e = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);
            if (!pddlFmIsImplied(e, c, pddl, param))
                return 0;
        }
        return 1;

    }else if (s->type == PDDL_FM_ATOM && c->type == PDDL_FM_AND){
        pddl_fm_junc_t *p = OBJ(c, junc);
        pddl_list_t *item;
        PDDL_LIST_FOR_EACH(&p->part, item){
            pddl_fm_t *e = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);
            if (pddlFmIsImplied(s, e, pddl, param))
                return 1;
        }
        return 0;

    }else if (s->type == PDDL_FM_ATOM && c->type == PDDL_FM_OR){
        pddl_fm_junc_t *p = OBJ(c, junc);
        pddl_list_t *item;
        PDDL_LIST_FOR_EACH(&p->part, item){
            pddl_fm_t *e = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);
            if (!pddlFmIsImplied(s, e, pddl, param))
                return 0;
        }
        return 1;
    }

    return 0;
}

static int fmTraverse(pddl_fm_t *c,
                      int (*pre)(pddl_fm_t *, void *),
                      int (*post)(pddl_fm_t *, void *),
                      void *u)
{
    int ret;

    if (pre != NULL){
        ret = pre(c, u);
        if (ret == -1)
            return 0;
        if (ret == -2)
            return -1;
    }

    ret = cond_cls[c->type].traverse(c, pre, post, u);
    if (ret < 0)
        return ret;

    if (post != NULL)
        if (post(c, u) != 0)
            return -1;
    return 0;
}

void pddlFmTraverse(pddl_fm_t *c,
                    int (*pre)(pddl_fm_t *, void *),
                    int (*post)(pddl_fm_t *, void *),
                    void *u)
{
    fmTraverse(c, pre, post, u);
}

static int fmRebuild(pddl_fm_t **c,
                     int (*pre)(pddl_fm_t **, void *),
                     int (*post)(pddl_fm_t **, void *),
                     void *u)
{
    int ret;

    if (pre != NULL){
        ret = pre(c, u);
        if (ret == -1)
            return 0;
        if (ret == -2)
            return -1;
    }

    ret = cond_cls[(*c)->type].rebuild(c, pre, post, u);
    if (ret < 0)
        return ret;

    if (post != NULL)
        if (post(c, u) != 0)
            return -1;
    return 0;
}

void pddlFmRebuild(pddl_fm_t **c,
                   int (*pre)(pddl_fm_t **, void *),
                   int (*post)(pddl_fm_t **, void *),
                   void *u)
{
    fmRebuild(c, pre, post, u);
}

struct test_static {
    const pddl_t *pddl;
    int ret;
};
static int atomIsStatic(pddl_fm_t *c, void *_ts)
{
    struct test_static *ts = _ts;
    if (c->type == PDDL_FM_ATOM){
        const pddl_fm_atom_t *a = OBJ(c, atom);
        if (!pddlPredIsStatic(ts->pddl->pred.pred + a->pred)){
            ts->ret = 0;
            return -2;
        }
        return 0;
    }
    return 0;
}

static int pddlFmIsStatic(pddl_fm_t *c, const pddl_t *pddl)
{
    struct test_static ts;
    ts.pddl = pddl;
    ts.ret = 1;

    pddlFmTraverse(c, atomIsStatic, NULL, &ts);
    return ts.ret;
}

pddl_fm_when_t *pddlFmRemoveFirstNonStaticWhen(pddl_fm_t *c,
                                               const pddl_t *pddl)
{
    pddl_fm_junc_t *cp;
    pddl_fm_t *cw;
    pddl_list_t *item, *tmp;

    if (c->type != PDDL_FM_AND)
        return NULL;
    cp = PDDL_FM_CAST(c, junc);

    PDDL_LIST_FOR_EACH_SAFE(&cp->part, item, tmp){
        cw = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);
        if (cw->type == PDDL_FM_WHEN){
            pddl_fm_when_t *w = PDDL_FM_CAST(cw, when);
            if (!pddlFmIsStatic(w->pre, pddl)){
                pddlListDel(item);
                return w;
            }
        }
    }

    return NULL;
}

pddl_fm_when_t *pddlFmRemoveFirstWhen(pddl_fm_t *c, const pddl_t *pddl)
{
    pddl_fm_junc_t *cp;
    pddl_fm_t *cw;
    pddl_list_t *item, *tmp;

    if (c->type != PDDL_FM_AND)
        return NULL;
    cp = PDDL_FM_CAST(c, junc);

    PDDL_LIST_FOR_EACH_SAFE(&cp->part, item, tmp){
        cw = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);
        if (cw->type == PDDL_FM_WHEN){
            pddl_fm_when_t *w = PDDL_FM_CAST(cw, when);
            pddlListDel(item);
            return w;
        }
    }

    return NULL;
}

pddl_fm_t *pddlFmNewAnd2(pddl_fm_t *a, pddl_fm_t *b)
{
    pddl_fm_junc_t *p = fmPartNew(PDDL_FM_AND);
    fmPartAdd(p, a);
    fmPartAdd(p, b);
    return &p->fm;
}

pddl_fm_t *pddlFmNewEmptyAnd(void)
{
    pddl_fm_junc_t *p = fmPartNew(PDDL_FM_AND);
    return &p->fm;
}

pddl_fm_t *pddlFmNewEmptyOr(void)
{
    pddl_fm_junc_t *p = fmPartNew(PDDL_FM_OR);
    return &p->fm;
}

pddl_fm_atom_t *pddlFmNewEmptyAtom(int num_args)
{
    pddl_fm_atom_t *atom = fmAtomNew();

    if (num_args > 0){
        atom->arg_size = num_args;
        atom->arg = ALLOC_ARR(pddl_fm_atom_arg_t, atom->arg_size);
        for (int i = 0; i < atom->arg_size; ++i){
            atom->arg[i].param = -1;
            atom->arg[i].obj = PDDL_OBJ_ID_UNDEF;
        }
    }

    return atom;
}

pddl_fm_bool_t *pddlFmNewBool(int is_true)
{
    return fmBoolNew(is_true);
}

static int hasAtom(pddl_fm_t *c, void *_ret)
{
    int *ret = _ret;

    if (c->type == PDDL_FM_ATOM){
        *ret = 1;
        return -2;
    }
    return 0;
}

int pddlFmHasAtom(const pddl_fm_t *c)
{
    int ret = 0;
    pddlFmTraverse((pddl_fm_t *)c, hasAtom, NULL, &ret);
    return ret;
}

/*** PARSE ***/
static int parseAtomArg(pddl_fm_atom_arg_t *arg,
                        const pddl_lisp_node_t *root,
                        const parse_ctx_t *ctx)
{
    if (root->value[0] == '?'){
        if (ctx->params == NULL){
            ERR_LISP_RET(ctx->err, -1, root, "%sUnexpected variable `%s'",
                         ctx->err_prefix, root->value);
        }

        int param = pddlParamsGetId(ctx->params, root->value);
        if (param < 0){
            ERR_LISP_RET(ctx->err, -1, root, "%sUnkown variable `%s'",
                         ctx->err_prefix, root->value);
        }
        arg->param = param;
        arg->obj = PDDL_OBJ_ID_UNDEF;

    }else{
        pddl_obj_id_t obj = pddlObjsGet(ctx->objs, root->value);
        if (obj < 0){
            ERR_LISP_RET(ctx->err, -1, root, "%sUnkown constant/object `%s'",
                         ctx->err_prefix, root->value);
        }
        arg->param = -1;
        arg->obj = obj;
    }

    return 0;
}

static pddl_fm_t *parseAtom(const pddl_lisp_node_t *root,
                            const parse_ctx_t *ctx,
                            int negated)
{
    pddl_fm_atom_t *atom;
    const char *name;
    int pred;

    // Get predicate name
    name = pddlLispNodeHead(root);
    if (name == NULL){
        ERR_LISP_RET(ctx->err, NULL, root,
                     "%sMissing head of the expression", ctx->err_prefix);
    }

    // And resolve it against known predicates
    pred = pddlPredsGet(ctx->preds, name);
    if (pred == -1){
        ERR_LISP_RET(ctx->err, NULL, root,
                     "%sUnkown predicate `%s'", ctx->err_prefix, name);
    }

    // Check correct number of predicates
    if (root->child_size - 1 != ctx->preds->pred[pred].param_size){
        ERR_LISP_RET(ctx->err, NULL, root,
                     "%sInvalid number of arguments of the predicate `%s'",
                     ctx->err_prefix, name);
    }

    // Check that all children are terminals
    for (int i = 1; i < root->child_size; ++i){
        if (root->child[i].value == NULL){
            ERR_LISP_RET(ctx->err, NULL, root->child + i,
                         "%sInvalid %d'th argument of the predicate `%s'",
                         ctx->err_prefix, i, name);
        }
    }

    atom = fmAtomNew();
    atom->pred = pred;
    atom->arg_size = root->child_size - 1;
    atom->arg = ALLOC_ARR(pddl_fm_atom_arg_t, atom->arg_size);
    for (int i = 0; i < atom->arg_size; ++i){
        if (parseAtomArg(atom->arg + i, root->child + i + 1, ctx) != 0){
            fmAtomDel(atom);
            PDDL_TRACE_RET(ctx->err, NULL);
        }
    }
    atom->neg = negated;

    return &atom->fm;
}

static pddl_fm_t *parseAssign(const pddl_lisp_node_t *root,
                              const parse_ctx_t *ctx,
                              int negated)
{
    const char *head;
    const pddl_lisp_node_t *nfunc, *nval;
    pddl_fm_t *lvalue;
    pddl_fm_func_op_t *assign;
    parse_ctx_t sub_ctx;

    head = pddlLispNodeHead(root);
    if (head == NULL
            || strcmp(head, "=") != 0
            || root->child_size != 3){
        ERR_LISP_RET2(ctx->err, NULL, root, "Invalid (= ...) expression.");
    }

    nfunc = root->child + 1;
    nval = root->child + 2;

    if (nfunc->child_size < 1 || nfunc->child[0].value == NULL)
        ERR_LISP_RET2(ctx->err, NULL, root, "Invalid function in (= ...).");
    if (nval->value == NULL){
        ERR_LISP_RET2(ctx->err, NULL, root, "Only (= ... N) expressions where"
                      " N is a number are supported.");
    }

    sub_ctx = *ctx;
    sub_ctx.preds = sub_ctx.funcs;
    lvalue = parseAtom(nfunc, &sub_ctx, negated);
    if (lvalue == NULL)
        PDDL_TRACE_RET(ctx->err, NULL);

    assign = fmFuncOpNew(PDDL_FM_ASSIGN);
    assign->value = atoi(nval->value);
    assign->lvalue = OBJ(lvalue, atom);
    return &assign->fm;
}

static pddl_fm_t *parseIncrease(const pddl_lisp_node_t *root,
                                const parse_ctx_t *ctx,
                                int negated)
{
    pddl_fm_func_op_t *inc;
    pddl_fm_t *fvalue;
    parse_ctx_t sub_ctx;

    if (root->child_size != 3
            || root->child[1].value != NULL
            || root->child[1].child_size != 1
            || root->child[1].child[0].value == NULL
            || strcmp(root->child[1].child[0].value, "total-cost") != 0){
        ERR_LISP_RET(ctx->err, NULL, root,
                     "%sOnly (increase (total-cost) int-value) is supported;",
                     ctx->err_prefix);
    }

    if (root->child[2].value != NULL){
        inc = fmFuncOpNew(PDDL_FM_INCREASE);
        inc->value = atoi(root->child[2].value);
        if (inc->value < 0){
            ERR_LISP_RET(ctx->err, NULL, root,
                         "%sOnly non-negative actions costs are supported;",
                         ctx->err_prefix);
        }

    }else{
        sub_ctx = *ctx;
        sub_ctx.preds = sub_ctx.funcs;
        fvalue = parseAtom(root->child + 2, &sub_ctx, negated);
        if (fvalue == NULL)
            PDDL_TRACE_RET(ctx->err, NULL);
        inc = fmFuncOpNew(PDDL_FM_INCREASE);
        inc->fvalue = (pddl_fm_atom_t *)fvalue;
    }

    return &inc->fm;
}

static pddl_fm_t *parsePart(int part_type,
                            const pddl_lisp_node_t *root,
                            const parse_ctx_t *ctx,
                            int negated)
{
    pddl_fm_junc_t *part;
    pddl_fm_t *fm;
    int i;

    part = fmPartNew(part_type);
    for (i = 1; i < root->child_size; ++i){
        fm = parse(root->child + i, ctx, negated);
        if (fm == NULL){
            fmPartDel(part);
            PDDL_TRACE_RET(ctx->err, NULL);
        }
        pddlListAppend(&part->part, &fm->conn);
    }

    return &part->fm;
}

static pddl_fm_t *parseImply(const pddl_lisp_node_t *left,
                             const pddl_lisp_node_t *right,
                             const parse_ctx_t *ctx,
                             int negated)
{
    pddl_fm_junc_t *part;
    pddl_fm_imply_t *imp;
    pddl_fm_t *cleft = NULL, *cright = NULL;

    if (negated){
        if ((cleft = parse(left, ctx, 0)) == NULL)
            PDDL_TRACE_RET(ctx->err, NULL);

        if ((cright = parse(right, ctx, 1)) == NULL){
            pddlFmDel(cleft);
            PDDL_TRACE_RET(ctx->err, NULL);
        }

        part = fmPartNew(PDDL_FM_AND);
        pddlListAppend(&part->part, &cleft->conn);
        pddlListAppend(&part->part, &cright->conn);
        return &part->fm;

    }else{
        if ((cleft = parse(left, ctx, 0)) == NULL)
            PDDL_TRACE_RET(ctx->err, NULL);

        if ((cright = parse(right, ctx, 0)) == NULL){
            pddlFmDel(cleft);
            PDDL_TRACE_RET(ctx->err, NULL);
        }

        imp = fmImplyNew();
        imp->left = cleft;
        imp->right = cright;
        return &imp->fm;
    }
}

static int parseQuantParams(pddl_params_t *params,
                            const pddl_lisp_node_t *root,
                            const parse_ctx_t *ctx)
{
    pddl_param_t *param;

    pddlParamsInit(params);

    // Parse all parameters of the quantifier
    if (pddlParamsParse(params, root, ctx->types, ctx->err) != 0){
        pddlParamsFree(params);
        PDDL_TRACE_RET(ctx->err, -1);
    }

    // And also add all global parameters that are not shadowed
    for (int i = 0; ctx->params != NULL && i < ctx->params->param_size; ++i){
        int use = 1;
        for (int j = 0; j < params->param_size; ++j){
            if (strcmp(params->param[j].name, ctx->params->param[i].name) == 0){
                use = 0;
                break;
            }
        }

        if (use){
            param = pddlParamsAdd(params);
            pddlParamInitCopy(param, ctx->params->param + i);
            param->inherit = i;
        }
    }

    return 0;
}

static pddl_fm_t *parseQuant(int quant_type,
                             const pddl_lisp_node_t *root,
                             const parse_ctx_t *ctx,
                             int negated)
{
    pddl_fm_quant_t *q;
    pddl_params_t params;
    parse_ctx_t sub_ctx;

    if (root->child_size != 3
            || root->child[1].value != NULL
            || root->child[2].value != NULL){
        if (quant_type == PDDL_FM_FORALL){
            ERR_LISP(ctx->err, root,
                     "%sInvalid (forall ...) condition", ctx->err_prefix);
        }else{
            ERR_LISP(ctx->err, root,
                     "%sInvalid (exists ...) condition", ctx->err_prefix);
        }
        return NULL;
    }

    if (parseQuantParams(&params, root->child + 1, ctx) != 0)
        PDDL_TRACE_RET(ctx->err, NULL);

    if (params.param_size == 0){
        pddlParamsFree(&params);
        ERR_LISP_RET(ctx->err, NULL, root,
                     "%sMissing variables in the quantifier",
                     ctx->err_prefix);
    }

    sub_ctx = *ctx;
    sub_ctx.params = &params;
    pddl_fm_t *fm = parse(root->child + 2, &sub_ctx, negated);
    if (fm == NULL){
        pddlParamsFree(&params);
        PDDL_TRACE_RET(ctx->err, NULL);
    }

    q = fmQuantNew(quant_type);
    q->param = params;
    q->qfm = fm;

    return &q->fm;
}

static pddl_fm_t *parseWhen(const pddl_lisp_node_t *root,
                            const parse_ctx_t *ctx)
{
    pddl_fm_when_t *w;
    pddl_fm_t *pre, *eff;

    if (root->child_size != 3
            || root->child[1].value != NULL
            || root->child[2].value != NULL){
        ERR_LISP_RET(ctx->err, NULL, root,
                     "%sInvalid (when ...)", ctx->err_prefix);
    }

    if ((pre = parse(root->child + 1, ctx, 0)) == NULL)
        PDDL_TRACE_RET(ctx->err, NULL);

    if ((eff = parse(root->child + 2, ctx, 0)) == NULL){
        pddlFmDel(pre);
        PDDL_TRACE_RET(ctx->err, NULL);
    }

    w = fmWhenNew();
    w->pre = pre;
    w->eff = eff;
    return &w->fm;
}

static pddl_fm_t *parse(const pddl_lisp_node_t *root,
                        const parse_ctx_t *ctx,
                        int negated)
{
    int kw;

    kw = pddlLispNodeHeadKw(root);

    if (kw == PDDL_KW_NOT){
        if (root->child_size != 2)
            ERR_LISP_RET(ctx->err, NULL, root,
                         "%sInvalid (not ...)", ctx->err_prefix);

        return parse(root->child + 1, ctx, !negated);

    }else if (kw == PDDL_KW_AND){
        if (root->child_size <= 1)
            ERR_LISP_RET(ctx->err, NULL, root,
                         "%sEmpty (and) expression", ctx->err_prefix);

        if (negated){
            return parsePart(PDDL_FM_OR, root, ctx, negated);
        }else{
            return parsePart(PDDL_FM_AND, root, ctx, negated);
        }

    }else if (kw == PDDL_KW_OR){
        if (root->child_size <= 1)
            ERR_LISP_RET(ctx->err, NULL, root,
                         "%sEmpty (or) expression", ctx->err_prefix);

        if (negated){
            return parsePart(PDDL_FM_AND, root, ctx, negated);
        }else{
            return parsePart(PDDL_FM_OR, root, ctx, negated);
        }

    }else if (kw == PDDL_KW_IMPLY){
        if (root->child_size != 3)
            ERR_LISP_RET(ctx->err, NULL, root,
                         "%s(imply ...) requires two arguments",
                         ctx->err_prefix);

        return parseImply(root->child + 1, root->child + 2, ctx, negated);

    }else if (kw == PDDL_KW_FORALL){
        // TODO: :conditional-effects || :universal-preconditions
        if (negated){
            return parseQuant(PDDL_FM_EXIST, root, ctx, negated);
        }else{
            return parseQuant(PDDL_FM_FORALL, root, ctx, negated);
        }

    }else if (kw == PDDL_KW_EXISTS){
        // TODO: :existential-preconditions
        if (negated){
            return parseQuant(PDDL_FM_FORALL, root, ctx, negated);
        }else{
            return parseQuant(PDDL_FM_EXIST, root, ctx, negated);
        }

    }else if (kw == PDDL_KW_WHEN){
        // Conditional effect cannot be negated
        return parseWhen(root, ctx);

    }else if (kw == PDDL_KW_INCREASE){
        return parseIncrease(root, ctx, negated);

    }else if (kw == -1){
        return parseAtom(root, ctx, negated);
    }

    if (root->child_size >= 1 && root->child[0].value != NULL){
        ERR_LISP_RET(ctx->err, NULL, root, "%sUnexpected token `%s'",
                     ctx->err_prefix, root->child[0].value);
    }else{
        ERR_LISP_RET(ctx->err, NULL, root,
                     "%sUnexpected token", ctx->err_prefix);
    }
}

pddl_fm_t *pddlFmParse(const pddl_lisp_node_t *root,
                       pddl_t *pddl,
                       const pddl_params_t *params,
                       const char *err_prefix,
                       pddl_err_t *err)
{
    parse_ctx_t ctx;
    pddl_fm_t *c;

    ctx.types = &pddl->type;
    ctx.objs = &pddl->obj;
    ctx.preds = &pddl->pred;
    ctx.funcs = &pddl->func;
    ctx.params = params;
    ctx.err_prefix = err_prefix;
    ctx.err = err;

    c = parse(root, &ctx, 0);
    if (c == NULL)
        PDDL_TRACE_RET(err, NULL);
    return c;
}

static pddl_fm_t *parseInitFunc(const pddl_lisp_node_t *n, pddl_t *pddl,
                                pddl_err_t *err)
{
    parse_ctx_t ctx;
    pddl_params_t params;
    pddl_fm_t *c;

    pddlParamsInit(&params);
    ctx.types = &pddl->type;
    ctx.objs = &pddl->obj;
    ctx.preds = &pddl->pred;
    ctx.funcs = &pddl->func;
    ctx.params = &params;
    ctx.err_prefix = "";
    ctx.err = err;

    c = parseAssign(n, &ctx, 0);
    pddlParamsFree(&params);

    if (c == NULL)
        PDDL_TRACE_RET(err, NULL);
    return c;
}

static pddl_fm_t *parseInitFact(const pddl_lisp_node_t *n, pddl_t *pddl,
                                pddl_err_t *err)
{
    parse_ctx_t ctx;
    pddl_params_t params;
    pddl_fm_t *c;

    pddlParamsInit(&params);
    ctx.types = &pddl->type;
    ctx.objs = &pddl->obj;
    ctx.preds = &pddl->pred;
    ctx.funcs = &pddl->func;
    ctx.params = &params;
    ctx.err_prefix = "";
    ctx.err = err;

    c = parseAtom(n, &ctx, 0);
    pddlParamsFree(&params);

    if (c == NULL)
        PDDL_TRACE_RET(err, NULL);
    return c;
}

static pddl_fm_t *parseInitFactFunc(const pddl_lisp_node_t *n, pddl_t *pddl,
                                    pddl_err_t *err)
{
    const char *head;

    if (n->child_size < 1)
        ERR_LISP_RET2(err, NULL, n, "Invalid expression in :init.");

    head = pddlLispNodeHead(n);
    if (head == NULL)
        ERR_LISP_RET2(err, NULL, n, "Invalid expression in :init.");
    if (strcmp(head, "=") == 0
            && n->child_size == 3
            && n->child[1].value == NULL){
        return parseInitFunc(n, pddl, err);
    }else{
        return parseInitFact(n, pddl, err);
    }
}

pddl_fm_junc_t *pddlFmParseInit(const pddl_lisp_node_t *root, pddl_t *pddl,
                                pddl_err_t *err)
{
    const pddl_lisp_node_t *n;
    pddl_fm_junc_t *and;
    pddl_fm_t *c;

    and = fmPartNew(PDDL_FM_AND);

    for (int i = 1; i < root->child_size; ++i){
        n = root->child + i;
        if ((c = parseInitFactFunc(n, pddl, err)) == NULL){
            fmPartDel(and);
            PDDL_TRACE_PREPEND_RET(err, NULL, "While parsing :init in %s: ",
                                   pddl->problem_lisp->filename);
        }
        fmPartAdd(and, c);
    }

    return and;
}

pddl_fm_t *pddlFmAtomToAnd(pddl_fm_t *atom)
{
    pddl_fm_junc_t *and;

    and = fmPartNew(PDDL_FM_AND);
    fmPartAdd(and, atom);
    return &and->fm;
}

pddl_fm_atom_t *pddlFmCreateFactAtom(int pred, int arg_size, 
                                     const pddl_obj_id_t *arg)
{
    pddl_fm_atom_t *a;

    a = fmAtomNew();
    a->pred = pred;
    a->arg_size = arg_size;
    a->arg = ALLOC_ARR(pddl_fm_atom_arg_t, arg_size);
    for (int i = 0; i < arg_size; ++i){
        a->arg[i].param = -1;
        a->arg[i].obj = arg[i];
    }
    return a;
}

void pddlFmJuncAdd(pddl_fm_junc_t *part, pddl_fm_t *c)
{
    fmPartAdd(part, c);
}

void pddlFmJuncRm(pddl_fm_junc_t *part, pddl_fm_t *c)
{
    pddlListDel(&c->conn);
}

int pddlFmJuncIsEmpty(const pddl_fm_junc_t *part)
{
    return pddlListEmpty(&part->part);
}

void pddlFmReplace(pddl_fm_t *c, pddl_fm_t *r)
{
    if (pddlListEmpty(&c->conn))
        return;
    c->conn.prev->next = &r->conn;
    c->conn.next->prev = &r->conn;
    r->conn.next = c->conn.next;
    r->conn.prev = c->conn.prev;
    pddlListInit(&c->conn);
}


/*** CHECK ***/
int pddlFmCheckPre(const pddl_fm_t *fm,
                   const pddl_require_flags_t *require,
                   pddl_err_t *err)
{
    pddl_fm_junc_t *p;
    pddl_fm_quant_t *q;
    pddl_fm_atom_t *atom;
    pddl_fm_imply_t *imp;
    pddl_fm_t *c;
    pddl_list_t *item;

    if (fm->type == PDDL_FM_AND
            || fm->type == PDDL_FM_OR){
        if (fm->type == PDDL_FM_OR && !require->disjunctive_pre){
            PDDL_ERR(err, "(or ...) can be used only with"
                      " :disjunctive-preconditions");
            return -1;
        }

        p = OBJ(fm, junc);
        PDDL_LIST_FOR_EACH(&p->part, item){
            c = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);
            if (pddlFmCheckPre(c, require, err) != 0)
                PDDL_TRACE_RET(err, -1);
        }

        return 0;

    }else if (fm->type == PDDL_FM_FORALL){
        if (!require->universal_pre){
            PDDL_ERR(err, "(forall ...) can be used only with"
                      " :universal-preconditions");
            return -1;
        }

        q = OBJ(fm, quant);
        return pddlFmCheckPre(q->qfm, require, err);

    }else if (fm->type == PDDL_FM_EXIST){
        if (!require->existential_pre){
            PDDL_ERR(err, "(exists ...) can be used only with"
                      " :existential-preconditions");
            return -1;
        }

        q = OBJ(fm, quant);
        return pddlFmCheckPre(q->qfm, require, err);

    }else if (fm->type == PDDL_FM_WHEN){
        PDDL_ERR(err, "(when ...) cannot be part of preconditions");
        return -1;

    }else if (fm->type == PDDL_FM_ATOM){
        atom = OBJ(fm, atom);
        if (atom->neg && !require->negative_pre){
            PDDL_ERR(err, "For negative preconditions add"
                      " :negative-preconditions");
            return -1;
        }

        return 0;

    }else if (fm->type == PDDL_FM_IMPLY){
        imp = OBJ(fm, imply);
        if (!require->disjunctive_pre){
            PDDL_ERR(err, "(imply ...) can be used only with"
                      " :disjunctive-preconditions");
            return -1;
        }

        if (pddlFmCheckPre(imp->left, require, err) != 0)
            return -1;
        if (pddlFmCheckPre(imp->right, require, err) != 0)
            return -1;
        return 0;

    }else if (fm->type == PDDL_FM_ASSIGN || fm->type == PDDL_FM_INCREASE){
        return 0;
    }

    return -1;
}


static int checkCEffect(const pddl_fm_t *fm,
                        const pddl_require_flags_t *require,
                        pddl_err_t *err);
static int checkPEffect(const pddl_fm_t *fm,
                        const pddl_require_flags_t *require,
                        pddl_err_t *err);
static int checkCondEffect(const pddl_fm_t *fm,
                           const pddl_require_flags_t *require,
                           pddl_err_t *err);

static int checkCEffect(const pddl_fm_t *fm,
                        const pddl_require_flags_t *require,
                        pddl_err_t *err)
{
    pddl_fm_quant_t *forall;
    pddl_fm_when_t *when;

    if (fm->type == PDDL_FM_FORALL){
        if (!require->conditional_eff){
            PDDL_ERR(err, "(forall ...) is allowed in effects only if"
                      " :conditional-effects is specified as requirement");
            return -1;
        }

        forall = OBJ(fm, quant);
        return pddlFmCheckEff(forall->qfm, require, err);

    }else if (fm->type == PDDL_FM_WHEN){
        if (!require->conditional_eff){
            PDDL_ERR(err, "(when ...) is allowed in effects only if"
                      " :conditional-effects is specified as requirement");
            return -1;
        }

        when = OBJ(fm, when);
        if (pddlFmCheckPre(when->pre, require, err) != 0)
            return -1;
        return checkCondEffect(when->eff, require, err);

    }else{
        if (checkPEffect(fm, require, err) != 0){
            PDDL_ERR(err, "A single effect has to be either literal or"
                      " conditional effect (+ universal quantifier).");
            return -1;
        }
        return 0;
    }
}

static int checkPEffect(const pddl_fm_t *fm,
                        const pddl_require_flags_t *require,
                        pddl_err_t *err)
{
    if (fm->type == PDDL_FM_ATOM
            || fm->type == PDDL_FM_ASSIGN
            || fm->type == PDDL_FM_INCREASE){
        return 0;
    }
    return -1;
}

static int checkCondEffect(const pddl_fm_t *fm,
                           const pddl_require_flags_t *require,
                           pddl_err_t *err)
{
    const pddl_fm_junc_t *part;
    const pddl_fm_t *sub;
    pddl_list_t *item;

    if (checkPEffect(fm, require, err) == 0)
        return 0;

    if (fm->type == PDDL_FM_AND){
        part = OBJ(fm, junc);
        PDDL_LIST_FOR_EACH(&part->part, item){
            sub = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);
            if (checkPEffect(sub, require, err) != 0){
                PDDL_ERR(err, "Conditional effect can contain only literals"
                          " and conjuction of literals.");
                return -1;
            }
        }

        return 0;
    }

    return -1;
}

int pddlFmCheckEff(const pddl_fm_t *fm,
                   const pddl_require_flags_t *require,
                   pddl_err_t *err)
{
    const pddl_fm_junc_t *and;
    const pddl_fm_t *sub;
    pddl_list_t *item;

    if (fm->type == PDDL_FM_AND){
        and = OBJ(fm, junc);
        PDDL_LIST_FOR_EACH(&and->part, item){
            sub = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);
            if (checkCEffect(sub, require, err) != 0)
                return -1;
        }

        return 0;

    }else{
        return checkCEffect(fm, require, err);
    }
}


static int setPredRead(pddl_fm_t *fm, void *data)
{
    pddl_fm_atom_t *atom;
    pddl_preds_t *preds = data;

    if (fm->type == PDDL_FM_ATOM){
        atom = OBJ(fm, atom);
        preds->pred[atom->pred].read = 1;
    }
    return 0;
}

void pddlFmSetPredRead(const pddl_fm_t *fm, pddl_preds_t *preds)
{
    pddlFmTraverse((pddl_fm_t *)fm, setPredRead, NULL, preds);
}


static int setPredReadWrite(pddl_fm_t *fm, void *data)
{
    pddl_fm_atom_t *atom;
    pddl_fm_when_t *when;
    pddl_preds_t *preds = data;

    if (fm->type == PDDL_FM_WHEN){
        when = OBJ(fm, when);
        pddlFmTraverse((pddl_fm_t *)when->pre, setPredRead, NULL, data);
        pddlFmTraverse((pddl_fm_t *)when->eff,
                       setPredReadWrite, NULL, data);
        return -1;

    }else if (fm->type == PDDL_FM_ATOM){
        atom = OBJ(fm, atom);
        preds->pred[atom->pred].write = 1;
    }
    return 0;
}

void pddlFmSetPredReadWriteEff(const pddl_fm_t *fm, pddl_preds_t *preds)
{
    pddlFmTraverse((pddl_fm_t *)fm, setPredReadWrite, NULL, preds);
}

/*** INSTANTIATE QUANTIFIERS ***/
struct instantiate_cond {
    int param_id;
    pddl_obj_id_t obj_id;
};
typedef struct instantiate_cond instantiate_cond_t;

static int instantiateParentParam(pddl_fm_t *c, void *data)
{
    if (c->type == PDDL_FM_ATOM){
        const pddl_params_t *params = data;
        pddl_fm_atom_t *a = OBJ(c, atom);
        for (int i = 0; i < params->param_size; ++i){
            if (params->param[i].inherit < 0)
                continue;

            for (int j = 0; j < a->arg_size; ++j){
                if (a->arg[j].param == i)
                    a->arg[j].param = params->param[i].inherit;
            }
        }

    }else if (c->type == PDDL_FM_ASSIGN
            || c->type == PDDL_FM_INCREASE){
        if (OBJ(c, func_op)->lvalue)
            return instantiateParentParam(&OBJ(c, func_op)->lvalue->fm, data);
        if (OBJ(c, func_op)->fvalue)
            return instantiateParentParam(&OBJ(c, func_op)->fvalue->fm, data);
    }

    return 0;
}

static int instantiateCond(pddl_fm_t *c, void *data)
{
    const instantiate_cond_t *d = data;

    if (c->type == PDDL_FM_ATOM){
        pddl_fm_atom_t *a = OBJ(c, atom);
        for (int i = 0; i < a->arg_size; ++i){
            if (a->arg[i].param == d->param_id){
                a->arg[i].param = -1;
                a->arg[i].obj = d->obj_id;
            }
        }

    }else if (c->type == PDDL_FM_ASSIGN
            || c->type == PDDL_FM_INCREASE){
        if (OBJ(c, func_op)->lvalue)
            return instantiateCond(&OBJ(c, func_op)->lvalue->fm, data);
        if (OBJ(c, func_op)->fvalue)
            return instantiateCond(&OBJ(c, func_op)->fvalue->fm, data);
    }

    return 0;
}

static pddl_fm_junc_t *instantiatePart(pddl_fm_junc_t *p,
                                       int param_id,
                                       const pddl_obj_id_t *objs,
                                       int objs_size)
{
    pddl_fm_junc_t *out;
    pddl_fm_t *c, *newc;
    pddl_list_t *item;
    instantiate_cond_t set;

    out = fmPartNew(p->fm.type);

    for (int i = 0; i < objs_size; ++i){
        PDDL_LIST_FOR_EACH(&p->part, item){
            c = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);
            newc = pddlFmClone(c);
            set.param_id = param_id;
            set.obj_id = objs[i];
            pddlFmTraverse(newc, NULL, instantiateCond, &set);
            fmPartAdd(out, newc);
        }
    }

    pddlFmDel(&p->fm);
    return out;
}

static pddl_fm_t *instantiateQuant(pddl_fm_quant_t *q,
                                   const pddl_types_t *types)
{
    pddl_fm_junc_t *top;
    const pddl_param_t *param;
    const pddl_obj_id_t *obj;
    int obj_size, bval;

    // The instantiation of universal/existential quantifier is a
    // conjuction/disjunction of all instances.
    if (q->fm.type == PDDL_FM_FORALL){
        top = fmPartNew(PDDL_FM_AND);
    }else{
        top = fmPartNew(PDDL_FM_OR);
    }
    fmPartAdd(top, q->qfm);
    q->qfm = NULL;

    // Apply object to each (non-inherited) parameter according to its type
    for (int i = 0; i < q->param.param_size; ++i){
        param = q->param.param + i;
        if (param->inherit >= 0)
            continue;

        obj = pddlTypesObjsByType(types, param->type, &obj_size);
        if (obj_size == 0){
            bval = q->fm.type == PDDL_FM_FORALL;
            pddlFmDel(&top->fm);
            pddlFmDel(&q->fm);
            return &fmBoolNew(bval)->fm;

        }else{
            top = instantiatePart(top, i, obj, obj_size);
        }
    }

    // Replace all parameters inherited from the parent with IDs of the
    // parent parameters.
    pddlFmTraverse(&top->fm, NULL, instantiateParentParam, &q->param);

    pddlFmDel(&q->fm);
    return &top->fm;
}

static int instantiateForall(pddl_fm_t **c, void *data)
{
    const pddl_types_t *types = data;

    if ((*c)->type != PDDL_FM_FORALL)
        return 0;

    *c = instantiateQuant(OBJ(*c, quant), types);
    return 0;
}

static int instantiateExist(pddl_fm_t **c, void *data)
{
    const pddl_types_t *types = data;

    if ((*c)->type != PDDL_FM_EXIST)
        return 0;

    *c = instantiateQuant(OBJ(*c, quant), types);
    return 0;
}

static void pddlFmInstantiateQuant(pddl_fm_t **fm, const pddl_types_t *types)
{
    pddlFmRebuild(fm, NULL, instantiateForall, (void *)types);
    pddlFmRebuild(fm, NULL, instantiateExist, (void *)types);
}



/*** SIMPLIFY ***/
static pddl_fm_t *removeBoolPart(pddl_fm_junc_t *part)
{
    pddl_list_t *item, *tmp;
    pddl_fm_t *c;
    int bval;

    PDDL_LIST_FOR_EACH_SAFE(&part->part, item, tmp){
        c = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);
        if (c->type != PDDL_FM_BOOL)
            continue;

        bval = OBJ(c, bool)->val;
        if (part->fm.type == PDDL_FM_AND){
            if (!bval){
                pddlFmDel(&part->fm);
                return &fmBoolNew(0)->fm;
            }else{
                pddlListDel(item);
                pddlFmDel(c);
            }

        }else{ // PDDL_FM_OR
            if (bval){
                pddlFmDel(&part->fm);
                return &fmBoolNew(1)->fm;
            }else{
                pddlListDel(item);
                pddlFmDel(c);
            }
        }
    }

    if (pddlListEmpty(&part->part)){
        if (part->fm.type == PDDL_FM_AND){
            pddlFmDel(&part->fm);
            return &fmBoolNew(1)->fm;

        }else{ // PDDL_FM_OR
            pddlFmDel(&part->fm);
            return &fmBoolNew(0)->fm;
        }
    }

    return &part->fm;
}

static pddl_fm_t *removeBoolWhen(pddl_fm_when_t *when)
{
    pddl_fm_t *c;
    int bval;

    if (when->pre->type != PDDL_FM_BOOL)
        return &when->fm;

    bval = OBJ(when->pre, bool)->val;
    if (bval){
        c = when->eff;
        when->eff = NULL;
        pddlFmDel(&when->fm);
        return c;

    }else{ // !bval
        pddlFmDel(&when->fm);
        return &fmBoolNew(1)->fm;
    }
}

static int atomIsInInit(const pddl_t *pddl, const pddl_fm_atom_t *atom)
{
    pddl_list_t *item;
    const pddl_fm_t *c;

    PDDL_LIST_FOR_EACH(&pddl->init->part, item){
        c = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);
        if (c->type == PDDL_FM_ATOM
                && pddlFmAtomCmpNoNeg(atom, OBJ(c, atom)) == 0)
            return 1;
    }
    return 0;
}

/** Returns true if atom at least partially matches grounded atom ground_atom
 *  (disregarding negative flag), i.e., true is returned if the objects (in
 *  place of arguments) match. */
static int atomPartialMatchNoNeg(const pddl_fm_atom_t *atom,
                                 const pddl_fm_atom_t *ground_atom)
{
    int cmp;

    cmp = atom->pred - ground_atom->pred;
    if (cmp == 0){
        if (atom->arg_size != ground_atom->arg_size)
            return 0;

        for (int i = 0; i < atom->arg_size && cmp == 0; ++i){
            if (atom->arg[i].param >= 0)
                continue;
            cmp = atom->arg[i].obj - ground_atom->arg[i].obj;
        }
    }

    return cmp == 0;
}

static int atomIsPartiallyInInit(const pddl_t *pddl,
                                 const pddl_fm_atom_t *atom)
{
    pddl_list_t *item;
    const pddl_fm_t *c;

    PDDL_LIST_FOR_EACH(&pddl->init->part, item){
        c = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);
        if (c->type == PDDL_FM_ATOM
                && atomPartialMatchNoNeg(atom, OBJ(c, atom))){
            return 1;
        }
    }
    return 0;
}

static pddl_fm_t *removeBoolAtom(pddl_fm_atom_t *atom, const pddl_t *pddl)
{
    int bval;

    if (pddlPredIsStatic(&pddl->pred.pred[atom->pred])){
        if (atom->pred == pddl->pred.eq_pred){
            ASSERT(atom->arg_size == 2);
            if (atom->arg[0].obj >= 0 && atom->arg[1].obj >= 0){
                // Evaluate fully grounded (= ...) atom
                if (atom->arg[0].obj == atom->arg[1].obj){
                    bval = !atom->neg;
                }else{
                    bval = atom->neg;
                }
                pddlFmDel(&atom->fm);
                return &fmBoolNew(bval)->fm;
            }

        }else if (pddlFmAtomIsGrounded(atom)){
            // If the atom is static and fully grounded we can evaluate it
            // right now by comparing it to the inital state
            if (atomIsInInit(pddl, atom)){
                bval = !atom->neg;
            }else{
                bval = atom->neg;
            }
            pddlFmDel(&atom->fm);
            return &fmBoolNew(bval)->fm;

        }else if (atom->neg && !atomIsPartiallyInInit(pddl, atom)){
            // If the atom is static but not fully grounded we can evaluate
            // it if there is no atom matching the grounded parts
            bval = atom->neg;
            pddlFmDel(&atom->fm);
            return &fmBoolNew(bval)->fm;
        }
    }

    return &atom->fm;
}

static pddl_fm_t *removeBoolImply(pddl_fm_imply_t *imp)
{
    if (imp->left->type == PDDL_FM_BOOL){
        pddl_fm_bool_t *b = OBJ(imp->left, bool);
        if (b->val){
            pddl_fm_t *ret = imp->right;
            imp->right = NULL;
            pddlFmDel(&imp->fm);
            return ret;

        }else{
            pddlFmDel(&imp->fm);
            return &fmBoolNew(1)->fm;
        }
    }

    return &imp->fm;
}

static int removeBool(pddl_fm_t **c, void *data)
{
    const pddl_t *pddl = data;

    if ((*c)->type == PDDL_FM_ATOM){
        *c = removeBoolAtom(OBJ(*c, atom), pddl);

    }else if ((*c)->type == PDDL_FM_AND
            || (*c)->type == PDDL_FM_OR){
        *c = removeBoolPart(OBJ(*c, junc));

    }else if ((*c)->type == PDDL_FM_WHEN){
        *c = removeBoolWhen(OBJ(*c, when));

    }else if ((*c)->type == PDDL_FM_IMPLY){
        *c = removeBoolImply(OBJ(*c, imply));
    }

    return 0;
}

static pddl_fm_t *flattenPart(pddl_fm_junc_t *part)
{
    pddl_list_t *item, *tmp;
    pddl_fm_t *c;
    pddl_fm_junc_t *p;

    if (pddlListEmpty(&part->part))
        return &part->fm;

    PDDL_LIST_FOR_EACH_SAFE(&part->part, item, tmp){
        c = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);

        if (c->type == part->fm.type){
            // Flatten con/disjunctions
            p = OBJ(c, junc);
            fmPartStealPart(part, p);

            pddlListDel(item);
            pddlFmDel(c);

        }else if ((c->type == PDDL_FM_AND || c->type == PDDL_FM_OR)
                && pddlListEmpty(&OBJ(c, junc)->part)){
            pddlListDel(item);
            pddlFmDel(c);
        }

    }

    // If the con/disjunction contains only one atom, remove the
    // con/disjunction and return the atom directly
    if (pddlListPrev(&part->part) == pddlListNext(&part->part)){
        item = pddlListNext(&part->part);
        c = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);
        pddlListDel(item);
        pddlFmDel(&part->fm);
        return c;
    }

    return &part->fm;
}

/** Splits (when ...) if its precondition is disjunction */
static pddl_fm_t *flattenWhen(pddl_fm_when_t *when)
{
    pddl_list_t *item;
    pddl_fm_t *c;
    pddl_fm_junc_t *pre;
    pddl_fm_junc_t *and;
    pddl_fm_when_t *add;

    if (!when->pre || when->pre->type != PDDL_FM_OR)
        return &when->fm;

    and = fmPartNew(PDDL_FM_AND);
    pre = OBJ(when->pre, junc);
    when->pre = NULL;

    while (!pddlListEmpty(&pre->part)){
        item = pddlListNext(&pre->part);
        pddlListDel(item);
        c = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);
        add = fmWhenClone(when);
        add->pre = c;
        fmPartAdd(and, &add->fm);
    }

    pddlFmDel(&pre->fm);
    pddlFmDel(&when->fm);

    return &and->fm;
}

static int flatten(pddl_fm_t **c, void *data)
{
    if ((*c)->type == PDDL_FM_AND
            || (*c)->type == PDDL_FM_OR){
        *c = flattenPart(OBJ(*c, junc));

    }else if ((*c)->type == PDDL_FM_WHEN){
        *c = flattenWhen(OBJ(*c, when));
    }

    return 0;
}

static pddl_fm_junc_t *moveDisjunctionsCreate1(pddl_fm_junc_t *top,
                                               pddl_fm_junc_t *or)
{
    pddl_fm_junc_t *ret;
    pddl_list_t *item1, *item2;
    pddl_fm_t *c1, *c2;
    pddl_fm_junc_t *add;

    ret = fmPartNew(PDDL_FM_OR);
    PDDL_LIST_FOR_EACH(&top->part, item1){
        c1 = PDDL_LIST_ENTRY(item1, pddl_fm_t, conn);
        PDDL_LIST_FOR_EACH(&or->part, item2){
            c2 = PDDL_LIST_ENTRY(item2, pddl_fm_t, conn);
            add = OBJ(c1, junc);
            add = fmPartClone(add);
            fmPartAdd(add, pddlFmClone(c2));
            fmPartAdd(ret, &add->fm);
        }
    }

    pddlFmDel(&top->fm);
    return ret;
}

static pddl_fm_t *moveDisjunctionsCreate(pddl_fm_junc_t *and,
                                         pddl_list_t *or_list)
{
    pddl_list_t *or_item;
    pddl_fm_junc_t *or;
    pddl_fm_junc_t *ret;

    ret = fmPartNew(PDDL_FM_OR);
    fmPartAdd(ret, &and->fm);
    while (!pddlListEmpty(or_list)){
        or_item = pddlListNext(or_list);
        pddlListDel(or_item);
        or = OBJ(PDDL_LIST_ENTRY(or_item, pddl_fm_t, conn), junc);
        ret = moveDisjunctionsCreate1(ret, or);
        pddlFmDel(&or->fm);
    }

    return &ret->fm;
}

static pddl_fm_t *moveDisjunctionsUpAnd(pddl_fm_junc_t *and)
{
    pddl_list_t *item, *tmp;
    pddl_list_t or_list;
    pddl_fm_t *c;

    pddlListInit(&or_list);
    PDDL_LIST_FOR_EACH_SAFE(&and->part, item, tmp){
        c = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);
        if (c->type != PDDL_FM_OR)
            continue;

        pddlListDel(item);
        pddlListAppend(&or_list, item);
    }

    if (pddlListEmpty(&or_list)){
        return &and->fm;
    }

    return moveDisjunctionsCreate(and, &or_list);
}

static int moveDisjunctionsUp(pddl_fm_t **c, void *data)
{
    if ((*c)->type == PDDL_FM_AND)
        *c = moveDisjunctionsUpAnd(OBJ(*c, junc));

    if ((*c)->type == PDDL_FM_OR)
        *c = flattenPart(OBJ(*c, junc));
    return 0;
}

/** (imply ...) is considered static if it has a simple flattened left and
 *  right side and the left side consists solely of static predicates. */
static int isStaticImply(const pddl_fm_imply_t *imp, const pddl_t *pddl)
{
    ASSERT(imp->left != NULL && imp->right != NULL);
    pddl_fm_junc_t *and;
    pddl_fm_atom_t *atom;
    pddl_fm_t *c;
    pddl_list_t *item;

    if (imp->left->type == PDDL_FM_ATOM){
        atom = OBJ(imp->left, atom);
        if (atom->pred < 0)
            return 0;
        if (!pddlPredIsStatic(&pddl->pred.pred[atom->pred]))
            return 0;

    }else if (imp->left->type == PDDL_FM_AND){
        and = OBJ(imp->left, junc);
        PDDL_LIST_FOR_EACH(&and->part, item){
            c = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);
            if (c->type != PDDL_FM_ATOM)
                return 0;

            atom = OBJ(c, atom);
            if (atom->pred < 0)
                return 0;
            if (!pddlPredIsStatic(&pddl->pred.pred[atom->pred]))
                return 0;
        }

    }else{
        return 0;
    }

    if (imp->right->type == PDDL_FM_ATOM){
        return 1;

    }else if (imp->right->type == PDDL_FM_AND){
        and = OBJ(imp->right, junc);
        PDDL_LIST_FOR_EACH(&and->part, item){
            c = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);
            if (c->type != PDDL_FM_ATOM)
                return 0;
        }

    }else{
        return 0;
    }

    return 1;
}

static int removeNonStaticImply(pddl_fm_t **c, void *data)
{
    pddl_fm_imply_t *imp;
    const pddl_t *pddl = data;

    if ((*c)->type != PDDL_FM_IMPLY)
        return 0;
    imp = OBJ(*c, imply);
    if (!isStaticImply(imp, pddl)){
        pddl_fm_junc_t *or;
        or = fmPartNew(PDDL_FM_OR);
        pddlFmJuncAdd(or, pddlFmNegate(imp->left, pddl));
        pddlFmJuncAdd(or, imp->right);
        *c = &or->fm;

        imp->right = NULL;
        pddlFmDel(&imp->fm);
    }

    return 0;
}


static void implyAtomParams(const pddl_fm_atom_t *atom, pddl_iset_t *params)
{
    for (int i = 0; i < atom->arg_size; ++i){
        if (atom->arg[i].param >= 0)
            pddlISetAdd(params, atom->arg[i].param);
    }
}

static int implyParams(pddl_fm_t *c, void *data)
{
    pddl_iset_t *params = data;
    pddl_fm_imply_t *imp;
    pddl_list_t *item;
    pddl_fm_junc_t *and;
    pddl_fm_t *p;

    if (c->type == PDDL_FM_IMPLY){
        imp = OBJ(c, imply);
        if (imp->left->type == PDDL_FM_ATOM){
            implyAtomParams(OBJ(imp->left, atom), params);

        }else if (imp->left->type == PDDL_FM_AND){
            and = OBJ(imp->left, junc);
            PDDL_LIST_FOR_EACH(&and->part, item){
                p = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);
                if (p->type == PDDL_FM_ATOM)
                    implyAtomParams(OBJ(p, atom), params);
            }
        }
    }

    return 0;
}

struct instantiate_ctx {
    const pddl_iset_t *params;
    const pddl_obj_id_t *arg;
};
typedef struct instantiate_ctx instantiate_ctx_t;

static int instantiateTraverse(pddl_fm_t *fm, void *ud)
{
    const instantiate_ctx_t *ctx = ud;
    if (fm->type == PDDL_FM_ATOM){
        pddl_fm_atom_t *atom = OBJ(fm, atom);
        for (int i = 0; i < atom->arg_size; ++i){
            for (int j = 0; j < pddlISetSize(ctx->params); ++j){
                if (atom->arg[i].param == pddlISetGet(ctx->params, j)){
                    atom->arg[i].param = -1;
                    atom->arg[i].obj = ctx->arg[j];
                    break;
                }
            }
        }
    }

    return 0;
}

static pddl_fm_t *instantiate(pddl_fm_t *fm,
                              const pddl_iset_t *params,
                              const pddl_obj_id_t *arg,
                              int eq_pred)
{
    pddl_fm_junc_t *and;
    pddl_fm_atom_t *eq;
    pddl_fm_t *c = pddlFmClone(fm);
    instantiate_ctx_t ctx;

    and = fmPartNew(PDDL_FM_AND);
    for (int i = 0; i < pddlISetSize(params); ++i){
        int param = pddlISetGet(params, i);
        eq = fmAtomNew();
        eq->pred = eq_pred;
        eq->arg_size = 2;
        eq->arg = ALLOC_ARR(pddl_fm_atom_arg_t, 2);
        eq->arg[0].param = param;
        eq->arg[0].obj = PDDL_OBJ_ID_UNDEF;
        eq->arg[1].param = -1;
        eq->arg[1].obj = arg[i];
        pddlFmJuncAdd(and, &eq->fm);
    }

    ctx.params = params;
    ctx.arg = arg;
    pddlFmTraverse(c, NULL, instantiateTraverse, &ctx);

    pddlFmJuncAdd(and, c);
    return &and->fm;
}

static void removeStaticImplyRec(pddl_fm_junc_t *top,
                                 pddl_fm_t *fm,
                                 const pddl_t *pddl,
                                 const pddl_params_t *params,
                                 const pddl_iset_t *imp_params,
                                 int pidx,
                                 pddl_obj_id_t *arg)
{
    const pddl_obj_id_t *obj;
    int obj_size;

    if (pidx == pddlISetSize(imp_params)){
        pddl_fm_t *c = instantiate(fm, imp_params, arg, pddl->pred.eq_pred);
        pddlFmJuncAdd(top, c);
    }else{
        int param = pddlISetGet(imp_params, pidx);
        obj = pddlTypesObjsByType(&pddl->type, params->param[param].type,
                                  &obj_size);
        for (int i = 0; i < obj_size; ++i){
            arg[pidx] = obj[i];
            removeStaticImplyRec(top, fm, pddl, params,
                                 imp_params, pidx + 1, arg);
        }
    }
}

/** Implications are removed by instantiation of the left sides and putting
 *  the instantiated objects to (= ...) predicate. */
static int removeStaticImply(pddl_fm_t **fm, const pddl_t *pddl,
                             const pddl_params_t *params)
{
    pddl_fm_junc_t *or;
    PDDL_ISET(imply_params);
    pddl_obj_id_t *obj;

    if (params == NULL)
        return 0;

    pddlFmTraverse(*fm, NULL, implyParams, &imply_params);
    if (pddlISetSize(&imply_params) > 0){
        obj = ALLOC_ARR(pddl_obj_id_t, pddlISetSize(&imply_params));
        or = fmPartNew(PDDL_FM_OR);
        removeStaticImplyRec(or, *fm, pddl, params, &imply_params, 0, obj);
        FREE(obj);
        pddlFmDel(*fm);
        *fm = &or->fm;
    }
    pddlISetFree(&imply_params);
    return 0;
}

pddl_fm_t *pddlFmNormalize(pddl_fm_t *fm,
                           const pddl_t *pddl,
                           const pddl_params_t *params)
{
    pddl_fm_t *c = fm;

    // TODO: Check return values
    pddlFmInstantiateQuant(&c, &pddl->type);
    pddlFmRebuild(&c, NULL, removeNonStaticImply, (void *)pddl);
    removeStaticImply(&c, pddl, params);
    pddlFmRebuild(&c, NULL, removeBool, (void *)pddl);
    pddlFmRebuild(&c, NULL, flatten, NULL);
    pddlFmRebuild(&c, NULL, moveDisjunctionsUp, NULL);
    pddlFmRebuild(&c, NULL, flatten, NULL);
    c = pddlFmDeduplicateAtoms(c, pddl);
    return c;
}

static void _deduplicateAtoms(pddl_fm_junc_t *p)
{
    pddl_list_t *item = pddlListNext(&p->part);
    while (item != &p->part){
        pddl_fm_t *c1 = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);
        if (c1->type != PDDL_FM_ATOM){
            item = pddlListNext(item);
            continue;
        }

        pddl_list_t *item2 = pddlListNext(item);
        for (; item2 != &p->part;){
            pddl_fm_t *c2 = PDDL_LIST_ENTRY(item2, pddl_fm_t, conn);
            if (c2->type == PDDL_FM_ATOM
                    && pddlFmAtomCmp(OBJ(c1, atom), OBJ(c2, atom)) == 0){
                pddl_list_t *item_del = item2;
                item2 = pddlListNext(item2);
                pddlListDel(item_del);
                pddlFmDel(c2);

            }else{
                item2 = pddlListNext(item2);
            }
        }
        item = pddlListNext(item);
    }
}

static int deduplicateAtoms(pddl_fm_t **c, void *data)
{
    if ((*c)->type == PDDL_FM_AND || (*c)->type == PDDL_FM_OR)
        _deduplicateAtoms(OBJ(*c, junc));
    return 0;
}

pddl_fm_t *pddlFmDeduplicateAtoms(pddl_fm_t *fm, const pddl_t *pddl)
{
    pddl_fm_t *c = fm;
    pddlFmRebuild(&c, NULL, deduplicateAtoms, NULL);
    return c;
}

static void _deduplicate(pddl_fm_junc_t *p)
{
    pddl_list_t *item = pddlListNext(&p->part);
    while (item != &p->part){
        pddl_fm_t *c1 = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);

        pddl_list_t *item2 = pddlListNext(item);
        for (; item2 != &p->part;){
            pddl_fm_t *c2 = PDDL_LIST_ENTRY(item2, pddl_fm_t, conn);
            if (pddlFmEq(c1, c2)){
                pddl_list_t *item_del = item2;
                item2 = pddlListNext(item2);
                pddlListDel(item_del);
                pddlFmDel(c2);

            }else{
                item2 = pddlListNext(item2);
            }
        }
        item = pddlListNext(item);
    }
}

static int deduplicate(pddl_fm_t **c, void *data)
{
    if ((*c)->type == PDDL_FM_AND || (*c)->type == PDDL_FM_OR)
        _deduplicate(OBJ(*c, junc));
    return 0;
}

pddl_fm_t *pddlFmDeduplicate(pddl_fm_t *fm, const pddl_t *pddl)
{
    pddl_fm_t *c = fm;
    pddlFmRebuild(&c, NULL, deduplicate, NULL);
    return c;
}


static int removeConflictsInEff(pddl_fm_junc_t *p)
{
    pddl_list_t *item, *item2, *tmp;
    pddl_fm_t *c1, *c2;
    pddl_fm_atom_t *a1, *a2;
    int change = 0;

    for (item = pddlListNext(&p->part); item != &p->part;){
        c1 = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);
        if (c1->type != PDDL_FM_ATOM){
            item = pddlListNext(item);
            continue;
        }
        a1 = OBJ(c1, atom);

        for (item2 = pddlListNext(item); item2 != &p->part;){
            c2 = PDDL_LIST_ENTRY(item2, pddl_fm_t, conn);
            if (c2->type != PDDL_FM_ATOM){
                item2 = pddlListNext(item2);
                continue;
            }
            a2 = OBJ(c2, atom);

            if (pddlFmAtomInConflict(a1, a2, NULL)){
                if (a1->neg){
                    tmp = pddlListPrev(item);
                    pddlListDel(item);
                    pddlFmDel(&a1->fm);
                    item = tmp;
                    change = 1;
                    break;

                }else{
                    tmp = pddlListPrev(item2);
                    pddlListDel(item2);
                    pddlFmDel(&a2->fm);
                    item2 = tmp;
                    change = 1;
                }
            }
            item2 = pddlListNext(item2);
        }

        item = pddlListNext(item);
    }

    return change;
}

static int deconflictEffPost(pddl_fm_t **c, void *data)
{
    if ((*c)->type == PDDL_FM_AND || (*c)->type == PDDL_FM_OR){
        if (removeConflictsInEff(OBJ(*c, junc)))
            *((int *)data) = 1;
    }
    return 0;
}

static int deconflictEffPre(pddl_fm_t **c, void *data)
{
    if ((*c)->type == PDDL_FM_WHEN){
        pddl_fm_when_t *w = OBJ(*c, when);
        pddlFmRebuild(&w->eff, deconflictEffPre, deconflictEffPost, data);
        return -1;
    }
    return 0;
}

pddl_fm_t *pddlFmDeconflictEff(pddl_fm_t *fm, const pddl_t *pddl,
                               const pddl_params_t *params)
{
    pddl_fm_t *c = fm;
    int change = 0;
    pddlFmRebuild(&c, deconflictEffPre, deconflictEffPost, &change);
    if (change)
        c = pddlFmNormalize(c, pddl, params);
    return c;
}

struct simplify {
    const pddl_t *pddl;
    const pddl_params_t *params;
    int change;
};

static int reorderEqPredicates(pddl_fm_t **c, void *data)
{
    struct simplify *d = data;
    if ((*c)->type == PDDL_FM_ATOM){
        pddl_fm_atom_t *a = PDDL_FM_CAST(*c, atom);
        if (a->pred == d->pddl->pred.eq_pred){
            if (a->arg[0].param >= 0 && a->arg[1].param >= 0){
                if (a->arg[0].param > a->arg[1].param){
                    int p;
                    PDDL_SWAP(a->arg[0].param, a->arg[1].param, p);
                }
            }else if (a->arg[0].param >= 0){
                // Do nothing, it's already ordered
            }else if (a->arg[1].param >= 0){
                a->arg[0].param = a->arg[1].param;
                a->arg[1].obj = a->arg[0].obj;
                a->arg[0].obj = PDDL_OBJ_ID_UNDEF;
                a->arg[1].param = -1;
            }else{
                pddl_fm_t *b = NULL;
                if (a->arg[0].obj != a->arg[1].obj){
                    b = &pddlFmNewBool(0)->fm;
                }else{
                    b = &pddlFmNewBool(1)->fm;
                }
                pddlFmDel(*c);
                *c = b;
            }
        }
    }

    return 0;
}

static int simplifyBoolsInPart(pddl_fm_t **c, void *data)
{
    struct simplify *d = data;
    if ((*c)->type == PDDL_FM_AND || (*c)->type == PDDL_FM_OR){
        pddl_fm_junc_t *p = PDDL_FM_CAST(*c, junc);
        pddl_list_t *item, *itmp;
        PDDL_LIST_FOR_EACH_SAFE(&p->part, item, itmp){
            pddl_fm_t *cb = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);
            if (cb->type == PDDL_FM_BOOL){
                pddl_fm_bool_t *b = PDDL_FM_CAST(cb, bool);
                if (b->val){
                    if ((*c)->type == PDDL_FM_AND){
                        pddlListDel(&cb->conn);
                        pddlFmDel(cb);
                        d->change = 1;
                    }else{
                        pddlFmDel(*c);
                        *c = &pddlFmNewBool(1)->fm;
                        d->change = 1;
                        return 0;
                    }
                }else{
                    if ((*c)->type == PDDL_FM_AND){
                        pddlFmDel(*c);
                        *c = &pddlFmNewBool(0)->fm;
                        d->change = 1;
                        return 0;
                    }else{
                        pddlListDel(&cb->conn);
                        pddlFmDel(cb);
                        d->change = 1;
                    }
                }
            }
        }
    }
    return 0;
}

static int simplifySingletonPart(pddl_fm_t **c, void *data)
{
    struct simplify *d = data;
    if ((*c)->type == PDDL_FM_AND || (*c)->type == PDDL_FM_OR){
        pddl_fm_junc_t *p = PDDL_FM_CAST(*c, junc);
        pddl_list_t *next = pddlListNext(&p->part);
        if (next != &p->part && pddlListNext(next) == &p->part){
            pddl_fm_t *e = PDDL_LIST_ENTRY(next, pddl_fm_t, conn);
            pddlListDel(&e->conn);
            pddlFmDel(*c);
            *c = e;
            d->change = 1;
        }
    }
    return 0;
}

static int simplifyNestedPart(pddl_fm_t **c, void *data)
{
    if ((*c)->type == PDDL_FM_AND || (*c)->type == PDDL_FM_OR){
        pddl_fm_junc_t *p = PDDL_FM_CAST(*c, junc);
        *c = flattenPart(p);
    }
    return 0;
}

static int simplifyConflictAtoms(pddl_fm_t **c, void *data)
{
    if ((*c)->type != PDDL_FM_AND && (*c)->type != PDDL_FM_OR)
        return 0;

    struct simplify *d = data;
    pddl_fm_junc_t *p = OBJ(*c, junc);
    pddl_list_t *item;

    PDDL_LIST_FOR_EACH(&p->part, item){
        pddl_fm_t *c1 = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);
        if (c1->type != PDDL_FM_ATOM)
            continue;
        pddl_fm_atom_t *a1 = OBJ(c1, atom);

        pddl_list_t *item2 = pddlListNext(item);
        for (; item2 != &p->part; item2 = pddlListNext(item2)){
            pddl_fm_t *c2 = PDDL_LIST_ENTRY(item2, pddl_fm_t, conn);
            if (c2->type != PDDL_FM_ATOM)
                continue;
            pddl_fm_atom_t *a2 = OBJ(c2, atom);

            if (pddlFmAtomInConflict(a1, a2, d->pddl)){
                if ((*c)->type == PDDL_FM_AND){
                    pddlFmDel(*c);
                    *c = &pddlFmNewBool(0)->fm;
                    d->change = 1;
                    return 0;
                }else{
                    pddlFmDel(*c);
                    *c = &pddlFmNewBool(1)->fm;
                    d->change = 1;
                    return 0;
                }
            }
        }
    }

    return 0;
}

static int simplifyConflictEqAtoms(pddl_fm_t **c, void *data)
{
    if ((*c)->type != PDDL_FM_AND)
        return 0;

    // Here, we assume that the arguments are already sorted with
    // reorderEqPredicates
    struct simplify *d = data;
    int eq_pred = d->pddl->pred.eq_pred;
    pddl_fm_junc_t *p = OBJ(*c, junc);
    pddl_list_t *item;

    PDDL_LIST_FOR_EACH(&p->part, item){
        pddl_fm_t *c1 = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);
        if (c1->type != PDDL_FM_ATOM)
            continue;
        pddl_fm_atom_t *a1 = OBJ(c1, atom);
        if (a1->pred != eq_pred
                || a1->neg
                || a1->arg[0].param < 0
                || a1->arg[1].param >= 0){
            continue;
        }
        // Now a1 := (= p o)

        pddl_list_t *item2, *itmp;
        PDDL_LIST_FOR_EACH_SAFE(&p->part, item2, itmp){
            pddl_fm_t *c2 = PDDL_LIST_ENTRY(item2, pddl_fm_t, conn);
            if (c2->type != PDDL_FM_ATOM)
                continue;
            pddl_fm_atom_t *a2 = OBJ(c2, atom);
            if (a2->pred == eq_pred
                    && a2->arg[0].param == a1->arg[0].param
                    && a2->arg[1].param < 0
                    && a2->arg[1].obj != a1->arg[1].obj){
                if (a2->neg){
                    // a1 := (= p o); a2 := (not (= p o')), o != o', so a2 is
                    // redundant
                    pddlListDel(&c2->conn);
                    pddlFmDel(c2);
                    d->change = 1;
                }else{
                    // a1 := (= p o); a2 := (= p o'), o != o', which can
                    // never be true
                    pddlFmDel(*c);
                    *c = &pddlFmNewBool(0)->fm;
                    d->change = 1;
                    return 0;
                }
            }
        }
    }

    return 0;
}

static int entailsAny(const pddl_fm_t *c,
                      const pddl_fm_junc_t *p,
                      const pddl_t *pddl,
                      const pddl_params_t *param)
{
    pddl_list_t *item;
    PDDL_LIST_FOR_EACH(&p->part, item){
        const pddl_fm_t *s = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);
        if (s == c)
            continue;
        if (pddlFmIsEntailed(s, c, pddl, param))
            return 1;
    }
    return 0;
}

/** ((A or B) and (A => B)) -> B
 *  ((A and B) and (A => B)) -> A */
static int simplifyByEntailement(pddl_fm_t **c, void *data)
{
    struct simplify *d = data;
    if ((*c)->type == PDDL_FM_AND){
        pddl_fm_junc_t *p = OBJ(*c, junc);
        pddl_list_t *item = pddlListNext(&p->part);
        while (item != &p->part){
            pddl_fm_t *s = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);

            pddl_list_t *item2, *tmp;
            PDDL_LIST_FOR_EACH_SAFE(&p->part, item2, tmp){
                if (item2 == item)
                    continue;
                pddl_fm_t *x = PDDL_LIST_ENTRY(item2, pddl_fm_t, conn);
                if (pddlFmIsEntailed(x, s, d->pddl, d->params)){
                    pddlListDel(&x->conn);
                    pddlFmDel(x);
                    d->change = 1;
                }
            }

            item = pddlListNext(item);
        }

    }else if ((*c)->type == PDDL_FM_OR){
        pddl_fm_junc_t *p = OBJ(*c, junc);
        pddl_list_t *item, *tmp;
        PDDL_LIST_FOR_EACH_SAFE(&p->part, item, tmp){
            pddl_fm_t *x = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);
            if (entailsAny(x, p, d->pddl, d->params)){
                pddlListDel(&x->conn);
                pddlFmDel(x);
                d->change = 1;
            }
        }
    }
    return 0;
}

static int atomHasNegationInDisjunction(const pddl_fm_atom_t *atom,
                                        const pddl_fm_junc_t *disj,
                                        pddl_fm_atom_t **witness)
{
    *witness = NULL;
    pddl_fm_const_it_atom_t it;
    const pddl_fm_atom_t *datom;
    PDDL_FM_FOR_EACH_ATOM(&disj->fm, &it, datom){
        if (fmAtomEqNoNeg(atom, datom) && atom->neg == !datom->neg){
            *witness = (pddl_fm_atom_t *)datom;
            return 1;
        }
    }
    return 0;
}

/** ((A or not B) and B) -> A and B
 *  ((A and not B) or B) -> A or B */
static int simplifyByNegationDistribution(pddl_fm_t **c, void *data)
{
    struct simplify *d = data;
    if ((*c)->type == PDDL_FM_AND || (*c)->type == PDDL_FM_OR){
        int other_type = PDDL_FM_OR;
        if ((*c)->type == PDDL_FM_OR)
            other_type = PDDL_FM_AND;

        pddl_fm_junc_t *p = OBJ(*c, junc);
        pddl_list_t *item = pddlListNext(&p->part);
        while (item != &p->part){
            pddl_fm_t *s1 = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);
            if (s1->type != other_type && s1->type != PDDL_FM_ATOM)
                continue;

            pddl_list_t *item2 = pddlListNext(item);
            while (item2 != &p->part){
                pddl_fm_t *s2 = PDDL_LIST_ENTRY(item2, pddl_fm_t, conn);
                pddl_fm_atom_t *atom = NULL;
                pddl_fm_junc_t *part = NULL;
                if (s1->type == other_type && s2->type == PDDL_FM_ATOM){
                    atom = OBJ(s2, atom);
                    part = OBJ(s1, junc);

                }else if (s2->type == other_type && s1->type == PDDL_FM_ATOM){
                    atom = OBJ(s1, atom);
                    part = OBJ(s2, junc);
                }

                if (atom != NULL && part != NULL){
                    pddl_fm_atom_t *witness;
                    if (atomHasNegationInDisjunction(atom, part, &witness)){
                        pddlFmJuncRm(part, &witness->fm);
                        d->change = 1;
                        return 0;
                    }
                }

                item2 = pddlListNext(item2);
            }

            item = pddlListNext(item);
        }
    }
    return 0;
}

pddl_fm_t *pddlFmSimplify(pddl_fm_t *fm,
                          const pddl_t *pddl,
                          const pddl_params_t *params)
{
    struct simplify d;
    pddl_fm_t *c = fm;

    d.pddl = pddl;
    d.params = params;
    d.change = 0;

    pddlFmRebuild(&c, NULL, reorderEqPredicates, &d);
    do {
        d.change = 0;
        pddlFmRebuild(&c, NULL, simplifyBoolsInPart, &d);
        pddlFmRebuild(&c, NULL, simplifySingletonPart, &d);
        pddlFmRebuild(&c, NULL, simplifyNestedPart, &d);
        pddlFmRebuild(&c, NULL, simplifyConflictAtoms, &d);
        pddlFmRebuild(&c, NULL, simplifyConflictEqAtoms, &d);
        pddlFmRebuild(&c, NULL, simplifyByEntailement, &d);
        pddlFmRebuild(&c, NULL, simplifyByNegationDistribution, &d);
        c = pddlFmDeduplicate(c, pddl);
    } while (d.change);
    return c;
}

int pddlFmAtomIsGrounded(const pddl_fm_atom_t *atom)
{
    for (int i = 0; i < atom->arg_size; ++i){
        if (atom->arg[i].param >= 0)
            return 0;
    }
    return 1;
}

static int cmpAtomArgs(const pddl_fm_atom_t *a1, const pddl_fm_atom_t *a2)
{
    int cmp = 0;
    if (a1->arg_size != a2->arg_size)
        return a1->arg_size - a2->arg_size;
    for (int i = 0; i < a1->arg_size && cmp == 0; ++i){
        cmp = a1->arg[i].param - a2->arg[i].param;
        if (cmp == 0)
            cmp = a1->arg[i].obj - a2->arg[i].obj;
    }
    return cmp;
}

static int cmpAtoms(const pddl_fm_atom_t *a1, const pddl_fm_atom_t *a2,
                    int neg)
{
    int cmp;

    cmp = a1->pred - a2->pred;
    if (cmp == 0){
        cmp = cmpAtomArgs(a1, a2);
        if (cmp == 0 && neg)
            return a1->neg - a2->neg;
    }

    return cmp;
}

int pddlFmAtomCmp(const pddl_fm_atom_t *a1,
                  const pddl_fm_atom_t *a2)
{
    return cmpAtoms(a1, a2, 1);
}

int pddlFmAtomCmpNoNeg(const pddl_fm_atom_t *a1,
                       const pddl_fm_atom_t *a2)
{
    return cmpAtoms(a1, a2, 0);
}

static int atomNegPred(const pddl_fm_atom_t *a, const pddl_t *pddl)
{
    int pred = a->pred;
    if (pddl->pred.pred[a->pred].neg_of >= 0)
        pred = PDDL_MIN(pred, pddl->pred.pred[a->pred].neg_of);
    return pred;
}

int pddlFmAtomInConflict(const pddl_fm_atom_t *a1,
                         const pddl_fm_atom_t *a2,
                         const pddl_t *pddl)
{
    if (a1->pred == a2->pred && a1->neg != a2->neg)
        return cmpAtomArgs(a1, a2) == 0;
    if (pddl != NULL
            && a1->pred != a2->pred
            && atomNegPred(a1, pddl) == atomNegPred(a2, pddl)
            && a1->neg == a2->neg){
        return cmpAtomArgs(a1, a2) == 0;
    }
    return 0;
}

static void fmAtomRemapObjs(pddl_fm_atom_t *a, const pddl_obj_id_t *remap)
{
    for (int i = 0; i < a->arg_size; ++i){
        if (a->arg[i].obj >= 0)
            a->arg[i].obj = remap[a->arg[i].obj];
    }
}

static int fmRemapObjs(pddl_fm_t *c, void *_remap)
{
    const pddl_obj_id_t *remap = _remap;
    if (c->type == PDDL_FM_ATOM){
        pddl_fm_atom_t *a = PDDL_FM_CAST(c, atom);
        fmAtomRemapObjs(a, remap);

    }else if (c->type == PDDL_FM_ASSIGN || c->type == PDDL_FM_INCREASE){
        pddl_fm_func_op_t *a = PDDL_FM_CAST(c, func_op);
        if (a->lvalue != NULL)
            fmAtomRemapObjs(a->lvalue, remap);
        if (a->fvalue != NULL)
            fmAtomRemapObjs(a->fvalue, remap);
    }

    return 0;
}

void pddlFmRemapObjs(pddl_fm_t *c, const pddl_obj_id_t *remap)
{
    pddlFmTraverse(c, NULL, fmRemapObjs, (void *)remap);
}

static int atomIsInvalid(const pddl_fm_atom_t *a)
{
    for (int i = 0; i < a->arg_size; ++i){
        if (a->arg[i].param < 0 && a->arg[i].obj < 0)
            return 1;
    }
    return 0;
}

static int fmRemoveInvalidAtoms(pddl_fm_t **c, void *_)
{
    if ((*c)->type == PDDL_FM_ATOM){
        if (atomIsInvalid(OBJ(*c, atom))){
            pddlFmDel(*c);
            *c = NULL;
            return 0;
        }
    }else if ((*c)->type == PDDL_FM_ASSIGN
            || (*c)->type == PDDL_FM_INCREASE){
        pddl_fm_func_op_t *f = OBJ(*c, func_op);
        if (f->lvalue == NULL
                || atomIsInvalid(f->lvalue)
                || (f->fvalue != NULL && atomIsInvalid(f->fvalue))
                || (f->fvalue == NULL && f->value < 0)){
            pddlFmDel(*c);
            *c = NULL;
            return 0;
        }
    }
    return 0;
}

pddl_fm_t *pddlFmRemoveInvalidAtoms(pddl_fm_t *c)
{
    pddlFmRebuild(&c, NULL, fmRemoveInvalidAtoms, NULL);
    return c;
}

struct pred_remap {
    const int *pred_remap;
    const int *func_remap;
    int fail;
};

static int fmRemapPreds(pddl_fm_t *c, void *_remap)
{
    struct pred_remap *remap = _remap;
    if (c->type == PDDL_FM_ATOM){
        pddl_fm_atom_t *a = PDDL_FM_CAST(c, atom);
        if (remap->pred_remap[a->pred] < 0)
            remap->fail = 1;
        a->pred = remap->pred_remap[a->pred];

    }else if (c->type == PDDL_FM_ASSIGN){
        pddl_fm_func_op_t *a = PDDL_FM_CAST(c, func_op);
        if (a->lvalue != NULL){
            if (remap->func_remap[a->lvalue->pred] < 0)
                remap->fail = 1;
            a->lvalue->pred = remap->func_remap[a->lvalue->pred];
        }
        if (a->fvalue != NULL){
            if (remap->func_remap[a->fvalue->pred] < 0)
                remap->fail = 1;
            a->fvalue->pred = remap->func_remap[a->fvalue->pred];
        }
    }

    return 0;
}

int pddlFmRemapPreds(pddl_fm_t *c,
                     const int *pred_remap,
                     const int *func_remap)
{
    struct pred_remap remap = { pred_remap, func_remap, 0};
    pddlFmTraverse(c, NULL, fmRemapPreds, (void *)&remap);
    if (remap.fail)
        return -1;
    return 0;
}


/*** PRINT ***/
static void fmPartPrint(const pddl_t *pddl,
                        pddl_fm_junc_t *fm,
                        const char *name,
                        const pddl_params_t *params,
                        FILE *fout)
{
    pddl_list_t *item;
    const pddl_fm_t *child;

    fprintf(fout, "(%s", name);
    PDDL_LIST_FOR_EACH(&fm->part, item){
        child = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);
        fprintf(fout, " ");
        pddlFmPrint(pddl, child, params, fout);
    }
    fprintf(fout, ")");
}

static void fmQuantPrint(const pddl_t *pddl,
                         const pddl_fm_quant_t *q,
                         const char *name,
                         const pddl_params_t *params,
                         FILE *fout)
{
    fprintf(fout, "(%s", name);

    fprintf(fout, " (");
    pddlParamsPrint(&q->param, fout);
    fprintf(fout, ") ");

    pddlFmPrint(pddl, q->qfm, &q->param, fout);

    fprintf(fout, ")");
}

static void fmWhenPrint(const pddl_t *pddl,
                        const pddl_fm_when_t *w,
                        const pddl_params_t *params,
                        FILE *fout)
{
    fprintf(fout, "(when ");
    pddlFmPrint(pddl, w->pre, params, fout);
    fprintf(fout, " ");
    pddlFmPrint(pddl, w->eff, params, fout);
    fprintf(fout, ")");
}

static void fmAtomPrint(const pddl_t *pddl,
                        const pddl_fm_atom_t *atom,
                        const pddl_params_t *params,
                        FILE *fout, int is_func)
{
    const pddl_pred_t *pred;
    int i;

    if (is_func){
        pred = pddl->func.pred + atom->pred;
    }else{
        pred = pddl->pred.pred + atom->pred;
    }

    fprintf(fout, "(");
    if (atom->neg)
        fprintf(fout, "N:");
    if (pred->read)
        fprintf(fout, "R");
    if (pred->write)
        fprintf(fout, "W");
    fprintf(fout, ":%s", pred->name);

    for (i = 0; i < atom->arg_size; ++i){
        fprintf(fout, " ");
        if (atom->arg[i].param >= 0){
            if (params->param[atom->arg[i].param].name != NULL){
                fprintf(fout, "%s", params->param[atom->arg[i].param].name);
            }else{
                fprintf(fout, "x%d", atom->arg[i].param);
            }
        }else{
            fprintf(fout, "%s", pddl->obj.obj[atom->arg[i].obj].name);
        }
    }

    fprintf(fout, ")");
}

static void fmBoolPrint(const pddl_fm_bool_t *b, FILE *fout)
{
    if (b->val){
        fprintf(fout, "TRUE");
    }else{
        fprintf(fout, "FALSE");
    }
}

static void fmImplyPrint(const pddl_fm_imply_t *imp,
                         const pddl_t *pddl,
                         const pddl_params_t *params,
                         FILE *fout)

{
    fprintf(fout, "(imply ");
    if (imp->left)
        pddlFmPrint(pddl, imp->left, params, fout);
    fprintf(fout, " ");
    if (imp->right)
        pddlFmPrint(pddl, imp->right, params, fout);
    fprintf(fout, ")");
}

void pddlFmPrint(const struct pddl *pddl,
                 const pddl_fm_t *fm,
                 const pddl_params_t *params,
                 FILE *fout)
{
    if (fm->type == PDDL_FM_AND){
        fmPartPrint(pddl, OBJ(fm, junc), "and", params, fout);

    }else if (fm->type == PDDL_FM_OR){
        fmPartPrint(pddl, OBJ(fm, junc), "or", params, fout);

    }else if (fm->type == PDDL_FM_FORALL){
        fmQuantPrint(pddl, OBJ(fm, quant), "forall", params, fout);

    }else if (fm->type == PDDL_FM_EXIST){
        fmQuantPrint(pddl, OBJ(fm, quant), "exists", params, fout);

    }else if (fm->type == PDDL_FM_WHEN){
        fmWhenPrint(pddl, OBJ(fm, when), params, fout);

    }else if (fm->type == PDDL_FM_ATOM){
        fmAtomPrint(pddl, OBJ(fm, atom), params, fout, 0);

    }else if (fm->type == PDDL_FM_ASSIGN){
        fmFuncOpPrintPDDL(OBJ(fm, func_op), pddl, params, fout);

    }else if (fm->type == PDDL_FM_INCREASE){
        fmFuncOpPrintPDDL(OBJ(fm, func_op), pddl, params, fout);

    }else if (fm->type == PDDL_FM_BOOL){
        fmBoolPrint(OBJ(fm, bool), fout);

    }else if (fm->type == PDDL_FM_IMPLY){
        fmImplyPrint(OBJ(fm, imply), pddl, params, fout);

    }else{
        PANIC("Unknown type!");
    }
}

const char *pddlFmFmt(const pddl_fm_t *fm,
                      const pddl_t *pddl,
                      const pddl_params_t *params,
                      char *s,
                      size_t s_size)
{
    FILE *fout = fmemopen(s, s_size - 1, "w");
    pddlFmPrint(pddl, fm, params, fout);
    fflush(fout);
    if (ferror(fout) != 0 && s_size >= 4){
        s[s_size - 4] = '.';
        s[s_size - 3] = '.';
        s[s_size - 2] = '.';
    }
    fclose(fout);
    s[s_size - 1] = 0x0;
    return s;
}

void pddlFmPrintPDDL(const pddl_fm_t *fm,
                     const pddl_t *pddl,
                     const pddl_params_t *params,
                     FILE *fout)
{
    cond_cls[fm->type].print_pddl(fm, pddl, params, fout);
}

const char *pddlFmPDDLFmt(const pddl_fm_t *fm,
                          const pddl_t *pddl,
                          const pddl_params_t *params,
                          char *s,
                          size_t s_size)
{
    FILE *fout = fmemopen(s, s_size - 1, "w");
    pddlFmPrintPDDL(fm, pddl, params, fout);
    fflush(fout);
    if (ferror(fout) != 0 && s_size >= 4){
        s[s_size - 4] = '.';
        s[s_size - 3] = '.';
        s[s_size - 2] = '.';
    }
    fclose(fout);
    s[s_size - 1] = 0x0;
    return s;
}


const pddl_fm_t *pddlFmConstItInit(pddl_fm_const_it_t *it,
                                   const pddl_fm_t *fm,
                                   int type)
{
    ZEROIZE(it);

    if (fm == NULL)
        return NULL;

    if (type < 0 && fm->type != PDDL_FM_AND && fm->type != PDDL_FM_OR)
        return fm;

    if (fm->type == type)
        return fm;

    if (fm->type == PDDL_FM_AND || fm->type == PDDL_FM_OR){
        const pddl_fm_junc_t *p = PDDL_FM_CAST(fm, junc);
        it->list = &p->part;
        for (it->cur = pddlListNext((pddl_list_t *)it->list);
                it->cur != it->list;
                it->cur = pddlListNext((pddl_list_t *)it->cur)){
            const pddl_fm_t *c = PDDL_LIST_ENTRY(it->cur, pddl_fm_t, conn);
            if (type < 0 || c->type == type)
                return c;
        }
        return NULL;
    }

    return NULL;
}

const pddl_fm_t *pddlFmConstItNext(pddl_fm_const_it_t *it, int type)
{
    if (it->cur == it->list)
        return NULL;

    for (it->cur = pddlListNext((pddl_list_t *)it->cur);
            it->cur != it->list;
            it->cur = pddlListNext((pddl_list_t *)it->cur)){
        const pddl_fm_t *c = PDDL_LIST_ENTRY(it->cur, pddl_fm_t, conn);
        if (type < 0 || c->type == type)
            return c;
    }

    return NULL;
}

const pddl_fm_atom_t *pddlFmConstItAtomInit(pddl_fm_const_it_atom_t *it,
                                            const pddl_fm_t *fm)
{
    const pddl_fm_t *c;
    if ((c = pddlFmConstItInit(it, fm, PDDL_FM_ATOM)) == NULL)
        return NULL;
    return PDDL_FM_CAST(c, atom);
}

const pddl_fm_atom_t *pddlFmConstItAtomNext(pddl_fm_const_it_atom_t *it)
{
    const pddl_fm_t *c;
    if ((c = pddlFmConstItNext(it, PDDL_FM_ATOM)) == NULL)
        return NULL;
    return PDDL_FM_CAST(c, atom);
}

const pddl_fm_when_t *pddlFmConstItWhenInit(pddl_fm_const_it_when_t *it,
                                            const pddl_fm_t *fm)
{
    const pddl_fm_t *c;
    if ((c = pddlFmConstItInit(it, fm, PDDL_FM_WHEN)) == NULL)
        return NULL;
    return PDDL_FM_CAST(c, when);
}

const pddl_fm_when_t *pddlFmConstItWhenNext(pddl_fm_const_it_when_t *it)
{
    const pddl_fm_t *c;
    if ((c = pddlFmConstItNext(it, PDDL_FM_WHEN)) == NULL)
        return NULL;
    return PDDL_FM_CAST(c, when);
}



static const pddl_fm_t *constItEffNextCond(pddl_fm_const_it_eff_t *it,
                                           const pddl_fm_t **pre)
{
    if (pre != NULL)
        *pre = NULL;

    if (it->when_cur != NULL){
        it->when_cur = pddlListNext((pddl_list_t *)it->when_cur);
        if (it->when_cur == it->when_list){
            it->when_cur = it->when_list = NULL;
            it->when_pre = NULL;
        }else{
            if (pre != NULL)
                *pre = it->when_pre;
            return PDDL_LIST_ENTRY(it->when_cur, pddl_fm_t, conn);
        }
    }

    if (it->list == it->cur)
        return NULL;
    if (it->cur == NULL){
        it->cur = pddlListNext((pddl_list_t *)it->list);
    }else{
        it->cur = pddlListNext((pddl_list_t *)it->cur);
    }
    if (it->list == it->cur)
        return NULL;
    return PDDL_LIST_ENTRY(it->cur, pddl_fm_t, conn);
}

static const pddl_fm_atom_t *constItEffWhen(pddl_fm_const_it_eff_t *it,
                                            const pddl_fm_when_t *w,
                                            const pddl_fm_t **pre)
{
    if (w->eff == NULL){
        return NULL;

    }else if (w->eff->type == PDDL_FM_ATOM){
        if (pre != NULL)
            *pre = w->pre;
        return PDDL_FM_CAST(w->eff, atom);

    }else if (w->eff->type == PDDL_FM_AND){
        const pddl_fm_junc_t *p = PDDL_FM_CAST(w->eff, junc);
        it->when_pre = w->pre;
        it->when_list = &p->part;
        it->when_cur = it->when_list;
        return NULL;

    }else{
        ASSERT_RUNTIME_M(
                         w->eff->type != PDDL_FM_OR
                         && w->eff->type != PDDL_FM_FORALL
                         && w->eff->type != PDDL_FM_EXIST
                         && w->eff->type != PDDL_FM_IMPLY
                         && w->eff->type != PDDL_FM_WHEN,
                         "Effect is not normalized.");
    }
    return NULL;
}

const pddl_fm_atom_t *pddlFmConstItEffInit(pddl_fm_const_it_eff_t *it,
                                           const pddl_fm_t *fm,
                                           const pddl_fm_t **pre)
{
    ZEROIZE(it);

    if (pre != NULL)
        *pre = NULL;
    if (fm == NULL)
        return NULL;

    if (fm->type == PDDL_FM_ATOM){
        return PDDL_FM_CAST(fm, atom);

    }else if (fm->type == PDDL_FM_AND){
        const pddl_fm_junc_t *p = PDDL_FM_CAST(fm, junc);
        it->list = &p->part;
        return pddlFmConstItEffNext(it, pre);

    }else if (fm->type == PDDL_FM_WHEN){
        const pddl_fm_when_t *w = PDDL_FM_CAST(fm, when);
        const pddl_fm_atom_t *a = constItEffWhen(it, w, pre);
        if (a != NULL)
            return a;
        return pddlFmConstItEffNext(it, pre);

    }else{
        ASSERT_RUNTIME_M(
                         fm->type != PDDL_FM_OR
                         && fm->type != PDDL_FM_FORALL
                         && fm->type != PDDL_FM_EXIST
                         && fm->type != PDDL_FM_IMPLY,
                         "Effect is not normalized.");
    }
    return NULL;
}

const pddl_fm_atom_t *pddlFmConstItEffNext(pddl_fm_const_it_eff_t *it,
                                           const pddl_fm_t **pre)
{
    const pddl_fm_t *c;

    while (1){
        c = constItEffNextCond(it, pre);
        if (c == NULL){
            return NULL;

        }else if (c->type == PDDL_FM_ATOM){
            return PDDL_FM_CAST(c, atom);

        }else if (c->type == PDDL_FM_WHEN){
            const pddl_fm_when_t *w = PDDL_FM_CAST(c, when);
            const pddl_fm_atom_t *a = constItEffWhen(it, w, pre);
            if (a != NULL)
                return a;
        }else{
            ASSERT_RUNTIME_M(
                             c->type != PDDL_FM_AND
                             && c->type != PDDL_FM_OR
                             && c->type != PDDL_FM_FORALL
                             && c->type != PDDL_FM_EXIST
                             && c->type != PDDL_FM_IMPLY
                             && c->type != PDDL_FM_WHEN,
                             "Effect is not normalized.");
        }
    }
}
