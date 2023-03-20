/***
 * cpddl
 * -------
 * Copyright (c)2019 Daniel Fiser <danfis@danfis.cz>,
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

#include "pddl/plan_file.h"
#include "internal.h"

struct parse {
    pddl_plan_file_fdr_t *pfdr;
    int *fdr_state;
    const pddl_fdr_t *fdr;

    pddl_plan_file_strips_t *pstrips;
    pddl_iset_t strips_state;
    const pddl_strips_t *strips;
};

static void planFileFDRAddState(pddl_plan_file_fdr_t *p,
                                const pddl_fdr_t *fdr,
                                const int *state)
{
    if (p->state_size == p->state_alloc){
        if (p->state_alloc == 0)
            p->state_alloc = 4;
        p->state_alloc *= 2;
        p->state = REALLOC_ARR(p->state, int *, p->state_alloc);
    }

    p->state[p->state_size] = ALLOC_ARR(int, fdr->var.var_size);
    memcpy(p->state[p->state_size], state, sizeof(int) * fdr->var.var_size);
    ++p->state_size;
}

static void planFileStripsAddState(pddl_plan_file_strips_t *p,
                                   const pddl_strips_t *strips,
                                   const pddl_iset_t *state)
{
    if (p->state_size == p->state_alloc){
        if (p->state_alloc == 0)
            p->state_alloc = 4;
        p->state_alloc *= 2;
        p->state = REALLOC_ARR(p->state, pddl_iset_t, p->state_alloc);
    }

    pddl_iset_t *s = p->state + p->state_size;
    pddlISetInit(s);
    pddlISetUnion(s, state);
    ++p->state_size;
}

static int readFile(struct parse *parse,
                    const char *filename,
                    pddl_err_t *err,
                    int (*cb)(struct parse *, const char *, pddl_err_t *))
{
    FILE *fin;

    if ((fin = fopen(filename, "r")) == NULL)
        PDDL_ERR_RET(err, -1, "Could not open file '%s'", filename);

    int ret = 0;
    size_t len = 0;
    char *line = NULL;
    ssize_t nread;
    while ((nread = getline(&line, &len, fin)) != -1){
        // Filter out comments
        for (int i = 0; i < nread; ++i){
            if (line[i] == ';'){
                line[i] = 0x0;
                break;
            }
        }
        char *s = strstr(line, "(");
        if (s == NULL)
            continue;
        char *name = s + 1;
        for (; *s != ')' && s != 0x0; ++s);
        if (*s != ')')
            continue;
        *s = 0x0;
        for (--s; s > name && *s == ' '; --s)
            *s = 0x0;

        if ((ret = cb(parse, name, err)) != 0)
            break;
    }
    if (line != NULL)
        free(line);
    fclose(fin);
    return ret;
}

static int parseFDR(struct parse *parse,
                    const char *name,
                    pddl_err_t *err)
{
    pddl_plan_file_fdr_t *p = parse->pfdr;
    int *state = parse->fdr_state;
    const pddl_fdr_t *fdr = parse->fdr;
    const int *cur_state = p->state[p->state_size - 1];
    int found = 0;
    for (int op_id = 0; op_id < fdr->op.op_size; ++op_id){
        const pddl_fdr_op_t *op = fdr->op.op[op_id];
        if (strcmp(op->name, name) == 0
                && pddlFDROpIsApplicable(op, cur_state)){
            pddlFDROpApplyOnState(op, fdr->var.var_size, cur_state, state);
            planFileFDRAddState(p, fdr, state);
            pddlIArrAdd(&p->op, op_id);
            p->cost += op->cost;
            found = 1;
        }
    }

    if (!found){
        PDDL_ERR(err, "Could not find a matching operator for '%s'.", name);
        return -1;
    }
    return 0;
}

static int parseStrips(struct parse *parse,
                       const char *name,
                       pddl_err_t *err)
{
    pddl_plan_file_strips_t *p = parse->pstrips;
    pddl_iset_t *state = &parse->strips_state;
    const pddl_strips_t *strips = parse->strips;
    const pddl_iset_t *cur_state = p->state + p->state_size - 1;

    int found = 0;
    for (int op_id = 0; op_id < strips->op.op_size; ++op_id){
        const pddl_strips_op_t *op = strips->op.op[op_id];
        if (strcmp(op->name, name) == 0
                && pddlISetIsSubset(&op->pre, cur_state)){
            pddlISetMinus2(state, cur_state, &op->del_eff);
            pddlISetUnion(state, &op->add_eff);
            planFileStripsAddState(p, strips, state);
            pddlIArrAdd(&p->op, op_id);
            p->cost += op->cost;
            found = 1;
            break;
        }
    }

    if (!found){
        PDDL_ERR(err, "Could not find a matching operator for '%s'.", name);
        return -1;
    }
    return 0;
}

int pddlPlanFileFDRInit(pddl_plan_file_fdr_t *p,
                        const pddl_fdr_t *fdr,
                        const char *filename,
                        pddl_err_t *err)
{
    ZEROIZE(p);
    planFileFDRAddState(p, fdr, fdr->init);

    struct parse parse;
    parse.pfdr = p;
    parse.fdr_state = ALLOC_ARR(int, fdr->var.var_size);
    parse.fdr = fdr;
    int ret = readFile(&parse, filename, err, parseFDR);
    FREE(parse.fdr_state);

    return ret;
}


void pddlPlanFileFDRFree(pddl_plan_file_fdr_t *p)
{
    pddlIArrFree(&p->op);
    for (int i = 0; i < p->state_size; ++i)
        FREE(p->state[i]);
    if (p->state != NULL)
        FREE(p->state);
}

int pddlPlanFileStripsInit(pddl_plan_file_strips_t *p,
                           const pddl_strips_t *strips,
                           const char *filename,
                           pddl_err_t *err)
{
    ZEROIZE(p);
    planFileStripsAddState(p, strips, &strips->init);

    struct parse parse;
    parse.pstrips = p;
    pddlISetInit(&parse.strips_state);
    parse.strips = strips;
    int ret = readFile(&parse, filename, err, parseStrips);
    pddlISetFree(&parse.strips_state);

    return ret;
}

void pddlPlanFileStripsFree(pddl_plan_file_strips_t *p)
{
    pddlIArrFree(&p->op);
    for (int i = 0; i < p->state_size; ++i)
        pddlISetFree(&p->state[i]);
    if (p->state != NULL)
        FREE(p->state);
}

int pddlPlanFileParseOptimalCost(const char *filename, pddl_err_t *err)
{
    FILE *fin;

    if ((fin = fopen(filename, "r")) == NULL)
        PDDL_ERR_RET(err, -1, "Could not open file '%s'", filename);

    size_t len = 0;
    char *line = NULL;
    ssize_t nread;
    while ((nread = getline(&line, &len, fin)) != -1){
        if (line[0] != ';')
            continue;
        char *s = strstr(line, "Optimal cost:");
        if (s == NULL)
            s = strstr(line, "Optimal Cost:");

        if (s != NULL){
            s += 13;
            int cost = atoi(s);
            return cost;
        }
    }
    if (line != NULL)
        free(line);
    fclose(fin);
    return -1;
}
