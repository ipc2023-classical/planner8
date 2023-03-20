/***
 * cpddl
 * -------
 * Copyright (c)2019 Daniel Fiser <danfis@danfis.cz>,
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

#include "pddl/sort.h"
#include "pddl/hfunc.h"
#include "pddl/pddl.h"
#include "pddl/lifted_mgroup.h"
#include "internal.h"

#define LINESIZE 1024


static int cmpLiftedMGroups(const void *a, const void *b, void *_)
{
    const pddl_lifted_mgroup_t *m1 = a;
    const pddl_lifted_mgroup_t *m2 = b;
    int cmp = m1->cond.size - m2->cond.size;
    for (int i = 0; cmp == 0 && i < m1->cond.size; ++i){
        const pddl_fm_t *c1 = m1->cond.fm[i];
        const pddl_fm_atom_t *a1 = PDDL_FM_CAST(c1, atom);
        const pddl_fm_t *c2 = m2->cond.fm[i];
        const pddl_fm_atom_t *a2 = PDDL_FM_CAST(c2, atom);
        cmp = a1->pred - a2->pred;
        for (int j = 0; cmp == 0 && j < a1->arg_size; ++j){
            cmp = a1->arg[j].param - a2->arg[j].param;
            if (cmp == 0)
                cmp = a1->arg[j].obj - a2->arg[j].obj;
        }
    }
    if (cmp == 0)
        cmp = m1->param.param_size - m2->param.param_size;
    for (int i = 0; cmp == 0 && i < m1->param.param_size; ++i){
        cmp = m1->param.param[i].is_counted_var
                - m2->param.param[i].is_counted_var;
        if (cmp == 0)
            cmp = m1->param.param[i].type - m2->param.param[i].type;
    }

    return cmp;
}

void pddlLiftedMGroupInitEmpty(pddl_lifted_mgroup_t *dst)
{
    ZEROIZE(dst);
}

void pddlLiftedMGroupInitCopy(pddl_lifted_mgroup_t *dst,
                              const pddl_lifted_mgroup_t *src)
{
    ZEROIZE(dst);
    pddlParamsInitCopy(&dst->param, &src->param);
    for (int i = 0; i < src->cond.size; ++i)
        pddlFmArrAdd(&dst->cond, pddlFmClone(src->cond.fm[i]));
    dst->is_exactly_one = src->is_exactly_one;
    dst->is_static = src->is_static;
}

void pddlLiftedMGroupInitCandFromPred(pddl_lifted_mgroup_t *mgroup,
                                      const pddl_pred_t *pred,
                                      int counted_var)
{
    ZEROIZE(mgroup);
    pddlParamsInit(&mgroup->param);
    pddlFmArrInit(&mgroup->cond);

    for (int param_id = 0; param_id < pred->param_size; ++param_id){
        pddl_param_t *param = pddlParamsAdd(&mgroup->param);
        param->type = pred->param[param_id];
        if (counted_var == param_id)
            param->is_counted_var = 1;
    }

    pddl_fm_atom_t *atom;
    atom = pddlFmNewEmptyAtom(pred->param_size);
    atom->pred = pred->id;
    for (int param_id = 0; param_id < pred->param_size; ++param_id){
        atom->arg[param_id].param = param_id;
        atom->arg[param_id].obj = PDDL_OBJ_ID_UNDEF;
    }
    pddlFmArrAdd(&mgroup->cond, &atom->fm);

    pddlLiftedMGroupSort(mgroup);
}

void pddlLiftedMGroupFree(pddl_lifted_mgroup_t *mgroup)
{
    for (int i = 0; i < mgroup->cond.size; ++i)
        pddlFmDel((pddl_fm_t *)mgroup->cond.fm[i]);
    pddlParamsFree(&mgroup->param);
    pddlFmArrFree(&mgroup->cond);
}

int pddlLiftedMGroupEq(const pddl_lifted_mgroup_t *m1,
                       const pddl_lifted_mgroup_t *m2)
{
    return cmpLiftedMGroups(m1, m2, NULL) == 0;
}

static int cmpAtoms(const void *a, const void *b, void *_)
{
    const pddl_fm_t *c1 = *(const pddl_fm_t **)a;
    const pddl_fm_t *c2 = *(const pddl_fm_t **)b;
    const pddl_fm_atom_t *a1 = PDDL_FM_CAST(c1, atom);
    const pddl_fm_atom_t *a2 = PDDL_FM_CAST(c2, atom);
    return a1->pred - a2->pred;
}

void pddlLiftedMGroupSort(pddl_lifted_mgroup_t *m)
{
    if (m->cond.size > 1){
        pddlSort(m->cond.fm, m->cond.size, sizeof(const pddl_fm_t *),
                 cmpAtoms, NULL);
    }

    if (m->param.param_size <= 1)
        return;

    int *remap_param = ALLOC_ARR(int, m->param.param_size);
    int *remap_param_inv = ALLOC_ARR(int, m->param.param_size);
    for (int i = 0; i < m->param.param_size; ++i){
        remap_param[i] = -1;
        remap_param_inv[i] = -1;
    }

    int num_non_counted = 0;
    for (int i = 0; i < m->param.param_size; ++i){
        if (!m->param.param[i].is_counted_var)
            ++num_non_counted;
    }

    int next = 0;
    int next_counted = num_non_counted;
    for (int i = 0; i < m->cond.size; ++i){
        const pddl_fm_atom_t *a = PDDL_FM_CAST(m->cond.fm[i], atom);
        for (int ai = 0; ai < a->arg_size; ++ai){
            if (a->arg[ai].obj != PDDL_OBJ_ID_UNDEF)
                continue;
            int param = a->arg[ai].param;
            if (m->param.param[param].is_counted_var){
                if (remap_param[param] < 0){
                    remap_param_inv[next_counted] = param;
                    remap_param[param] = next_counted++;
                }
            }else{
                if (remap_param[param] < 0){
                    remap_param_inv[next] = param;
                    remap_param[param] = next++;
                }
            }
        }
    }

#ifdef PDDL_DEBUG
    for (int i = 0; i < m->param.param_size; ++i){
        ASSERT(remap_param[i] >= 0);
    }
#endif /* PDDL_DEBUG */

    pddl_params_t param;
    pddlParamsInit(&param);
    for (int i = 0; i < m->param.param_size; ++i){
        ASSERT_RUNTIME(remap_param_inv[i] >= 0);
        pddl_param_t *p = pddlParamsAdd(&param);
        *p = m->param.param[remap_param_inv[i]];
    }
    pddlParamsFree(&m->param);
    m->param = param;

    for (int i = 0; i < m->cond.size; ++i){
        pddl_fm_atom_t *a = PDDL_FM_CAST(m->cond.fm[i], atom);
        for (int ai = 0; ai < a->arg_size; ++ai){
            if (a->arg[ai].param >= 0){
                ASSERT_RUNTIME(remap_param[a->arg[ai].param] >= 0);
                a->arg[ai].param = remap_param[a->arg[ai].param];
            }
        }
    }

    FREE(remap_param);
    FREE(remap_param_inv);
}

int pddlLiftedMGroupNumCountedVars(const pddl_lifted_mgroup_t *mg)
{
    int count = 0;
    for (int i = 0; i < mg->param.param_size; ++i){
        if (mg->param.param[i].is_counted_var)
            ++count;
    }
    return count;
}

int pddlLiftedMGroupNumFixedVars(const pddl_lifted_mgroup_t *mg)
{
    return mg->param.param_size - pddlLiftedMGroupNumCountedVars(mg);
}

void pddlLiftedMGroupRemoveFixedAtoms(pddl_lifted_mgroup_t *mg)
{
    int *remap_param = CALLOC_ARR(int, mg->param.param_size);

    int num_del = 0;
    for (int ci = 0; ci < mg->cond.size; ++ci){
        pddl_fm_t *c = (pddl_fm_t *)mg->cond.fm[ci];
        ASSERT(c->type == PDDL_FM_ATOM);
        pddl_fm_atom_t *a = PDDL_FM_CAST(c, atom);
        int has_counted = 0;
        for (int argi = 0; argi < a->arg_size; ++argi){
            if (a->arg[argi].param >= 0
                    && mg->param.param[a->arg[argi].param].is_counted_var){
                has_counted = 1;
                break;
            }
        }

        if (!has_counted){
            pddlFmDel(c);
            mg->cond.fm[ci] = NULL;
            ++num_del;
        }else{
            for (int argi = 0; argi < a->arg_size; ++argi){
                if (a->arg[argi].param >= 0){
                    remap_param[a->arg[argi].param] = 1;
                }
            }
        }
    }

    if (num_del == 0){
        if (remap_param != NULL)
            FREE(remap_param);
        return;
    }

    int ins = 0;
    for (int ci = 0; ci < mg->cond.size; ++ci){
        if (mg->cond.fm[ci] != NULL)
            mg->cond.fm[ins++] = mg->cond.fm[ci];
    }
    mg->cond.size = ins;

    ins = 0;
    for (int i = 0; i < mg->param.param_size; ++i){
        if (remap_param[i] == 0){
            remap_param[i] = -1;
        }else{
            remap_param[i] = ins++;
        }
    }

    pddlParamsRemap(&mg->param, remap_param);
    for (int ci = 0; ci < mg->cond.size; ++ci){
        pddl_fm_t *c = (pddl_fm_t *)mg->cond.fm[ci];
        ASSERT(c->type == PDDL_FM_ATOM);
        pddl_fm_atom_t *a = PDDL_FM_CAST(c, atom);
        for (int argi = 0; argi < a->arg_size; ++argi){
            if (a->arg[argi].param >= 0){
                ASSERT(remap_param[a->arg[argi].param] >= 0);
                a->arg[argi].param = remap_param[a->arg[argi].param];
            }
        }
    }

    if (remap_param != NULL)
        FREE(remap_param);
}

static int atomHasCountedVar(const pddl_fm_atom_t *a,
                             const pddl_params_t *param)
{
    for (int i = 0; i < a->arg_size; ++i){
        if (a->arg[i].param >= 0
                && param->param[a->arg[i].param].is_counted_var)
            return 1;
    }
    return 0;
}

void pddlLiftedMGroupDoubleCounted(pddl_lifted_mgroup_t *mg)
{
    int map[mg->param.param_size];
    for (int i = 0; i < mg->param.param_size; ++i)
        map[i] = i;

    int old_param_size = mg->param.param_size;
    for (int pi = 0; pi < old_param_size; ++pi){
        if (!mg->param.param[pi].is_counted_var)
            continue;
        pddl_param_t *p = pddlParamsAdd(&mg->param);
        p->type = mg->param.param[pi].type;
        p->is_counted_var = 1;
        map[pi] = mg->param.param_size - 1;
    }

    int old_cond_size = mg->cond.size;
    for (int ci = 0; ci < old_cond_size; ++ci){
        const pddl_fm_atom_t *a = PDDL_FM_CAST(mg->cond.fm[ci], atom);
        if (!atomHasCountedVar(a, &mg->param))
            continue;
        pddl_fm_t *newc = pddlFmClone(&a->fm);
        pddl_fm_atom_t *newa = PDDL_FM_CAST(newc, atom);
        for (int ai = 0; ai < newa->arg_size; ++ai){
            int pi = newa->arg[ai].param;
            if (pi >= 0)
                newa->arg[ai].param = map[pi];
        }
        pddlFmArrAdd(&mg->cond, &newa->fm);
    }
}


#define MAX_LINE_SIZE 1024
static void printMGroup(const pddl_t *pddl,
                        const pddl_lifted_mgroup_t *mgroup,
                        FILE *fout,
                        pddl_err_t *err)
{
    char line[MAX_LINE_SIZE];
    int used = 0;
    used += snprintf(line, MAX_LINE_SIZE - used, "{");

    for (int i = 0; i < mgroup->cond.size; ++i){
        if (i > 0)
            used += snprintf(line + used, MAX_LINE_SIZE - used, ", ");

        pddl_fm_atom_t *atom = PDDL_FM_CAST(mgroup->cond.fm[i], atom);
        used += snprintf(line + used, MAX_LINE_SIZE - used,
                         "%s", pddl->pred.pred[atom->pred].name);
        for (int j = 0; j < atom->arg_size; ++j){
            if (atom->arg[j].param >= 0){
                int param_id = atom->arg[j].param;
                const pddl_param_t *p = mgroup->param.param + param_id;
                ASSERT(!pddlTypesAreDisjunct(&pddl->type, p->type,
                            pddl->pred.pred[atom->pred].param[j]));
                if (p->is_counted_var){
                    used += snprintf(line + used, MAX_LINE_SIZE - used,
                                     " C%d:%s",
                                     param_id, pddl->type.type[p->type].name);
                }else{
                    used += snprintf(line + used, MAX_LINE_SIZE - used,
                                     " V%d:%s",
                                     param_id, pddl->type.type[p->type].name);
                }
            }else{
                used += snprintf(line + used, MAX_LINE_SIZE - used, " %s",
                                 pddl->obj.obj[atom->arg[j].obj].name);
            }
        }
    }

    used += snprintf(line + used, MAX_LINE_SIZE - used, "}");
    if (mgroup->is_exactly_one)
        used += snprintf(line + used, MAX_LINE_SIZE - used, ":=1");
    if (mgroup->is_static)
        used += snprintf(line + used, MAX_LINE_SIZE - used, ":S");

    if (fout != NULL)
        fprintf(fout, "%s", line);
    if (err != NULL)
        PDDL_INFO(err, "%s", line);
}

void pddlLiftedMGroupPrint(const pddl_t *pddl,
                           const pddl_lifted_mgroup_t *mgroup,
                           FILE *fout)
{
    printMGroup(pddl, mgroup, fout, NULL);
    fprintf(fout, "\n");
}

void pddlLiftedMGroupLog(const pddl_t *pddl,
                         const pddl_lifted_mgroup_t *mgroup,
                         pddl_err_t *err)
{
    printMGroup(pddl, mgroup, NULL, err);
}

const char *pddlLiftedMGroupFmt(const pddl_t *pddl,
                                const pddl_lifted_mgroup_t *mgroup,
                                char *s,
                                size_t s_size)
{
    FILE *fout = fmemopen(s, s_size - 1, "w");
    printMGroup(pddl, mgroup, fout, NULL);
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


void pddlLiftedMGroupsInit(pddl_lifted_mgroups_t *lm)
{
    ZEROIZE(lm);
}

void pddlLiftedMGroupsInitCopy(pddl_lifted_mgroups_t *dst,
                               const pddl_lifted_mgroups_t *src)
{
    pddlLiftedMGroupsInit(dst);
    for (int i = 0; i < src->mgroup_size; ++i)
        pddlLiftedMGroupsAdd(dst, src->mgroup + i);
}

void pddlLiftedMGroupsFree(pddl_lifted_mgroups_t *lm)
{
    for (int i = 0; i < lm->mgroup_size; ++i)
        pddlLiftedMGroupFree(lm->mgroup + i);
    if (lm->mgroup != NULL)
        FREE(lm->mgroup);
}

void pddlLiftedMGroupsAdd(pddl_lifted_mgroups_t *lm,
                          const pddl_lifted_mgroup_t *lmg)
{
    if (lm->mgroup_size == lm->mgroup_alloc){
        if (lm->mgroup_alloc == 0)
            lm->mgroup_alloc = 2;
        lm->mgroup_alloc *= 2;
        lm->mgroup = REALLOC_ARR(lm->mgroup, pddl_lifted_mgroup_t,
                                 lm->mgroup_alloc);
    }

    pddl_lifted_mgroup_t *add = lm->mgroup + lm->mgroup_size++;
    pddlLiftedMGroupInitCopy(add, lmg);
    pddlLiftedMGroupSort(add);
}

void pddlLiftedMGroupsAddInst(pddl_lifted_mgroups_t *lm,
                              const pddl_lifted_mgroup_t *lmg,
                              const pddl_obj_id_t *args)
{
    pddlLiftedMGroupsAdd(lm, lmg);

    pddl_lifted_mgroup_t *mg = lm->mgroup + lm->mgroup_size - 1;
    for (int i = 0; i < mg->cond.size; ++i){
        pddl_fm_atom_t *a = PDDL_FM_CAST(mg->cond.fm[i], atom);
        for (int ai = 0; ai < a->arg_size; ++ai){
            if (a->arg[ai].param >= 0 && args[a->arg[ai].param] >= 0){
                a->arg[ai].obj = args[a->arg[ai].param];
                a->arg[ai].param = -1;
            }
        }
    }

    int remap_param[mg->param.param_size];
    int idx = 0;
    for (int i = 0; i < mg->param.param_size; ++i){
        if (args[i] >= 0){
            remap_param[i] = -1;
        }else{
            mg->param.param[idx] = mg->param.param[i];
            remap_param[i] = idx++;
        }
    }
    mg->param.param_size = idx;

    for (int i = 0; i < mg->cond.size; ++i){
        const pddl_fm_atom_t *a = PDDL_FM_CAST(mg->cond.fm[i], atom);
        for (int ai = 0; ai < a->arg_size; ++ai){
            if (a->arg[ai].param >= 0)
                a->arg[ai].param = remap_param[a->arg[ai].param];
        }
    }
}


void pddlLiftedMGroupsSortAndUniq(pddl_lifted_mgroups_t *lm)
{
    if (lm->mgroup_size == 0)
        return;

    pddlSort(lm->mgroup, lm->mgroup_size, sizeof(pddl_lifted_mgroup_t),
             cmpLiftedMGroups, NULL);

    int ins = 1;
    for (int i = 1; i < lm->mgroup_size; ++i){
        if (pddlLiftedMGroupEq(lm->mgroup + ins - 1, lm->mgroup + i)){
            pddlLiftedMGroupFree(lm->mgroup + i);
        }else{
            lm->mgroup[ins++] = lm->mgroup[i];
        }
    }
    lm->mgroup_size = ins;
}

int pddlLiftedMGroupsEq(const pddl_lifted_mgroups_t *lmg1,
                        const pddl_lifted_mgroups_t *lmg2)
{
    if (lmg1->mgroup_size != lmg2->mgroup_size)
        return 0;
    for (int i = 0; i < lmg1->mgroup_size; ++i){
        if (!pddlLiftedMGroupEq(lmg1->mgroup + i, lmg2->mgroup + i))
            return 0;
    }
    return 1;
}

void pddlLiftedMGroupsDoubleCounted(pddl_lifted_mgroups_t *mgs)
{
    for (int i = 0; i < mgs->mgroup_size; ++i)
        pddlLiftedMGroupDoubleCounted(mgs->mgroup + i);
}

void pddlLiftedMGroupsPrint(const pddl_t *pddl,
                            const pddl_lifted_mgroups_t *lm,
                            FILE *fout)
{
    fprintf(fout, "< ");
    for (int i = 0; i < lm->mgroup_size; ++i){
        if (i > 0)
            fprintf(fout, ", ");
        pddlLiftedMGroupPrint(pddl, lm->mgroup + i, fout);
    }
    fprintf(fout, ">\n");
}
