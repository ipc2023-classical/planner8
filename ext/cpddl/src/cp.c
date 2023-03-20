/***
 * Copyright (c)2022 Daniel Fiser <danfis@danfis.cz>
 *
 *  This file is part of cpddl.
 *
 *  Distributed under the OSI-approved BSD License (the "License");
 *  see accompanying file BDS-LICENSE for details or see
 *  <http://www.opensource.org/licenses/bsd-license.php>.
 *
 *  This software is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the License for more information.
 */

#include "internal.h"
#include "pddl/cp.h"
#include "pddl/hfunc.h"
#include "pddl/subprocess.h"
#include "_cp.h"

#define HASH_SEED 7307

static int default_solver = PDDL_CP_SOLVER_DEFAULT;

struct simplify_change {
    int change;
    int *ivar_change;
};
typedef struct simplify_change simplify_change_t;

static int *simplifyIVarAllowed(pddl_cp_t *cp, int arity, const int *var,
                                int num_val_tuples, const int *val_tuples,
                                int *out_num_val_tuples,
                                simplify_change_t *change)
{
    pddl_iset_t *var_dom = CALLOC_ARR(pddl_iset_t, arity);
    int *out = ALLOC_ARR(int, arity * num_val_tuples);
    *out_num_val_tuples = 0;
    int ins = 0;
    for (int ti = 0, idx = 0; ti < num_val_tuples; ++ti, idx += arity){
        int is_consistent = 1;
        for (int vi = 0; vi < arity; ++vi){
            if (!pddlISetIn(val_tuples[idx + vi],
                            &cp->ivar.ivar[var[vi]].domain)){
                is_consistent = 0;
                break;
            }
        }
        if (is_consistent){
            for (int vi = 0; vi < arity; ++vi){
                int val = val_tuples[idx + vi];
                out[ins++] = val;
                pddlISetAdd(var_dom + vi, val);
            }
            ++(*out_num_val_tuples);
        }
    }

    if (ins == 0){
        FREE(out);
        out = NULL;
    }else if (ins < arity * num_val_tuples){
        out = REALLOC_ARR(out, int, ins);
    }

    for (int vi = 0; vi < arity; ++vi){
        pddl_iset_t *domain = &cp->ivar.ivar[var[vi]].domain;
        int old_size = pddlISetSize(domain);
        pddlISetIntersect(domain, var_dom + vi);
        if (change != NULL && old_size != pddlISetSize(domain)){
            change->ivar_change[vi] = 1;
            change->change = 1;
        }
        pddlISetFree(var_dom + vi);
    }
    FREE(var_dom);

    return out;
}

static void pddlCPIVarFree(pddl_cp_ivar_t *v)
{
    if (v->name != NULL)
        FREE(v->name);
    pddlISetFree(&v->domain);
}

static void pddlCPIVarsFree(pddl_cp_ivars_t *v)
{
    for (int i = 0; i < v->ivar_size; ++i)
        pddlCPIVarFree(v->ivar + i);
}

static int pddlCPIVarsAdd(pddl_cp_ivars_t *v, int min_val, int max_val,
                          const char *name)
{
    if (v->ivar_size == v->ivar_alloc){
        if (v->ivar_alloc == 0)
            v->ivar_alloc = 4;
        v->ivar_alloc *= 2;
        v->ivar = REALLOC_ARR(v->ivar, pddl_cp_ivar_t, v->ivar_alloc);
    }

    pddl_cp_ivar_t *var = v->ivar + v->ivar_size++;
    ZEROIZE(var);
    var->id = v->ivar_size - 1;
    for (int i = min_val; i <= max_val; ++i)
        pddlISetAdd(&var->domain, i);
    if (name != NULL){
        var->name = STRDUP(name);
    }else{
        char n[128];
        snprintf(n, 128, "x%d", var->id);
        var->name = STRDUP(n);
    }

    return var->id;
}

static pddl_htable_key_t pddlCPIValTupleHash(const pddl_cp_ival_tuple_t *t)
{
    return pddlFastHash_64(t->ival, sizeof(int) * t->arity * t->num_tuples,
                           HASH_SEED);
}

static pddl_htable_key_t ivalTupleHash(const pddl_list_t *key, void *_)
{
    const pddl_cp_ival_tuple_t *t;
    t = pddl_container_of(key, pddl_cp_ival_tuple_t, htable);
    return t->hash;
}

static int ivalTupleEq(const pddl_list_t *k1, const pddl_list_t *k2, void *_)
{
    const pddl_cp_ival_tuple_t *t1, *t2;
    t1 = pddl_container_of(k1, pddl_cp_ival_tuple_t, htable);
    t2 = pddl_container_of(k2, pddl_cp_ival_tuple_t, htable);
    int cmp = t1->arity - t2->arity;
    if (cmp == 0)
        cmp = t1->num_tuples - t2->num_tuples;
    if (cmp == 0){
        cmp = memcmp(t1->ival, t2->ival,
                     sizeof(int) * t1->arity * t1->num_tuples);
    }
    return cmp == 0;
}

static void pddlCPIValTuplesInit(pddl_cp_ival_tuples_t *tuples)
{
    ZEROIZE(tuples);
    tuples->htable = pddlHTableNew(ivalTupleHash, ivalTupleEq, NULL);
}

static void pddlCPIValTuplesFree(pddl_cp_ival_tuples_t *tuples)
{
    // TODO
    pddlHTableDel(tuples->htable);
    for (int i = 0; i < tuples->tuple_size; ++i){
        ASSERT(tuples->tuple[i]->idx == i);
        ASSERT(tuples->tuple[i] != NULL);
        if (tuples->tuple[i] == NULL)
            continue;
        if (tuples->tuple[i]->ival != NULL)
            FREE(tuples->tuple[i]->ival);
        FREE(tuples->tuple[i]);
    }
    if (tuples->tuple != NULL)
        FREE(tuples->tuple);
}

static pddl_cp_ival_tuple_t *pddlCPIValTuplesAdd(pddl_cp_ival_tuples_t *tuples,
                                                 int arity,
                                                 int num_tuples,
                                                 const int *ival)
{
    pddl_cp_ival_tuple_t *tup = ZALLOC(pddl_cp_ival_tuple_t);
    tup->arity = arity;
    tup->num_tuples = num_tuples;
    tup->ival = (int *)ival;
    pddlListInit(&tup->htable);
    tup->hash = pddlCPIValTupleHash(tup);

    pddl_list_t *ins;
    if ((ins = pddlHTableInsertUnique(tuples->htable, &tup->htable)) == NULL){
        if (tuples->tuple_size == tuples->tuple_alloc){
            if (tuples->tuple_alloc == 0)
                tuples->tuple_alloc = 4;
            tuples->tuple_alloc *= 2;
            tuples->tuple = REALLOC_ARR(tuples->tuple, pddl_cp_ival_tuple_t *,
                                        tuples->tuple_alloc);
        }
        tup->idx = tuples->tuple_size;
        tuples->tuple[tuples->tuple_size++] = tup;
        tup->ival = ALLOC_ARR(int, tup->arity * tup->num_tuples);
        memcpy(tup->ival, ival, sizeof(int) * tup->arity * tup->num_tuples);
        return tup;

    }else{
        FREE(tup);
        tup = pddl_container_of(ins, pddl_cp_ival_tuple_t, htable);
        return tup;
    }
}

static void pddlCPIValTupleUnref(pddl_cp_ival_tuples_t *tuples,
                                 pddl_cp_ival_tuple_t *tuple)
{
    ASSERT(tuples->tuple[tuple->idx] == tuple);
    ASSERT(tuple->ref >= 1);
    if (--tuple->ref == 0){
#ifdef PDDL_DEBUG
        ASSERT(pddlHTableErase(tuples->htable, &tuple->htable) == 0);
#else
        pddlHTableErase(tuples->htable, &tuple->htable);
#endif

        int idx = tuple->idx;
        if (tuple->ival != NULL)
            FREE(tuple->ival);
        FREE(tuple);
        tuples->tuple[idx] = NULL;
        --tuples->tuple_size;
        if (idx != tuples->tuple_size){
            tuples->tuple[idx] = tuples->tuple[tuples->tuple_size];
            tuples->tuple[idx]->idx = idx;
        }
    }
}

static void pddlCPConstIVarAllowedFree(pddl_cp_constr_ivar_allowed_t *c)
{
    if (c->ivar != NULL)
        FREE(c->ivar);
}

static void pddlCPConstrIVarAllowedFree(pddl_cp_constrs_ivar_allowed_t *c)
{
    for (int i = 0; i < c->constr_size; ++i)
        pddlCPConstIVarAllowedFree(c->constr + i);
    if (c->constr != NULL)
        FREE(c->constr);
}

static void pddlCPConstrIVarAllowedAdd(pddl_cp_constrs_ivar_allowed_t *c,
                                       int arity, const int *var,
                                       pddl_cp_ival_tuple_t *tup)
{
    if (c->constr_size == c->constr_alloc){
        if (c->constr_alloc == 0)
            c->constr_alloc = 4;
        c->constr_alloc *= 2;
        c->constr = REALLOC_ARR(c->constr, pddl_cp_constr_ivar_allowed_t,
                                c->constr_alloc);
    }
    pddl_cp_constr_ivar_allowed_t *cstr = c->constr + c->constr_size++;
    ZEROIZE(cstr);
    cstr->arity = arity;
    cstr->ivar = ALLOC_ARR(int, arity);
    memcpy(cstr->ivar, var, sizeof(int) * arity);
    cstr->ival = tup;
    ++cstr->ival->ref;
}

void pddlCPSolFree(pddl_cp_sol_t *sol)
{
    for (int i = 0; i < sol->num_solutions; ++i)
        FREE(sol->isol[i]);
    if (sol->isol != NULL)
        FREE(sol->isol);
    ZEROIZE(sol);
}

int *pddlCPSolGet(pddl_cp_sol_t *sol, int sol_id)
{
    return sol->isol[sol_id];
}

void pddlCPSolAdd(const pddl_cp_t *cp, pddl_cp_sol_t *sol, const int *isol)
{
    int *s = pddlCPSolAddEmpty(cp, sol);
    memcpy(s, isol, sizeof(int) * sol->ivar_size);
}

int *pddlCPSolAddEmpty(const pddl_cp_t *cp, pddl_cp_sol_t *sol)
{
    if (sol->num_solutions == sol->isol_alloc){
        if (sol->isol_alloc == 0)
            sol->isol_alloc = 2;
        sol->isol_alloc *= 2;
        sol->isol = REALLOC_ARR(sol->isol, int *, sol->isol_alloc);
    }
    sol->ivar_size = cp->ivar.ivar_size;
    int sol_id = sol->num_solutions++;
    sol->isol[sol_id] = ALLOC_ARR(int, sol->ivar_size);
    return sol->isol[sol_id];
}

static int writeInt(int fd, const int *data, int size)
{
    int written = 0;
    size = sizeof(int) * size;
    while (size != written){
        ssize_t wret = write(fd, ((const char *)data) + written, size - written);
        if (wret < 0)
            return -1;
        written += wret;
        ASSERT(written <= size);
    }
    return 0;
}

static int readInt(int fd, int *data, int size)
{
    int readsize = 0;
    size = sizeof(int) * size;
    while (size != readsize){
        ssize_t rret = read(fd, ((char *)data) + readsize,
                            size - readsize);
        if (rret < 0)
            return -1;
        readsize += rret;
        ASSERT(readsize <= size);
    }
    return 0;
}

int pddlCPSolSerializeToFD(const pddl_cp_sol_t *sol, int fd)
{
    if (writeInt(fd, &sol->ivar_size, 1) != 0)
        return -1;
    if (writeInt(fd, &sol->num_solutions, 1) != 0)
        return -1;
    for (int i = 0; i < sol->num_solutions; ++i){
        if (writeInt(fd, sol->isol[i], sol->ivar_size) != 0)
            return -1;
    }
    return 0;
}

int pddlCPSolDeserializeFromFD(pddl_cp_sol_t *sol, int fd)
{
    ZEROIZE(sol);
    if (readInt(fd, &sol->ivar_size, 1) != 0)
        return -1;
    if (readInt(fd, &sol->num_solutions, 1) != 0)
        return -1;
    if (sol->num_solutions == 0)
        return 0;

    sol->isol_alloc = sol->num_solutions;
    sol->isol = ALLOC_ARR(int *, sol->isol_alloc);
    for (int i = 0; i < sol->num_solutions; ++i)
        sol->isol[i] = CALLOC_ARR(int, sol->ivar_size);

    for (int i = 0; i < sol->num_solutions; ++i){
        if (readInt(fd, sol->isol[i], sol->ivar_size) != 0)
            return -1;
    }
    return 0;
}

int pddlCPSolDeserializeFromMem(pddl_cp_sol_t *sol, void *_mem, size_t _memsize)
{
    ZEROIZE(sol);
    int *mem = _mem;
    int memsize = _memsize / sizeof(int);
    if (memsize < sizeof(int) * 2)
        return -1;
    sol->ivar_size = mem[0];
    sol->num_solutions = mem[1];
    if (sol->num_solutions == 0)
        return 0;

    sol->isol_alloc = sol->num_solutions;
    sol->isol = ALLOC_ARR(int *, sol->isol_alloc);
    for (int i = 0; i < sol->num_solutions; ++i)
        sol->isol[i] = CALLOC_ARR(int, sol->ivar_size);

    int memidx = 2;
    for (int i = 0; i < sol->num_solutions; ++i){
        if (memsize - memidx < sol->ivar_size)
            return -1;
        memcpy(sol->isol[i], mem + memidx, sizeof(int) * sol->ivar_size);
        memidx += sol->ivar_size;
    }
    return 0;
}

void pddlCPInit(pddl_cp_t *cp)
{
    ZEROIZE(cp);
    pddlCPIValTuplesInit(&cp->ival_tuple);
}

void pddlCPFree(pddl_cp_t *cp)
{
    pddlCPConstrIVarAllowedFree(&cp->c_ivar_allowed);
    pddlCPIValTuplesFree(&cp->ival_tuple);
    pddlCPIVarsFree(&cp->ivar);
    pddlISetFree(&cp->obj_ivars);
}

int pddlCPAddIVar(pddl_cp_t *cp, int min_val, int max_val, const char *name)
{
    if (cp->c_ivar_allowed.constr_size > 0){
        PANIC("CP: Adding variables after any constraints is prohibited!");
    }else if (cp->objective != OBJ_SAT){
        PANIC("CP: Adding variables after setting objective is prohibited!");
    }
    return pddlCPIVarsAdd(&cp->ivar, min_val, max_val, name);
}

int pddlCPAddIVarDomain(pddl_cp_t *cp, const pddl_iset_t *dom, const char *name)
{
    int id = pddlCPAddIVar(cp, 0, 0, name);
    pddlISetEmpty(&cp->ivar.ivar[id].domain);
    pddlISetUnion(&cp->ivar.ivar[id].domain, dom);
    return id;
}

void pddlCPAddConstrIVarEq(pddl_cp_t *cp, int var_id, int value)
{
    if (cp->unsat)
        return;

    pddl_cp_ivar_t *var = cp->ivar.ivar + var_id;
    if (pddlISetIn(value, &var->domain)){
        pddlISetEmpty(&var->domain);
        pddlISetAdd(&var->domain, value);
    }else{
        pddlISetEmpty(&var->domain);
    }
}

void pddlCPAddConstrIVarDomainArr(pddl_cp_t *cp, int var_id,
                                  int size, const int *val)
{
    if (cp->unsat)
        return;

    PDDL_ISET(dom);
    for (int i = 0; i < size; ++i)
        pddlISetAdd(&dom, val[i]);
    pddlISetIntersect(&cp->ivar.ivar[var_id].domain, &dom);
    pddlISetFree(&dom);
}

void pddlCPAddConstrIVarAllowed(pddl_cp_t *cp, int arity, const int *var,
                                int num_val_tuples, const int *val_tuples)
{
    if (cp->unsat)
        return;

    if (arity == 1){
        pddlCPAddConstrIVarDomainArr(cp, var[0], num_val_tuples, val_tuples);
        return;
    }

    int val_size;
    int *val = simplifyIVarAllowed(cp, arity, var, num_val_tuples,
                                   val_tuples, &val_size, NULL);
    if (val == NULL){
        cp->unsat = 1;
        return;
    }

    pddl_cp_ival_tuple_t *tup;
    tup = pddlCPIValTuplesAdd(&cp->ival_tuple, arity, val_size, val);
    pddlCPConstrIVarAllowedAdd(&cp->c_ivar_allowed, arity, var, tup);
    FREE(val);
}

void pddlCPSimplify(pddl_cp_t *cp)
{
    if (cp->unsat)
        return;

    simplify_change_t change[2];
    change[0].change = 1;
    change[0].ivar_change = ALLOC_ARR(int, cp->ivar.ivar_size);
    change[1].change = 1;
    change[1].ivar_change = CALLOC_ARR(int, cp->ivar.ivar_size);
    for (int i = 0; i < cp->ivar.ivar_size; ++i)
        change[0].ivar_change[i] = 1;
    int change_idx = 0, other = 1;
    do {
        other = (change_idx + 1) % 2;
        change[other].change = 0;
        ZEROIZE_ARR(change[other].ivar_change, cp->ivar.ivar_size);
       
        for (int ci = 0; ci < cp->c_ivar_allowed.constr_size; ++ci){
            pddl_cp_constr_ivar_allowed_t *c = cp->c_ivar_allowed.constr + ci;
            int skip = 1;
            for (int vi = 0; vi < c->arity; ++vi){
                if (change[change_idx].ivar_change[vi]){
                    skip = 0;
                    break;
                }
            }

            if (skip)
                continue;
            int val_size = 0;
            int *val = simplifyIVarAllowed(cp, c->arity, c->ivar,
                                           c->ival->num_tuples, c->ival->ival,
                                           &val_size, change + other);
            if (val == NULL){
                cp->unsat = 1;
                break;

            }else if (val_size != c->ival->num_tuples){
                pddlCPIValTupleUnref(&cp->ival_tuple, c->ival);
                c->ival = pddlCPIValTuplesAdd(&cp->ival_tuple, c->arity,
                                              val_size, val);
                ++c->ival->ref;
            }
            FREE(val);
        }
        change_idx = other;
    } while (change[other].change && !cp->unsat);

    if (change[0].ivar_change != NULL)
        FREE(change[0].ivar_change);
    if (change[1].ivar_change != NULL)
        FREE(change[1].ivar_change);

    if (cp->unsat)
        return;

    // Remove constraints restricted to a single tuple -- these must
    // already be reflected in domains of respective variables
    int ins = 0;
    for (int ci = 0; ci < cp->c_ivar_allowed.constr_size; ++ci){
        pddl_cp_constr_ivar_allowed_t *c;
        c = cp->c_ivar_allowed.constr + ci;
        if (c->ival->num_tuples == 1){
            pddlCPIValTupleUnref(&cp->ival_tuple, c->ival);
            c->ival = NULL;
            pddlCPConstIVarAllowedFree(c);
        }else{
            cp->c_ivar_allowed.constr[ins++] = *c;
        }
    }
    cp->c_ivar_allowed.constr_size = ins;
}

void pddlCPSetObjectiveMinCountDiffAllIVars(pddl_cp_t *cp)
{
    cp->objective = OBJ_MIN_COUNT_DIFF;
    pddlISetEmpty(&cp->obj_ivars);
    for (int vi = 0; vi < cp->ivar.ivar_size; ++vi)
        pddlISetAdd(&cp->obj_ivars, vi);
}

void pddlCPSetObjectiveMinCountDiff(pddl_cp_t *cp, const pddl_iset_t *ivars)
{
    cp->objective = OBJ_MIN_COUNT_DIFF;
    pddlISetEmpty(&cp->obj_ivars);
    pddlISetUnion(&cp->obj_ivars, ivars);
}

void pddlCPWriteMinizinc(const pddl_cp_t *cp, FILE *fout)
{
    if (cp->unsat){
        fprintf(fout, "%% UNSATISFIABLE\n");
        return;
    }

    fprintf(fout, "include \"table.mzn\";\n");
    if (cp->objective == OBJ_MIN_COUNT_DIFF)
        fprintf(fout, "include \"nvalue_fn.mzn\";\n");

    // Define integer variables
    for (int vi = 0; vi < cp->ivar.ivar_size; ++vi){
        const pddl_cp_ivar_t *var = cp->ivar.ivar + vi;
        fprintf(fout, "var {");
        for (int i = 0; i < pddlISetSize(&var->domain); ++i){
            if (i != 0)
                fprintf(fout, ",");
            fprintf(fout, "%d", pddlISetGet(&var->domain, i));
        }
        fprintf(fout, "}: x%d; %% %s\n", var->id, var->name);
    }

    // Define integer value tuples
    for (int vi = 0; vi < cp->ival_tuple.tuple_size; ++vi){
        const pddl_cp_ival_tuple_t *tup = cp->ival_tuple.tuple[vi];
        fprintf(fout, "array[1..%d,1..%d] of int: ival%d = [",
                tup->num_tuples, tup->arity, tup->idx);
        for (int row = 0, idx = 0; row < tup->num_tuples; ++row){
            fprintf(fout, "|%d", tup->ival[idx++]);
            for (int i = 1; i < tup->arity; ++i){
                fprintf(fout, ",%d", tup->ival[idx++]);
            }
        }
        fprintf(fout, "|];\n");
    }

    // Define table constraints
    for (int ci = 0; ci < cp->c_ivar_allowed.constr_size; ++ci){
        const pddl_cp_constr_ivar_allowed_t *c;
        c = cp->c_ivar_allowed.constr + ci;
        fprintf(fout, "constraint table([x%d", c->ivar[0]);
        for (int i = 1; i < c->arity; ++i)
            fprintf(fout, ",x%d", c->ivar[i]);
        fprintf(fout, "], ival%d);\n", c->ival->idx);
    }

    // Specify solution
    if (cp->objective == OBJ_SAT){
        fprintf(fout, "solve satisfy;\n");

    }else if (cp->objective == OBJ_MIN_COUNT_DIFF){
        ASSERT_RUNTIME(pddlISetSize(&cp->obj_ivars) > 0);
        fprintf(fout, "var int: obj_val = nvalue([x%d",
                pddlISetGet(&cp->obj_ivars, 0));
        for (int i = 1; i < pddlISetSize(&cp->obj_ivars); ++i)
            fprintf(fout, ",x%d", pddlISetGet(&cp->obj_ivars, i));
        fprintf(fout, "]);\n");
        fprintf(fout, "solve minimize obj_val;\n");
    }

    // Output format
    fprintf(fout, "output [\"\\(x0)");
    for (int i = 1; i < cp->ivar.ivar_size; ++i)
        fprintf(fout, " \\(x%d)", i);
    fprintf(fout, "\\n\"];\n");
    fflush(fout);
}

void pddlCPSetDefaultSolver(pddl_cp_solver_t solver_id)
{
    default_solver = solver_id;
}

static int solve(const pddl_cp_t *cp,
                 const pddl_cp_solve_config_t *cfg,
                 pddl_cp_sol_t *sol,
                 pddl_err_t *err)
{
    pddl_cp_solver_t solver = cfg->solver;
    if (solver == PDDL_CP_SOLVER_DEFAULT){
#ifdef PDDL_CPOPTIMIZER
        solver = PDDL_CP_SOLVER_CPOPTIMIZER;
#else /* PDDL_CPOPTIMIZER */
        solver = PDDL_CP_SOLVER_MINIZINC;
#endif /* PDDL_CPOPTIMIZER */
        if (default_solver != PDDL_CP_SOLVER_DEFAULT)
            solver = default_solver;
    }

    switch (solver){
        case PDDL_CP_SOLVER_CPOPTIMIZER:
            return pddlCPSolve_CPOptimizer(cp, cfg, sol, err);
        case PDDL_CP_SOLVER_MINIZINC:
            return pddlCPSolve_Minizinc(cp, cfg, sol, err);
        default:
            PANIC("Unkown solver ID %d", solver);
    }
    PANIC("Unkown solver ID %d", solver);
    return -1;
}

struct solve_arg {
    const pddl_cp_t *cp;
    const pddl_cp_solve_config_t *cfg;
    pddl_cp_sol_t *sol;
    pddl_err_t *err;
};

static int _solveInSubprocess(int fdout, void *userdata)
{
    struct solve_arg *arg = userdata;
    int ret = solve(arg->cp, arg->cfg, arg->sol, arg->err);
    if (ret == PDDL_CP_FOUND || ret == PDDL_CP_FOUND_SUBOPTIMAL){
        LOG(arg->err, "Found solution -- serializing the solution for the"
             " parent process...");
        if (pddlCPSolSerializeToFD(arg->sol, fdout) != 0){
            LOG(arg->err, "Failed to serialize output!");
            return PDDL_CP_ABORTED;
        }
    }
    return ret;
}

static int solveInSubprocess(const pddl_cp_t *cp,
                             const pddl_cp_solve_config_t *cfg,
                             pddl_cp_sol_t *sol,
                             pddl_err_t *err)
{
    pddl_exec_status_t status;
    void *data;
    int data_size;

    struct solve_arg arg = { cp, cfg, sol, err };
    if (pddlForkPipe(_solveInSubprocess, &arg, &data, &data_size,
                     &status, err) != 0){
        return PDDL_CP_ABORTED;
    }
    if (status.signaled)
        return PDDL_CP_ABORTED;

    int ret = status.exit_status;
    LOG(err, "Exit status: %d", ret);
    if (ret == PDDL_CP_FOUND || ret == PDDL_CP_FOUND_SUBOPTIMAL){
        LOG(err, "Parsing solutions...");
        pddlCPSolDeserializeFromMem(sol, data, data_size);
    }
    return ret;
}

static int onlyOneSolution(const pddl_cp_t *cp, pddl_cp_sol_t *sol)
{
    for (int i = 0; i < cp->ivar.ivar_size; ++i){
        if (pddlISetSize(&cp->ivar.ivar[i].domain) > 1)
            return -1;
    }
    sol->ivar_size = cp->ivar.ivar_size;
    sol->num_solutions = 1;
    sol->isol = ALLOC(int *);
    sol->isol_alloc = 1;
    sol->isol[0] = ALLOC_ARR(int, sol->ivar_size);
    for (int i = 0; i < cp->ivar.ivar_size; ++i)
        sol->isol[0][i] = pddlISetGet(&cp->ivar.ivar[i].domain, 0);

    return 0;
}

int pddlCPSolve(const pddl_cp_t *cp,
                const pddl_cp_solve_config_t *cfg,
                pddl_cp_sol_t *sol,
                pddl_err_t *err)
{
    CTX(err, "cp_solve", "CP-solve");
    ZEROIZE(sol);

    if (cp->unsat){
        CTXEND(err);
        return PDDL_CP_NO_SOLUTION;
    }

    if (onlyOneSolution(cp, sol) == 0){
        CTXEND(err);
        return PDDL_CP_FOUND;
    }

    int ret = 0;
    if (cfg->run_in_subprocess){
        ret = solveInSubprocess(cp, cfg, sol, err);
    }else{
        ret = solve(cp, cfg, sol, err);
    }
    CTXEND(err);
    return ret;
}
