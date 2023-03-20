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

#include "pddl/outbox.h"
#include "internal.h"

void pddlOutBoxesInit(pddl_outboxes_t *b)
{
    ZEROIZE(b);
}

void pddlOutBoxesFree(pddl_outboxes_t *b)
{
    for (int i = 0; i < b->box_size; ++i){
        for (int l = 0; l < b->box[i].line_size; ++l)
            FREE(b->box[i].line[l]);
        if (b->box[i].line != NULL)
            FREE(b->box[i].line);
    }
    if (b->box != NULL)
        FREE(b->box);
}

pddl_outbox_t *pddlOutBoxesAdd(pddl_outboxes_t *b)
{
    if (b->box_size == b->box_alloc){
        if (b->box_alloc == 0)
            b->box_alloc = 2;
        b->box_alloc *= 2;
        b->box = REALLOC_ARR(b->box, pddl_outbox_t, b->box_alloc);
    }
    pddl_outbox_t *box = b->box + b->box_size++;
    ZEROIZE(box);
    return box;
}

void pddlOutBoxAddLine(pddl_outbox_t *box, const char *line)
{
    if (box->line_size == box->line_alloc){
        if (box->line_alloc == 0)
            box->line_alloc = 2;
        box->line_alloc *= 2;
        box->line = REALLOC_ARR(box->line, char *, box->line_alloc);
    }

    int len = strlen(line);
    box->line[box->line_size] = ALLOC_ARR(char, len + 1);
    strcpy(box->line[box->line_size], line);
    box->line[box->line_size][len] = 0x0;
    ++box->line_size;
    box->max_line_len = PDDL_MAX(box->max_line_len, len);
}

void pddlOutBoxesMerge(pddl_outboxes_t *dst,
                       const pddl_outboxes_t *src,
                       int max_len)
{
    char line[max_len + 1];
    int start = 0;
    while (start < src->box_size){
        int len = src->box[start].max_line_len;
        int end = start + 1;
        for (; end < src->box_size; ++end){
            if (len + 3 + src->box[end].max_line_len < max_len){
                len += 3 + src->box[end].max_line_len;
            }else{
                break;
            }
        }

        int max_line_size = 0;
        for (int i = start; i < end; ++i)
            max_line_size = PDDL_MAX(max_line_size, src->box[i].line_size);

        pddl_outbox_t *box = pddlOutBoxesAdd(dst);
        for (int line_i = 0; line_i < max_line_size; ++line_i){
            int used = 0;
            for (int bi = start; bi < end; ++bi){
                if (bi != start)
                    used += sprintf(line + used, " | ");
                if (line_i < src->box[bi].line_size){
                    used += sprintf(line + used, "%s",
                                    src->box[bi].line[line_i]);
                    int remain = strlen(src->box[bi].line[line_i]);
                    remain = src->box[bi].max_line_len - remain;
                    for (int i = 0; i < remain; ++i)
                        line[used++] = ' ';
                }else{
                    for (int i = 0; i < src->box[bi].max_line_len; ++i)
                        line[used++] = ' ';
                }
            }
            line[len] = 0x0;
            pddlOutBoxAddLine(box, line);
        }

        start = end;
    }
}

void pddlOutBoxesPrint(const pddl_outboxes_t *b, FILE *fout, pddl_err_t *err)
{
    int line_len = 0;
    for (int i = 0; i < b->box_size; ++i)
        line_len = PDDL_MAX(line_len, b->box[i].max_line_len);
    line_len += 4;

    char *line = ALLOC_ARR(char, line_len + 1);
    line[line_len] = 0x0;
    for (int i = 0; i < line_len; ++i)
        line[i] = '-';
    if (err != NULL)
        PDDL_INFO(err, "%s", line);
    if (fout != NULL)
        fprintf(fout, "%s\n", line);
    for (int bi = 0; bi < b->box_size; ++bi){
        const pddl_outbox_t *box = b->box + bi;
        for (int li = 0; li < box->line_size; ++li){
            line[0] = '|';
            line[1] = ' ';
            int used = sprintf(line + 2, "%s", box->line[li]);
            for (int i = 0; i < line_len - 4 - used; ++i)
                line[i + 2 + used] = ' ';
            line[line_len - 1] = '|';
            line[line_len - 2] = ' ';
            if (err != NULL)
                PDDL_INFO(err, "%s", line);
            if (fout != NULL)
                fprintf(fout, "%s\n", line);
        }
        for (int i = 0; i < line_len; ++i)
            line[i] = '-';
        if (err != NULL)
            PDDL_INFO(err, "%s", line);
        if (fout != NULL)
            fprintf(fout, "%s\n", line);
    }
    FREE(line);
}
