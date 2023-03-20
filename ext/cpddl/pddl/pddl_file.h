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

#ifndef __PDDL_FILE_H__
#define __PDDL_FILE_H__

#include <pddl/err.h>
#include <pddl/config.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define PDDL_FILE_MAX_PATH_LEN 4096
#define PDDL_FILE_MAX_NAME_LEN 128

struct pddl_files {
    char domain_pddl[PDDL_FILE_MAX_PATH_LEN];
    char problem_pddl[PDDL_FILE_MAX_PATH_LEN];
};
typedef struct pddl_files pddl_files_t;

int pddlFiles1(pddl_files_t *files, const char *s, pddl_err_t *err);
int pddlFiles(pddl_files_t *files, const char *s1, const char *s2,
              pddl_err_t *err);
int pddlFilesFindOptimalCost(pddl_files_t *files, pddl_err_t *err);

int pddlIsFile(const char *);
int pddlIsDir(const char *);
char *pddlDirname(const char *fn);
char **pddlListDir(const char *dname, int *list_size, pddl_err_t *err);
char **pddlListDirPDDLFiles(const char *dname, int *list_size, pddl_err_t *err);


struct pddl_bench_task {
    pddl_files_t pddl_files;
    int optimal_cost;
    char bench_name[PDDL_FILE_MAX_NAME_LEN];
    char domain_name[PDDL_FILE_MAX_NAME_LEN];
    char problem_name[PDDL_FILE_MAX_NAME_LEN];
};
typedef struct pddl_bench_task pddl_bench_task_t;

struct pddl_bench {
    pddl_bench_task_t *task;
    int task_size;
    int task_alloc;
};
typedef struct pddl_bench pddl_bench_t;

void pddlBenchInit(pddl_bench_t *bench);
void pddlBenchFree(pddl_bench_t *bench);
int pddlBenchLoadDir(pddl_bench_t *bench, const char *dirpath);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_FILE_H__ */
