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

#ifndef __PDDL_OUTBOX_H__
#define __PDDL_OUTBOX_H__

#include <pddl/err.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct pddl_outbox {
    char **line;
    int line_size;
    int line_alloc;
    int max_line_len;
};
typedef struct pddl_outbox pddl_outbox_t;

struct pddl_outboxes {
    pddl_outbox_t *box;
    int box_size;
    int box_alloc;
};
typedef struct pddl_outboxes pddl_outboxes_t;

void pddlOutBoxesInit(pddl_outboxes_t *b);
void pddlOutBoxesFree(pddl_outboxes_t *b);
pddl_outbox_t *pddlOutBoxesAdd(pddl_outboxes_t *b);
void pddlOutBoxAddLine(pddl_outbox_t *pddl_outbox, const char *line);
void pddlOutBoxesMerge(pddl_outboxes_t *dst,
                       const pddl_outboxes_t *src,
                       int max_len);
void pddlOutBoxesPrint(const pddl_outboxes_t *b, FILE *fout, pddl_err_t *err);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_OUTBOX_H__ */
