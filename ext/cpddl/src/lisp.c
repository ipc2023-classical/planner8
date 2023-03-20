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
#include "pddl/lisp.h"
#include "lisp_err.h"


#define IS_WS(c) ((c) == ' ' || (c) == '\n' || (c) == '\r' || (c) == '\t')
#define IS_ALPHA(c) (!IS_WS(c) && (c) != ')' && (c) != '(' && (c) != ';')

struct _kw_t {
    const char *text;
    int kw;
};
typedef struct _kw_t kw_t;

static kw_t kw[] = {
    { "define", PDDL_KW_DEFINE },
    { "domain", PDDL_KW_DOMAIN },
    { ":domain", PDDL_KW_DOMAIN2 },
    { ":requirements", PDDL_KW_REQUIREMENTS },
    { ":types", PDDL_KW_TYPES },
    { ":predicates", PDDL_KW_PREDICATES },
    { ":constants", PDDL_KW_CONSTANTS },
    { ":action", PDDL_KW_ACTION },
    { ":parameters", PDDL_KW_PARAMETERS },
    { ":precondition", PDDL_KW_PRE },
    { ":effect", PDDL_KW_EFF },
    { ":derived", PDDL_KW_DERIVED },

    { ":strips", PDDL_KW_STRIPS },
    { ":typing", PDDL_KW_TYPING },
    { ":negative-preconditions", PDDL_KW_NEGATIVE_PRE },
    { ":disjunctive-preconditions", PDDL_KW_DISJUNCTIVE_PRE },
    { ":equality", PDDL_KW_EQUALITY },
    { ":existential-preconditions", PDDL_KW_EXISTENTIAL_PRE },
    { ":universal-preconditions", PDDL_KW_UNIVERSAL_PRE },
    { ":conditional-effects", PDDL_KW_CONDITIONAL_EFF },
    { ":numeric-fluents", PDDL_KW_NUMERIC_FLUENT },
    { ":numeric-fluents", PDDL_KW_OBJECT_FLUENT },
    { ":durative-actions", PDDL_KW_DURATIVE_ACTION },
    { ":duration-inequalities", PDDL_KW_DURATION_INEQUALITY },
    { ":continuous-effects", PDDL_KW_CONTINUOUS_EFF },
    { ":derived-predicates", PDDL_KW_DERIVED_PRED },
    { ":timed-initial-literals", PDDL_KW_TIMED_INITIAL_LITERAL },
    { ":durative-actions", PDDL_KW_DURATIVE_ACTION },
    { ":preferences", PDDL_KW_PREFERENCE },
    { ":constraints", PDDL_KW_CONSTRAINT },
    { ":action-costs", PDDL_KW_ACTION_COST },
    { ":multi-agent", PDDL_KW_MULTI_AGENT },
    { ":unfactored-privacy", PDDL_KW_UNFACTORED_PRIVACY },
    { ":factored_privacy", PDDL_KW_FACTORED_PRIVACY },
    { ":quantified-preconditions", PDDL_KW_QUANTIFIED_PRE },
    { ":fluents", PDDL_KW_FLUENTS },
    { ":adl", PDDL_KW_ADL },
    { ":multi-agent", PDDL_KW_MULTI_AGENT },
    { ":unfactored-privacy", PDDL_KW_UNFACTORED_PRIVACY },
    { ":factored-privacy", PDDL_KW_FACTORED_PRIVACY },

    { ":functions", PDDL_KW_FUNCTIONS },
    { "number", PDDL_KW_NUMBER },
    { "problem", PDDL_KW_PROBLEM },
    { ":objects", PDDL_KW_OBJECTS },
    { ":init", PDDL_KW_INIT },
    { ":goal", PDDL_KW_GOAL },
    { ":metric", PDDL_KW_METRIC },
    { "minimize", PDDL_KW_MINIMIZE },
    { "maximize", PDDL_KW_MAXIMIZE },
    { "increase", PDDL_KW_INCREASE },

    { "and", PDDL_KW_AND },
    { "or", PDDL_KW_OR },
    { "not", PDDL_KW_NOT },
    { "imply", PDDL_KW_IMPLY },
    { "exists", PDDL_KW_EXISTS },
    { "forall", PDDL_KW_FORALL },
    { "when", PDDL_KW_WHEN },
    { "either", PDDL_KW_EITHER },

    { ":private", PDDL_KW_PRIVATE },
    { ":agent", PDDL_KW_AGENT },
};
static int kw_size = sizeof(kw) / sizeof(kw_t);

static int recongnizeKeyword(const char *text)
{
    for (int i = 0; i < kw_size; ++i){
        if (strcmp(text, kw[i].text) == 0)
            return kw[i].kw;
    }

    return -1;
}

static pddl_lisp_node_t *lispNodeInit(pddl_lisp_node_t *n)
{
    n->value = NULL;
    n->kw = -1;
    n->lineno = -1;
    n->child = NULL;
    n->child_size = 0;
    return n;
}

static void lispNodeFree(pddl_lisp_node_t *n)
{
    for (int i = 0; i < n->child_size; ++i)
        lispNodeFree(n->child + i);
    if (n->child != NULL)
        FREE(n->child);
}

static pddl_lisp_node_t *lispNodeAddChild(pddl_lisp_node_t *r)
{
    pddl_lisp_node_t *n;

    ++r->child_size;
    r->child = REALLOC_ARR(r->child, pddl_lisp_node_t,
                           r->child_size);
    n = r->child + r->child_size - 1;
    lispNodeInit(n);
    return n;
}

static int parseExp(const char *fn, pddl_lisp_node_t *root, int *lineno,
                    char *data, int from, int size, int *cont,
                    pddl_err_t *err)
{
    pddl_lisp_node_t *sub;
    int i = from;
    char c;

    if (i >= size){
        PDDL_ERR_RET(err, -1, "Invalid PDDL file `%s'."
                     " Mission expression on line %d.",
                     fn, *lineno);
    }

    c = data[i];
    while (i < size){
        if (IS_WS(c)){
            // Skip whitespace
            if (c == '\n')
                *lineno += 1;
            c = data[++i];
            continue;

        }else if (c == ';'){
            // Skip comments
            for (++i; i < size && data[i] != '\n'; ++i);
            if (i < size)
                c = data[i];
            continue;

        }else if (c == '('){
            // Parse subexpression
            sub = lispNodeAddChild(root);
            sub->lineno = *lineno;
            if (parseExp(fn, sub, lineno, data, i + 1, size, &i, err) != 0)
                PDDL_TRACE_RET(err, -1);

            c = data[i];
            continue;

        }else if (c == ')'){
            // Finalize expression
            if (cont != NULL)
                *cont = i + 1;
            return 0;

        }else{
            sub = lispNodeAddChild(root);
            sub->value = data + i;
            sub->lineno = *lineno;
            for (; i < size && IS_ALPHA(data[i]); ++i){
                if (data[i] >= 'A' && data[i] <= 'Z')
                    data[i] = data[i] - 'A' + 'a';
            }

            c = data[i];
            data[i] = 0x0;
            sub->kw = recongnizeKeyword(sub->value);
        }
    }
    PDDL_ERR_RET(err, -1, "Invalid PDDL file `%s'."
                 " Missing ending parenthesis.", fn);
}

pddl_lisp_t *pddlLispParse(const char *fn, pddl_err_t *err)
{
    int fd, i, lineno;
    struct stat st;
    char *data;
    pddl_lisp_t *lisp;
    pddl_lisp_node_t root;

    fd = open(fn, O_RDONLY);
    if (fd == -1){
        perror("");
        PDDL_ERR_RET(err, NULL, "Could not not open file `%s'.", fn);
    }

    if (fstat(fd, &st) != 0){
        PDDL_ERR(err, "Could not determine size of the file `%s'.", fn);
        close(fd);
        return NULL;
    }

    data = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    close(fd);
    if (data == MAP_FAILED){
        PDDL_ERR(err, "Could not mmap file `%s'.", fn);
        return NULL;
    }

    lineno = 1;
    // skip initial whitespace and comments
    for (i = 0; i < (int)st.st_size && data[i] != '('; ++i){
        if (data[i] == ';')
            for (; i < (int)st.st_size && data[i] != '\n'; ++i);
        if (data[i] == '\n'){
            ++lineno;
        }else if (!IS_WS(data[i])){
            PDDL_ERR(err, "Incorrect PDDL file `%s'. Unexpected `%c' on line %d.",
                     fn, data[i], lineno);
            munmap((void *)data, st.st_size);
            return NULL;
        }
    }
    lispNodeInit(&root);
    root.lineno = lineno;
    if (parseExp(fn, &root, &lineno, data, i + 1, st.st_size, NULL, err) != 0){
        PDDL_TRACE(err);
        munmap((void *)data, st.st_size);
        lispNodeFree(&root);
        return NULL;
    }

    lisp = ALLOC(pddl_lisp_t);
    lisp->filename = STRDUP(fn);
    lisp->data = data;
    lisp->size = st.st_size;
    lisp->root = root;

    return lisp;
}

static void remapLispNodeValues(pddl_lisp_node_t *n,
                                char *new_data,
                                const char *old_data)
{
    if (n->value != NULL)
        n->value = new_data + (n->value - old_data);

    for (int i = 0; i < n->child_size; ++i)
        remapLispNodeValues(n->child + i, new_data, old_data);
}

pddl_lisp_t *pddlLispClone(const pddl_lisp_t *src)
{
    pddl_lisp_t *lisp = ZALLOC(pddl_lisp_t);
    if (src->filename)
        lisp->filename = STRDUP(src->filename);
    pddlLispNodeInitCopy(&lisp->root, &src->root);
    lisp->size = src->size;
    lisp->data = mmap(NULL, src->size, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANON, -1, 0);
    if (lisp->data == MAP_FAILED)
        PANIC("Error: Could not allocated enough memory.");
    memcpy(lisp->data, src->data, src->size);
    remapLispNodeValues(&lisp->root, lisp->data, src->data);
    return lisp;
}

void pddlLispDel(pddl_lisp_t *lisp)
{
    if (lisp->filename)
        FREE(lisp->filename);
    if (lisp->data != NULL)
        munmap((void *)lisp->data, lisp->size);

    lispNodeFree(&lisp->root);
    FREE(lisp);
}

static void nodeDebug(const pddl_lisp_node_t *node, FILE *fout, int prefix)
{
    for (int i = 0; i < prefix; ++i)
        fprintf(fout, " ");

    if (node->value != NULL){
        fprintf(fout, "%s [%d] :: %d\n", node->value, node->kw,
                node->lineno);
    }else{
        fprintf(fout, "( :: %d\n", node->lineno);

        for (int i = 0; i < node->child_size; ++i)
            nodeDebug(node->child + i, fout, prefix + 4);

        for (int i = 0; i < prefix; ++i)
            fprintf(fout, " ");
        fprintf(fout, ")\n");
    }
}

void pddlLispPrintDebug(const pddl_lisp_t *lisp, FILE *fout)
{
    nodeDebug(&lisp->root, fout, 0);
}

void pddlLispNodeInitCopy(pddl_lisp_node_t *dst, const pddl_lisp_node_t *src)
{
    lispNodeInit(dst);
    dst->value = src->value;
    dst->kw = src->kw;
    dst->lineno = src->lineno;

    dst->child_size = src->child_size;
    dst->child = ALLOC_ARR(pddl_lisp_node_t, dst->child_size);
    for (int i = 0; i < dst->child_size; ++i){
        lispNodeInit(dst->child + i);
        pddlLispNodeInitCopy(dst->child + i, src->child + i);
    }
}

void pddlLispNodeFree(pddl_lisp_node_t *node)
{
    lispNodeFree(node);
}

const pddl_lisp_node_t *pddlLispFindNode(
            const pddl_lisp_node_t *root, int kw)
{
    for (int i = 0; i < root->child_size; ++i){
        if (pddlLispNodeHeadKw(root->child + i) == kw)
            return root->child + i;
    }
    return NULL;
}

int pddlLispParseTypedList(const pddl_lisp_node_t *root,
                           int child_from, int child_to,
                           pddl_lisp_parse_typed_list_fn cb,
                           void *ud,
                           pddl_err_t *err)
{
    pddl_lisp_node_t *n;
    int type_from;

    type_from = child_from;
    for (int i = child_from; i < child_to; ++i){
        n = root->child + i;

        if (n->value == NULL)
            ERR_LISP_RET2(err, -1, n, "Invalid typed list. Unexpected token");

        if (strcmp(n->value, "-") == 0){
            if (type_from == i)
                ERR_LISP_RET2(err, -1, n, "Invalid typed list. Unexpected `-'");
            if (i + 1 >= child_to)
                ERR_LISP_RET2(err, -1, n,
                              "Invalid typed list. Unspecified type after `-'");
            if (cb(root, type_from, i, i + 1, ud, err) != 0)
                PDDL_TRACE_RET(err, -1);
            type_from = i + 2;
            ++i;
        }
    }

    if (type_from < child_to){
        if (cb(root, type_from, child_to, -1, ud, err) != 0)
            PDDL_TRACE_RET(err, -1);
    }

    return 0;
}
