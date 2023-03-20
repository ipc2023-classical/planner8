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
#include "pddl/op_mutex_redundant.h"

struct reduce_gen {
    /** ID of the symmetry generator */
    int gen_id;
    /** True if this symmetry is destroyed */
    int is_destroyed;
    /** Reference to the symmetry generator */
    const pddl_strips_sym_gen_t *gen;
    /** Operators that are not identity in this symmetry and that are
     *  op-mutex with their image. */
    pddl_iset_t relevant_op;
    /** Operator mutexes for relevant operators */
    pddl_iset_t *op_mutex_with;
    int op_size;

    /** Selected redundant set */
    pddl_iset_t redundant_set;
    /** Number of symmetries that would be destroyed by removing the
     *  selected redundant set */
    int num_destroyed_syms;

    /** Operators that need to be pruned in order to preserve this symmetry */
    pddl_iset_t to_preserve;
    /** Temporary storage for computing .to_preserve */
    pddl_iset_t tmp_to_preserve;
};
typedef struct reduce_gen reduce_gen_t;

struct reduce {
    pddl_iset_t *op_mutex_with;
    int op_size;
    reduce_gen_t *gen;
    int gen_size;
    pddl_iset_t pruned_ops;
};
typedef struct reduce reduce_t;


static void reduceGenInit(reduce_gen_t *rgen,
                          int gen_id,
                          const reduce_t *red,
                          const pddl_strips_sym_gen_t *gen,
                          const pddl_strips_sym_t *sym,
                          const pddl_op_mutex_pairs_t *op_mutex)
{
    ZEROIZE(rgen);
    rgen->gen_id = gen_id;
    rgen->gen = gen;

    for (int op_id = 0; op_id < red->op_size; ++op_id){
        if (gen->op[op_id] != op_id
                && pddlOpMutexPairsIsMutex(op_mutex, op_id, gen->op[op_id]))
            pddlISetAdd(&rgen->relevant_op, op_id);
    }

    rgen->op_size = red->op_size;
    rgen->op_mutex_with = CALLOC_ARR(pddl_iset_t, rgen->op_size);

    int op_id;
    PDDL_ISET_FOR_EACH(&rgen->relevant_op, op_id){
        pddlISetIntersect2(rgen->op_mutex_with + op_id,
                           red->op_mutex_with + op_id,
                           &rgen->relevant_op);
    }
}

static void reduceGenFree(reduce_gen_t *rgen)
{
    pddlISetFree(&rgen->relevant_op);
    pddlISetFree(&rgen->to_preserve);
    pddlISetFree(&rgen->tmp_to_preserve);

    for (int i = 0; i < rgen->op_size; ++i){
        pddlISetFree(rgen->op_mutex_with + i);
    }
    if (rgen->op_mutex_with != NULL)
        FREE(rgen->op_mutex_with);
}

static void reduceInit(reduce_t *red,
                       const pddl_strips_sym_t *sym,
                       const pddl_op_mutex_pairs_t *op_mutex)
{
    ZEROIZE(red);
    red->op_size = op_mutex->op_size;
    red->op_mutex_with = CALLOC_ARR(pddl_iset_t, red->op_size);
    for (int op_id = 0; op_id < red->op_size; ++op_id){
        pddlOpMutexPairsMutexWith(op_mutex, op_id,
                                  red->op_mutex_with + op_id);
    }

    red->gen_size = sym->gen_size;
    red->gen = CALLOC_ARR(reduce_gen_t, sym->gen_size);
    for (int i = 0; i < sym->gen_size; ++i){
        reduceGenInit(red->gen + i, i, red,
                      sym->gen + i, sym, op_mutex);
    }
}

static void reduceFree(reduce_t *red)
{
    for (int i = 0; i < red->gen_size; ++i)
        reduceGenFree(red->gen + i);
    if (red->gen != NULL)
        FREE(red->gen);
}

/*
static int isDestroyed(const reduce_gen_t *rgen, const pddl_iset_t *pruned_ops)
{
    int op;
    PDDL_ISET_FOR_EACH(pruned_ops, op){
        if (!pddlISetIn(rgen->gen->op[op], pruned_ops))
            return 1;
    }
    return 0;
}

static void destroyedSymmetries(reduce_gen_t *rgen,
                                const reduce_t *red,
                                int prune_op)
{
    pddlISetEmpty(&rgen->destroyed_syms);

    for (int gi = 0; gi < red->gen_size; ++gi){
        const pddl_sym_gen_t *gen = red->gen[gi].gen;
        int op;

        if (prune_op >= 0
                && gen->op[prune_op] != prune_op
                && !pddlISetIn(gen->op[prune_op], &rgen->redundant_set)
                && !pddlISetIn(gen->op[prune_op], &red->pruned_ops)){
            pddlISetAdd(&rgen->destroyed_syms, gi);
            continue;
        }

        int cont = 0;
        PDDL_ISET_FOR_EACH(&rgen->redundant_set, op){
            if (gen->op[op] != prune_op
                    && !pddlISetIn(gen->op[op], &rgen->redundant_set)
                    && !pddlISetIn(gen->op[op], &red->pruned_ops)){
                pddlISetAdd(&rgen->destroyed_syms, gi);
                cont = 1;
                break;
            }
        }
        if (cont)
            continue;

        PDDL_ISET_FOR_EACH(&red->pruned_ops, op){
            if (gen->op[op] != prune_op
                    && !pddlISetIn(gen->op[op], &rgen->redundant_set)
                    && !pddlISetIn(gen->op[op], &red->pruned_ops)){
                pddlISetAdd(&rgen->destroyed_syms, gi);
                break;
            }
        }
    }
}
*/

static void resetToPreserve(reduce_t *red)
{
    for (int gi = 0; gi < red->gen_size; ++gi){
        reduce_gen_t *rgen = red->gen + gi;
        pddlISetEmpty(&rgen->tmp_to_preserve);
        int op;
        PDDL_ISET_FOR_EACH(&red->pruned_ops, op){
            if (!pddlISetIn(rgen->gen->op[op], &red->pruned_ops))
                pddlISetAdd(&rgen->tmp_to_preserve, rgen->gen->op[op]);
        }
    }
}

static void updateToPreserveSet(reduce_gen_t *rgen, int prune_op,
                                pddl_iset_t *to_preserve)
{
    if (pddlISetIn(prune_op, to_preserve)){
        pddlISetRm(to_preserve, prune_op);
    }else if (rgen->gen->op[prune_op] != prune_op){
        pddlISetAdd(to_preserve, rgen->gen->op[prune_op]);
    }
}

static void updateToPreserve(reduce_t *red, int prune_op)
{
    for (int gi = 0; gi < red->gen_size; ++gi){
        reduce_gen_t *rgen = red->gen + gi;
        updateToPreserveSet(rgen, prune_op, &rgen->tmp_to_preserve);
    }
}

static int numDestroyedSymmetriesWithToPreserve(reduce_gen_t *rgen,
                                                const reduce_t *red,
                                                int prune_op)
{
    int num = 0;

    for (int gi = 0; gi < red->gen_size; ++gi){
        if (prune_op >= 0 && gi == rgen->gen_id){
            ++num;
            continue;
        }

        const reduce_gen_t *gen = red->gen + gi;
        int size = pddlISetSize(&gen->tmp_to_preserve);
        if (size > 1
                || (size == 1 && !pddlISetIn(prune_op, &gen->tmp_to_preserve))
                || (size == 0 && prune_op >= 0
                        && gen->gen->op[prune_op] != prune_op)){
            ++num;
        }
    }
    return num;
}

static int selectCandidate(reduce_gen_t *rgen,
                           const reduce_t *red,
                           const pddl_iset_t *cand)
{
    int sel = -1;
    int destroy = INT_MAX;
    int size = 0;
    int op;

    PDDL_ISET_FOR_EACH(cand, op){
        int dest = numDestroyedSymmetriesWithToPreserve(rgen, red, op);
        int siz = pddlISetSize(&rgen->op_mutex_with[op]);

        if (dest < destroy || (dest == destroy && siz > size)){
        //if (siz > size){
            sel = op;
            destroy = dest;
            size = siz;
        }
    }

    return sel;
}

static void findRedundantSet(reduce_gen_t *rgen, reduce_t *red)
{

    pddlISetEmpty(&rgen->redundant_set);
    rgen->num_destroyed_syms = 0;
    if (pddlISetSize(&rgen->relevant_op) == 0)
        return;

    resetToPreserve(red);

    PDDL_ISET(cand);
    pddlISetUnion(&cand, &rgen->relevant_op);
    while (pddlISetSize(&cand) > 0){
        int prune_op = selectCandidate(rgen, red, &cand);
        int keep_op = rgen->gen->op[prune_op];
        pddlISetAdd(&rgen->redundant_set, prune_op);
        pddlISetRm(&cand, prune_op);
        pddlISetIntersect(&cand, &rgen->op_mutex_with[keep_op]);

        updateToPreserve(red, prune_op);
    }

    rgen->num_destroyed_syms
            = numDestroyedSymmetriesWithToPreserve(rgen, red, -1);

    pddlISetFree(&cand);
}

static void pruneRedundantSet(reduce_t *red, const pddl_iset_t *redundant)
{
    pddlISetUnion(&red->pruned_ops, redundant);
    for (int gi = 0; gi < red->gen_size; ++gi){
        reduce_gen_t *rgen = red->gen + gi;
        pddlISetMinus(&rgen->relevant_op, redundant);
        for (int oi = 0; oi < rgen->op_size; ++oi)
            pddlISetMinus(&rgen->op_mutex_with[oi], redundant);

        int op;
        PDDL_ISET_FOR_EACH(redundant, op)
            updateToPreserveSet(rgen, op, &rgen->to_preserve);
        rgen->is_destroyed = 0;
        if (pddlISetSize(&rgen->to_preserve) > 0)
            rgen->is_destroyed = 1;
    }
}

static void computeRedundantSets(reduce_t *red, pddl_err_t *err)
{
    PDDL_INFO(err, "  --> Computing redundant sets for each symmetry");
    for (int gi = 0; gi < red->gen_size; ++gi){
        reduce_gen_t *rgen = red->gen + gi;
        if (rgen->is_destroyed){
            pddlISetEmpty(&rgen->redundant_set);
            rgen->num_destroyed_syms = 0;
            continue;
        }

        findRedundantSet(rgen, red);
        /*
        PDDL_INFO(err, "    --> Sym %d: size: %d, destroyed symmetries: %d,"
                      " relevant ops: %d",
                 gi, pddlISetSize(&rgen->redundant_set),
                 rgen->num_destroyed_syms,
                 pddlISetSize(&rgen->relevant_op));
        */
    }
}

static int selectRedundantSet(const reduce_t *red, pddl_err_t *err)
{
    int best_size = -1;
    int best_sym_destroyed = INT_MAX;
    int best = -1;
    for (int gi = 0; gi < red->gen_size; ++gi){
        const reduce_gen_t *rgen = red->gen + gi;
        if (rgen->is_destroyed)
            continue;
        if (pddlISetSize(&rgen->redundant_set) == 0)
            continue;

        int size = pddlISetSize(&rgen->redundant_set);
        int destr = rgen->num_destroyed_syms;
        if (destr < best_sym_destroyed
                || (destr == best_sym_destroyed && size > best_size)){
        //if (size > best_size){
            best_size = size;
            best_sym_destroyed = destr;
            best = gi;
        }
    }

    return best;
}

int pddlOpMutexFindRedundantGreedy(const pddl_op_mutex_pairs_t *op_mutex,
                                   const pddl_strips_sym_t *sym,
                                   const pddl_op_mutex_redundant_config_t *cfg,
                                   pddl_iset_t *redundant_out,
                                   pddl_err_t *err)
{
    CTX(err, "opm_redundant_greedy", "OPM-Redundant-Greedy");
    reduce_t red;
    int change, gen_id;

    PDDL_INFO(err, "Redundant set with op-mutexes and symmetries:");

    reduceInit(&red, sym, op_mutex);
    PDDL_INFO(err, "  --> Initialized");

    change = 1;
    while (change){
        change = 0;
        computeRedundantSets(&red, err);

        if ((gen_id = selectRedundantSet(&red, err)) >= 0){
            pruneRedundantSet(&red, &red.gen[gen_id].redundant_set);
            change = 1;
            PDDL_INFO(err, "  --> Selected redundant set from symmetry"
                     " %d with size %d destroying %d symmetries"
                     " :: overall: %d",
                     gen_id,
                     pddlISetSize(&red.gen[gen_id].redundant_set),
                     red.gen[gen_id].num_destroyed_syms,
                     pddlISetSize(&red.pruned_ops));
        }
    }

    PDDL_INFO(err, "Op-mutex symmetry redundant operators: %d",
             pddlISetSize(&red.pruned_ops));


    pddlISetUnion(redundant_out, &red.pruned_ops);
    reduceFree(&red);
    CTXEND(err);
    return 0;
}
