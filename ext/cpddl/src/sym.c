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
#include "pddl/config.h"
#ifdef PDDL_BLISS

#include <bliss/bliss_C.h>
#include "pddl/iarr.h"
#include "pddl/sym.h"

struct pdg_sym {
    const pddl_strips_t *strips;
    pddl_strips_sym_t *sym;
};

static void genCreateOpCycles(pddl_strips_sym_gen_t *gen, int op_size)
{
    int *op_used = CALLOC_ARR(int, op_size);
    for (int i = 0; i < op_size; ++i){
        if (op_used[i] || gen->op[i] == i)
            continue;

        if (gen->op_cycle_size == gen->op_cycle_alloc){
            if (gen->op_cycle_alloc == 0)
                gen->op_cycle_alloc = 2;
            gen->op_cycle_alloc *= 2;
            gen->op_cycle = REALLOC_ARR(gen->op_cycle, pddl_iset_t,
                                        gen->op_cycle_alloc);
        }
        pddl_iset_t *cycle = gen->op_cycle + gen->op_cycle_size++;
        pddlISetInit(cycle);

        int op = i;
        op_used[op] = 1;
        pddlISetAdd(cycle, op);
        while (!op_used[gen->op[op]]){
            op_used[gen->op[op]] = 1;
            pddlISetAdd(cycle, gen->op[op]);
            op = gen->op[op];
        }
    }

    if (op_used != NULL)
        FREE(op_used);
}

static BlissGraph *pdgConstruct(const pddl_strips_t *strips)
{
    BlissGraph *pdg;
    int color_init = 2;
    int color_goal = 4;
    int color_op = 8;

    pdg = bliss_new_digraph(0);
    for (int i = 0; i < strips->fact.fact_size; ++i)
        bliss_add_vertex(pdg, 0); // fact vertex
    for (int fact_id = 0; fact_id < strips->fact.fact_size; ++fact_id){
        int color = 1;
        if (pddlISetIn(fact_id, &strips->init))
            color |= color_init;
        if (pddlISetIn(fact_id, &strips->goal))
            color |= color_goal;
        bliss_add_vertex(pdg, color); // fact true vertex
        bliss_add_vertex(pdg, 0); // fact false vertex
    }
    for (int op_id = 0; op_id < strips->op.op_size; ++op_id){
        const pddl_strips_op_t *op = strips->op.op[op_id];
        bliss_add_vertex(pdg, color_op + op->cost); // operator vertex
    }

    int fsize = strips->fact.fact_size;
    for (int fact_id = 0; fact_id < strips->fact.fact_size; ++fact_id){
        bliss_add_edge(pdg, fact_id, fsize + 2 * fact_id);
        bliss_add_edge(pdg, fact_id, fsize + 2 * fact_id + 1);
    }

    for (int op_id = 0; op_id < strips->op.op_size; ++op_id){
        const pddl_strips_op_t *op = strips->op.op[op_id];
        int voi = 3 * fsize + op_id;
        int fact;
        PDDL_ISET_FOR_EACH(&op->pre, fact){
            int v = fsize + 2 * strips->fact.fact[fact]->id;
            bliss_add_edge(pdg, v, voi);
        }
        PDDL_ISET_FOR_EACH(&op->add_eff, fact){
            int v = fsize + 2 * strips->fact.fact[fact]->id;
            bliss_add_edge(pdg, voi, v);
        }
        PDDL_ISET_FOR_EACH(&op->del_eff, fact){
            int v = fsize + 2 * strips->fact.fact[fact]->id + 1;
            bliss_add_edge(pdg, voi, v);
        }
    }

    return pdg;
}

static void pdgAutomorphismHook(void *ud, unsigned int n,
                                const unsigned int *aut)
{
    struct pdg_sym *p = ud;
    const pddl_strips_t *strips = p->strips;
    pddl_strips_sym_t *sym = p->sym;
    int fact_size = strips->fact.fact_size;
    int op_size = strips->op.op_size;
    int op_offset = 3 * fact_size;

    if (sym->gen_size == sym->gen_alloc){
        if (sym->gen_alloc == 0)
            sym->gen_alloc = 2;
        sym->gen_alloc *= 2;
        sym->gen = REALLOC_ARR(sym->gen, pddl_strips_sym_gen_t, sym->gen_alloc);
    }
    pddl_strips_sym_gen_t *gen = sym->gen + sym->gen_size++;
    ZEROIZE(gen);

    gen->fact = CALLOC_ARR(int, fact_size);
    gen->fact_inv = CALLOC_ARR(int, fact_size);
    for (int i = 0; i < fact_size; ++i){
        gen->fact[i] = i;
        gen->fact_inv[i] = i;
    }
    gen->op = CALLOC_ARR(int, op_size);
    gen->op_inv = CALLOC_ARR(int, op_size);
    for (int i = 0; i < op_size; ++i){
        gen->op[i] = i;
        gen->op_inv[i] = i;
    }

    for (int f1 = 0; f1 < fact_size; ++f1){
        int f2 = aut[f1];
        gen->fact[f1] = f2;
        gen->fact_inv[f2] = f1;
    }

    for (int o1 = 0; o1 < op_size; ++o1){
        int o2 = aut[op_offset + o1] - op_offset;
        gen->op[o1] = o2;
        gen->op_inv[o2] = o1;
    }

    genCreateOpCycles(gen, op_size);
}

void pddlStripsSymInitPDG(pddl_strips_sym_t *sym, const pddl_strips_t *strips)
{
    ZEROIZE(sym);
    if (strips->has_cond_eff){
        PANIC("pddlStripsInitSymPDG() does not support conditional"
                    " effects.");
    }

    BlissGraph *pdg = pdgConstruct(strips);
    struct pdg_sym pdg_sym = { strips, sym };
    bliss_find_automorphisms(pdg, pdgAutomorphismHook, &pdg_sym, NULL);
    bliss_release(pdg);

    sym->fact_size = strips->fact.fact_size;
    sym->op_size = strips->op.op_size;
}

void pddlStripsSymFree(pddl_strips_sym_t *sym)
{
    for (int i = 0; i < sym->gen_size; ++i){
        pddl_strips_sym_gen_t *gen = sym->gen + i;

        if (gen->fact != NULL)
            FREE(gen->fact);
        if (gen->fact_inv != NULL)
            FREE(gen->fact_inv);
        if (gen->op != NULL)
            FREE(gen->op);
        if (gen->op_inv != NULL)
            FREE(gen->op_inv);
        for (int j = 0; j < gen->op_cycle_size; ++j)
            pddlISetFree(gen->op_cycle + j);
        if (gen->op_cycle != NULL)
            FREE(gen->op_cycle);
    }
    if (sym->gen != NULL)
        FREE(sym->gen);
}

static void applyGenOnFactSet(const pddl_strips_sym_gen_t *gen, const pddl_iset_t *in,
                              pddl_iset_t *out)
{
    int v;
    pddlISetEmpty(out);
    PDDL_ISET_FOR_EACH(in, v)
        pddlISetAdd(out, gen->fact[v]);
}

static void applyGenOnOpSet(const pddl_strips_sym_gen_t *gen,
                            const pddl_iset_t *in,
                            pddl_iset_t *out)
{
    int v;
    pddlISetEmpty(out);
    PDDL_ISET_FOR_EACH(in, v)
        pddlISetAdd(out, gen->op[v]);
}

static void allSymmetries(const pddl_strips_sym_t *sym,
                          pddl_set_iset_t *sym_set,
                          void (*apply)(const pddl_strips_sym_gen_t *,
                                        const pddl_iset_t *in,
                                        pddl_iset_t *out))
{
    PDDL_IARR(queue);
    PDDL_ISET(img_set);

    // Initialize queue with all sets that are already in sym_set
    PDDL_SET_ISET_FOR_EACH_ID(sym_set, i)
        pddlIArrAdd(&queue, i);
    while (pddlIArrSize(&queue) > 0){
        // Get the next set in queu
        int in_id = queue.arr[--queue.size];
        const pddl_iset_t *in_set = pddlSetISetGet(sym_set, in_id);

        for (int i = 0; i < sym->gen_size; ++i){
            const pddl_strips_sym_gen_t *gen = sym->gen + i;
            // Remember the size of set of sets to recognize a brand new
            // symmetric set
            int sym_set_size = pddlSetISetSize(sym_set);

            // Create the symmetric set
            apply(gen, in_set, &img_set);

            // Add the symmetric set and if it is a new set, add it to the
            // queue
            int out_id = pddlSetISetAdd(sym_set, &img_set);
            if (out_id >= sym_set_size)
                pddlIArrAdd(&queue, out_id);
        }
    }

    pddlISetFree(&img_set);
    pddlIArrFree(&queue);
}

void pddlStripsSymAllFactSetSymmetries(const pddl_strips_sym_t *sym,
                                       pddl_set_iset_t *sym_set)
{
    allSymmetries(sym, sym_set, applyGenOnFactSet);
}

void pddlStripsSymAllOpSetSymmetries(const pddl_strips_sym_t *sym,
                                     pddl_set_iset_t *sym_set)
{
    allSymmetries(sym, sym_set, applyGenOnOpSet);
}

void pddlStripsSymOpTransitiveClosure(const pddl_strips_sym_t *sym,
                                      int op_id,
                                      pddl_iset_t *transitive_closure)
{
    pddl_set_iset_t opset;
    pddlSetISetInit(&opset);

    PDDL_ISET(op);
    pddlISetAdd(&op, op_id);
    pddlSetISetAdd(&opset, &op);
    pddlISetFree(&op);

    pddlStripsSymAllOpSetSymmetries(sym, &opset);
    const pddl_iset_t *sym_op;
    PDDL_SET_ISET_FOR_EACH(&opset, sym_op){
        ASSERT(pddlISetSize(sym_op) == 1);
        pddlISetUnion(transitive_closure, sym_op);
    }

    pddlSetISetFree(&opset);
}

void pddlStripsSymOpSet(const pddl_strips_sym_t *sym,
                        int gen_id,
                        const pddl_iset_t *in,
                        pddl_iset_t *out)
{
    const pddl_strips_sym_gen_t *gen = sym->gen + gen_id;
    int op;
    pddlISetEmpty(out);
    PDDL_ISET_FOR_EACH(in, op)
        pddlISetAdd(out, gen->op[op]);
}

void pddlStripsSymPrintDebug(const pddl_strips_sym_t *sym, FILE *fout)
{
    int *fact_used;

    fact_used = CALLOC_ARR(int, sym->fact_size);

    for (int gi = 0; gi < sym->gen_size; ++gi){
        const pddl_strips_sym_gen_t *gen = sym->gen + gi;

        fprintf(fout, "gen %d:\n", gi);

        ZEROIZE_ARR(fact_used, sym->fact_size);
        fprintf(fout, "  facts:");
        for (int i = 0; i < sym->fact_size; ++i){
            if (fact_used[i] || gen->fact[i] == i)
                continue;

            int fact = i;
            fact_used[fact] = 1;
            fprintf(fout, " [%d", fact);
            fflush(fout);
            while (!fact_used[gen->fact[fact]]){
                fact_used[gen->fact[fact]] = 1;
                fprintf(fout, ",%d", gen->fact[fact]);
                fflush(fout);
                fact = gen->fact[fact];
            }
            fprintf(fout, "]");
        }
        fprintf(fout, "\n");

        fprintf(fout, "  ops:");
        for (int ci = 0; ci < gen->op_cycle_size; ++ci){
            fprintf(fout, " [%d", pddlISetGet(&gen->op_cycle[ci], 0));
            for (int i = 1; i < pddlISetSize(&gen->op_cycle[ci]); ++i)
                fprintf(fout, ",%d", pddlISetGet(&gen->op_cycle[ci], i));
            fprintf(fout, "]");
        }
        fprintf(fout, "\n");
    }

    if (fact_used != NULL)
        FREE(fact_used);
}

#else /* PDDL_BLISS */

#include "pddl/sym.h"

#define ERROR PANIC("sym module requires bliss library")

void pddlStripsSymInitPDG(pddl_strips_sym_t *sym, const pddl_strips_t *strips)
{
    ERROR;
}

void pddlStripsSymFree(pddl_strips_sym_t *sym)
{
    ERROR;
}

void pddlStripsSymAllFactSetSymmetries(const pddl_strips_sym_t *sym,
                                       pddl_set_iset_t *sym_set)
{
    ERROR;
}

void pddlStripsSymAllOpSetSymmetries(const pddl_strips_sym_t *sym,
                                     pddl_set_iset_t *sym_set)
{
    ERROR;
}

void pddlStripsSymOpTransitiveClosure(const pddl_strips_sym_t *sym,
                                      int op_id,
                                      pddl_iset_t *transitive_closure)
{
    ERROR;
}

void pddlStripsSymOpSet(const pddl_strips_sym_t *sym,
                        int gen_id,
                        const pddl_iset_t *inset,
                        pddl_iset_t *outset)
{
    ERROR;
}

void pddlStripsSymPrintDebug(const pddl_strips_sym_t *sym, FILE *fout)
{
    ERROR;
}
#endif /* PDDL_BLISS */
