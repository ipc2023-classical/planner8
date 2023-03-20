/***
 * cpddl
 * -------
 * Copyright (c)2018 Daniel Fiser <danfis@danfis.cz>,
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
#include "pddl/sort.h"
#include "pddl/fdr_app_op.h"
#include "pddl/cg.h"

/**
 * Base building structure for a tree node.
 */
struct pddl_fdr_app_op_tree {
    int var; /*!< Decision variable */
    pddl_iset_t ops; /*!< List of immediate operators that are returned once
                       this node is reached */
    struct pddl_fdr_app_op_tree **val; /*!< Subtrees indexed by the value of
                                         the decision variable */
    int val_size;
    struct pddl_fdr_app_op_tree *def; /*!< Default subtree containing operators
                                        without precondition on the decision
                                        variable */
};
typedef struct pddl_fdr_app_op_tree pddl_fdr_app_op_tree_t;

/** Creates a new tree node (and recursively all subtrees) */
static pddl_fdr_app_op_tree_t *treeNew(const int *op_ids, int len,
                                       const int *var,
                                       const pddl_fdr_ops_t *ops);

static int opsSortCmp(const void *a, const void *b, void *ud)
{
    const pddl_fdr_app_op_t *s = ud;
    int op_id1 = *(const int *)a;
    int op_id2 = *(const int *)b;
    const pddl_fdr_op_t *opa = s->ops->op[op_id1];
    const pddl_fdr_op_t *opb = s->ops->op[op_id2];
    const pddl_fdr_part_state_t *prea = &opa->pre;
    const pddl_fdr_part_state_t *preb = &opb->pre;

    for (int i = 0; i < s->var_size; ++i){
        int var = s->var_order[i];
        int aset = pddlFDRPartStateIsSet(prea, var);
        int bset = pddlFDRPartStateIsSet(preb, var);

        if (aset && bset){
            int aval = pddlFDRPartStateGet(prea, var);
            int bval = pddlFDRPartStateGet(preb, var);

            if (aval < bval){
                return -1;
            }else if (aval > bval){
                return 1;
            }
        }else if (!aset && bset){
            return -1;
        }else if (aset && !bset){
            return 1;
        }
    }

    // make the sort stable
    if (op_id1 < op_id2)
        return -1;
    return 1;
}

static int *sortedOps(const pddl_fdr_app_op_t *app)
{
    int *op_ids;

    op_ids = ALLOC_ARR(int, app->ops->op_size);
    for (int i = 0; i < app->ops->op_size; ++i)
        op_ids[i] = i;

    pddlSort(op_ids, app->ops->op_size, sizeof(int), opsSortCmp, (void *)app);
    return op_ids;
}

static void treeBuildSetOps(pddl_fdr_app_op_tree_t *tree, const int *ops, int len)
{
    pddlISetEmpty(&tree->ops);
    for (int i = 0; i < len; ++i)
        pddlISetAdd(&tree->ops, ops[i]);
}

static int treeBuildDef(pddl_fdr_app_op_tree_t *tree,
                        const int *op_ids, int len,
                        const int *var,
                        const pddl_fdr_ops_t *ops)
{
    int size;

    for (size = 1;
         size < len
            && !pddlFDRPartStateIsSet(&ops->op[op_ids[size]]->pre, *var);
         ++size);

    tree->var = *var;
    tree->def = treeNew(op_ids, size, var + 1, ops);

    return size;
}

static void treeBuildPrepareVal(pddl_fdr_app_op_tree_t *tree, int val)
{
    int i;

    tree->val_size = val + 1;
    tree->val = ALLOC_ARR(pddl_fdr_app_op_tree_t *, tree->val_size);

    for (i = 0; i < tree->val_size; ++i)
        tree->val[i] = NULL;
}

static int treeBuildVal(pddl_fdr_app_op_tree_t *tree,
                        const int *op_ids, int len,
                        const int *var,
                        const pddl_fdr_ops_t *ops)
{
    int val = pddlFDRPartStateGet(&ops->op[op_ids[0]]->pre, *var);

    int size;
    for (size = 1;
         size < len
            && pddlFDRPartStateGet(&ops->op[op_ids[size]]->pre, *var) == val;
         ++size);

    tree->var = *var;
    tree->val[val] = treeNew(op_ids, size, var + 1, ops);

    return size;
}

static pddl_fdr_app_op_tree_t *treeNew(const int *op_ids, int len,
                                       const int *var,
                                       const pddl_fdr_ops_t *ops)
{
    pddl_fdr_app_op_tree_t *tree;
    const pddl_fdr_op_t *last_op = ops->op[op_ids[len - 1]];
    const pddl_fdr_part_state_t *last_pre = &last_op->pre;
    const pddl_fdr_op_t *first_op = ops->op[op_ids[0]];
    const pddl_fdr_part_state_t *first_pre = &first_op->pre;
    int start;

    tree = ALLOC(pddl_fdr_app_op_tree_t);
    tree->var = -1;
    pddlISetInit(&tree->ops);
    tree->val = NULL;
    tree->val_size = 0;
    tree->def = NULL;

    if (len == 0)
        return tree;

    // Find first variable that is set for at least one operator.
    // The operators are sorted so that it is enough to check the last
    // operator in the array.
    for (; *var != -1; ++var){
        if (pddlFDRPartStateIsSet(last_pre, *var))
            break;
    }

    if (*var == -1){
        // If there isn't any operator with set value anymore insert all
        // operators as immediate ops and exit.
        treeBuildSetOps(tree, op_ids, len);
        return tree;
    }

    // Now we know that array of operators contain at least one operator
    // with set value of current variable.

    // Prepare val array -- we now that the last operator in array has
    // largest value.
    treeBuildPrepareVal(tree, pddlFDRPartStateGet(last_pre, *var));

    // Initialize index of the first element with current value
    start = 0;

    // First check unset values from the beggining of the array
    if (!pddlFDRPartStateIsSet(first_pre, *var)){
        start = treeBuildDef(tree, op_ids, len, var, ops);
    }

    // Then build subtree for each value
    while (start < len){
        start += treeBuildVal(tree, op_ids + start, len - start, var, ops);
    }

    return tree;
}

static void treeDel(pddl_fdr_app_op_tree_t *tree)
{
    int i;

    pddlISetFree(&tree->ops);
    if (tree->val){
        for (i = 0; i < tree->val_size; ++i)
            if (tree->val[i])
                treeDel(tree->val[i]);
        FREE(tree->val);
    }

    if (tree->def)
        treeDel(tree->def);

    FREE(tree);
}

void pddlFDRAppOpInit(pddl_fdr_app_op_t *app,
                      const pddl_fdr_vars_t *vars,
                      const pddl_fdr_ops_t *ops,
                      const pddl_fdr_part_state_t *goal)
{
    int *sorted_ops = NULL;

    app->ops = ops;

    app->var_size = vars->var_size;
    app->var_order = ALLOC_ARR(int, vars->var_size + 1);
    pddl_cg_t cg;
    pddlCGInit(&cg, vars, ops, 0);
    pddlCGVarOrdering(&cg, goal, app->var_order);
    pddlCGFree(&cg);
    app->var_order[vars->var_size] = -1;

    if (ops->op_size > 0)
        sorted_ops = sortedOps(app);

    app->root = treeNew(sorted_ops, ops->op_size, app->var_order, ops);

    if (sorted_ops)
        FREE(sorted_ops);
}

void pddlFDRAppOpFree(pddl_fdr_app_op_t *app)
{
    if (app->root)
        treeDel(app->root);
    if (app->var_order != NULL)
        FREE(app->var_order);
}

static int treeFind(const pddl_fdr_app_op_tree_t *tree,
                    const int *vals,
                    pddl_iset_t *ops)
{
    // insert all immediate operators
    pddlISetUnion(ops, &tree->ops);
    int found = pddlISetSize(&tree->ops);

    // check whether this node should check on any variable value
    if (tree->var != -1){
        // get corresponding value from state
        int val = vals[tree->var];

        // and use tree corresponding to the value if present
        if (val != -1
                && val < tree->val_size
                && tree->val[val]){
            found += treeFind(tree->val[val], vals, ops);
        }

        // use default tree if present
        if (tree->def)
            found += treeFind(tree->def, vals, ops);
    }

    return found;
}

int pddlFDRAppOpFind(const pddl_fdr_app_op_t *app,
                     const int *state,
                     pddl_iset_t *ops)
{
    if (app->root == NULL)
        return 0;
    return treeFind(app->root, state, ops);
}
