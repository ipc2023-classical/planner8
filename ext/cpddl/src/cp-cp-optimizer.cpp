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
#include "_cp.h"

#ifdef PDDL_CPOPTIMIZER
#define IL_STD
#include <ilcp/cp.h>
#include <ilcplex/cpxconst.h>

#if CPX_VERSION_VERSION < 12 || (CPX_VERSION_VERSION == 12 && CPX_VERSION_RELEASE < 9)
# define NO_LOGGER
#endif

#ifndef NO_LOGGER
class Logger : public IloCP::Callback {
    pddl_err_t *err;

  public:
    Logger(pddl_err_t *err) : err(err){}
#if CPX_VERSION_VERSION == 12 && CPX_VERSION_RELEASE == 9
    virtual void invoke(IloCP cp, Callback::Type reason)
#else
    virtual void invoke(IloCP cp, Callback::Reason reason)
#endif
    {
        if (reason == Periodic){
            /*
            PDDL_INFO(err, "    cpoptimizer: mem: %ldMB, solutions: %d",
                     (long)cp.getInfo(IloCP::MemoryUsage) / (1024L * 1024L),
                     (int)cp.getInfo(IloCP::NumberOfSolutions));
            */

        }else if (reason == Solution){
            LOG(err, "cpoptimizer: mem: %ldMB, solutions: %d",
                     (long)cp.getInfo(IloCP::MemoryUsage) / (1024L * 1024L),
                     (int)cp.getInfo(IloCP::NumberOfSolutions));

        }else if (reason == Proof){
            LOG(err, "cpoptimizer: mem: %ldMB, solutions: %d, proof",
                (long)cp.getInfo(IloCP::MemoryUsage) / (1024L * 1024L),
                (int)cp.getInfo(IloCP::NumberOfSolutions));

        }else if (reason == ObjBound){
            LOG(err, "cpoptimizer: mem: %ldMB, solutions: %d, new bound: %.2f",
                (long)cp.getInfo(IloCP::MemoryUsage) / (1024L * 1024L),
                (int)cp.getInfo(IloCP::NumberOfSolutions),
                (double)cp.getObjBound());
        }
    }
};
#endif /* NO_LOGGER */

int pddlCPSolve_CPOptimizer(const pddl_cp_t *cp,
                            const pddl_cp_solve_config_t *cfg,
                            pddl_cp_sol_t *sol,
                            pddl_err_t *err)
{
    ZEROIZE(sol);

    IloEnv env;
    LOG(err, "CP Optimizer %s", env.getVersion());
    IloModel model(env);

    IloIntVarArray cpvar(env, cp->ivar.ivar_size);
    for (int vi = 0; vi < cp->ivar.ivar_size; ++vi){
        const pddl_iset_t *dom = &cp->ivar.ivar[vi].domain;
        IloIntArray vals(env, pddlISetSize(dom));
        for (int i = 0; i < pddlISetSize(dom); ++i)
            vals[i] = pddlISetGet(dom, i);
        cpvar[vi] = IloIntVar(env, vals, cp->ivar.ivar[vi].name);
    }

    for (int ti = 0; ti < cp->ival_tuple.tuple_size; ++ti){
        const pddl_cp_ival_tuple_t *tup = cp->ival_tuple.tuple[ti];
        IloIntTupleSet cpval(env, tup->arity);
        for (int vi = 0, idx = 0; vi < tup->num_tuples; ++vi, idx += tup->arity){
            IloIntArray cpval_tuple(env, tup->arity);
            for (int i = 0; i < tup->arity; ++i)
                cpval_tuple[i] = tup->ival[idx + i];
            cpval.add(cpval_tuple);
        }

        for (int ci = 0; ci < cp->c_ivar_allowed.constr_size; ++ci){
            const pddl_cp_constr_ivar_allowed_t *c;
            c = cp->c_ivar_allowed.constr + ci;
            if (c->ival->idx != tup->idx)
                continue;
            ASSERT(c->arity == tup->arity);
            IloIntVarArray cvar(env, c->arity);
            for (int i = 0; i < c->arity; ++i){
                ASSERT(c->ivar[i] <= cp->ivar.ivar_size);
                cvar[i] = cpvar[c->ivar[i]];
            }
            model.add(IloAllowedAssignments(env, cvar, cpval));
        }
    }

    if (cp->objective == OBJ_MIN_COUNT_DIFF){
        IloIntVarArray cvar(env, pddlISetSize(&cp->obj_ivars));
        for (int i = 0; i < pddlISetSize(&cp->obj_ivars); ++i)
            cvar[i] = cpvar[pddlISetGet(&cp->obj_ivars, i)];
        IloObjective obj = IloMinimize(env, IloCountDifferent(cvar));
        model.add(obj);
    }

    IloCP solve(model);
    //solve.dumpModel("model.cpo");
#ifndef NO_LOGGER
    Logger logger(err);
    solve.addCallback(&logger);
#endif /* NO_LOGGER */
    solve.setParameter(IloCP::LogVerbosity, IloCP::Quiet);
    if (cfg->num_threads <= 1){
        solve.setParameter(IloCP::Workers, 1);
    }else{
        solve.setParameter(IloCP::Workers, cfg->num_threads);
    }
    if (cfg->max_search_time > 0.)
        solve.setParameter(IloCP::TimeLimit, cfg->max_search_time);

    // TODO: satisfy/optimize
    int ret = PDDL_CP_UNKNOWN;
    LOG(err, "Solving model ...");
    solve.startNewSearch();
    while (solve.next()){
        LOG(err, "Found solution.");
        if (cp->objective == OBJ_SAT){
            ASSERT(sol->num_solutions == 0);
            int *s = pddlCPSolAddEmpty(cp, sol);
            for (int i = 0; i < sol->ivar_size; ++i){
                if (solve.isExtracted(cpvar[i])){
                    s[i] = solve.getValue(cpvar[i]);
                }else{
                    s[i] = cpvar[i].getMin();
                }
            }
            ret = PDDL_CP_FOUND;
            // TODO: More solutions
            break;

        }else{
            // Each call of next() is guaranteed to return solution
            // strictly better than the previous one
            if (sol->num_solutions == 1){
                int *s = pddlCPSolGet(sol, 0);
                for (int i = 0; i < sol->ivar_size; ++i){
                    if (solve.isExtracted(cpvar[i])){
                        s[i] = solve.getValue(cpvar[i]);
                    }else{
                        s[i] = cpvar[i].getMin();
                    }
                }

            }else{
                ASSERT(sol->num_solutions == 0);
                int *s = pddlCPSolAddEmpty(cp, sol);
                for (int i = 0; i < sol->ivar_size; ++i){
                    if (solve.isExtracted(cpvar[i])){
                        s[i] = solve.getValue(cpvar[i]);
                    }else{
                        s[i] = cpvar[i].getMin();
                    }
                }
            }
            ret = PDDL_CP_FOUND_SUBOPTIMAL;
        }
    }

    switch (solve.getInfo(IloCP::FailStatus)){
        case IloCP::SearchHasNotFailed:
        case IloCP::SearchHasFailedNormally:
            if (sol->num_solutions == 0){
                ret = PDDL_CP_NO_SOLUTION;
                LOG(err, "Provably No Solution");
            }else{
                ret = PDDL_CP_FOUND;
                LOG(err, "Optimal solution");
            }
            break;
        case IloCP::SearchStoppedByLimit:
            LOG(err, "Terminated by a time or fail limit");
            if (sol->num_solutions == 0)
                ret = PDDL_CP_REACHED_LIMIT;
            break;
        case IloCP::SearchStoppedByLabel:
            LOG(err, "Terminated -- stopped-by-label");
            ret = PDDL_CP_UNKNOWN;
            break;
        case IloCP::SearchStoppedByExit:
            LOG(err, "Terminated by exitSearch()");
            ret = PDDL_CP_ABORTED;
            break;
        case IloCP::SearchStoppedByAbort:
            LOG(err, "Aborted");
            ret = PDDL_CP_ABORTED;
            break;
        case IloCP::UnknownFailureStatus:
            LOG(err, "Unknown failure");
            ret = PDDL_CP_UNKNOWN;
            break;
    }

    solve.endSearch();
    solve.end();
    env.end();
    return ret;
}

#else /* PDDL_CPOPTIMIZER */
int pddlCPSolve_CPOptimizer(const pddl_cp_t *cp,
                            const pddl_cp_solve_config_t *cfg,
                            pddl_cp_sol_t *sol,
                            pddl_err_t *err)
{
    PANIC("Compiled without IBM CP Optimizer");
    return -1;
}
#endif /* PDDL_CPOPTIMIZER */
