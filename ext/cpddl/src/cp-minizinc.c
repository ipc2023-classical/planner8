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
#include "pddl/subprocess.h"
#include "_cp.h"

int pddlCPSolve_Minizinc(const pddl_cp_t *cp,
                         const pddl_cp_solve_config_t *cfg,
                         pddl_cp_sol_t *sol,
                         pddl_err_t *err)
{
    ZEROIZE(sol);

    if (cfg->minizinc == NULL){
        LOG(err, "default minizinc [%s] version %s", PDDL_MINIZINC_BIN,
            PDDL_MINIZINC_VERSION);
    }else{
        LOG(err, "minizinc [%s]", cfg->minizinc);
    }
    char *buf = NULL;
    size_t bufsize = 0;

    FILE *fout = open_memstream(&buf, &bufsize);
    ASSERT_RUNTIME(fout != NULL);
    pddlCPWriteMinizinc(cp, fout);
    fflush(fout);
    fclose(fout);
    LOG(err, "Problem written in mzn format");

    char *argv[] = {
        (char *)(cfg->minizinc == NULL ? PDDL_MINIZINC_BIN : cfg->minizinc),
        "-a", // list all solutions
        "--soln-sep", "", // empty solution separator
        "--unsat-msg", "UNSAT", // message for unsatisfiable problem
        "--unknown-msg", "UNKNOWN", // message for unknown output
        "--unbounded-msg", "UNBOUND", // message for unbounded problem
        "--unsatorunbnd-msg", "UNSATorUNBOUND", // unsat or unbound message
        "--unknown-msg", "UNKNOWN", // "unknown" message
        "--error-msg", "ERR", // error message
        "--search-complete-msg", "SAT", // successfuly found solution message
        "-", // read .mzn from stdin
        NULL, NULL, // placeholders for other arguments
        NULL
    };
    char arg_time_limit[64];

    if (cfg->max_search_time > 0.){
        ASSERT(strcmp(argv[17], "SAT") == 0);
        sprintf(arg_time_limit, "%lu", (unsigned long)(1000. * cfg->max_search_time));
        argv[18] = "--time-limit";
        argv[19] = arg_time_limit;
        argv[20] = "-";
    }
    ASSERT(argv[21] == NULL);

    int ret = PDDL_CP_UNKNOWN;
    pddl_exec_status_t status;
    char *solbuf = NULL;
    int solbuf_size;
    int execret = pddlExecvp(argv, &status, buf, bufsize,
                             &solbuf, &solbuf_size, NULL, NULL, err);
    ASSERT_RUNTIME(execret == 0);
    if (status.signaled){
        LOG(err, "Something went wrong.");
        LOG(err, "Minizinc was killed by a signal.");
        ret = PDDL_CP_UNKNOWN;
        goto minizinc_end;
    }
    if (solbuf_size == 0){
        LOG(err, "Something went wrong.");
        LOG(err, "Minizinc did not print any output.");
        ret = PDDL_CP_UNKNOWN;
        goto minizinc_end;
    }

    // Read solution(s)
    int *solarr = ALLOC_ARR(int, cp->ivar.ivar_size);
    int solidx = 0;
    int offset = 0;
    int have_solution = 0;
    int sread;
    while (sscanf(solbuf + offset, "%d%n", solarr + solidx, &sread) > 0){
        // TODO: More solutions, for now only reading until we get the last
        //       solution
        offset += sread;
        solidx = (solidx + 1) % cp->ivar.ivar_size;
        have_solution = 1;
    }

    if (solidx != 0){
        LOG(err, "Something went wrong.");
        LOG(err, "Minizinc did not print the full output.");
        if (solarr != NULL)
            FREE(solarr);
        ret = PDDL_CP_UNKNOWN;
        goto minizinc_end;
    }

    // Skip whitespace before reading the result
    while (solbuf[offset] != '\x0'
            && (solbuf[offset] == ' '
                || solbuf[offset] == '\n'
                || solbuf[offset] == '\t')){
        ++offset;
    }

    if (strncmp(solbuf + offset, "SAT", 3) == 0){
        ret = PDDL_CP_FOUND;
        LOG(err, "Solved optimally");
    }else if (strncmp(solbuf + offset, "UNKNOWN", 7) == 0){
        if (have_solution){
            ret = PDDL_CP_FOUND_SUBOPTIMAL;
            LOG(err, "Solved sub-optimally");
        }else{
            ret = PDDL_CP_ABORTED;
            LOG(err, "No solution found before time limit");
        }
    }else if (strncmp(solbuf + offset, "UNSAT", 5) == 0){
        ret = PDDL_CP_NO_SOLUTION;
        LOG(err, "Unsolvable");
    }else if (strncmp(solbuf + offset, "UNBOUND", 7) == 0){
        ret = PDDL_CP_NO_SOLUTION;
        LOG(err, "Unbounded");
    }else if (strncmp(solbuf + offset, "UNSATorUNBOUND", 15) == 0){
        ret = PDDL_CP_NO_SOLUTION;
        LOG(err, "Unsolvable or unbounded");
    }else if (strncmp(solbuf + offset, "ERR", 3) == 0){
        ret = PDDL_CP_ABORTED;
        LOG(err, "Error");
    }else{
        LOG(err, "Something went wrong.");
        LOG(err, "Minizinc did not print the full output -- missing result indicator.");
        if (solarr != NULL)
            FREE(solarr);
        ret = PDDL_CP_UNKNOWN;
        goto minizinc_end;
    }

    if (ret == PDDL_CP_FOUND || ret == PDDL_CP_FOUND_SUBOPTIMAL)
        pddlCPSolAdd(cp, sol, solarr);


    if (solarr != NULL)
        FREE(solarr);
minizinc_end:
    if (solbuf != NULL)
        FREE(solbuf);
    if (buf != NULL)
        free(buf);
    return ret;
}
