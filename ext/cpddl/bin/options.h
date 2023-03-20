#ifndef OPTIONS_H
#define OPTIONS_H

#include <pddl/pddl.h>
#include "process_strips.h"

enum {
    LIFTED_PLAN_NONE = 0,
    LIFTED_PLAN_ASTAR,
    LIFTED_PLAN_GBFS,
    LIFTED_PLAN_LAZY
};

enum {
    LIFTED_PLAN_SUCC_GEN_DL = 0,
    LIFTED_PLAN_SUCC_GEN_SQL,
};

enum {
    LIFTED_PLAN_HEUR_BLIND = 0,
    LIFTED_PLAN_HEUR_HMAX,
    LIFTED_PLAN_HEUR_HADD,
    LIFTED_PLAN_HEUR_HOMO_LMC,
    LIFTED_PLAN_HEUR_HOMO_FF,
};

enum {
    GROUND_TRIE = 0,
    GROUND_SQL,
    GROUND_DL
};

enum {
    MG_NONE = 0,
    MG_FAM,
    MG_H2
};

enum {
    GROUND_PLAN_NONE = 0,
    GROUND_PLAN_ASTAR,
    GROUND_PLAN_GBFS,
    GROUND_PLAN_LAZY
};

enum {
    GROUND_PLAN_HEUR_BLIND = 0,
    GROUND_PLAN_HEUR_LMC,
    GROUND_PLAN_HEUR_MAX,
    GROUND_PLAN_HEUR_ADD,
    GROUND_PLAN_HEUR_FF,
    GROUND_PLAN_HEUR_FLOW,
    GROUND_PLAN_HEUR_POT,
};

enum {
    SYMBA_NONE = 0,
    SYMBA_FW,
    SYMBA_BW,
    SYMBA_FWBW,
};

struct options {
    int help;
    int version;
    int max_mem;
    char *log_out;
    char *prop_out;
    pddl_files_t files;

    struct {
        int force_adl;
        int remove_empty_types;
        int compile_away_cond_eff;
        int compile_in_lmg;
        int compile_in_lmg_mutex;
        int compile_in_lmg_dead_end;
        int enforce_unit_cost;
        char *domain_out;
        char *problem_out;
        int stop;
    } pddl;

    struct {
        int max_candidates;
        int max_mgroups;
        int fd;
        int fd_monotonicity;
        int enable;
        char *out;
        char *fd_monotonicity_out;
        int stop;
    } lmg;

    struct {
        int enable;
        int ignore_costs;
    } lifted_endomorph;

    struct {
        int search;
        int succ_gen;
        int heur;
        pddl_homomorphism_config_t homomorph_cfg;
        int random_seed;
        int homomorph_samples;
        char *plan_out;
    } lifted_planner;

    struct {
        pddl_ground_config_t cfg;
        int method;

        int mgroup;
        int mgroup_remove_subsets;
        char *mgroup_out;
    } ground;

    struct {
        int compile_away_cond_eff;
        pddl_process_strips_t process;
        char *py_out;
        char *fam_dump;
        char *h2_dump;
        char *h3_dump;
        int stop;
    } strips;

    struct {
        int method;
        int fam_lmg;
        int fam_maximal;
        float fam_time_limit;
        int fam_limit;
        int remove_subsets;
        int cover_number;
        char *out;
    } mg;

    struct {
        int enable;
        pddl_red_black_fdr_config_t cfg;
        char *out;
    } rb_fdr;

    struct {
        unsigned flag;
        unsigned var_flag;
        int order_vars_cg;
        char *out;
        int pretty_print_vars;
        int pretty_print_cg;
        int pot;
        pddl_hpot_config_t pot_cfg;
        int to_tnf;
        int to_tnf_multiply;
    } fdr;

    struct {
        int search;
        int heur;
        int heur_op_mutex;
        int heur_op_mutex_ts;
        int heur_op_mutex_op_fact;
        int heur_op_mutex_hm_op;
        char *plan_out;
        pddl_hpot_config_t pot_cfg;
    } ground_planner;

    struct {
        int search;
        pddl_symbolic_task_config_t cfg;
        int bw_off_if_constr_failed;
        char *out;
    } symba;

    struct {
        int lmg;
        int reversibility_simple;
        int reversibility_iterative;
        int mgroups;
    } report;

    struct {
        int max_depth;
        int use_mutex;
    } reversibility;

    struct {
        int enable;
        char *out_task;
        char *out_fdr;
    } asnets;
};
typedef struct options options_t;
extern options_t opt;

int setOptions(int argc, char *argv[], pddl_err_t *err);

extern FILE *log_out;
extern FILE *prop_out;

#endif /* OPTIONS_H */
