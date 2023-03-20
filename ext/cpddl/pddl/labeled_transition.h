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

#ifndef __PDDL_LABELED_TRANSITION_H__
#define __PDDL_LABELED_TRANSITION_H__

#include <pddl/transition.h>
#include <pddl/label.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct pddl_labeled_transitions {
    pddl_label_set_t *label;
    pddl_transitions_t trans;
};
typedef struct pddl_labeled_transitions pddl_labeled_transitions_t;

struct pddl_labeled_transitions_set {
    pddl_labeled_transitions_t *trans;
    int trans_size;
    int trans_alloc;
};
typedef struct pddl_labeled_transitions_set pddl_labeled_transitions_set_t;

#define PDDL_LABELED_TRANSITIONS_SET_FOR_EACH(TR, FROM, LABEL, TO) \
    for (int ___ti = 0; ___ti < (TR)->trans_size; ++___ti) \
        for (int ___tri = 0; \
                ___tri < (TR)->trans[___ti].trans.trans_size && \
                    ((FROM) = (TR)->trans[___ti].trans.trans[___tri].from, \
                    (TO) = (TR)->trans[___ti].trans.trans[___tri].to, 1); \
                ++___tri) \
            PDDL_ISET_FOR_EACH(&(TR)->trans[___ti].label->label, LABEL)

#define PDDL_LABELED_TRANSITIONS_SET_FOR_EACH_LABEL_SET(TR, FROM, \
                                                        LABEL_SET_PTR, TO) \
    for (int ___ti = 0; ___ti < (TR)->trans_size; ++___ti) \
        for (int ___tri = 0; \
                ___tri < (TR)->trans[___ti].trans.trans_size && \
                    ((FROM) = (TR)->trans[___ti].trans.trans[___tri].from, \
                    (TO) = (TR)->trans[___ti].trans.trans[___tri].to, \
                    (LABEL_SET_PTR) = (TR)->trans[___ti].label, 1); \
                ++___tri)


/**
 * Initializes empty set of labeled transitions
 */
void pddlLabeledTransitionsSetInit(pddl_labeled_transitions_set_t *t);

/**
 * Free allocated memory.
 */
void pddlLabeledTransitionsSetFree(pddl_labeled_transitions_set_t *t);

/**
 * Adds and returns a struct corresponding to the given label.
 */
pddl_labeled_transitions_t *
    pddlLabeledTransitionsSetAddLabel(pddl_labeled_transitions_set_t *t,
                                      pddl_label_set_t *label,
                                      int *added);

/**
 * Adds transitions (from, to) with the given label.
 * Returns 1 if the label was already there, 0 otherwise.
 */
int pddlLabeledTransitionsSetAdd(pddl_labeled_transitions_set_t *t,
                                 pddl_label_set_t *label,
                                 int from,
                                 int to);

/**
 * Sort labeled transitions.
 */
void pddlLabeledTransitionsSetSort(pddl_labeled_transitions_set_t *t);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_LABELED_TRANSITION_H__ */
