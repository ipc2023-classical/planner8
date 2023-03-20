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

#include "pddl/pddl.h"
#include "pddl/strips.h"
#include "internal.h"

void pddlGroundConfigLog(const pddl_ground_config_t *cfg, pddl_err_t *err)
{
    if (cfg->lifted_mgroups == NULL){
        LOG(err, "lifted_mgroups->mgroup_size = 0");
    }else{
        LOG_CONFIG_INT(cfg, lifted_mgroups->mgroup_size, err);
    }
    LOG_CONFIG_BOOL(cfg, prune_op_pre_mutex, err);
    LOG_CONFIG_BOOL(cfg, prune_op_dead_end, err);
    LOG_CONFIG_BOOL(cfg, remove_static_facts, err);
}

static void copyBasicInfo(pddl_strips_t *dst, const pddl_strips_t *src)
{
    if (src->domain_name)
        dst->domain_name = STRDUP(src->domain_name);
    if (src->problem_name)
        dst->problem_name = STRDUP(src->problem_name);
    if (src->domain_file)
        dst->domain_file = STRDUP(src->domain_file);
    if (src->problem_file)
        dst->problem_file = STRDUP(src->problem_file);
}

void pddlStripsInit(pddl_strips_t *strips)
{
    ZEROIZE(strips);
    pddlFactsInit(&strips->fact);
    pddlStripsOpsInit(&strips->op);
    pddlISetInit(&strips->init);
    pddlISetInit(&strips->goal);
}

void pddlStripsMakeUnsolvable(pddl_strips_t *strips)
{
    pddlStripsFree(strips);
    pddlStripsInit(strips);

    int f_init, f_goal, f_aux;
    pddl_fact_t fact;
    pddlFactInit(&fact);
    fact.name = STRDUP("I");
    f_init = pddlFactsAdd(&strips->fact, &fact);
    pddlFactFree(&fact);

    pddlFactInit(&fact);
    fact.name = STRDUP("G");
    f_goal = pddlFactsAdd(&strips->fact, &fact);
    pddlFactFree(&fact);

    pddlFactInit(&fact);
    fact.name = STRDUP("P");
    f_aux = pddlFactsAdd(&strips->fact, &fact);
    pddlFactFree(&fact);

    pddlISetAdd(&strips->init, f_init);
    pddlISetAdd(&strips->goal, f_goal);

    pddl_strips_op_t op;
    pddlStripsOpInit(&op);
    pddlISetAdd(&op.pre, f_aux);
    pddlISetAdd(&op.add_eff, f_goal);
    pddlStripsOpFinalize(&op, STRDUP("unreachable-op"));
    pddlStripsOpsAdd(&strips->op, &op);
    pddlStripsOpFree(&op);

    strips->goal_is_unreachable = 1;
}

void pddlStripsFree(pddl_strips_t *strips)
{
    if (strips->domain_name)
        FREE(strips->domain_name);
    if (strips->problem_name)
        FREE(strips->problem_name);
    if (strips->domain_file)
        FREE(strips->domain_file);
    if (strips->problem_file)
        FREE(strips->problem_file);
    pddlFactsFree(&strips->fact);
    pddlStripsOpsFree(&strips->op);
    pddlISetFree(&strips->init);
    pddlISetFree(&strips->goal);
    ZEROIZE(strips);
}

void pddlStripsInitCopy(pddl_strips_t *dst, const pddl_strips_t *src)
{
    pddlStripsInit(dst);
    copyBasicInfo(dst, src);
    dst->cfg = src->cfg;

    pddlFactsCopy(&dst->fact, &src->fact);
    pddlStripsOpsCopy(&dst->op, &src->op);
    pddlISetUnion(&dst->init, &src->init);
    pddlISetUnion(&dst->goal, &src->goal);
    dst->goal_is_unreachable = src->goal_is_unreachable;
    dst->has_cond_eff = src->has_cond_eff;
}

// TODO: Make public function
static int addNegFact(pddl_strips_t *strips, int fact_id)
{
    pddl_fact_t *fact = strips->fact.fact[fact_id];
    ASSERT_RUNTIME(fact->neg_of == -1);

    pddl_fact_t neg;
    char name[512];
    snprintf(name, 512, "NOT-%s", fact->name);
    pddlFactInit(&neg);
    neg.name = STRDUP(name);
    int neg_id = pddlFactsAdd(&strips->fact, &neg);
    pddlFactFree(&neg);

    pddl_fact_t *neg_fact = strips->fact.fact[neg_id];
    neg_fact->neg_of = fact_id;
    fact->neg_of = neg_id;
    neg_fact->is_private = fact->is_private;

    for (int opi = 0; opi < strips->op.op_size; ++opi){
        pddl_strips_op_t *op = strips->op.op[opi];
        if (pddlISetIn(fact_id, &op->del_eff))
            pddlISetAdd(&op->add_eff, neg_id);
        if (pddlISetIn(fact_id, &op->add_eff))
            pddlISetAdd(&op->del_eff, neg_id);
        for (int cei = 0; cei < op->cond_eff_size; ++cei){
            pddl_strips_op_cond_eff_t *ce = op->cond_eff + cei;
            if (pddlISetIn(fact_id, &ce->del_eff))
                pddlISetAdd(&ce->add_eff, neg_id);
            if (pddlISetIn(fact_id, &ce->add_eff))
                pddlISetAdd(&ce->del_eff, neg_id);
        }
    }

    if (!pddlISetIn(fact_id, &strips->init))
        pddlISetAdd(&strips->init, neg_id);

    return neg_id;
}

static void opCompileAwayCondEffNegPreRec(pddl_strips_t *strips,
                                          const pddl_strips_op_t *op_in,
                                          const pddl_iset_t *neg_pre,
                                          int neg_pre_size,
                                          int cur_neg_pre)
{
    pddl_strips_op_t op;

    int neg_fact;
    PDDL_ISET_FOR_EACH(neg_pre + cur_neg_pre, neg_fact){
        if (pddlISetIn(strips->fact.fact[neg_fact]->neg_of, &op_in->pre))
            continue;
        pddlStripsOpInit(&op);
        pddlStripsOpCopyWithoutCondEff(&op, op_in);
        pddlISetAdd(&op.pre, neg_fact);

        if (cur_neg_pre == neg_pre_size - 1){
            pddlStripsOpNormalize(&op);
            if (pddlISetSize(&op.add_eff) > 0)
                pddlStripsOpsAdd(&strips->op, &op);
        }else{
            opCompileAwayCondEffNegPreRec(strips, &op, neg_pre, neg_pre_size,
                                          cur_neg_pre + 1);
        }

        pddlStripsOpFree(&op);
    }
}

static void opCompileAwayCondEffNegPre(pddl_strips_t *strips,
                                       const pddl_strips_op_t *src_op,
                                       const pddl_strips_op_t *op,
                                       const int *neg_ce,
                                       int neg_ce_size)
{
    // Prepare negative preconditions
    pddl_iset_t neg_pre[neg_ce_size];
    for (int i = 0; i < neg_ce_size; ++i)
        pddlISetInit(neg_pre + i);

    for (int i = 0; i < neg_ce_size; ++i){
        const pddl_strips_op_cond_eff_t *ce = src_op->cond_eff + neg_ce[i];
        int fact_id;
        PDDL_ISET_FOR_EACH(&ce->pre, fact_id){
            int neg_fact_id = strips->fact.fact[fact_id]->neg_of;
            ASSERT_RUNTIME(neg_fact_id >= 0);
            pddlISetAdd(&neg_pre[i], neg_fact_id);
        }
    }

    opCompileAwayCondEffNegPreRec(strips, op, neg_pre, neg_ce_size, 0);

    for (int i = 0; i < neg_ce_size; ++i)
        pddlISetFree(neg_pre + i);
}

static void opCompileAwayCondEffComb(pddl_strips_t *strips,
                                     const pddl_strips_op_t *src_op,
                                     const int *neg_ce,
                                     int neg_ce_size,
                                     const int *pos_ce,
                                     int pos_ce_size)
{
    pddl_strips_op_t op;

    pddlStripsOpInit(&op);
    pddlStripsOpCopyWithoutCondEff(&op, src_op);

    // First merge in positive conditional effects
    for (int i = 0; i < pos_ce_size; ++i){
        pddlISetUnion(&op.pre, &src_op->cond_eff[pos_ce[i]].pre);
        pddlISetMinus(&op.add_eff, &src_op->cond_eff[pos_ce[i]].del_eff);
        pddlISetUnion(&op.del_eff, &src_op->cond_eff[pos_ce[i]].del_eff);
        pddlISetUnion(&op.add_eff, &src_op->cond_eff[pos_ce[i]].add_eff);
    }

    if (neg_ce_size > 0){
        // Then, recursivelly, set up negative preconditions
        opCompileAwayCondEffNegPre(strips, src_op, &op, neg_ce, neg_ce_size);
    }else{
        // Or add operator if there are no negative preconditions
        pddlStripsOpNormalize(&op);
        if (pddlISetSize(&op.add_eff) > 0)
            pddlStripsOpsAdd(&strips->op, &op);
    }

    pddlStripsOpFree(&op);
}

static void opCompileAwayCondEff(pddl_strips_t *strips,
                                 const pddl_strips_op_t *op)
{
    ASSERT_RUNTIME(op->cond_eff_size < sizeof(unsigned long) * 8);
    int neg_ce[op->cond_eff_size];
    int neg_ce_size;
    int pos_ce[op->cond_eff_size];
    int pos_ce_size;

    unsigned long max = 1ul << op->cond_eff_size;
    for (unsigned long comb = 0; comb < max; ++comb){
        unsigned long c = comb;
        neg_ce_size = pos_ce_size = 0;
        for (int i = 0; i < op->cond_eff_size; ++i){
            if (c & 0x1ul){
                pos_ce[pos_ce_size++] = i;
            }else{
                neg_ce[neg_ce_size++] = i;
            }
            c >>= 1ul;
        }
        opCompileAwayCondEffComb(strips, op, neg_ce, neg_ce_size,
                                 pos_ce, pos_ce_size);
    }
}

static void compileAwayCondEffCreateNegFacts(pddl_strips_t *strips)
{
    int fact_size = strips->fact.fact_size;

    for (int opi = 0; opi < strips->op.op_size; ++opi){
        const pddl_strips_op_t *op = strips->op.op[opi];
        for (int cei = 0; cei < op->cond_eff_size; ++cei){
            const pddl_strips_op_cond_eff_t *ce = op->cond_eff + cei;
            int fact_id;
            PDDL_ISET_FOR_EACH(&ce->pre, fact_id){
                if (strips->fact.fact[fact_id]->neg_of == -1){
                    addNegFact(strips, fact_id);
                    ASSERT_RUNTIME(strips->fact.fact[fact_id]->neg_of != -1);
                }
            }
        }
    }

    if (strips->fact.fact_size != fact_size){
        // Sort facts if any was added
        int *fact_remap;
        fact_remap = CALLOC_ARR(int, strips->fact.fact_size);
        pddlFactsSort(&strips->fact, fact_remap);
        pddlISetRemap(&strips->init, fact_remap);
        pddlISetRemap(&strips->goal, fact_remap);
        pddlStripsOpsRemapFacts(&strips->op, fact_remap);
        FREE(fact_remap);
    }
}

void pddlStripsCompileAwayCondEff(pddl_strips_t *strips)
{
    if (!strips->has_cond_eff || strips->op.op_size == 0)
        return;

    // First create all negations that we might need
    compileAwayCondEffCreateNegFacts(strips);

    int op_size = strips->op.op_size;
    for (int opi = 0; opi < op_size; ++opi){
        const pddl_strips_op_t *op = strips->op.op[opi];
        if (op->cond_eff_size > 0)
            opCompileAwayCondEff(strips, op);
    }

    // Remove operators with conditional effects
    int *op_map;
    op_map = CALLOC_ARR(int, strips->op.op_size);
    for (int opi = 0; opi < strips->op.op_size; ++opi){
        if (strips->op.op[opi]->cond_eff_size > 0)
            op_map[opi] = 1;
    }
    pddlStripsOpsDelOps(&strips->op, op_map);
    FREE(op_map);

    // Remove duplicate operators
    pddlStripsOpsDeduplicate(&strips->op);
    // And sort operators to get deterministinc results.
    pddlStripsOpsSort(&strips->op);

    strips->has_cond_eff = 0;
}

void pddlStripsCrossRefFactsOps(const pddl_strips_t *strips,
                                void *_fact_arr,
                                unsigned long el_size,
                                long pre_offset,
                                long add_offset,
                                long del_offset)
{
    char *fact_arr = _fact_arr;
    for (int op_id = 0; op_id < strips->op.op_size; ++op_id){
        const pddl_strips_op_t *op = strips->op.op[op_id];
        int fact_id;

        if (pre_offset >= 0){
            PDDL_ISET_FOR_EACH(&op->pre, fact_id){
                char *el = fact_arr + (el_size * fact_id);
                pddl_iset_t *s = (pddl_iset_t *)(el + pre_offset);
                pddlISetAdd(s, op_id);
            }
        }

        if (add_offset >= 0){
            PDDL_ISET_FOR_EACH(&op->add_eff, fact_id){
                char *el = fact_arr + (el_size * fact_id);
                pddl_iset_t *s = (pddl_iset_t *)(el + add_offset);
                pddlISetAdd(s, op_id);
            }
        }

        if (del_offset >= 0){
            PDDL_ISET_FOR_EACH(&op->del_eff, fact_id){
                char *el = fact_arr + (el_size * fact_id);
                pddl_iset_t *s = (pddl_iset_t *)(el + del_offset);
                pddlISetAdd(s, op_id);
            }
        }
    }
}

void pddlStripsApplicableOps(const pddl_strips_t *strips,
                             const pddl_iset_t *state,
                             pddl_iset_t *app_ops)
{
    for (int i = 0; i < strips->op.op_size; ++i){
        const pddl_strips_op_t *op = strips->op.op[i];
        if (pddlISetIsSubset(&op->pre, state))
            pddlISetAdd(app_ops, i);
    }
}

static int isFAMGroup(const pddl_iset_t *facts,
                      const pddl_iset_t *pre,
                      const pddl_iset_t *add_eff,
                      const pddl_iset_t *del_eff)
{
    PDDL_ISET(predel);

    pddlISetIntersect2(&predel, pre, del_eff);
    int add_size = pddlISetIntersectionSize(add_eff, facts);
    if (add_size > pddlISetIntersectionSize(&predel, facts)){
        pddlISetFree(&predel);
        return 0;
    }

    pddlISetFree(&predel);
    return 1;
}

static int condEffAreDisjunct(const pddl_strips_op_t *op,
                              const pddl_iset_t *facts)        
{
    PDDL_ISET(del_eff);
    PDDL_ISET(del_eff2);
    PDDL_ISET(add_eff);
    PDDL_ISET(add_eff2);

    for (int cei = 0; cei < op->cond_eff_size; ++cei){
        pddlISetIntersect2(&del_eff, &op->cond_eff[cei].del_eff, facts);
        pddlISetIntersect2(&add_eff, &op->cond_eff[cei].add_eff, facts);

        for (int cei2 = cei + 1; cei2 < op->cond_eff_size; ++cei2){
            pddlISetIntersect2(&del_eff2, &op->cond_eff[cei2].del_eff, facts);
            pddlISetIntersect2(&add_eff2, &op->cond_eff[cei2].add_eff, facts);
            if (!pddlISetIsDisjunct(&del_eff, &del_eff2)
                    || !pddlISetIsDisjunct(&add_eff, &add_eff2)){
                pddlISetFree(&del_eff);
                pddlISetFree(&del_eff2);
                pddlISetFree(&add_eff);
                pddlISetFree(&add_eff2);
                return 0;
            }
        }
    }

    pddlISetFree(&del_eff);
    pddlISetFree(&del_eff2);
    pddlISetFree(&add_eff);
    pddlISetFree(&add_eff2);

    return 1;
}

static int isFAMGroupCERec(const pddl_strips_t *strips,
                           const pddl_iset_t *facts,
                           const pddl_strips_op_t *op,
                           int cond_eff_i,
                           const pddl_iset_t *pre_in,
                           const pddl_iset_t *add_eff_in,
                           const pddl_iset_t *del_eff_in)
{
    PDDL_ISET(pre);
    PDDL_ISET(add_eff);
    PDDL_ISET(del_eff);

    for (int cei = cond_eff_i; cei < op->cond_eff_size; ++cei){
        pddlISetUnion2(&pre, pre_in, &op->cond_eff[cei].pre);
        pddlISetUnion2(&add_eff, add_eff_in, &op->cond_eff[cei].add_eff);
        pddlISetUnion2(&del_eff, del_eff_in, &op->cond_eff[cei].del_eff);
        if (!isFAMGroup(facts, &pre, &add_eff, &del_eff)){
            pddlISetFree(&pre);
            pddlISetFree(&add_eff);
            pddlISetFree(&del_eff);
            return 0;
        }

        if (cond_eff_i + 1 < op->cond_eff_size){
            if (!isFAMGroupCERec(strips, facts, op, cond_eff_i + 1,
                                 &pre, &add_eff, &del_eff)){
                pddlISetFree(&pre);
                pddlISetFree(&add_eff);
                pddlISetFree(&del_eff);
                return 0;
            }
        }
    }

    pddlISetFree(&pre);
    pddlISetFree(&add_eff);
    pddlISetFree(&del_eff);
    return 1;
}

static int isFAMGroupCE(const pddl_strips_t *strips,
                        const pddl_iset_t *facts,
                        const pddl_strips_op_t *op)
{
    if (condEffAreDisjunct(op, facts)){
        PDDL_ISET(pre);
        PDDL_ISET(add_eff);
        PDDL_ISET(del_eff);

        for (int cei = 0; cei < op->cond_eff_size; ++cei){
            pddlISetUnion2(&pre, &op->pre, &op->cond_eff[cei].pre);
            pddlISetUnion2(&add_eff, &op->add_eff, &op->cond_eff[cei].add_eff);
            pddlISetUnion2(&del_eff, &op->del_eff, &op->cond_eff[cei].del_eff);
            if (!isFAMGroup(facts, &pre, &add_eff, &del_eff)){
                pddlISetFree(&pre);
                pddlISetFree(&add_eff);
                pddlISetFree(&del_eff);
                return 0;
            }
        }

        pddlISetFree(&pre);
        pddlISetFree(&add_eff);
        pddlISetFree(&del_eff);
        return 1;

    }else{
        return isFAMGroupCERec(strips, facts, op, 0,
                               &op->pre, &op->add_eff, &op->del_eff);
    }
}

int pddlStripsIsFAMGroup(const pddl_strips_t *strips, const pddl_iset_t *facts)
{
    for (int oi = 0; oi < strips->op.op_size; ++oi){
        const pddl_strips_op_t *op = strips->op.op[oi];
        if (!isFAMGroup(facts, &op->pre, &op->add_eff, &op->del_eff))
            return 0;

        if (op->cond_eff_size > 0 && !isFAMGroupCE(strips, facts, op))
            return 0;
    }

    return 1;
}

int pddlStripsIsExactlyOneMGroup(const pddl_strips_t *strips,
                                 const pddl_iset_t *facts)
{
    if (pddlISetIsDisjunct(facts, &strips->init))
        return 0;

    for (int op_id = 0; op_id < strips->op.op_size; ++op_id){
        const pddl_strips_op_t *op = strips->op.op[op_id];
        if (!pddlISetIsDisjunct(&op->del_eff, facts)
                && pddlISetIsDisjunct(&op->add_eff, facts)){
            return 0;
        }

        for (int ce_id = 0; ce_id < op->cond_eff_size; ++ce_id){
            const pddl_strips_op_cond_eff_t *ce = op->cond_eff + ce_id;
            if (!pddlISetIsDisjunct(&ce->del_eff, facts)
                    && pddlISetIsDisjunct(&ce->add_eff, facts)
                    && pddlISetIsDisjunct(&op->add_eff, facts)){
                return 0;
            }
        }
    }

    return 1;
}

static void resetHasCondEffFlag(pddl_strips_t *strips)
{
    int has_cond_eff = 0;
    for (int op_id = 0; op_id < strips->op.op_size; ++op_id){
        if (strips->op.op[op_id]->cond_eff_size > 0){
            has_cond_eff = 1;
            break;
        }
    }
    strips->has_cond_eff = has_cond_eff;
}

int pddlStripsMergeCondEffIfPossible(pddl_strips_t *strips)
{
    if (!strips->has_cond_eff)
        return 0;

    int change = 0;
    for (int op_id = 0; op_id < strips->op.op_size; ++op_id){
        pddl_strips_op_t *op = strips->op.op[op_id];
        int ins = 0;
        for (int cei = 0; cei < op->cond_eff_size; ++cei){
            if (pddlISetIsSubset(&op->cond_eff[cei].pre, &op->pre)){
                pddlISetUnion(&op->add_eff, &op->cond_eff[cei].add_eff);
                pddlISetUnion(&op->del_eff, &op->cond_eff[cei].del_eff);
                pddlISetFree(&op->cond_eff[cei].pre);
                pddlISetFree(&op->cond_eff[cei].add_eff);
                pddlISetFree(&op->cond_eff[cei].del_eff);
                change = 1;
            }else{
                op->cond_eff[ins++] = op->cond_eff[cei];
            }
        }
        op->cond_eff_size = ins;
    }

    for (int op_id = 0; op_id < strips->op.op_size; ++op_id){
        pddl_strips_op_t *op = strips->op.op[op_id];
        if (op->cond_eff_size == 0)
            continue;
        if (pddlISetSize(&op->add_eff) > 0 || pddlISetSize(&op->del_eff) > 0)
            continue;

        int can_flatten = 1;
        for (int cei = 1; cei < op->cond_eff_size; ++cei){
            if (!pddlISetEq(&op->cond_eff[cei - 1].pre, &op->cond_eff[cei].pre)){
                can_flatten = 0;
                break;
            }
        }

        if (can_flatten){
            pddlISetUnion(&op->pre, &op->cond_eff[0].pre);
            for (int cei = 0; cei < op->cond_eff_size; ++cei){
                pddlISetUnion(&op->del_eff, &op->cond_eff[cei].del_eff);
                pddlISetUnion(&op->add_eff, &op->cond_eff[cei].add_eff);
            }

            pddlStripsOpFreeAllCondEffs(op);
            pddlStripsOpNormalize(op);
            change = 1;
        }
    }

    if (change){
        resetHasCondEffFlag(strips);
        return 1;
    }
    return 0;
}

void pddlStripsReduce(pddl_strips_t *strips,
                      const pddl_iset_t *del_facts,
                      const pddl_iset_t *del_ops)
{
    if (del_ops != NULL && pddlISetSize(del_ops) > 0)
        pddlStripsOpsDelOpsSet(&strips->op, del_ops);

    if (del_facts != NULL && pddlISetSize(del_facts) > 0){
        pddlStripsOpsRemoveFacts(&strips->op, del_facts);

        int *remap_fact = CALLOC_ARR(int, strips->fact.fact_size);
        pddlFactsDelFacts(&strips->fact, del_facts, remap_fact);
        pddlStripsOpsRemapFacts(&strips->op, remap_fact);

        pddlISetMinus(&strips->init, del_facts);
        pddlISetRemap(&strips->init, remap_fact);
        pddlISetMinus(&strips->goal, del_facts);
        pddlISetRemap(&strips->goal, remap_fact);

        if (remap_fact != NULL)
            FREE(remap_fact);

        PDDL_ISET(useless_ops);
        for (int op_id = 0; op_id < strips->op.op_size; ++op_id){
            const pddl_strips_op_t *op = strips->op.op[op_id];
            if (pddlISetSize(&op->add_eff) == 0
                    && pddlISetSize(&op->del_eff) == 0
                    && op->cond_eff_size == 0){
                pddlISetAdd(&useless_ops, op_id);
            }
        }
        if (pddlISetSize(&useless_ops) > 0)
            pddlStripsOpsDelOpsSet(&strips->op, &useless_ops);
        pddlISetFree(&useless_ops);

        if (strips->has_cond_eff)
            resetHasCondEffFlag(strips);
    }
}

int pddlStripsRemoveStaticFacts(pddl_strips_t *strips, pddl_err_t *err)
{
    int num = 0;
    int *nonstatic_facts = CALLOC_ARR(int, strips->fact.fact_size);

    int fact;
    PDDL_ISET_FOR_EACH(&strips->init, fact)
        nonstatic_facts[fact] = -1;

    for (int op_id = 0; op_id < strips->op.op_size; ++op_id){
        const pddl_strips_op_t *op = strips->op.op[op_id];
        int fact;
        PDDL_ISET_FOR_EACH(&op->add_eff, fact){
            if (nonstatic_facts[fact] == 0)
                nonstatic_facts[fact] = 1;
        }
        PDDL_ISET_FOR_EACH(&op->del_eff, fact)
            nonstatic_facts[fact] = 1;
        for (int ce_id = 0; ce_id < op->cond_eff_size; ++ce_id){
            const pddl_strips_op_cond_eff_t *ce = op->cond_eff + ce_id;
            PDDL_ISET_FOR_EACH(&ce->add_eff, fact){
                if (nonstatic_facts[fact] == 0)
                    nonstatic_facts[fact] = 1;
            }
            PDDL_ISET_FOR_EACH(&ce->del_eff, fact)
                nonstatic_facts[fact] = 1;
        }
    }

    PDDL_ISET(del_facts);
    for (int fact_id = 0; fact_id < strips->fact.fact_size; ++fact_id){
        if (nonstatic_facts[fact_id] <= 0){
            pddlISetAdd(&del_facts, fact_id);
            ++num;
        }
    }

    PDDL_LOG(err, "Found %{found_static_facts}d static facts",
             pddlISetSize(&del_facts));
    if (pddlISetSize(&del_facts) > 0){
        pddlStripsReduce(strips, &del_facts, NULL);
        PDDL_LOG(err, "Removed %{rm_static_facts}d static facts",
                 pddlISetSize(&del_facts));
    }

    pddlISetFree(&del_facts);
    if (nonstatic_facts != NULL)
        FREE(nonstatic_facts);

    return num;
}

static int isDelEffMutex(const pddl_mutex_pairs_t *mutex,
                         const pddl_iset_t *pre,
                         const pddl_iset_t *pre2,
                         int fact_id)
{
    if (pddlMutexPairsIsMutexFactSet(mutex, fact_id, pre))
        return 1;
    if (pre2 != NULL && pddlMutexPairsIsMutexFactSet(mutex, fact_id, pre2))
        return 1;
    return 0;
}

static void findUselessDelEffs(const pddl_strips_t *strips,
                               const pddl_mutex_pairs_t *mutex,
                               const pddl_iset_t *pre,
                               const pddl_iset_t *pre2,
                               const pddl_iset_t *del_eff,
                               pddl_iset_t *useless)
{
    int del_fact;
    PDDL_ISET_FOR_EACH(del_eff, del_fact){
        if (mutex != NULL && isDelEffMutex(mutex, pre, pre2, del_fact)){
            pddlISetAdd(useless, del_fact);

        }else if (strips->fact.fact[del_fact]->neg_of >= 0){
            int neg = strips->fact.fact[del_fact]->neg_of;
            int pre_fact;
            PDDL_ISET_FOR_EACH(pre, pre_fact){
                if (pre_fact == neg){
                    pddlISetAdd(useless, del_fact);
                    break;
                }
            }
            if (pre2 != NULL){
                PDDL_ISET_FOR_EACH(pre2, pre_fact){
                    if (pre_fact == neg){
                        pddlISetAdd(useless, del_fact);
                        break;
                    }
                }
            }
        }
    }
}

int pddlStripsRemoveUselessDelEffs(pddl_strips_t *strips,
                                   const pddl_mutex_pairs_t *mutex,
                                   pddl_iset_t *changed_ops,
                                   pddl_err_t *err)
{
    CTX(err, "rm_useless_del_effs", "rm-useless-del-effs");
    int ret = 0;
    PDDL_LOG(err, "Removing useless delete effects."
             " num mutex pairs: %{num_mutex_pairs}d",
             (mutex != NULL ? mutex->num_mutex_pairs : -1 ));

    PDDL_ISET(useless);
    for (int op_id = 0; op_id < strips->op.op_size; ++op_id){
        pddl_strips_op_t *op = strips->op.op[op_id];

        int changed = 0;
        pddlISetEmpty(&useless);
        findUselessDelEffs(strips, mutex, &op->pre, NULL,
                           &op->del_eff, &useless);
        if (pddlISetSize(&useless) > 0){
            pddlISetMinus(&op->del_eff, &useless);
            changed = 1;
            if (changed_ops != NULL)
                pddlISetAdd(changed_ops, op_id);
        }

        for (int cei = 0; cei < op->cond_eff_size; ++cei){
            pddl_strips_op_cond_eff_t *ce = op->cond_eff + cei;
            pddlISetEmpty(&useless);
            findUselessDelEffs(strips, mutex, &op->pre, &ce->pre,
                               &ce->del_eff, &useless);
            if (pddlISetSize(&useless) > 0){
                pddlISetMinus(&ce->del_eff, &useless);
                changed = 1;
                if (changed_ops != NULL)
                    pddlISetAdd(changed_ops, op_id);
            }
        }

        if (changed)
            ++ret;
    }
    pddlISetFree(&useless);

    PDDL_LOG(err, "Removing useless delete effects DONE."
             " (modified ops: %{modified_ops}d)", ret);
    CTXEND(err);
    return ret;
}

int pddlStripsFindUnreachableOps(const pddl_strips_t *strips,
                                 const pddl_mutex_pairs_t *mutex,
                                 pddl_iset_t *unreachable_ops,
                                 pddl_err_t *err)
{
    PDDL_ISET(part_state);
    int num = 0;
    for (int oi = 0; oi < strips->op.op_size; ++oi){
        const pddl_strips_op_t *op = strips->op.op[oi];
        if (pddlMutexPairsIsMutexSet(mutex, &op->pre)){
            pddlISetAdd(unreachable_ops, oi);
            ++num;
            continue;
        }

        pddlISetMinus2(&part_state, &op->pre, &op->del_eff);
        pddlISetUnion(&part_state, &op->add_eff);
        if (pddlMutexPairsIsMutexSet(mutex, &part_state)){
            pddlISetAdd(unreachable_ops, oi);
            ++num;
        }
    }
    pddlISetFree(&part_state);
    PDDL_LOG(err, "Found %{found_unreachable_ops}d unreachable operators.", num);
    return 0;
}

void pddlStripsFindOpsEmptyAddEff(const pddl_strips_t *strips, pddl_iset_t *ops)
{
    for (int oi = 0; oi < strips->op.op_size; ++oi){
        const pddl_strips_op_t *op = strips->op.op[oi];
        if (pddlISetSize(&op->add_eff) == 0){
            int found = 1;
            for (int cei = 0; cei < op->cond_eff_size; ++cei){
                if (pddlISetSize(&op->cond_eff[cei].add_eff) != 0){
                    found = 0;
                    break;
                }
            }
            if (found)
                pddlISetAdd(ops, oi);
        }
    }
}

static void printPythonISet(const pddl_iset_t *s, FILE *fout)
{
    int i;
    fprintf(fout, "set([");
    PDDL_ISET_FOR_EACH(s, i)
        fprintf(fout, " %d,", i);
    fprintf(fout, "])");
}

void pddlStripsPrintPython(const pddl_strips_t *strips, FILE *fout)
{
    int f;

    fprintf(fout, "{\n");
    fprintf(fout, "'domain_file' : '%s',\n", strips->domain_file);
    fprintf(fout, "'problem_file' : '%s',\n", strips->problem_file);
    fprintf(fout, "'domain_name' : '%s',\n", strips->domain_name);
    fprintf(fout, "'problem_name' : '%s',\n", strips->problem_name);

    fprintf(fout, "'fact' : [\n");
    for (int i = 0; i < strips->fact.fact_size; ++i)
        fprintf(fout, "    '(%s)',\n", strips->fact.fact[i]->name);
    fprintf(fout, "],\n");

    fprintf(fout, "'op' : [\n");
    for (int i = 0; i < strips->op.op_size; ++i){
        const pddl_strips_op_t *op = strips->op.op[i];
        fprintf(fout, "    {\n");
        fprintf(fout, "        'name' : '%s',\n", op->name);
        fprintf(fout, "        'cost' : '%d',\n", op->cost);

        fprintf(fout, "        'pre' : ");
        printPythonISet(&op->pre, fout);
        fprintf(fout, ",\n");
        fprintf(fout, "        'add' : ");
        printPythonISet(&op->add_eff, fout);
        fprintf(fout, ",\n");
        fprintf(fout, "        'del' : ");
        printPythonISet(&op->del_eff, fout);
        fprintf(fout, ",\n");

        fprintf(fout, "        'cond_eff' : [\n");
        for (int j = 0; j < op->cond_eff_size; ++j){
            const pddl_strips_op_cond_eff_t *ce = op->cond_eff + j;
            fprintf(fout, "            {\n");
            fprintf(fout, "                'pre' : ");
            printPythonISet(&ce->pre, fout);
            fprintf(fout, ",\n");
            fprintf(fout, "                'add' : ");
            printPythonISet(&ce->add_eff, fout);
            fprintf(fout, ",\n");
            fprintf(fout, "                'del' : ");
            printPythonISet(&ce->del_eff, fout);
            fprintf(fout, ",\n");
            fprintf(fout, "            },\n");
        }
        fprintf(fout, "        ]\n");

        fprintf(fout, "    },\n");
    }
    fprintf(fout, "],\n");

    fprintf(fout, "'init' : [");
    PDDL_ISET_FOR_EACH(&strips->init, f)
        fprintf(fout, "%d, ", f);
    fprintf(fout, "],\n");

    fprintf(fout, "'goal' : [");
    PDDL_ISET_FOR_EACH(&strips->goal, f)
        fprintf(fout, "%d, ", f);
    fprintf(fout, "],\n");

    fprintf(fout, "'goal_is_unreachable' : %s,\n",
            (strips->goal_is_unreachable ? "True" : "False" ));
    fprintf(fout, "'has_cond_eff' : %s,\n",
            (strips->has_cond_eff ? "True" : "False" ));
    fprintf(fout, "}\n");
}

void pddlStripsPrintPDDLDomain(const pddl_strips_t *strips, FILE *fout)
{
    int fact_id;

    fprintf(fout, "(define (domain %s)\n", strips->domain_name);

    fprintf(fout, "(:predicates\n");
    for (int i = 0; i < strips->fact.fact_size; ++i)
        fprintf(fout, "    (F%d) ;; %s\n", i, strips->fact.fact[i]->name);
    fprintf(fout, ")\n");
    fprintf(fout, "(:functions (total-cost))\n");

    for (int i = 0; i < strips->op.op_size; ++i){
        const pddl_strips_op_t *op = strips->op.op[i];
        char *name = STRDUP(op->name);
        for (char *c = name; *c != 0x0; ++c){
            if (*c == ' ' || *c == '(' || *c == ')')
                *c = '_';
        }
        fprintf(fout, "(:action %s\n", name);
        fprintf(fout, "    :precondition (and");
        PDDL_ISET_FOR_EACH(&op->pre, fact_id)
            fprintf(fout, " (F%d)", fact_id);
        fprintf(fout, ")\n");

        fprintf(fout, "    :effect (and");
        PDDL_ISET_FOR_EACH(&op->add_eff, fact_id)
            fprintf(fout, " (F%d)", fact_id);
        PDDL_ISET_FOR_EACH(&op->del_eff, fact_id)
            fprintf(fout, " (not (F%d))", fact_id);
        for (int cei = 0; cei < op->cond_eff_size; ++cei){
            const pddl_strips_op_cond_eff_t *ce = op->cond_eff + cei;
            fprintf(fout, " (when (and");
            PDDL_ISET_FOR_EACH(&ce->pre, fact_id)
                fprintf(fout, " (F%d)", fact_id);
            fprintf(fout, ") (and");
            PDDL_ISET_FOR_EACH(&ce->add_eff, fact_id)
                fprintf(fout, " (F%d)", fact_id);
            PDDL_ISET_FOR_EACH(&ce->del_eff, fact_id)
                fprintf(fout, " (not (F%d))", fact_id);
            fprintf(fout, ")");
        }

        fprintf(fout, " (increase (total-cost) %d)", op->cost);
        fprintf(fout, ")\n");

        fprintf(fout, ")\n");
        FREE(name);
    }

    fprintf(fout, ")\n");
}

void pddlStripsPrintPDDLProblem(const pddl_strips_t *strips, FILE *fout)
{
    int fact_id;

    fprintf(fout, "(define (problem %s) (:domain %s)\n",
            strips->problem_name, strips->domain_name);

    fprintf(fout, "(:init\n");
    PDDL_ISET_FOR_EACH(&strips->init, fact_id)
        fprintf(fout, "    (F%d)\n", fact_id);
    fprintf(fout, ")\n");

    fprintf(fout, "(:goal (and");
    PDDL_ISET_FOR_EACH(&strips->goal, fact_id)
        fprintf(fout, " (F%d)", fact_id);
    fprintf(fout, "))\n");
    fprintf(fout, "(:metric minimize (total-cost))\n");
    fprintf(fout, ")\n");
}

void pddlStripsPrintDebug(const pddl_strips_t *strips, FILE *fout)
{
    fprintf(fout, "Fact[%d]:\n", strips->fact.fact_size);
    pddlFactsPrint(&strips->fact, "  ", "\n", fout);

    fprintf(fout, "Op[%d]:\n", strips->op.op_size);
    pddlStripsOpsPrintDebug(&strips->op, &strips->fact, fout);

    fprintf(fout, "Init State:");
    pddlFactsPrintSet(&strips->init, &strips->fact, " ", "", fout);
    fprintf(fout, "\n");

    fprintf(fout, "Goal:");
    pddlFactsPrintSet(&strips->goal, &strips->fact, " ", "", fout);
    fprintf(fout, "\n");
    if (strips->goal_is_unreachable)
        fprintf(fout, "Goal is unreachable\n");
    if (strips->has_cond_eff)
        fprintf(fout, "Has conditional effects\n");
}

void pddlStripsLogInfo(const pddl_strips_t *strips, pddl_err_t *err)
{
    PDDL_LOG(err, "Number of Strips Operators: %{ops}d", strips->op.op_size);
    PDDL_LOG(err, "Number of Strips Facts: %{facts}d", strips->fact.fact_size);
    PDDL_LOG(err, "Goal is unreachable: %{goal_unreachable}d", strips->goal_is_unreachable);
    PDDL_LOG(err, "Has Conditional Effects: %{has_ce}d", strips->has_cond_eff);
    int count = 0;
    for (int i = 0; i < strips->op.op_size; ++i){
        if (strips->op.op[i]->cond_eff_size > 0)
            ++count;
    }
    PDDL_LOG(err, "Number of Strips Operators"
             " with Conditional Effects: %{ops_ce}d", count);
}
