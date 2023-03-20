#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include "pddl/pddl.h"
#include "opts.h"

#define PATHSIZE 512

#define COMMAND_GEN 1
#define COMMAND_RUN 2
#define COMMAND_IS_FINISHED 3

#define TARGET_RCI_CPU 1
#define TARGET_FAI0 2
#define TARGET_FAI1 3
#define TARGET_FAI14 4
#define TARGET_FAI_ALL 5

struct {
    int help;
    int command;
    char *progpath;
    char *run_script;
    char *topdir;
    int task_id;
    int max_time;
    int max_mem;
    char *bench_path;
    int target;
    int force;
    int no_systemd;
} cfg;

static pddl_err_t err = PDDL_ERR_INIT;

static void cleanTaskDir(const char *dir)
{
    char fn[PATHSIZE];
    snprintf(fn, PATHSIZE - 1, "%s/task.out", dir);
    unlink(fn);
    snprintf(fn, PATHSIZE - 1, "%s/task.err", dir);
    unlink(fn);
    snprintf(fn, PATHSIZE - 1, "%s/task.finished", dir);
    unlink(fn);
    snprintf(fn, PATHSIZE - 1, "%s/task.memout", dir);
    unlink(fn);
    snprintf(fn, PATHSIZE - 1, "%s/task.timeout", dir);
    unlink(fn);
    snprintf(fn, PATHSIZE - 1, "%s/task.time", dir);
    unlink(fn);
    snprintf(fn, PATHSIZE - 1, "%s/task.status", dir);
    unlink(fn);
    snprintf(fn, PATHSIZE - 1, "%s/task.signum", dir);
    unlink(fn);
    snprintf(fn, PATHSIZE - 1, "%s/task.segfault", dir);
    unlink(fn);
}

static int writeFileInDir(const char *dir,
                          const char *fname,
                          const char *cont)
{
    char fn[PATHSIZE];
    snprintf(fn, PATHSIZE - 1, "%s/%s", dir, fname);
    FILE *fout = fopen(fn, "w");
    if (fout == NULL){
        fprintf(stderr, "Error: Failed to create file %s", fn);
        return -1;
    }
    fprintf(fout, "%s", cont);
    fclose(fout);

    return 0;
}

static int writeTimeFileInDir(const char *dir,
                              const char *fname,
                              const pddl_timer_t *timer)
{
    char c[512];
    sprintf(c, "%f\n", pddlTimerElapsedInSF(timer));
    return writeFileInDir(dir, fname, c);
}

static int setConfig(int argc, char *argv[])
{
    optsAddFlag("help", 'h', &cfg.help, 0, "Print help");
    optsAddStr("dir", 'D', &cfg.topdir, NULL,
               "Diretory where the task will run.");
    optsAddInt("max-time", 'T', &cfg.max_time, 1800,
               "Maximum time in seconds.");
    optsAddInt("max-mem", 'M', &cfg.max_mem, 8192,
               "Maximum memory in MB.");
    optsAddStr("bench", 'B', &cfg.bench_path, NULL,
               "Path to the directory with benchmark tasks.");
    optsAddIntSwitch("target", 't', &cfg.target,
                     "Target queue/host/... One of:"
                     " rci-cpu, fai0, fai1, faiall",
                     4,
                     "rci-cpu", TARGET_RCI_CPU,
                     "fai0", TARGET_FAI0,
                     "fai1", TARGET_FAI1,
                     "fai14", TARGET_FAI14,
                     "faiall", TARGET_FAI_ALL);
    optsAddFlag("force", 0x0, &cfg.force, 0,
                "Run task even if it is finished.");
    optsAddFlag("no-systemd", 'S', &cfg.no_systemd, 0,
                "Set to true if systemd is not available");

    int ret = opts(&argc, argv);
    if (ret != 0)
        cfg.help = 1;

    if (argc > 1){
        if (strcmp(argv[1], "gen") == 0){
            cfg.command = COMMAND_GEN;
            if (argc == 4){
                cfg.topdir = argv[2];
                cfg.run_script = argv[3];
            }else{
                fprintf(stderr, "Error: Invalid command.\n");
                cfg.help = 1;
            }

        }else if (strcmp(argv[1], "run") == 0){
            cfg.command = COMMAND_RUN;
            if (argc == 3){
                cfg.task_id = atoi(argv[2]);
            }else{
                fprintf(stderr, "Error: Invalid command.\n");
                cfg.help = 1;
            }

        }else if (strcmp(argv[1], "is-finished") == 0){
            cfg.command = COMMAND_IS_FINISHED;
            if (argc <= 2){
                fprintf(stderr, "Error: Missing directories to check.\n");
                cfg.help = 1;
            }
        }
    }

    if (cfg.command == 0){
        fprintf(stderr, "Error: no command!\n");
        cfg.help = 1;
    }

    if (cfg.command == COMMAND_GEN){
        if (cfg.bench_path == NULL || !pddlIsDir(cfg.bench_path)){
            fprintf(stderr, "Error: Bench must be specified.\n");
            return -1;
        }
        if (cfg.topdir == NULL || pddlIsDir(cfg.topdir)){
            fprintf(stderr, "Error: dst-dir must not exist.\n");
            return -1;
        }
        if (cfg.run_script == NULL || !pddlIsFile(cfg.run_script)){
            fprintf(stderr, "Error: run-script %s does not exist.\n",
                    cfg.run_script);
            return -1;
        }

    }else if (cfg.command == COMMAND_RUN){
        if (cfg.topdir == NULL || !pddlIsDir(cfg.topdir)){
            fprintf(stderr, "Error: directory %s does not exist.\n",
                    cfg.topdir);
            return -1;
        }
    }


    if (cfg.help){
        fprintf(stderr, "Usage: %s [OPTIONS] gen dst-dir run-script\n", argv[0]);
        fprintf(stderr, "Usage: %s [OPTIONS] run task-id\n", argv[0]);
        fprintf(stderr, "Usage: %s [OPTIONS] is-finished dir [dir [...]]\n", argv[0]);
        optsPrint(stderr);
        fprintf(stderr, "Note: Don't forget to run 'loginctl enable-linger USER'"
                " on your computing nodes.\n");
        return -1;
    }

    if (cfg.run_script != NULL)
        cfg.run_script = realpath(cfg.run_script, NULL);

    cfg.progpath = realpath(argv[0], NULL);

    if (cfg.command != COMMAND_IS_FINISHED){
        PDDL_INFO(&err, "cfg.progpath = '%s'", cfg.progpath);
        PDDL_INFO(&err, "cfg.run_script = '%s'", cfg.run_script);
        PDDL_INFO(&err, "cfg.topdir = '%s'", cfg.topdir);
        PDDL_INFO(&err, "cfg.task_id = %d", cfg.task_id);
        PDDL_INFO(&err, "cfg.max_time = %ds", cfg.max_time);
        PDDL_INFO(&err, "cfg.max_mem = %dMB", cfg.max_mem);
        PDDL_INFO(&err, "cfg.bench = '%s'", cfg.bench_path);
        if (cfg.target == TARGET_RCI_CPU){
            PDDL_INFO(&err, "cfg.target = rci-cpu");
        }else if (cfg.target == TARGET_FAI0){
            PDDL_INFO(&err, "cfg.target = fai0");
        }else if (cfg.target == TARGET_FAI1){
            PDDL_INFO(&err, "cfg.target = fai1");
        }else if (cfg.target == TARGET_FAI14){
            PDDL_INFO(&err, "cfg.target = fai14");
        }else if (cfg.target == TARGET_FAI_ALL){
            PDDL_INFO(&err, "cfg.target = faiall");
        }else{
            PDDL_INFO(&err, "cfg.target = none");
        }
        PDDL_INFO(&err, "cfg.force = %d", cfg.force);
        PDDL_INFO(&err, "cfg.no_systemd = %d", cfg.no_systemd);
    }
    return 0;
}

static int genRunFile(char *fn, const char *topdir, int offset)
{
    if (offset == 0){
        snprintf(fn, PATHSIZE - 1, "%s/run.sh", topdir);
    }else{
        snprintf(fn, PATHSIZE - 1, "%s/run-%d.sh", topdir, offset);
    }
    FILE *fout = fopen(fn, "w");
    if (fout == NULL){
        fprintf(stderr, "Error: Failed to create file %s", fn);
        return -1;
    }

    fprintf(fout, "#!/bin/bash\n");

    if (cfg.target == TARGET_RCI_CPU){
        int mem = cfg.max_mem + 50;
        int max_time = cfg.max_time + 30;
        int days = max_time / (24 * 3600);
        int hours = (max_time % (24 * 3600)) / 3600;
        int minutes = ((max_time % (24 * 3600)) % 3600) / 60;
        int seconds = ((max_time % (24 * 3600)) % 3600) % 60;
        if (max_time < 4 * 3600){
            fprintf(fout, "#SBATCH -p cpufast # partition (queue)\n");
        }else{
            fprintf(fout, "#SBATCH -p cpu # partition (queue)\n");
        }
        fprintf(fout, "#SBATCH -N 1 # number of nodes\n");
        fprintf(fout, "#SBATCH -n 1 # number of cores\n");
        fprintf(fout, "#SBATCH --cpus-per-task=2\n");
        fprintf(fout, "##SBATCH -w X # specific nodes\n");
        fprintf(fout, "#SBATCH -x n[21-33] # exclude nodes\n");
        fprintf(fout, "#SBATCH --mem %dM # memory limit\n", mem);
        fprintf(fout, "#SBATCH -t %d-%d:%d:%d # time (D-HH:MM:SS)\n",
                days, hours, minutes, seconds);
        fprintf(fout, "#SBATCH -o %s/%%6a/run.out # STDOUT\n", topdir);
        fprintf(fout, "#SBATCH -e %s/%%6a/run.err # STDOUT\n", topdir);
        fprintf(fout, "##SBATCH --hint=nomultithread\n");

        fprintf(fout, "\n");
        if (offset == 0){
            fprintf(fout, "ID=${SLURM_ARRAY_TASK_ID}\n");
        }else{
            fprintf(fout, "ID=$((${SLURM_ARRAY_TASK_ID} + %d))\n", offset);
        }

    }else if (cfg.target == TARGET_FAI0
                || cfg.target == TARGET_FAI1
                || cfg.target == TARGET_FAI14
                || cfg.target == TARGET_FAI_ALL){
        int num_cores = 1;
        num_cores = cfg.max_mem / 4096;
        if (cfg.max_mem % 4096 > 0)
            num_cores += 1;

        fprintf(fout, "#$ -S /bin/bash\n");
        fprintf(fout, "#$ -V\n");
        fprintf(fout, "#$ -cwd\n");
        fprintf(fout, "#$ -e %s/job-$TASK_ID.err\n", topdir);
        fprintf(fout, "#$ -o %s/job-$TASK_ID.out\n", topdir);
        if (num_cores > 1){
            fprintf(fout, "#$ -pe smp %d\n", num_cores);
        }
        if (cfg.target == TARGET_FAI0){
            fprintf(fout, "#$ -q all.q@@fai0x\n");
        }else if (cfg.target == TARGET_FAI1){
            fprintf(fout, "#$ -q all.q@fai11,all.q@fai12,all.q@fai13\n");
        }else if (cfg.target == TARGET_FAI14){
            fprintf(fout, "#$ -q all.q@fai14\n");
        }else{
            fprintf(fout, "#$ -q all.q@@allhosts\n");
        }

        fprintf(fout, "\n");
        if (offset == 0){
            fprintf(fout, "ID=$((${SGE_TASK_ID} - 1))\n");
        }else{
            fprintf(fout, "ID=$((${SGE_TASK_ID} - 1 + %d))\n", offset);
        }
    }

    fprintf(fout, "\n");
    fprintf(fout, "%s", cfg.progpath);
    fprintf(fout, " --dir %s", topdir);
    fprintf(fout, " --max-time %d", cfg.max_time);
    fprintf(fout, " --max-mem %d", cfg.max_mem);
    if (cfg.no_systemd)
        fprintf(fout, " --no-systemd");
    fprintf(fout, " run ${ID}");
    fprintf(fout, "\n");
    fclose(fout);

    return 0;
}

static int genRunMakefile(const pddl_bench_t *bench, const char *topdir)
{
    char fn[PATHSIZE];
    snprintf(fn, PATHSIZE - 1, "%s/Makefile", topdir);
    FILE *fout = fopen(fn, "w");
    if (fout == NULL){
        fprintf(stderr, "Error: Failed to create file %s", fn);
        return -1;
    }

    for (int ti = 0; ti < bench->task_size; ++ti){
        fprintf(fout, "TASK += %s/%06d/task.finished\n", topdir, ti);
    }
    fprintf(fout, "\n");
    fprintf(fout, "all: $(TASK)\n");
    fprintf(fout, "\n");
    for (int ti = 0; ti < bench->task_size; ++ti){
        fprintf(fout, "%s/%06d/task.finished:\n", topdir, ti);
        fprintf(fout, "\t");
        fprintf(fout, "%s", cfg.progpath);
        fprintf(fout, " --dir %s", topdir);
        fprintf(fout, " --max-time %d", cfg.max_time);
        fprintf(fout, " --max-mem %d", cfg.max_mem);
        if (cfg.no_systemd)
            fprintf(fout, " --no-systemd");
        fprintf(fout, " run %d", ti);
        fprintf(fout, "\n");
    }
    fclose(fout);

    return 0;
}

static void taskDir(const char *base, int id, char *dir)
{
    snprintf(dir, PATHSIZE - 1, "%s/%06d", base, id);
}

static int cmdGen(void)
{
    pddl_bench_t bench;
    pddlBenchInit(&bench);
    pddlBenchLoadDir(&bench, cfg.bench_path);
    if (bench.task_size == 0){
        fprintf(stderr, "Empty benchmark directory %s\n", cfg.bench_path);
        return -1;
    }

    pddl_rand_t rnd;
    pddlRandInitAuto(&rnd);
    for (int ti = bench.task_size - 1; ti > 0; --ti){
        int idx = pddlRand(&rnd, 0, ti);
        if (idx != ti){
            pddl_bench_task_t tmp = bench.task[ti];
            bench.task[ti] = bench.task[idx];
            bench.task[idx] = tmp;
        }
    }
    PDDL_INFO(&err, "Found %d tasks", bench.task_size);

    if (mkdir(cfg.topdir, 0755) != 0){
        fprintf(stderr, "Error: Failed to create directory %s\n", cfg.topdir);
        return -1;
    }
    char *topdir = realpath(cfg.topdir, NULL);

    char dir[PATHSIZE];
    for (int ti = 0; ti < bench.task_size; ++ti){
        const pddl_bench_task_t *task = bench.task + ti;
        taskDir(topdir, ti, dir);
        if (mkdir(dir, 0755) != 0){
            fprintf(stderr, "Error: Failed to create directory %s\n", dir);
            return -1;
        }

        char fn[PATHSIZE];
        snprintf(fn, PATHSIZE - 1, "%s/domain.pddl", dir);
        if (symlink(task->pddl_files.domain_pddl, fn) != 0){
            fprintf(stderr, "Error: Failed to create symlink %s -> %s\n",
                    fn, task->pddl_files.domain_pddl);
            return -1;
        }

        snprintf(fn, PATHSIZE - 1, "%s/problem.pddl", dir);
        if (symlink(task->pddl_files.problem_pddl, fn) != 0){
            fprintf(stderr, "Error: Failed to create symlink %s -> %s\n",
                    fn, task->pddl_files.domain_pddl);
            return -1;
        }

        snprintf(fn, PATHSIZE - 1, "%s/run.sh", dir);
        if (symlink(cfg.run_script, fn) != 0){
            fprintf(stderr, "Error: Failed to create symlink %s -> %s\n",
                    fn, task->pddl_files.domain_pddl);
            return -1;
        }

        snprintf(fn, PATHSIZE - 1, "%s/task.prop", dir);
        FILE *fout = fopen(fn, "w");
        if (fout == NULL){
            fprintf(stderr, "Error: Failed to create file %s", fn);
            return -1;
        }
        fprintf(fout, "domain_pddl = \"%s\"\n", task->pddl_files.domain_pddl);
        fprintf(fout, "problem_pddl = \"%s\"\n", task->pddl_files.problem_pddl);
        fprintf(fout, "bench_name = \"%s\"\n", task->bench_name);
        fprintf(fout, "domain_name = \"%s\"\n", task->domain_name);
        fprintf(fout, "problem_name = \"%s\"\n", task->problem_name);
        fprintf(fout, "optimal_cost = %d\n", task->optimal_cost);
        fclose(fout);
    }
    pddlBenchFree(&bench);

    char fn[PATHSIZE];
    snprintf(fn, PATHSIZE - 1, "%s/submit.sh", topdir);
    FILE *fout = fopen(fn, "w");
    if (fout == NULL){
        fprintf(stderr, "Error: Failed to create file %s", fn);
        return -1;
    }
    fprintf(fout, "#!/bin/bash\n");
    fprintf(fout, "set -e\n");

    fprintf(fout, "if [ -f %s/submitted ]; then\n", topdir);
    fprintf(fout, "    echo \"%s already submitted\"\n", topdir);
    fprintf(fout, "    exit 0\n");
    fprintf(fout, "fi\n");
    fprintf(fout, "\n");

    if (cfg.target == TARGET_RCI_CPU){
        char fnrun[PATHSIZE];
        for (int i = 0; i < bench.task_size; i += 1000){
            if (genRunFile(fnrun, topdir, i) != 0)
                return -1;
            int maxid = PDDL_MIN(999, bench.task_size - i - 1);
            fprintf(fout, "sbatch --array=0-%d %s\n", maxid, fnrun);
        }

    }else if (cfg.target == TARGET_FAI0
                || cfg.target == TARGET_FAI1
                || cfg.target == TARGET_FAI14
                || cfg.target == TARGET_FAI_ALL){
        char fnrun[PATHSIZE];
        if (genRunFile(fnrun, topdir, 0) != 0)
            return -1;
        
        fprintf(fout, "cd %s\n", topdir);
        fprintf(fout, "qsub -N '%s' -t 1-%d %s 2>&1 | tee submit.log\n",
                cfg.topdir, bench.task_size, fnrun);
    }
    fprintf(fout, "\n");

    fprintf(fout, "touch %s/submitted\n", topdir);
    fclose(fout);
    genRunMakefile(&bench, topdir);
    free(topdir);

    PDDL_INFO(&err, "Done.");
    return 0;
}

static int taskIsFinished(const char *topdir)
{
    char fn[PATHSIZE];
    snprintf(fn, PATHSIZE - 1, "%s/task.finished", topdir);
    PDDL_INFO(&err, "Checking whether the task is finished (%s)", fn);
    return pddlIsFile(fn);
}

static int timeout_reached = 0;
static void *thTimeout(void *_pid)
{
    int *pid = _pid;
    usleep(1000ul * 1000ul * cfg.max_time);
    timeout_reached = 1;
    PDDL_INFO(&err, "Timeout reached.");
    PDDL_INFO(&err, "Sending SIGTERM to %d", *pid);
    kill(*pid, SIGTERM);
    usleep(1000ul * 1000ul * 5);
    PDDL_INFO(&err, "Sending SIGKILL to %d", *pid);
    kill(*pid, SIGKILL);
    return NULL;
}

static int cmdRun(void)
{
    char topdir[PATHSIZE];
    taskDir(cfg.topdir, cfg.task_id, topdir);
    PDDL_INFO(&err, "Task directory: %s", topdir);

    if (!cfg.force && taskIsFinished(topdir)){
        PDDL_INFO(&err, "Task %d (%s) is already finished.",
                  cfg.task_id, topdir);
        return 0;
    }

    cleanTaskDir(topdir);

    pddl_timer_t timer;
    pddlTimerStart(&timer);

    char memlimit[32];
    sprintf(memlimit, "MemoryMax=%dM", cfg.max_mem);
    char timelimit[32];
    sprintf(timelimit, "RuntimeMaxSec=%d", cfg.max_time);
    if (cfg.no_systemd){
        PDDL_INFO(&err, "Forking and running (with ulimit)"
                  " '/bin/bash ./run.sh'"
                  " in directory %s",
                  topdir);
    }else{
        PDDL_INFO(&err, "Forking and running "
                  "'/usr/bin/systemd-run"
                  " --user"
                  " --scope"
                  " -p %s"
                  " -p %s"
                  " -G"
                  " /bin/bash ./run.sh'"
                  " in directory %s",
                  memlimit, timelimit, topdir);
    }
    int pid = fork();
    if (pid == 0){

        chdir(topdir);
        int fdout = open("task.out", O_WRONLY|O_CREAT, 0644);
        int fderr = open("task.err", O_WRONLY|O_CREAT, 0644);
        if (fdout < 0 || fderr < 0){
            fprintf(stderr, "Error: Could not create output files!\n");
            exit(-1);
        }

        close(1);
        dup(fdout);
        close(2);
        dup(fderr);
        if (cfg.no_systemd){
            struct rlimit mem_limit;
            mem_limit.rlim_cur = 1024ul * 1024ul * cfg.max_mem;
            mem_limit.rlim_max = mem_limit.rlim_cur + 1024ul;
            setrlimit(RLIMIT_AS, &mem_limit);
            struct rlimit time_limit;
            time_limit.rlim_cur = cfg.max_time + 10;
            time_limit.rlim_max = time_limit.rlim_cur + 15;
            setrlimit(RLIMIT_CPU, &time_limit);
            execl("/bin/bash", "/bin/bash", "./run.sh", NULL);

        }else{
            execl("/usr/bin/systemd-run",
                  "/usr/bin/systemd-run",
                  "--user",
                  "--scope",
                  "-p", memlimit,
                  "-p", timelimit,
                  "-G",
                  "/bin/bash", "./run.sh",
                  NULL);
        }

    }else if (pid > 0){
        pthread_t th;
        int wstatus;

        if (cfg.no_systemd){
            int ret = pthread_create(&th, NULL, thTimeout, &pid);
            if (ret != 0){
                perror("pthread failed");
                exit(-1);
            }
        }

        wait(&wstatus);
        if (cfg.no_systemd){
            pthread_cancel(th);
            pthread_join(th, NULL);
        }

        if (timeout_reached){
            writeFileInDir(topdir, "task.timeout", "");
        }

        if (WIFEXITED(wstatus)){
            int code = WEXITSTATUS(wstatus);
            PDDL_INFO(&err, "Exit status: %d", code);
            if (code == 137){
                PDDL_INFO(&err, "Probably ran out of memory");
                writeFileInDir(topdir, "task.memout", "");
            }else if (code == 139){
                PDDL_INFO(&err, "Probably segmentation fault");
                writeFileInDir(topdir, "task.segfault", "");
            }else if (code == 152){
                PDDL_INFO(&err, "Probably time out");
                writeFileInDir(topdir, "task.timeout", "");
            }

            char out[32];
            sprintf(out, "%d", code);
            writeFileInDir(topdir, "task.status", out);


        }else if (WIFSIGNALED(wstatus)){
            PDDL_INFO(&err, "Terminated with signal %d (%s)",
                      WTERMSIG(wstatus),
                      strsignal(WTERMSIG(wstatus)));

            char out[32];
            sprintf(out, "%d", WTERMSIG(wstatus));
            writeFileInDir(topdir, "task.signum", out);

            if (WTERMSIG(wstatus) == SIGKILL){
                PDDL_INFO(&err, "Probably ran out of memory");
                writeFileInDir(topdir, "task.memout", "");
            }
            if (WTERMSIG(wstatus) == SIGTERM){
                PDDL_INFO(&err, "Probably reached time limit");
                writeFileInDir(topdir, "task.timeout", "");
            }
        }
    }else{
        perror("fork failed: ");
        return -1;
    }

    pddlTimerStop(&timer);
    writeTimeFileInDir(topdir, "task.time", &timer);
    writeFileInDir(topdir, "task.finished", "");

    return 0;
}

static int cmdIsFinished(int num_dirs, char *dir[])
{
    for (int di = 0; di < num_dirs; ++di){
        char path[512];
        int finished = 0;
        for (int ti = 0; 1; ++ti){
            snprintf(path, 511, "%s/%06d", dir[di], ti);
            if (!pddlIsDir(path))
                break;
            if (!taskIsFinished(path)){
                printf("Not finished %s\n", path);
                return 1;
            }
            ++finished;
        }
        printf("Finished %s: %d\n", dir[di], finished);
    }

    return 0;
}

int main(int argc, char *argv[])
{
    pddlErrInfoEnable(&err, stderr);

    if (setConfig(argc, argv) != 0)
        return -1;

    switch(cfg.command){
        case COMMAND_GEN:
            return cmdGen();
        case COMMAND_RUN:
            return cmdRun();
        case COMMAND_IS_FINISHED:
            return cmdIsFinished(argc - 2, argv + 2);
    }
    return 0;
}

