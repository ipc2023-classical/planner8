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

#include "internal.h"
#include "pddl/mg_strips.h"

static void makeMGroupExactlyOne(pddl_mg_strips_t *mg_strips,
                                 const pddl_mgroup_t *mg_in)
{
    if (pddlISetIsDisjunct(&mg_in->mgroup, &mg_strips->strips.init))
        return;

    PDDL_ISET(facts);
    pddlISetUnion(&facts, &mg_in->mgroup);

    pddl_mgroup_t *mg = pddlMGroupsAdd(&mg_strips->mg, &facts);
    if (!pddlStripsIsExactlyOneMGroup(&mg_strips->strips, &facts)){
        pddl_fact_t none_of_those;
        pddlFactInit(&none_of_those);
        int name_size = 5;
        int fact_id;
        PDDL_ISET_FOR_EACH(&facts, fact_id){
            name_size += strlen(mg_strips->strips.fact.fact[fact_id]->name);
            name_size += 1;
        }
        none_of_those.name = ALLOC_ARR(char, name_size);
        char *cur = none_of_those.name;
        cur += sprintf(cur, "NOT:");
        PDDL_ISET_FOR_EACH(&facts, fact_id){
            cur += sprintf(cur, "%s;",
                           mg_strips->strips.fact.fact[fact_id]->name);
        }
        *(cur - 1) = 0x0;

        int new_fact_id = pddlFactsAdd(&mg_strips->strips.fact, &none_of_those);
        ASSERT(new_fact_id == mg_strips->strips.fact.fact_size - 1);
        pddlISetAdd(&mg->mgroup, new_fact_id);
        pddlFactFree(&none_of_those);

        for (int op_id = 0; op_id < mg_strips->strips.op.op_size; ++op_id){
            pddl_strips_op_t *op = mg_strips->strips.op.op[op_id];
            int in_del = !pddlISetIsDisjunct(&op->del_eff, &facts);
            int in_add = !pddlISetIsDisjunct(&op->add_eff, &facts);
            if (in_del && !in_add)
                pddlISetAdd(&op->add_eff, new_fact_id);
            if (!in_del && in_add)
                pddlISetAdd(&op->del_eff, new_fact_id);
        }

        if (pddlISetIsDisjunct(&mg_strips->strips.init, &facts))
            pddlISetAdd(&mg_strips->strips.init, new_fact_id);
    }

    pddlISetFree(&facts);
}

static void encodeBinaryFact(pddl_mg_strips_t *mg_strips, int fact_id,
                             int *covered)
{
    pddl_fact_t *fact = mg_strips->strips.fact.fact[fact_id];

    int not_id;
    if (fact->neg_of >= 0){
        not_id = fact->neg_of;
        covered[not_id] = 1;
    }else{
        pddl_fact_t not;
        pddlFactInit(&not);
        int name_size = strlen(mg_strips->strips.fact.fact[fact_id]->name) + 5;
        not.name = ALLOC_ARR(char, name_size);
        sprintf(not.name, "NOT:%s", mg_strips->strips.fact.fact[fact_id]->name);
        not_id = pddlFactsAdd(&mg_strips->strips.fact, &not);
        ASSERT(not_id == mg_strips->strips.fact.fact_size - 1);
        pddlFactFree(&not);

        fact->neg_of = not_id;
        mg_strips->strips.fact.fact[not_id]->neg_of = fact_id;
    }
    covered[fact_id] = 1;

    for (int op_id = 0; op_id < mg_strips->strips.op.op_size; ++op_id){
        pddl_strips_op_t *op = mg_strips->strips.op.op[op_id];
        int in_del = pddlISetIn(fact_id, &op->del_eff);
        int in_add = pddlISetIn(fact_id, &op->add_eff);
        int in_del_not = pddlISetIn(not_id, &op->del_eff);
        int in_add_not = pddlISetIn(not_id, &op->add_eff);

        if (in_del && !in_add)
            pddlISetAdd(&op->add_eff, not_id);
        if (!in_del && in_add)
            pddlISetAdd(&op->del_eff, not_id);
        if (in_del_not && !in_add_not)
            pddlISetAdd(&op->add_eff, fact_id);
        if (!in_del_not && in_add_not)
            pddlISetAdd(&op->del_eff, fact_id);
    }

    if (!pddlISetIn(fact_id, &mg_strips->strips.init))
        pddlISetAdd(&mg_strips->strips.init, not_id);

    PDDL_ISET(facts);
    PDDL_ISET_SET(&facts, fact_id, not_id);
    pddlMGroupsAdd(&mg_strips->mg, &facts);
    pddlISetFree(&facts);
}

static void encodeBinaryFacts(pddl_mg_strips_t *mg_strips)
{
    int fact_size = mg_strips->strips.fact.fact_size;
    int *covered;

    covered = CALLOC_ARR(int, fact_size);
    for (int mi = 0; mi < mg_strips->mg.mgroup_size; ++mi){
        const pddl_mgroup_t *mg = mg_strips->mg.mgroup + mi;
        int fact_id;
        PDDL_ISET_FOR_EACH(&mg->mgroup, fact_id)
            covered[fact_id] = 1;
    }

    for (int fact_id = 0; fact_id < fact_size; ++fact_id){
        if (!covered[fact_id])
            encodeBinaryFact(mg_strips, fact_id, covered);
    }

    FREE(covered);
}

static void encodeMGroups(pddl_mg_strips_t *mg_strips,
                          pddl_mgroups_t *mgroups)
{
    for (int mgi = 0; mgi < mgroups->mgroup_size; ++mgi){
        const pddl_mgroup_t *mg_in = mgroups->mgroup + mgi;
        if (pddlISetSize(&mg_in->mgroup) <= 1)
            continue;

        if (pddlStripsIsExactlyOneMGroup(&mg_strips->strips, &mg_in->mgroup)){
            // Copy exactly-one mutex groups directly to mg-strips
            pddlMGroupsAdd(&mg_strips->mg, &mg_in->mgroup);

        }else{
            // Mutex groups that are not exactly-one need "none-of-those"
            // fact
            makeMGroupExactlyOne(mg_strips, mg_in);
        }
    }
}

/*
static void encodeMGroupsNonOverlapLargestFirst(pddl_mg_strips_t *mg_strips,
                                                pddl_mgroups_t *mgroups)
{
    while (mgroups->mgroup_size > 0){
        pddl_mgroup_t *mg_in = mgroups->mgroup + 0;
        ASSERT(pddlISetSize(&mg_in->mgroup) > 1);
        if (pddlStripsIsExactlyOneMGroup(&mg_strips->strips, &mg_in->mgroup)){
            // Copy exactly-one mutex groups directly to mg-strips
            pddlMGroupsAdd(&mg_strips->mg, &mg_in->mgroup);

        }else{
            // Mutex groups that are not exactly-one need "none-of-those"
            // fact
            makeMGroupExactlyOne(mg_strips, mg_in);
        }

        for (int mi = 1; mi < mgroups->mgroup_size; ++mi)
            pddlISetMinus(&mgroups->mgroup[mi].mgroup, &mg_in->mgroup);
        pddlISetEmpty(&mg_in->mgroup);
        pddlMGroupsRemoveSubsets(mgroups);
        pddlMGroupsRemoveSmall(mgroups, 1);
        pddlMGroupsSortUniq(mgroups);
        pddlMGroupsSortBySizeDesc(mgroups);
    }
}
*/

static void findUncoveredDelEffs(pddl_iset_t *out, const pddl_strips_t *strips)
{
    PDDL_ISET(tmp);
    for (int op_id = 0; op_id < strips->op.op_size; ++op_id){
        const pddl_strips_op_t *op = strips->op.op[op_id];
        pddlISetMinus2(&tmp, &op->del_eff, &op->pre);
        pddlISetUnion(out, &tmp);
    }
    pddlISetFree(&tmp);
}

static void prepareMGroups(pddl_mgroups_t *dst, const pddl_mgroups_t *src,
                           const pddl_iset_t *uncovered_del_effs)
{
    pddlMGroupsInitCopy(dst, src);

    for (int mi = 0; mi < dst->mgroup_size; ++mi)
        pddlISetMinus(&dst->mgroup[mi].mgroup, uncovered_del_effs);

    pddlMGroupsRemoveSubsets(dst);
    pddlMGroupsRemoveSmall(dst, 1);
    pddlMGroupsSortUniq(dst);
    pddlMGroupsSortBySizeDesc(dst);
}
                                

void pddlMGStripsInit(pddl_mg_strips_t *mg_strips,
                      const pddl_strips_t *strips,
                      const pddl_mgroups_t *mgroups_in)
{
    if (strips->has_cond_eff)
        PANIC("pddlMGStripsInit: conditional effects not yet supported.");

    // Find facts that appear in delete effects but not in the precondition
    PDDL_ISET(uncovered_del_effs);
    findUncoveredDelEffs(&uncovered_del_effs, strips);

    // Prepare mutex groups: remove subsets, remove mutex groups having
    // less than two facts and sort them.
    pddl_mgroups_t mgroups;
    prepareMGroups(&mgroups, mgroups_in, &uncovered_del_effs);

    pddlStripsInitCopy(&mg_strips->strips, strips);
    pddlMGroupsInitEmpty(&mg_strips->mg);

    encodeMGroups(mg_strips, &mgroups);
    //encodeMGroupsNonOverlapLargestFirst(mg_strips, &mgroups);
    encodeBinaryFacts(mg_strips);

    pddlISetFree(&uncovered_del_effs);
    pddlMGroupsFree(&mgroups);

    //pddlStripsPrintDebug(&mg_strips->strips, stderr);
    //pddlMGroupsPrint(NULL, &mg_strips->strips, &mg_strips->mg, stderr);

    // Set flags
    for (int mi = 0; mi < mg_strips->mg.mgroup_size; ++mi){
        pddl_mgroup_t *mg = mg_strips->mg.mgroup + mi;
        mg->is_exactly_one = 1;
        mg->is_goal = !pddlISetIsDisjunct(&mg->mgroup, &mg_strips->strips.goal);
        mg->is_fam_group = pddlStripsIsFAMGroup(&mg_strips->strips,
                                                &mg->mgroup);
        ASSERT_RUNTIME(!pddlISetIsDisjunct(&mg_strips->strips.init,
                                           &mg->mgroup));
        ASSERT_RUNTIME(pddlStripsIsExactlyOneMGroup(&mg_strips->strips,
                                                    &mg->mgroup));
    }
}

void pddlMGStripsInitCopy(pddl_mg_strips_t *mg_strips,
                          const pddl_mg_strips_t *in)
{
    ZEROIZE(mg_strips);
    pddlStripsInitCopy(&mg_strips->strips, &in->strips);
    pddlMGroupsInitCopy(&mg_strips->mg, &in->mg);
}

static void fdrPreToPre(const pddl_fdr_vars_t *vars,
                        const pddl_fdr_part_state_t *fdr_pre,
                        pddl_iset_t *pre)
{
    for (int i = 0; i < fdr_pre->fact_size; ++i){
        const pddl_fdr_fact_t *f = fdr_pre->fact + i;
        pddlISetAdd(pre, vars->var[f->var].val[f->val].global_id);
    }
}

static void fdrEffToEff(const pddl_fdr_vars_t *vars,
                        const pddl_fdr_part_state_t *fdr_pre,
                        const pddl_fdr_part_state_t *fdr_eff,
                        pddl_iset_t *add_eff,
                        pddl_iset_t *del_eff)
{
    int prei = 0;
    for (int i = 0; i < fdr_eff->fact_size; ++i){
        const pddl_fdr_fact_t *f = fdr_eff->fact + i;
        pddlISetAdd(add_eff, vars->var[f->var].val[f->val].global_id);
        for (; prei < fdr_pre->fact_size
                && fdr_pre->fact[prei].var < f->var; ++prei);
        if (prei < fdr_pre->fact_size && fdr_pre->fact[prei].var == f->var){
            const pddl_fdr_fact_t *d = fdr_pre->fact + prei;
            pddlISetAdd(del_eff, vars->var[d->var].val[d->val].global_id);
        }else{
            for (int val = 0; val < vars->var[f->var].val_size; ++val){
                if (val != f->val)
                    pddlISetAdd(del_eff, vars->var[f->var].val[val].global_id);
            }
        }
    }
}

void pddlMGStripsInitFDR(pddl_mg_strips_t *mg_strips, const pddl_fdr_t *fdr)
{
    pddlStripsInit(&mg_strips->strips);
    pddlMGroupsInitEmpty(&mg_strips->mg);

    // Add facts
    for (int fact_id = 0; fact_id < fdr->var.global_id_size; ++fact_id){
        const pddl_fdr_val_t *val = fdr->var.global_id_to_val[fact_id];
        char name[256];
        int wsize = snprintf(name, 256, "%s %d-%d-%d",
                             val->name, val->var_id, val->val_id,
                             val->global_id);
        ASSERT_RUNTIME_M(wsize < 256, "Formatting of the fact name failed"
                                      " when translating from FDR to STRIPS");

        pddl_fact_t fact;
        pddlFactInit(&fact);
        fact.name = name;
        int id = pddlFactsAdd(&mg_strips->strips.fact, &fact);
        ASSERT_RUNTIME(id == fact_id);
        fact.name = NULL;
        pddlFactFree(&fact);
    }

    // Set .neg_of for binary variables
    for (int var_id = 0; var_id < fdr->var.var_size; ++var_id){
        const pddl_fdr_var_t *var = fdr->var.var + var_id;
        if (var->val_size == 2){
            int id1 = var->val[0].global_id;
            int id2 = var->val[1].global_id;
            mg_strips->strips.fact.fact[id1]->neg_of = id2;
            mg_strips->strips.fact.fact[id2]->neg_of = id1;
        }
    }

    // Add operators
    int has_cond_eff = 0;
    for (int op_id = 0; op_id < fdr->op.op_size; ++op_id){
        const pddl_fdr_op_t *fop = fdr->op.op[op_id];
        pddl_strips_op_t op;
        pddlStripsOpInit(&op);
        op.name = STRDUP(fop->name);
        op.cost = fop->cost;
        fdrPreToPre(&fdr->var, &fop->pre, &op.pre);
        fdrEffToEff(&fdr->var, &fop->pre, &fop->eff, &op.add_eff, &op.del_eff);

        for (int cei = 0; cei < fop->cond_eff_size; ++cei){
            const pddl_fdr_op_cond_eff_t *fce = fop->cond_eff + cei;
            pddl_fdr_part_state_t pre;
            pddlFDRPartStateInitCopy(&pre, &fop->pre);
            for (int i = 0; i < fce->pre.fact_size; ++i){
                pddlFDRPartStateSet(&pre, fce->pre.fact[i].var,
                                          fce->pre.fact[i].val);
            }

            pddl_strips_op_t ce;
            pddlStripsOpInit(&ce);
            fdrPreToPre(&fdr->var, &fce->pre, &ce.pre);
            fdrEffToEff(&fdr->var, &pre, &fce->eff, &ce.add_eff, &ce.del_eff);
            pddlStripsOpAddCondEff(&op, &ce);
            pddlStripsOpFree(&ce);

            pddlFDRPartStateFree(&pre);
            has_cond_eff = 1;
        }

        int id = pddlStripsOpsAdd(&mg_strips->strips.op, &op);
        ASSERT_RUNTIME(id == op_id);
        pddlStripsOpFree(&op);
    }
    mg_strips->strips.has_cond_eff = has_cond_eff;
    mg_strips->strips.goal_is_unreachable = fdr->goal_is_unreachable;

    // Set initial state
    for (int var_id = 0; var_id < fdr->var.var_size; ++var_id){
        int val = fdr->init[var_id];
        int fact_id = fdr->var.var[var_id].val[val].global_id;
        pddlISetAdd(&mg_strips->strips.init, fact_id);
    }

    // Set goal
    for (int i = 0; i < fdr->goal.fact_size; ++i){
        const pddl_fdr_fact_t *f = fdr->goal.fact + i;
        int fact_id = fdr->var.var[f->var].val[f->val].global_id;
        pddlISetAdd(&mg_strips->strips.goal, fact_id);
    }

    // Convert variables to exactly-one mutex groups
    for (int var = 0; var < fdr->var.var_size; ++var){
        PDDL_ISET(mg);
        for (int val = 0; val < fdr->var.var[var].val_size; ++val)
            pddlISetAdd(&mg, fdr->var.var[var].val[val].global_id);
        pddl_mgroup_t *m = pddlMGroupsAdd(&mg_strips->mg, &mg);
        m->is_exactly_one = 1;
        pddlISetFree(&mg);
    }
}

void pddlMGStripsFree(pddl_mg_strips_t *mg_strips)
{
    pddlStripsFree(&mg_strips->strips);
    pddlMGroupsFree(&mg_strips->mg);
}

double pddlMGStripsNumStatesApproxMC(const pddl_mg_strips_t *mg_strips,
                                     const pddl_mutex_pairs_t *mutex,
                                     const char *approxmc_bin,
                                     int fix_fact)
{
    double num = -1.;
    int fdin[2];
    int fdout[2];

    if (pipe(fdin) != 0){
        perror("pipe() filed");
        return -1.;
    }
    if (pipe(fdout) != 0){
        perror("pipe() filed");
        return -1.;
    }

    int pid = fork();
    if (pid == -1){
        perror("fork() filed");
        return -1.;

    }else if (pid == 0){
        // child
        close(fdin[1]);
        close(fdout[0]);

        dup2(fdin[0], STDIN_FILENO);
        dup2(fdout[1], STDOUT_FILENO);
        close(fdin[0]);
        close(fdout[1]);
        execl(approxmc_bin, approxmc_bin,
              "--seed", "1234",
              "--th", "1",
              "-v", "0",
              NULL);
        return -1.;

    }else{
        // parent
        close(fdin[0]);
        close(fdout[1]);

        int num_clauses = 0;
        num_clauses += mutex->num_mutex_pairs;
        num_clauses += mg_strips->mg.mgroup_size;
        if (fix_fact >= 0){
            num_clauses += 1;
            dprintf(fdin[1], "c ind %d", fix_fact + 1);
            for (int f = 0; f < mg_strips->strips.fact.fact_size; ++f){
                if (f == fix_fact)
                    continue;
                if (!pddlMutexPairsIsMutex(mutex, f, fix_fact))
                    dprintf(fdin[1], " %d", f + 1);
            }
            dprintf(fdin[1], " 0\n");
        }

        dprintf(fdin[1], "p cnf %d %d\n",
                mg_strips->strips.fact.fact_size,
                num_clauses);
        if (fix_fact >= 0)
            dprintf(fdin[1], "%d 0\n", fix_fact + 1);

        PDDL_MUTEX_PAIRS_FOR_EACH(mutex, f1, f2)
            dprintf(fdin[1], "%d %d 0\n", -(f1 + 1), -(f2 + 1));

        for (int mgi = 0; mgi < mg_strips->mg.mgroup_size; ++mgi){
            int fact_id;
            int first = 1;
            PDDL_ISET_FOR_EACH(&mg_strips->mg.mgroup[mgi].mgroup, fact_id){
                if (!first)
                    dprintf(fdin[1], " ");
                dprintf(fdin[1], "%d", fact_id + 1);
                first = 0;
            }
            dprintf(fdin[1], " 0\n");
        }

        close(fdin[1]);

        FILE *fin = fdopen(fdout[0], "r");
        ssize_t readsize;
        size_t size = 0;
        char *line = NULL;

        while ((readsize = getline(&line, &size, fin)) >= 0){
            //fprintf(stderr, "L: %s", line);
            char *found = strstr(line, "Number of solutions is:");
            if (found != NULL){
                char *k = found + 24;
                char *exp;
                for (exp = k; *exp != '\n' && *exp != ' '; ++exp);
                ASSERT_RUNTIME(*exp == ' ');
                *exp = 0x0;
                ASSERT_RUNTIME(*(++exp) == 'x');
                ASSERT_RUNTIME(*(++exp) == ' ');
                ASSERT_RUNTIME(*(++exp) == '2');
                ASSERT_RUNTIME(*(++exp) == '^');
                char *end = ++exp;
                for (; *end >= '0' && *end <= '9'; ++end);
                *end = 0x0;

                double dk = atof(k);
                double dexp = atof(exp);
                num = dk * exp2(dexp);
                //fprintf(stderr, "F: '%s' x 2^'%s' %f %f : %f\n",
                //        k, exp, dk, dexp, num);
            }
        }
        if (line != NULL)
            free(line);
        fclose(fin);

        wait(NULL);
        return num;
    }
}

void pddlMGStripsReduce(pddl_mg_strips_t *mg_strips,
                        const pddl_iset_t *del_facts,
                        const pddl_iset_t *del_ops)
{
    pddlStripsReduce(&mg_strips->strips, del_facts, del_ops);
    pddlMGroupsReduce(&mg_strips->mg, del_facts);
}

void pddlMGStripsReorderMGroups(pddl_mg_strips_t *mg_strips,
                                const int *reorder)
{
    pddl_mgroups_t mgs;
    pddlMGroupsInitEmpty(&mgs);
    for (int i = 0; i < mg_strips->mg.mgroup_size; ++i){
        const pddl_mgroup_t *mgin = mg_strips->mg.mgroup + reorder[i];
        pddl_mgroup_t *mg = pddlMGroupsAdd(&mgs, &mgin->mgroup);
        mg->is_exactly_one = mgin->is_exactly_one;
        mg->is_fam_group = mgin->is_fam_group;
        mg->is_goal = mgin->is_goal;
    }
    pddlMGroupsFree(&mg_strips->mg);
    mg_strips->mg = mgs;
}
