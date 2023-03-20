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

#ifndef __PDDL_LISP_H__
#define __PDDL_LISP_H__

#include <pddl/err.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef enum {
    PDDL_KW_DEFINE = 1,
    PDDL_KW_DOMAIN,
    PDDL_KW_DOMAIN2,
    PDDL_KW_REQUIREMENTS,
    PDDL_KW_TYPES,
    PDDL_KW_CONSTANTS,
    PDDL_KW_PREDICATES,
    PDDL_KW_ACTION,
    PDDL_KW_PARAMETERS,
    PDDL_KW_PRE,
    PDDL_KW_EFF,

    PDDL_KW_STRIPS,
    PDDL_KW_TYPING,
    PDDL_KW_NEGATIVE_PRE,
    PDDL_KW_DISJUNCTIVE_PRE,
    PDDL_KW_EQUALITY,
    PDDL_KW_EXISTENTIAL_PRE,
    PDDL_KW_UNIVERSAL_PRE,
    PDDL_KW_CONDITIONAL_EFF,
    PDDL_KW_NUMERIC_FLUENT,
    PDDL_KW_OBJECT_FLUENT,
    PDDL_KW_DURATIVE_ACTION,
    PDDL_KW_DURATION_INEQUALITY,
    PDDL_KW_CONTINUOUS_EFF,
    PDDL_KW_DERIVED_PRED,
    PDDL_KW_TIMED_INITIAL_LITERAL,
    PDDL_KW_PREFERENCE,
    PDDL_KW_CONSTRAINT,
    PDDL_KW_ACTION_COST,
    PDDL_KW_MULTI_AGENT,
    PDDL_KW_UNFACTORED_PRIVACY,
    PDDL_KW_FACTORED_PRIVACY,
    PDDL_KW_QUANTIFIED_PRE,
    PDDL_KW_FLUENTS,
    PDDL_KW_ADL,

    PDDL_KW_FUNCTIONS,
    PDDL_KW_NUMBER,
    PDDL_KW_PROBLEM,
    PDDL_KW_OBJECTS,
    PDDL_KW_INIT,
    PDDL_KW_GOAL,
    PDDL_KW_METRIC,
    PDDL_KW_MINIMIZE,
    PDDL_KW_MAXIMIZE,
    PDDL_KW_INCREASE,

    PDDL_KW_AND,
    PDDL_KW_OR,
    PDDL_KW_NOT,
    PDDL_KW_IMPLY,
    PDDL_KW_EXISTS,
    PDDL_KW_FORALL,
    PDDL_KW_WHEN,
    PDDL_KW_EITHER,

    PDDL_KW_PRIVATE,
    PDDL_KW_AGENT,

    PDDL_KW_DERIVED,
} pddl_kw_t;

typedef struct pddl_lisp_node pddl_lisp_node_t;
struct pddl_lisp_node {
    const char *value;
    int kw;
    int lineno;
    pddl_lisp_node_t *child;
    int child_size;
};

struct pddl_lisp {
    char *filename;
    pddl_lisp_node_t root;
    char *data;
    size_t size;
};
typedef struct pddl_lisp pddl_lisp_t;

/**
 * Parses the input file and returns the parsed pddl-lisp object.
 */
pddl_lisp_t *pddlLispParse(const char *fn, pddl_err_t *err);

/**
 * Deep-clones lisp structure.
 */
pddl_lisp_t *pddlLispClone(const pddl_lisp_t *src);

/**
 * Deletes pddl-lisp object.
 */
void pddlLispDel(pddl_lisp_t *lisp);

/**
 * Prints pddl-lisp object to the specified output.
 */
void pddlLispPrintDebug(const pddl_lisp_t *lisp, FILE *fout);

/**
 * Returns root's child with the specified head keyword.
 */
const pddl_lisp_node_t *pddlLispFindNode(
            const pddl_lisp_node_t *root, int kw);


/**
 * Callback for pddlLispParseTypedList().
 */
typedef int (*pddl_lisp_parse_typed_list_fn)(
                const pddl_lisp_node_t *root,
                int child_from, int child_to, int child_type, void *ud,
                pddl_err_t *err);

/**
 * Parse typed list.
 */
int pddlLispParseTypedList(const pddl_lisp_node_t *root,
                           int child_from, int child_to,
                           pddl_lisp_parse_typed_list_fn cb,
                           void *ud,
                           pddl_err_t *err);

/**
 * Copy pddl-lisp-node from src to dst.
 */
void pddlLispNodeInitCopy(pddl_lisp_node_t *dst, const pddl_lisp_node_t *src);

/**
 * Frees pddl-lisp-node -- use it as pair function to *Copy().
 */
void pddlLispNodeFree(pddl_lisp_node_t *node);

/**
 * Returns the value of the first sub-element of the specified node.
 */
_pddl_inline const char *pddlLispNodeHead(const pddl_lisp_node_t *n);

/**
 * Simliar to pddlLispNodeHead() but returns the keyword corresponding
 * to the head value.
 */
_pddl_inline int pddlLispNodeHeadKw(const pddl_lisp_node_t *n);

/**
 * Returns true if the node is () or (and)
 */
_pddl_inline int pddlLispNodeIsEmptyAnd(const pddl_lisp_node_t *n);

/**** INLINES: ****/
_pddl_inline const char *pddlLispNodeHead(const pddl_lisp_node_t *n)
{
    if (n->child_size == 0)
        return NULL;
    return n->child[0].value;
}

_pddl_inline int pddlLispNodeHeadKw(const pddl_lisp_node_t *n)
{
    if (n->child_size == 0)
        return -1;
    return n->child[0].kw;
}

_pddl_inline int pddlLispNodeIsEmptyAnd(const pddl_lisp_node_t *n)
{
    return n->child_size == 0
                || (n->child_size == 1
                        && n->child[0].child_size == 0
                        && n->child[0].value != NULL
                        && strcmp(n->child[0].value, "and") == 0);
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __PDDL_LISP_H__ */
