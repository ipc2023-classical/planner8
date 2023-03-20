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
#include "pddl/pddl_file.h"
#include "pddl/sort.h"

#define MAX_LEN 512
#define BUFSIZE 1024

int pddlIsDir(const char *d)
{
    struct stat st;
    if (stat(d, &st) == -1)
        return 0;

    if (S_ISDIR(st.st_mode))
        return 1;
    return 0;
}

int pddlIsFile(const char *d)
{
    struct stat st;
    if (stat(d, &st) == -1)
        return 0;

    if (S_ISREG(st.st_mode))
        return 1;
    return 0;
}

char *pddlDirname(const char *fn)
{
    char *dname = STRDUP(fn);
    int len = strlen(dname);
    int pos = len - 1;
    for (; pos >= 0 && dname[pos] != '/'; --pos);
    if (pos >= 0 && pos < len - 1)
        dname[pos + 1] = '\x0';

    char path[4096];
    if (realpath(dname, path) == NULL)
        PANIC("Could not resolve path %s", fn);
    FREE(dname);
    return STRDUP(path);
}

static int cmpFilename(const void *a, const void *b, void *_)
{
    char *s1 = *(char **)a;
    char *s2 = *(char **)b;
    return strcmp(s1, s2);
}

static char **_pddlListDir(const char *dname,
                           int *list_size,
                           const char *suff,
                           pddl_err_t *err)
{
    *list_size = 0;
    if (!pddlIsDir(dname))
        ERR_RET(err, NULL, "%s is not a directory", dname);

    int dname_size = strlen(dname);
    int suff_size = -1;
    if (suff != NULL)
        suff_size = strlen(suff);
    int alloc = 2;
    char **list = ALLOC_ARR(char *, alloc);

    DIR *dir = opendir(dname);
    if (dir == NULL){
        FREE(list);
        ERR_RET(err, NULL, "Could not open directory %s", dname);
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL){
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        int esize = strlen(entry->d_name);
        if (suff_size > 0){
            if (esize < suff_size
                    || strcmp(entry->d_name + esize - suff_size, suff) != 0){
                continue;
            }
        }

        if (*list_size == alloc){
            alloc *= 2;
            list = REALLOC_ARR(list, char *, alloc);
        }
        list[*list_size] = ALLOC_ARR(char, dname_size + esize + 2);
        sprintf(list[*list_size], "%s/%s", dname, entry->d_name);
        *list_size += 1;
    }
    closedir(dir);

    pddlSort(list, *list_size, sizeof(char *), cmpFilename, NULL);
    return list;
}

char **pddlListDir(const char *dname, int *list_size, pddl_err_t *err)
{
    return _pddlListDir(dname, list_size, NULL, err);
}

char **pddlListDirPDDLFiles(const char *dname, int *list_size, pddl_err_t *err)
{
    return _pddlListDir(dname, list_size, ".pddl", err);
}

static void extractDir(const char *path, char *dir)
{
    int len = strlen(path);
    strcpy(dir, path);
    int idx;
    for (idx = len - 1; idx >= 0 && dir[idx] != '/'; --idx);
    if (idx >= 0){
        dir[idx + 1] = 0x0;
    }else{
        strcpy(dir, "./");
    }
}

static void extractProblemName(const char *prob, char *name)
{
    int idx;
    for (idx = strlen(prob) - 1; idx >= 0 && prob[idx] != '/'; --idx);
    strcpy(name, prob + idx + 1);
    int len = strlen(name);
    if (strcmp(name + len - 5, ".pddl") == 0)
        name[len - 5] = 0x0;
}

static int findDomainReplace(const char *prob_name,
                             const char *find,
                             const char *replace,
                             char *domain_pddl,
                             int dirlen)
{
    char *s;

    domain_pddl[dirlen] = 0x0;
    if ((s = strstr(prob_name, find)) != NULL){
        int len = dirlen;
        if (s != prob_name){
            strncpy(domain_pddl + dirlen, prob_name, s - prob_name);
            len += s - prob_name;
        }
        strcpy(domain_pddl + len, replace);

        s += strlen(find);
        if (*s != 0x0)
            sprintf(domain_pddl + strlen(domain_pddl), "%s.pddl", s);

        if (pddlIsFile(domain_pddl))
            return 0;
    }

    return -1;
}

static int findDomainToProblem(const char *prob, char *domain_pddl)
{
    extractDir(prob, domain_pddl);
    int dirlen = strlen(domain_pddl);
    char *s;

    char prob_name[PDDL_FILE_MAX_PATH_LEN];
    extractProblemName(prob, prob_name);

    domain_pddl[dirlen] = 0x0;
    sprintf(domain_pddl + dirlen, "domain_%s.pddl", prob_name);
    if (pddlIsFile(domain_pddl))
        return 0;

    if (findDomainReplace(prob_name, "problem", "domain",
                          domain_pddl, dirlen) == 0){
        return 0;
    }
    if (findDomainReplace(prob_name, "satprob", "satdom",
                          domain_pddl, dirlen) == 0){
        return 0;
    }
    if (findDomainReplace(prob_name, "satprob", "dom",
                          domain_pddl, dirlen) == 0){
        return 0;
    }
    if (findDomainReplace(prob_name, "prob", "dom",
                          domain_pddl, dirlen) == 0){
        return 0;
    }

    domain_pddl[dirlen] = 0x0;
    sprintf(domain_pddl + dirlen, "domain-%s.pddl", prob_name);
    if (pddlIsFile(domain_pddl))
        return 0;

    domain_pddl[dirlen] = 0x0;
    sprintf(domain_pddl + dirlen, "%s-domain.pddl", prob_name);
    if (pddlIsFile(domain_pddl))
        return 0;

    domain_pddl[dirlen] = 0x0;
    strcpy(domain_pddl + dirlen, "domain.pddl");
    if (pddlIsFile(domain_pddl))
        return 0;

    domain_pddl[dirlen] = 0x0;
    s = prob_name;
    while (strlen(s) > 1 && (s = strstr(s + 1, "-")) != NULL){
        strncpy(domain_pddl + dirlen, prob_name, s - prob_name);
        strcpy(domain_pddl + dirlen + (s - prob_name), "-domain.pddl");
        if (pddlIsFile(domain_pddl))
            return 0;
    }

    return -1;
}

int pddlFiles1(pddl_files_t *files, const char *s, pddl_err_t *err)
{
    if (pddlIsFile(s)){
        if (strlen(s) >= PDDL_FILE_MAX_PATH_LEN - 1){
            PDDL_ERR_RET(err, -1, "Path(s) too long.");
        }

        if (findDomainToProblem(s, files->domain_pddl) == 0){
            strcpy(files->problem_pddl, s);
            return 0;
        }else{
            PDDL_ERR_RET(err, -1, "Cannot find domain pddl file.");
        }

    }else{
        if (strlen(s) + 5 >= PDDL_FILE_MAX_PATH_LEN - 1){
            PDDL_ERR_RET(err, -1, "Path(s) too long.");
        }

        char prob_pddl[MAX_LEN];
        strcpy(prob_pddl, s);
        strcpy(prob_pddl + strlen(prob_pddl), ".pddl");
        if (pddlIsFile(prob_pddl)){
            return pddlFiles1(files, prob_pddl, err);

        }else{
            PDDL_ERR_RET(err, -1, "Cannot find problem pddl file"
                         " (tried %s or %s).",
                         s, prob_pddl);
        }
    }
}

int pddlFiles(pddl_files_t *files, const char *s1, const char *s2,
              pddl_err_t *err)
{
    ZEROIZE(files);

    if (s1 == NULL && s2 == NULL){
        PDDL_ERR_RET(err, -1, "Unspecified specifiers.");

    }else if (s1 == NULL && s2 != NULL){
        return pddlFiles1(files, s2, err);

    }else if (s1 != NULL && s2 == NULL){
        return pddlFiles1(files, s1, err);

    }else{
        if (pddlIsFile(s1) && pddlIsFile(s2)){
            if (strlen(s1) >= PDDL_FILE_MAX_PATH_LEN - 1
                    || strlen(s2) >= PDDL_FILE_MAX_PATH_LEN - 1){
                PDDL_ERR_RET(err, -1, "Path(s) too long.");
            }
            strcpy(files->domain_pddl, s1);
            strcpy(files->problem_pddl, s2);
            return 0;

        }else if (pddlIsDir(s1)){
            if (strlen(s1) + strlen(s2) >= PDDL_FILE_MAX_PATH_LEN - 1){
                PDDL_ERR_RET(err, -1, "Path(s) too long.");
            }

            char prob[PDDL_FILE_MAX_PATH_LEN];
            strcpy(prob, s1);
            if (s1[strlen(s1) - 1] != '/')
                strcpy(prob + strlen(prob), "/");
            strcpy(prob + strlen(prob), s2);
            return pddlFiles1(files, prob, err);

        }else{
            PDDL_ERR_RET(err, -1, "Cannot find pddl files.");
        }
    }
}

int pddlFilesFindOptimalCost(pddl_files_t *files, pddl_err_t *err)
{
    int problem_len = strlen(files->problem_pddl);
    if (strcmp(files->problem_pddl + problem_len - 5, ".pddl") != 0)
        return -1;

    char plan_file[PDDL_FILE_MAX_PATH_LEN];
    strcpy(plan_file, files->problem_pddl);
    strcpy(plan_file + problem_len - 4, "plan");
    if (!pddlIsFile(plan_file))
        return -1;

    FILE *fin = fopen(plan_file, "r");
    if (fin == NULL)
        return -1;

    int optimal_cost = -1;
    char *line = NULL;
    size_t linesiz = 0;
    ssize_t readsiz;
    const char *found = NULL;
    for (int i = 0; i < 3 && (readsiz = getline(&line, &linesiz, fin)) > 0; ++i){
        if (line[0] == ';' && (found = strstr(line, "Optimal cost: ")) != NULL){
            optimal_cost = atoi(found + 14);
            break;
        }
    }
    if (line != NULL)
        free(line);
    fclose(fin);
    return optimal_cost;
}

void pddlBenchInit(pddl_bench_t *bench)
{
    ZEROIZE(bench);
}

void pddlBenchFree(pddl_bench_t *bench)
{
    if (bench->task != NULL)
        FREE(bench->task);
}

static void benchAdd(pddl_bench_t *bench, const pddl_files_t *fs)
{
    if (bench->task_size == bench->task_alloc){
        if (bench->task_alloc == 0)
            bench->task_alloc = 2;
        bench->task_alloc *= 2;
        bench->task = REALLOC_ARR(bench->task, pddl_bench_task_t,
                                  bench->task_alloc);
    }

    pddl_bench_task_t *task = bench->task + bench->task_size++;
    ZEROIZE(task);
    char *rpath = realpath(fs->domain_pddl, task->pddl_files.domain_pddl);
    ASSERT_RUNTIME(rpath != NULL);
    rpath = realpath(fs->problem_pddl, task->pddl_files.problem_pddl);
    ASSERT_RUNTIME(rpath != NULL);
    task->optimal_cost = pddlFilesFindOptimalCost(&task->pddl_files, NULL);

    char path[PDDL_FILE_MAX_PATH_LEN];
    strcpy(path, fs->problem_pddl);
    int len = strlen(path);
    char *cur = path + len - 1;
    for (; *cur != '.' && cur > path; --cur)
        ;
    if (*cur == '.')
        *cur = '\0';
    for (; *cur != '/' && cur > path; --cur)
        ;
    if (*cur == '/'){
        strcpy(task->problem_name, cur + 1);

        if (cur > path){
            *cur = '\0';
            for (--cur; *cur != '/' && cur > path; --cur)
                ;
            if (*cur == '/'){
                strcpy(task->domain_name, cur + 1);
            }

            if (cur > path){
                *cur = '\0';
                for (--cur; *cur != '/' && cur > path; --cur)
                    ;
                if (*cur == '/'){
                    if (strncmp(cur + 1, "seq-", 4) == 0){
                        if (cur > path){
                            *cur = '\0';
                            for (--cur; *cur != '/' && cur > path; --cur)
                                ;
                            if (*cur == '/')
                                strcpy(task->bench_name, cur + 1);
                        }
                    }else{
                        strcpy(task->bench_name, cur + 1);
                    }
                }
            }
        }
    }
}

int pddlBenchLoadDir(pddl_bench_t *bench, const char *dirpath)
{
    if (!pddlIsDir(dirpath))
        return -1;

    DIR *dir = opendir(dirpath);
    if (dir == NULL)
        return -1;

    char path[PDDL_FILE_MAX_PATH_LEN];
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL){
        if (strncmp(entry->d_name, ".", 1) == 0)
            continue;
        if (strstr(entry->d_name, "domain") != NULL)
            continue;

        int path_len = snprintf(path, PDDL_FILE_MAX_PATH_LEN - 1, "%s/%s",
                                dirpath, entry->d_name);
        char *rpath = realpath(path, NULL);
        if (rpath == NULL)
            continue;

        if (pddlIsDir(rpath)){
            if (pddlBenchLoadDir(bench, path) != 0){
                free(rpath);
                closedir(dir);
                return -1;
            }

        }else if (pddlIsFile(rpath)
                    && path_len > 4
                    && strcmp(path + path_len - 5, ".pddl") == 0){
            pddl_files_t fs;
            if (pddlFiles1(&fs, path, NULL) == 0)
                benchAdd(bench, &fs);
        }
        free(rpath);
    }
    closedir(dir);
    return 0;
}
