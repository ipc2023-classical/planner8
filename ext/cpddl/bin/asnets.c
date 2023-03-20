#include "pddl/pddl.h"
#include "opts.h"
#include "print_to_file.h"

static struct {
    int help;
    int version;
    int max_mem;
    char *log_out;
    char *prop_out;

    char *train;
    char *train_save_prefix;
    char *eval;
    int eval_write_plans;
    char *info;
    char *gen;
} opt;

static pddl_err_t err = PDDL_ERR_INIT;
static FILE *log_out = NULL;
static FILE *prop_out = NULL;
static char *config_file = NULL;

static void help(const char *argv0, FILE *fout)
{
    fprintf(fout, "Usage: %s [OPTIONS] config.toml\n", argv0);
    fprintf(fout, "version: %s\n", pddl_version);
    fprintf(fout, "\n");
    fprintf(fout, "OPTIONS:\n");
    optsPrint(fout);
}

static int parseOpts(int argc, char *argv[])
{
    optsAddFlag("help", 'h', &opt.help, 0, "Print this help.");
    optsAddFlag("version", 0x0, &opt.version, 0, "Print version and exit.");
    optsAddInt("max-mem", 0x0, &opt.max_mem, 0,
               "Maximum memory in MB if >0.");
    optsAddStr("log-out", 0x0, &opt.log_out, "stderr",
               "Set output file for logs.");
    optsAddStr("prop-out", 0x0, &opt.prop_out, 0x0,
               "Set output file for properties log.");
    optsAddStr("train", 't', &opt.train, NULL,
               "Train ASNets and save the model to the specified file.");
    optsAddStr("train-save-prefix", 0x0, &opt.train_save_prefix, NULL,
               "If set, whenever success rate improves, a new model is"
               " saved to the file with this prefix.");
    optsAddStr("eval", 'e', &opt.eval, NULL,
               "Evaluate model stored in the specified file.");
    optsAddFlag("eval-write-plans", 0x0, &opt.eval_write_plans, 0,
                "Write plans to files based on domain and problem names.");
    optsAddStr("info", 'i', &opt.info, NULL,
               "Print info about the stored model.");
    optsAddStr("gen", 'g', &opt.gen, NULL,
               "Generate a default configuration file.");

    if (opts(&argc, argv) != 0)
        return -1;

    int need_config = 0;
    if (opt.eval != NULL || opt.train != NULL)
        need_config = 1;

    if ((need_config && argc != 2) || (!need_config && argc != 1)){
        if (need_config && argc <= 1){
            fprintf(stderr, "Error: Missing config file\n");
        }else{
            for (int i = 1; i < argc; ++i){
                fprintf(stderr, "Error: Unrecognized argument: %s\n", argv[i]);
            }
        }
        help(argv[0], stderr);
        return -1;
    }

    int req_opts = (int)(opt.train != NULL);
    req_opts += (int)(opt.eval != NULL);
    req_opts += (int)(opt.info != NULL);
    req_opts += (int)(opt.gen != NULL);
    if (req_opts != 1){
        fprintf(stderr, "Error: Either --train, --eval, --info, or --gen option must be used.\n");
        help(argv[0], stderr);
        return -1;
    }

    if (opt.help){
        help(argv[0], stderr);
        return -1;
    }

    if (opt.version){
        fprintf(stdout, "%s\n", pddl_version);
        return 1;
    }

    if (opt.log_out != NULL){
        log_out = openFile(opt.log_out);
        pddlErrWarnEnable(&err, log_out);
        pddlErrInfoEnable(&err, log_out);
    }

    if (opt.prop_out != NULL){
        prop_out = openFile(opt.prop_out);
        pddlErrPropEnable(&err, prop_out);
    }

    if (opt.max_mem > 0){
        struct rlimit mem_limit;
        mem_limit.rlim_cur
            = mem_limit.rlim_max = opt.max_mem * 1024UL * 1024UL;
        setrlimit(RLIMIT_AS, &mem_limit);
    }

    if (argc > 1)
        config_file = argv[1];

    PDDL_LOG(&err, "Version: %{version}s", pddl_version);
    return 0;
}

int main(int argc, char *argv[])
{
    pddlErrStartCtxTimer(&err);
    pddl_timer_t timer;
    pddlTimerStart(&timer);

    if (parseOpts(argc, argv) != 0){
        if (pddlErrIsSet(&err)){
            fprintf(stderr, "Error: ");
            pddlErrPrint(&err, 1, stderr);
        }
        return -1;
    }

    if (opt.train != NULL && pddlIsFile(opt.train)){
        fprintf(stderr, "Error: File %s already exists.\n", opt.train);
        return -1;
    }

    pddl_asnets_config_t cfg;
    if (config_file != NULL){
        if (pddlASNetsConfigInitFromFile(&cfg, config_file, &err) != 0){
            if (pddlErrIsSet(&err)){
                fprintf(stderr, "Error: ");
                pddlErrPrint(&err, 1, stderr);
            }
            return -1;
        }
    }else{
        pddlASNetsConfigInit(&cfg);
    }

    if (opt.train_save_prefix != NULL)
        cfg.save_model_prefix = opt.train_save_prefix;


    pddl_asnets_t *asnets = NULL;
    if (opt.train != NULL || opt.eval != NULL){
        asnets = pddlASNetsNew(&cfg, &err);
        if (asnets == NULL){
            if (pddlErrIsSet(&err)){
                fprintf(stderr, "Error: ");
                pddlErrPrint(&err, 1, stderr);
            }
            return -1;
        }
    }

    int ret = 0;
    if (opt.train != NULL){
        ret = pddlASNetsTrain(asnets, &err);
        if (ret == 0){
            ret = pddlASNetsSave(asnets, opt.train, &err);
        }else{
            if (pddlErrIsSet(&err)){
                fprintf(stderr, "Error: ");
                pddlErrPrint(&err, 1, stderr);
            }
        }

    }else if (opt.eval != NULL){
        ret = pddlASNetsLoad(asnets, opt.eval, &err);
        if (ret < 0){
            fprintf(stderr, "Error: ");
            pddlErrPrint(&err, 1, stderr);
            return -1;
        }

        int num_solved = 0;
        int num_tasks = pddlASNetsNumGroundTasks(asnets);
        for (int task_id = 0; task_id < num_tasks; ++task_id){
            const pddl_asnets_ground_task_t *task;
            task = pddlASNetsGetGroundTask(asnets, task_id);
            PDDL_IARR(plan);
            int solved = pddlASNetsSolveTask(asnets, task, &plan, &err);
            PDDL_LOG(&err, "Task %{eval_domain}s %{eval_problem}s"
                     " solved: %{eval_solved}b, length: %{eval_length}d",
                     task->pddl.domain_lisp->filename,
                     task->pddl.problem_lisp->filename,
                     solved,
                     (solved ? pddlIArrSize(&plan) : -1));
            if (solved){
                ++num_solved;
                if (opt.eval_write_plans){
                    char fn[512];
                    snprintf(fn, 511, "%s--%s.plan", task->pddl.domain_name,
                             task->pddl.problem_name);
                    FILE *fout = fopen(fn, "w");
                    if (fout != NULL){
                        int op_id;
                        PDDL_IARR_FOR_EACH(&plan, op_id){
                            fprintf(fout, "(%s)\n", task->fdr.op.op[op_id]->name);
                        }
                        fclose(fout);
                    }else{
                        PDDL_LOG(&err, "Could not open file %s", fn);
                    }
                }
            }
            pddlIArrFree(&plan);
        }
        PDDL_LOG(&err, "Solved %{eval_num_solved}d out of"
                 " %{eval_num_tasks}d tasks", num_solved, num_tasks);

    }else if (opt.info != NULL){
        ret = pddlASNetsPrintModelInfo(opt.info, &err);
        if (ret < 0){
            fprintf(stderr, "Error: ");
            pddlErrPrint(&err, 1, stderr);
            return -1;
        }

    }else if (opt.gen != NULL){
        FILE *fout = fopen(opt.gen, "w");
        if (fout != NULL){
            pddlASNetsConfigWrite(&cfg, fout);
            fclose(fout);
        }else{
            fprintf(stderr, "Error: Could not open %s\n", opt.gen);
            ret = -1;
        }
    }

    pddlTimerStop(&timer);
    PDDL_LOG(&err, "Overall Elapsed Time: %{overall_elapsed_time}.4fs",
             pddlTimerElapsedInSF(&timer));

    pddlASNetsConfigFree(&cfg);
    if (asnets != NULL)
        pddlASNetsDel(asnets);
    if (log_out != NULL)
        closeFile(log_out);
    if (prop_out != NULL)
        closeFile(prop_out);
    return ret;
}
