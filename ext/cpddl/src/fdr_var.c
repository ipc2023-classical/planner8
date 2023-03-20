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
#include "pddl/outbox.h"
#include "pddl/fdr_var.h"

#define PDDL_FDR_VARS_METHOD_MASK 0xfu

struct vars_mgroup {
    pddl_iset_t uncovered; /*!< The set of uncovered facts from the mgroup */
    const pddl_mgroup_t *mgroup; /*!< The original mgroup */
};
typedef struct vars_mgroup vars_mgroup_t;

struct vars_mgroups {
    vars_mgroup_t *mgroup;
    int mgroup_size;
    const pddl_mgroups_t *mgroups;
    int has_uncovered;
};
typedef struct vars_mgroups vars_mgroups_t;

static void varsMGroupsInit(vars_mgroups_t *vmgs, const pddl_mgroups_t *mgs)
{
    ZEROIZE(vmgs);
    vmgs->mgroups = mgs;
    vmgs->has_uncovered = 0;
    vmgs->mgroup_size = mgs->mgroup_size;
    vmgs->mgroup = CALLOC_ARR(vars_mgroup_t, vmgs->mgroup_size);
    for (int i = 0; i < vmgs->mgroup_size; ++i){
        vmgs->mgroup[i].mgroup = mgs->mgroup + i;
        pddlISetUnion(&vmgs->mgroup[i].uncovered, &mgs->mgroup[i].mgroup);
        if (pddlISetSize(&vmgs->mgroup[i].uncovered) > 0)
            vmgs->has_uncovered = 1;
    }
}

static void varsMGroupsFree(vars_mgroups_t *vmgs)
{
    for (int i = 0; i < vmgs->mgroup_size; ++i)
        pddlISetFree(&vmgs->mgroup[i].uncovered);
    if (vmgs->mgroup != NULL)
        FREE(vmgs->mgroup);
}

/** Cover the given facts */
static void varsMGroupsCover(vars_mgroups_t *vmgs, const pddl_iset_t *_facts)
{
    // Copy the set in case _facts points to .uncovered of some vars_mgroup
    PDDL_ISET(facts);
    pddlISetUnion(&facts, _facts);
    vmgs->has_uncovered = 0;
    for (int i = 0; i < vmgs->mgroup_size; ++i){
        vars_mgroup_t *m = vmgs->mgroup + i;
        pddlISetMinus(&m->uncovered, &facts);
        if (pddlISetSize(&m->uncovered) > 0)
            vmgs->has_uncovered = 1;
    }
    pddlISetFree(&facts);
}

/** Return the first vars_mgroup that has uncovered one of the given facts */
static const vars_mgroup_t *varsMGroupsFindCovering(const vars_mgroups_t *vmgs,
                                                    const pddl_iset_t *facts)
{
    if (pddlISetSize(facts) == 0)
        return NULL;

    for (int i = 0; i < vmgs->mgroup_size; ++i){
        const vars_mgroup_t *m = vmgs->mgroup + i;
        if (pddlISetSize(&m->uncovered) > 0
                && !pddlISetIsDisjunct(&m->uncovered, facts)){
            return m;
        }
    }
    return NULL;
}

static int varsMGroupUncoveredDescCmp(const void *a, const void *b, void *_)
{
    const vars_mgroup_t *m1 = a;
    const vars_mgroup_t *m2 = b;
    int cmp = pddlISetSize(&m2->uncovered) - pddlISetSize(&m1->uncovered);
    if (cmp == 0)
        cmp = pddlISetCmp(&m1->uncovered, &m2->uncovered);
    if (cmp == 0)
        cmp = m1->mgroup->lifted_mgroup_id - m2->mgroup->lifted_mgroup_id;
    return cmp;
}

/** Sort mgroups in a descending order by the number of uncovered facts */
static void varsMGroupsSortUncoveredDesc(vars_mgroups_t *vmgs)
{
    pddlSort(vmgs->mgroup, vmgs->mgroup_size, sizeof(*vmgs->mgroup),
             varsMGroupUncoveredDescCmp, NULL);

}




void pddlFDRValInit(pddl_fdr_val_t *val)
{
    ZEROIZE(val);
}

void pddlFDRValFree(pddl_fdr_val_t *val)
{
    if (val->name != NULL)
        FREE(val->name);
}

void pddlFDRVarInit(pddl_fdr_var_t *var)
{
    ZEROIZE(var);
}

void pddlFDRVarFree(pddl_fdr_var_t *var)
{
    for (int i = 0; i < var->val_size; ++i)
        pddlFDRValFree(var->val + i);
    if (var->val != NULL)
        FREE(var->val);
}

/** Find facts that must be encoded as binary because they appear as delete
 *  effect, but it does not switch to any other fact (considering
 *  mutexes/mutex groups) and it may or may not be part of the state in
 *  which the operator is applied in.
 *  Consider the following example:
 *      operator: pre: f_1, add: f_2, del: f_3
 *      mutex group: { f_3, f_4 }
 *  Now, how can we encode del: f_3 using the given mutex group?
 *  If we encode it as "none-of-those", then the resulting state from the
 *  application of the operator on the state {f_1, f_4} would be incorrect.
 *  This is why f_3 must be encoded separately from all other mutex groups,
 *  otherwise we would need to create potentially exponentially more
 *  operators.
 */
static void factsRequiringBinaryEncoding(const pddl_strips_t *strips,
                                         const pddl_mgroups_t *mgs,
                                         const pddl_mutex_pairs_t *mutex,
                                         pddl_iset_t *binfs)
{
    const pddl_strips_op_t *op;
    int fact_id;

    int *mg_facts = CALLOC_ARR(int, strips->fact.fact_size);
    for (int mi = 0; mi < mgs->mgroup_size; ++mi){
        int fact_id;
        PDDL_ISET_FOR_EACH(&mgs->mgroup[mi].mgroup, fact_id)
            mg_facts[fact_id] = 1;
    }

    PDDL_STRIPS_OPS_FOR_EACH(&strips->op, op){
        PDDL_ISET_FOR_EACH(&op->del_eff, fact_id){
            if (!mg_facts[fact_id])
                continue;

            // This is the most common case -- the delete effect is
            // required by the precondition.
            if (pddlISetIn(fact_id, &op->pre))
                continue;

            // If the fact is mutex with the precondition than this delete
            // effect can be safely ignored (in fact it could have been
            // pruned away before), because this fact could not be part of
            // the state on which the operator is applied.
            if (pddlMutexPairsIsMutexFactSet(mutex, fact_id, &op->pre))
                continue;

            pddlISetAdd(binfs, fact_id);
        }
    }

    if (mg_facts != NULL)
        FREE(mg_facts);
}

static int needNoneOfThoseOp(const pddl_iset_t *group,
                             const pddl_strips_op_t *op,
                             const pddl_mutex_pairs_t *mutex)
{
    if (!pddlISetIsDisjunct(group, &op->add_eff))
        return 0;

    int ret = 0;
    PDDL_ISET(inter);
    pddlISetIntersect2(&inter, group, &op->del_eff);
    if (pddlISetSize(&inter) > 0){
        int fact_id;
        PDDL_ISET_FOR_EACH(&inter, fact_id){
            if (!pddlMutexPairsIsMutexFactSet(mutex, fact_id, &op->pre)){
                ret = 1;
                break;
            }
        }
    }
    pddlISetFree(&inter);
    return ret;
}

static int needNoneOfThose(const pddl_iset_t *group,
                           const pddl_strips_t *strips,
                           const pddl_mutex_pairs_t *mutex)
{
    if (pddlISetIsDisjunct(group, &strips->init))
        return 1;
    if (pddlISetSize(group) == 1)
        return 1;

    const pddl_strips_op_t *op;
    PDDL_STRIPS_OPS_FOR_EACH(&strips->op, op){
        if (needNoneOfThoseOp(group, op, mutex))
            return 1;
    }
    return 0;
}

struct var {
    pddl_iset_t fact; /*!< List of STRIPS facts the variable will be created
                          from */
    int none_of_those; /*!< True if "none-of-those" is required */
};
typedef struct var var_t;

struct vars {
    var_t *var;
    int var_size;
    int var_alloc;

    pddl_iset_t covered; /*!< Facts that are already covered by the vars
                             above. */
    vars_mgroups_t mgroups;
};
typedef struct vars vars_t;

static void varsInit(vars_t *vars, const pddl_mgroups_t *mgroups)
{
    ZEROIZE(vars);
    vars->var_alloc = 8;
    vars->var = CALLOC_ARR(var_t, vars->var_alloc);
    varsMGroupsInit(&vars->mgroups, mgroups);
}

static void varsFree(vars_t *vars)
{
    for (int i = 0; i < vars->var_size; ++i)
        pddlISetFree(&vars->var[i].fact);
    if (vars->var != NULL)
        FREE(vars->var);
    pddlISetFree(&vars->covered);
    varsMGroupsFree(&vars->mgroups);
}

static void varsAdd(vars_t *vars,
                    const pddl_strips_t *strips,
                    const pddl_mutex_pairs_t *mutex,
                    const pddl_iset_t *facts)
{
    var_t *var;

    if (vars->var_size == vars->var_alloc){
        vars->var_alloc *= 2;
        vars->var = REALLOC_ARR(vars->var, var_t, vars->var_alloc);
    }

    var = vars->var + vars->var_size++;
    ZEROIZE(var);
    pddlISetUnion(&var->fact, facts);
    var->none_of_those = needNoneOfThose(&var->fact, strips, mutex);

    pddlISetUnion(&vars->covered, &var->fact);
    varsMGroupsCover(&vars->mgroups, facts);
}

static void findEssentialFacts(const pddl_strips_t *strips,
                               const pddl_mgroups_t *mgroup,
                               pddl_iset_t *essential)
{
    int *fact_mgroups;

    fact_mgroups = CALLOC_ARR(int, strips->fact.fact_size);
    for (int i = 0; i < mgroup->mgroup_size; ++i){
        int fact;
        PDDL_ISET_FOR_EACH(&mgroup->mgroup[i].mgroup, fact)
            ++fact_mgroups[fact];
    }

    for (int fact_id = 0; fact_id < strips->fact.fact_size; ++fact_id){
        if (fact_mgroups[fact_id] == 1)
            pddlISetAdd(essential, fact_id);
    }

    FREE(fact_mgroups);
}

static void allocateEssential(vars_t *vars,
                              const pddl_strips_t *strips,
                              const pddl_mgroups_t *mg,
                              const pddl_mutex_pairs_t *mutex)
{
    PDDL_ISET(essential);
    findEssentialFacts(strips, mg, &essential);

    while (vars->mgroups.has_uncovered){
        varsMGroupsSortUncoveredDesc(&vars->mgroups);
        ASSERT(pddlISetSize(&vars->mgroups.mgroup[0].uncovered) > 0);

        const vars_mgroup_t *ess;
        ess = varsMGroupsFindCovering(&vars->mgroups, &essential);
        if (ess != NULL){
            pddlISetMinus(&essential, &ess->uncovered);
            varsAdd(vars, strips, mutex, &ess->uncovered);
        }else{
            const vars_mgroup_t *m = vars->mgroups.mgroup + 0;
            pddlISetMinus(&essential, &m->uncovered);
            varsAdd(vars, strips, mutex, &m->uncovered);
        }
    }

    pddlISetFree(&essential);
}

static void allocateLargest(vars_t *vars,
                            const pddl_strips_t *strips,
                            const pddl_mgroups_t *mg,
                            const pddl_mutex_pairs_t *mutex)
{
    while (vars->mgroups.has_uncovered){
        varsMGroupsSortUncoveredDesc(&vars->mgroups);
        ASSERT(pddlISetSize(&vars->mgroups.mgroup[0].uncovered) > 0);
        const vars_mgroup_t *m = vars->mgroups.mgroup + 0;
        varsAdd(vars, strips, mutex, &m->uncovered);
    }
}

static void allocateLargestMulti(vars_t *vars,
                                 const pddl_strips_t *strips,
                                 const pddl_mgroups_t *mg,
                                 const pddl_mutex_pairs_t *mutex)
{
    while (vars->mgroups.has_uncovered){
        varsMGroupsSortUncoveredDesc(&vars->mgroups);
        ASSERT(pddlISetSize(&vars->mgroups.mgroup[0].uncovered) > 0);
        const vars_mgroup_t *m = vars->mgroups.mgroup + 0;
        varsAdd(vars, strips, mutex, &m->mgroup->mgroup);
    }
}

static void allocateUncoveredSingleFacts(vars_t *vars,
                                         const pddl_strips_t *strips,
                                         const pddl_mutex_pairs_t *mutex,
                                         unsigned flags)
{
    PDDL_ISET(var_facts);

    int *covered = CALLOC_ARR(int, strips->fact.fact_size);
    int fact_id;
    PDDL_ISET_FOR_EACH(&vars->covered, fact_id)
        covered[fact_id] = 1;

    for (int fact_id = 0; fact_id < strips->fact.fact_size; ++fact_id){
        if (covered[fact_id])
            continue;

        pddlISetEmpty(&var_facts);
        pddlISetAdd(&var_facts, fact_id);
        covered[fact_id] = 1;

        if (!(flags & PDDL_FDR_VARS_NO_NEGATED_FACTS)){
            int neg_of = strips->fact.fact[fact_id]->neg_of;
            if (neg_of >= 0 && !covered[neg_of]){
                pddlISetAdd(&var_facts, neg_of);
                covered[neg_of] = 1;
            }
        }
        varsAdd(vars, strips, mutex, &var_facts);
    }

    if (covered != NULL)
        FREE(covered);
    pddlISetFree(&var_facts);
}

static int allocateVars(vars_t *vars,
                        const pddl_strips_t *strips,
                        const pddl_mgroups_t *mg,
                        const pddl_mutex_pairs_t *mutex,
                        unsigned flags)
{
    PDDL_ISET(var_facts);
    PDDL_ISET(binary_facts);
    int fact;

    // Find facts that must be encoded in binary no mather what and create
    // variables from them
    factsRequiringBinaryEncoding(strips, mg, mutex, &binary_facts);
    PDDL_ISET_FOR_EACH(&binary_facts, fact){
        pddlISetEmpty(&var_facts);
        pddlISetAdd(&var_facts, fact);
        varsAdd(vars, strips, mutex, &var_facts);
    }

    unsigned method = flags & PDDL_FDR_VARS_METHOD_MASK;
    if (method == PDDL_FDR_VARS_ESSENTIAL_FIRST){
        allocateEssential(vars, strips, mg, mutex);
    }else if (method == PDDL_FDR_VARS_LARGEST_FIRST){
        allocateLargest(vars, strips, mg, mutex);
    }else if (method == PDDL_FDR_VARS_LARGEST_FIRST_MULTI){
        allocateLargestMulti(vars, strips, mg, mutex);
    }else{
        PANIC("Unspecified method for variable allocation.");
    }

    allocateUncoveredSingleFacts(vars, strips, mutex, flags);

    pddlISetFree(&var_facts);
    pddlISetFree(&binary_facts);
    return 0;
}

static void createVars(pddl_fdr_vars_t *fdr_vars,
                       const vars_t *vars,
                       const pddl_strips_t *strips)
{
    fdr_vars->strips_id_size = strips->fact.fact_size;
    fdr_vars->strips_id_to_val = CALLOC_ARR(pddl_iset_t,
                                            strips->fact.fact_size);
    fdr_vars->var_size = vars->var_size;
    fdr_vars->var = CALLOC_ARR(pddl_fdr_var_t, vars->var_size);

    // Determine number of global IDs
    fdr_vars->global_id_size = 0;
    for (int i = 0; i < vars->var_size; ++i){
        fdr_vars->global_id_size += pddlISetSize(&vars->var[i].fact);
        fdr_vars->global_id_size += (vars->var[i].none_of_those ? 1 : 0);
    }
    fdr_vars->global_id_to_val = CALLOC_ARR(pddl_fdr_val_t *,
                                            fdr_vars->global_id_size);

    int global_id = 0;
    for (int var_id = 0; var_id < vars->var_size; ++var_id){
        var_t *v = vars->var + var_id;
        pddl_fdr_var_t *var = fdr_vars->var + var_id;
        var->var_id = var_id;

        // Compute number of values in the variable's domain
        var->val_size = pddlISetSize(&v->fact) + (v->none_of_those ? 1 : 0);
        var->val = CALLOC_ARR(pddl_fdr_val_t, var->val_size);

        // Set variable, value, and global ID of the values and set up
        // mapping from global ID to the variable value.
        for (int val_id = 0; val_id < var->val_size; ++val_id){
            pddl_fdr_val_t *val = var->val + val_id;
            pddlFDRValInit(val);
            val->var_id = var->var_id;
            val->val_id = val_id;
            val->global_id = global_id++;
            fdr_vars->global_id_to_val[val->global_id] = val;
        }

        // Set up value names from STRIPS fact names and the mapping from
        // STRIPS ID to the variable values
        for (int val_id = 0; val_id < pddlISetSize(&v->fact); ++val_id){
            int fact = pddlISetGet(&v->fact, val_id);
            pddl_fdr_val_t *val = var->val + val_id;
            if (strips->fact.fact[fact]->name != NULL)
                val->name = STRDUP(strips->fact.fact[fact]->name);
            val->strips_id = fact;
            pddlISetAdd(&fdr_vars->strips_id_to_val[fact], val->global_id);
        }

        // Set up "none-of-those" value
        var->val_none_of_those = -1;
        if (v->none_of_those){
            var->val_none_of_those = var->val_size - 1;
            pddl_fdr_val_t *val = var->val + var->val_none_of_those;
            val->name = STRDUP("none-of-those");
            val->strips_id = -1;
        }
    }
}

int pddlFDRVarsInitFromStrips(pddl_fdr_vars_t *fdr_vars,
                              const pddl_strips_t *strips,
                              const pddl_mgroups_t *mg,
                              const pddl_mutex_pairs_t *_mutex,
                              unsigned flags)
{
    vars_t vars;
    pddl_mutex_pairs_t mutex;

    pddlMutexPairsInitCopy(&mutex, _mutex);
    if (!(flags & PDDL_FDR_VARS_NO_NEGATED_FACTS)){
        for (int fact_id = 0; fact_id < strips->fact.fact_size; ++fact_id){
            const pddl_fact_t *fact = strips->fact.fact[fact_id];
            if (fact->neg_of > fact_id)
                pddlMutexPairsAdd(&mutex, fact_id, fact->neg_of);
        }
    }

    ZEROIZE(fdr_vars);

    varsInit(&vars, mg);
    if (allocateVars(&vars, strips, mg, &mutex, flags) != 0){
        varsFree(&vars);
        pddlMutexPairsFree(&mutex);
        return -1;
    }

    createVars(fdr_vars, &vars, strips);
    varsFree(&vars);
    pddlMutexPairsFree(&mutex);

    return 0;
}

void pddlFDRVarsFree(pddl_fdr_vars_t *vars)
{
    if (vars->global_id_to_val != NULL)
        FREE(vars->global_id_to_val);
    for (int i = 0; i < vars->strips_id_size; ++i)
        pddlISetFree(vars->strips_id_to_val + i);
    if (vars->strips_id_to_val != NULL)
        FREE(vars->strips_id_to_val);
    for (int i = 0; i < vars->var_size; ++i)
        pddlFDRVarFree(vars->var + i);
    if (vars->var != NULL)
        FREE(vars->var);
}

static void pddlFDRValCopy(pddl_fdr_val_t *dst, const pddl_fdr_val_t *src)
{
    ZEROIZE(dst);
    if (src->name != NULL)
        dst->name = STRDUP(src->name);
    dst->var_id = src->var_id;
    dst->val_id = src->val_id;
    dst->global_id = src->global_id;
    dst->strips_id = src->strips_id;
}

static void pddlFDRVarCopy(pddl_fdr_var_t *dst, const pddl_fdr_var_t *src)
{
    ZEROIZE(dst);
    dst->var_id = src->var_id;
    dst->val_size = src->val_size;
    dst->val = CALLOC_ARR(pddl_fdr_val_t, dst->val_size);
    for (int i = 0; i < dst->val_size; ++i)
        pddlFDRValCopy(dst->val + i, src->val + i);
    dst->val_none_of_those = src->val_none_of_those;
}

void pddlFDRVarsInitCopy(pddl_fdr_vars_t *dst, const pddl_fdr_vars_t *src)
{
    ZEROIZE(dst);
    dst->var_size = src->var_size;
    dst->var = CALLOC_ARR(pddl_fdr_var_t, dst->var_size);
    for (int i = 0; i < dst->var_size; ++i)
        pddlFDRVarCopy(dst->var + i, src->var + i);

    dst->global_id_size = src->global_id_size;
    if (dst->global_id_size > 0){
        dst->global_id_to_val = ALLOC_ARR(pddl_fdr_val_t *,
                                          dst->global_id_size);
        for (int i = 0; i < src->global_id_size; ++i){
            const pddl_fdr_val_t *sval = src->global_id_to_val[i];
            pddl_fdr_val_t *val = dst->var[sval->var_id].val + sval->val_id;
            dst->global_id_to_val[i] = val;
        }
    }

    dst->strips_id_size = src->strips_id_size;
    if (dst->strips_id_size > 0){
        dst->strips_id_to_val = CALLOC_ARR(pddl_iset_t, dst->strips_id_size);
        for (int i = 0; i < src->strips_id_size; ++i)
            pddlISetUnion(&dst->strips_id_to_val[i], &src->strips_id_to_val[i]);
    }
}

void pddlFDRVarsRemapFree(pddl_fdr_vars_remap_t *remap)
{
    for (int v = 0; v < remap->var_size; ++v)
        FREE(remap->remap[v]);
    if (remap->remap != NULL)
        FREE(remap->remap);
}

void pddlFDRVarsDelFacts(pddl_fdr_vars_t *vars,
                         const pddl_iset_t *del_facts,
                         pddl_fdr_vars_remap_t *remap)
{
    ZEROIZE(remap);
    remap->var_size = vars->var_size;
    remap->remap = ALLOC_ARR(const pddl_fdr_val_t **, remap->var_size);
    for (int v = 0; v < remap->var_size; ++v){
        remap->remap[v] = CALLOC_ARR(const pddl_fdr_val_t *,
                                     vars->var[v].val_size);
    }

    pddl_fdr_val_t delval;
    int fact_id;
    PDDL_ISET_FOR_EACH(del_facts, fact_id){
        const pddl_fdr_val_t *val = vars->global_id_to_val[fact_id];
        remap->remap[val->var_id][val->val_id] = &delval;
    }

    int global_id = 0;
    int var_ins = 0;
    for (int var_id = 0; var_id < vars->var_size; ++var_id){
        if (var_ins != var_id)
            vars->var[var_ins] = vars->var[var_id];
        pddl_fdr_var_t *var = vars->var + var_ins;
        var->var_id = var_ins;

        int val_ins = 0;
        for (int val_id = 0; val_id < var->val_size; ++val_id){
            pddl_fdr_val_t *val = var->val + val_id;
            if (remap->remap[var_id][val_id] == &delval){
                pddlFDRValFree(val);
                if (var->val_none_of_those == val_id)
                    var->val_none_of_those = -1;
                remap->remap[var_id][val_id] = NULL;

            }else{
                if (val_ins != val->val_id)
                    var->val[val_ins] = *val;
                val = var->val + val_ins;
                val->var_id = var_ins;
                val->val_id = val_ins;
                remap->remap[var_id][val_id] = val;
                vars->global_id_to_val[global_id] = val;
                val->global_id = global_id++;
                if (var->val_none_of_those == val_id)
                    var->val_none_of_those = val_ins;
                ++val_ins;
            }
        }
        if (val_ins <= 1){
            if (val_ins == 1){
                ZEROIZE_ARR(remap->remap[var_id], var->val_size);
                --global_id;
            }
            var->val_size = val_ins;
            pddlFDRVarFree(var);

        }else{
            var->val_size = val_ins;
            ++var_ins;
        }
    }

    vars->var_size = var_ins;
    vars->global_id_size = global_id;
}

pddl_fdr_val_t *pddlFDRVarsAddVal(pddl_fdr_vars_t *vars,
                                  int var_id,
                                  const char *name)
{
    pddl_fdr_var_t *var = vars->var + var_id;
    pddl_fdr_val_t *orig_ptr = var->val;
    ++var->val_size;
    var->val = REALLOC_ARR(var->val, pddl_fdr_val_t, var->val_size);

    pddl_fdr_val_t *val = var->val + var->val_size - 1;
    ZEROIZE(val);
    if (name != NULL)
        val->name = STRDUP(name);
    val->var_id = var_id;
    val->val_id = var->val_size - 1;
    val->global_id = vars->global_id_size++;
    vars->global_id_to_val = REALLOC_ARR(vars->global_id_to_val,
                                         pddl_fdr_val_t *,
                                         vars->global_id_size);
    vars->global_id_to_val[vars->global_id_size - 1] = val;
    val->strips_id = -1;

    // Fix the invalidated pointers
    if (orig_ptr != var->val){
        for (int vi = 0; vi < var->val_size; ++vi)
            vars->global_id_to_val[var->val[vi].global_id] = var->val + vi;
    }
    return val;
}

void pddlFDRVarsRemap(pddl_fdr_vars_t *vars, const int *remap)
{
    pddl_fdr_var_t *var_tmp = ALLOC_ARR(pddl_fdr_var_t, vars->var_size);
    memcpy(var_tmp, vars->var, sizeof(pddl_fdr_var_t) * vars->var_size);
    for (int var_id = 0; var_id < vars->var_size; ++var_id){
        vars->var[remap[var_id]] = var_tmp[var_id];
        pddl_fdr_var_t *var = vars->var + remap[var_id];
        var->var_id = remap[var_id];
        for (int val_id = 0; val_id < var->val_size; ++val_id)
            var->val[val_id].var_id = remap[var_id];
    }
    FREE(var_tmp);
}

void pddlFDRVarsPrintDebug(const pddl_fdr_vars_t *vars, FILE *fout)
{
    fprintf(fout, "Vars (%d):\n", vars->var_size);
    for (int vi = 0; vi < vars->var_size; ++vi){
        const pddl_fdr_var_t *var = vars->var + vi;
        fprintf(fout, "  Var %d:\n", var->var_id);
        for (int vali = 0; vali < var->val_size; ++vali){
            const pddl_fdr_val_t *val = var->val + vali;
            fprintf(fout, "    %d: %s (%d)\n", val->val_id, val->name,
                    val->global_id);
        }
    }
}

void pddlFDRVarsPrintTable(const pddl_fdr_vars_t *vars,
                           int linesize,
                           FILE *fout,
                           pddl_err_t *err)
{
    char line[256];
    pddl_outboxes_t boxes;
    pddlOutBoxesInit(&boxes);
    for (int vari = 0; vari < vars->var_size; ++vari){
        const pddl_fdr_var_t *var = vars->var + vari;
        pddl_outbox_t *box = pddlOutBoxesAdd(&boxes);
        snprintf(line, 256, "ID: %d, size: %d, none-of-those: %d,"
                            " is-black: %d",
                 var->var_id, var->val_size, var->val_none_of_those,
                 var->is_black);
        pddlOutBoxAddLine(box, line);
        for (int vali = 0; vali < var->val_size; ++vali){
            const pddl_fdr_val_t *val = var->val + vali;
            snprintf(line, 256, "%d:(%s), global-id: %d, strips-id: %d",
                     val->val_id, (val->name != NULL ? val->name : "NULL"),
                     val->global_id, val->strips_id);
            pddlOutBoxAddLine(box, line);
        }
    }

    pddl_outboxes_t merged;
    pddlOutBoxesInit(&merged);
    pddlOutBoxesMerge(&merged, &boxes, linesize);
    pddlOutBoxesPrint(&merged, fout, err);
    pddlOutBoxesFree(&merged);

    pddlOutBoxesFree(&boxes);
}
