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

#ifndef __PDDL_LABEL_H__
#define __PDDL_LABEL_H__

#include <pddl/strips_op.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct pddl_label {
    int cost;
    int op_id;
};
typedef struct pddl_label pddl_label_t;

struct pddl_label_set {
    pddl_iset_t label; /*!< Set of labels */
    int cost; /*!< Cost is the minimum over labels' costs */
    int ref; /*!< Reference counter */
    pddl_htable_key_t key; /*!< Key to hashtable */
    pddl_list_t htable; /*!< Connector to the hashtable */
};
typedef struct pddl_label_set pddl_label_set_t;

struct pddl_labels {
    pddl_label_t *label;
    int label_size;
    int label_alloc;
    pddl_htable_t *label_set;
};
typedef struct pddl_labels pddl_labels_t;

void pddlLabelsInitFromStripsOps(pddl_labels_t *lbs,
                                 const pddl_strips_ops_t *ops);
void pddlLabelsFree(pddl_labels_t *lbs);

/**
 * Adds a set of labels if not already there and returns reference to the
 * added set.
 */
pddl_label_set_t *pddlLabelsAddSet(pddl_labels_t *lbs, const pddl_iset_t *lbls);

/**
 * Dereference the given set of labels.
 */
void pddlLabelsSetDecRef(pddl_labels_t *lbs, pddl_label_set_t *set);

/**
 * Increase reference the given set of labels.
 */
void pddlLabelsSetIncRef(pddl_labels_t *lbs, pddl_label_set_t *set);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_LABEL_H__ */
