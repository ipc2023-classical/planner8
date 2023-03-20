#include <pddl/pddl.h>
#include "report.h"
#include "options.h"

static void mgroupCoverNumber(const pddl_mgroups_t *mgroups,
                              const pddl_strips_t *strips,
                              pddl_err_t *err)
{
    if (opt.mg.cover_number){
        PDDL_INFO(err, "Computing mutex group cover number");
        int num = pddlMGroupsCoverNumber(mgroups, strips->fact.fact_size);
        PDDL_INFO(err, "Mutex group cover number: %d", num);
    }
}

static void fdrVars(const pddl_strips_t *strips,
                    const pddl_mgroups_t *mgroups,
                    pddl_err_t *err)
{
    pddl_mutex_pairs_t mutex;
    pddlMutexPairsInitStrips(&mutex, strips);
    pddlMutexPairsAddMGroups(&mutex, mgroups);

    unsigned flags = PDDL_FDR_VARS_LARGEST_FIRST;
    flags |= PDDL_FDR_VARS_NO_NEGATED_FACTS;
    PDDL_INFO(err, "Creating FDR variables...");
    pddl_fdr_vars_t vars;
    pddlFDRVarsInitFromStrips(&vars, strips, mgroups, &mutex, flags);
    PDDL_INFO(err, "Created FDR variables: %d", vars.var_size);
    //pddlFDRVarsPrintDebug(&vars, stderr);
    pddlFDRVarsFree(&vars);

    pddlMutexPairsFree(&mutex);
}

static void mgroupDominance(const pddl_mgroups_t *m1,
                            const pddl_mgroups_t *m2,
                            const char *m1_name,
                            const char *m2_name,
                            pddl_err_t *err)
{
    int dom = 0;
    for (int i = 0; i < m1->mgroup_size; ++i){
        const pddl_mgroup_t *mi = m1->mgroup + i;
        int found = 0;
        for (int j = 0; j < m2->mgroup_size; ++j){
            const pddl_mgroup_t *mj = m2->mgroup + j;
            if (pddlISetEq(&mi->mgroup, &mj->mgroup)
                    || pddlISetIsSubset(&mi->mgroup, &mj->mgroup)){
                found = 1;
                break;
            }
        }
        if (!found)
            dom += 1;
    }
    PDDL_INFO(err, "mutex group dominance %s > %s: %d", m1_name, m2_name, dom);
}

void reportMGroups(const pddl_t *pddl,
                   const pddl_strips_t *strips,
                   pddl_err_t *err)
{
    pddl_lifted_mgroups_t lifted_mgroups;
    pddl_lifted_mgroups_t monotonicity_invariants;
    pddl_mgroups_t lmg_mgroups;
    pddl_mgroups_t h2_mgroups;
    pddl_mgroups_t fam_mgroups;

    pddl_lifted_mgroups_infer_limits_t limits
        = PDDL_LIFTED_MGROUPS_INFER_LIMITS_INIT;
    limits.max_candidates = opt.lmg.max_candidates;
    limits.max_mgroups = opt.lmg.max_mgroups;

    pddlLiftedMGroupsInit(&lifted_mgroups);
    pddlLiftedMGroupsInit(&monotonicity_invariants);
    if (opt.lmg.fd){
        pddlLiftedMGroupsInferMonotonicity(pddl, &limits,
                                           &monotonicity_invariants,
                                           &lifted_mgroups, err);
    }else{
        pddlLiftedMGroupsInferFAMGroups(pddl, &limits, &lifted_mgroups, err);
    }

    for (int li = 0; li < lifted_mgroups.mgroup_size; ++li){
        fprintf(stdout, "LMG:%d: ", li);
        pddlLiftedMGroupPrint(pddl, lifted_mgroups.mgroup + li, stdout);
    }

    for (int li = 0; li < monotonicity_invariants.mgroup_size; ++li){
        const pddl_lifted_mgroup_t *m = monotonicity_invariants.mgroup + li;
        fprintf(stdout, "Mono:%d: ", li);
        pddlLiftedMGroupPrint(pddl, m, stdout);
    }


    pddlMGroupsGround(&lmg_mgroups, pddl, &lifted_mgroups, strips);
    PDDL_INFO(err, "Ground mutex groups from lifted mutex groups: %d",
              lmg_mgroups.mgroup_size);

    for (int gi = 0; gi < lmg_mgroups.mgroup_size; ++gi){
        const pddl_mgroup_t *m = lmg_mgroups.mgroup + gi;
        fprintf(stdout, "LMG-Ground:%d:%d ", gi, m->lifted_mgroup_id);
        pddlMGroupPrint(pddl, strips, m, stdout);
    }

    pddlMGroupsRemoveSubsets(&lmg_mgroups);
    PDDL_INFO(err, "Ground maximal mutex groups from lifted mutex"
              " groups: %d", lmg_mgroups.mgroup_size);

    pddl_mutex_pairs_t mutex;
    pddlMutexPairsInitStrips(&mutex, strips);
    pddlMutexPairsAddMGroups(&mutex, &lmg_mgroups);
    PDDL_INFO(err, "Lifted mutex groups mutex-pairs: %d",
              mutex.num_mutex_pairs);
    pddlMutexPairsFree(&mutex);

    for (int gi = 0; gi < lmg_mgroups.mgroup_size; ++gi){
        const pddl_mgroup_t *m = lmg_mgroups.mgroup + gi;
        fprintf(stdout, "LMG-Ground-Maximal:%d:%d ",
                gi, m->lifted_mgroup_id);
        pddlMGroupPrint(pddl, strips, m, stdout);
    }
    mgroupCoverNumber(&lmg_mgroups, strips, err);
    fdrVars(strips, &lmg_mgroups, err);


    {
    pddl_mutex_pairs_t mutex;
    pddlMutexPairsInitStrips(&mutex, strips);
    pddlH2(strips, &mutex, NULL, NULL, 0., err);
    pddlMGroupsInitEmpty(&h2_mgroups);
    pddlMutexPairsInferMutexGroups(&mutex, &h2_mgroups, err);
    PDDL_INFO(err, "Found %d h2 mutex groups.",
              h2_mgroups.mgroup_size);

    for (int gi = 0; gi < h2_mgroups.mgroup_size; ++gi){
        const pddl_mgroup_t *m = h2_mgroups.mgroup + gi;
        fprintf(stdout, "H2-MGroup:%d ", gi);
        pddlMGroupPrint(pddl, strips, m, stdout);
    }
    mgroupCoverNumber(&h2_mgroups, strips, err);
    fdrVars(strips, &h2_mgroups, err);
    pddlMutexPairsFree(&mutex);
    }


    {
    pddl_famgroup_config_t cfg = PDDL_FAMGROUP_CONFIG_INIT;
    cfg.maximal = 1;
    if (opt.mg.fam_lmg){
        pddlMGroupsInitCopy(&fam_mgroups, &lmg_mgroups);
    }else{
        pddlMGroupsInitEmpty(&fam_mgroups);
    }
    if (pddlFAMGroupsInfer(&fam_mgroups, strips, &cfg, err) != 0){
        fprintf(stderr, "Error: ");
        pddlErrPrint(err, 1, stderr);
        exit(-1);
    }
    if (opt.mg.fam_lmg)
        pddlMGroupsRemoveSubsets(&fam_mgroups);

    pddl_mutex_pairs_t mutex;
    pddlMutexPairsInitStrips(&mutex, strips);
    pddlMutexPairsAddMGroups(&mutex, &fam_mgroups);
    PDDL_INFO(err, "fam-groups: %d, mutex-pairs: %d",
              fam_mgroups.mgroup_size, mutex.num_mutex_pairs);
    pddlMutexPairsFree(&mutex);

    for (int gi = 0; gi < fam_mgroups.mgroup_size; ++gi){
        const pddl_mgroup_t *m = fam_mgroups.mgroup + gi;
        fprintf(stdout, "FAM-Maximal:%d ", gi);
        pddlMGroupPrint(pddl, strips, m, stdout);
    }
    mgroupCoverNumber(&fam_mgroups, strips, err);
    fdrVars(strips, &fam_mgroups, err);
    }

    mgroupDominance(&lmg_mgroups, &h2_mgroups, "lmg", "h2", err);
    mgroupDominance(&h2_mgroups, &lmg_mgroups, "h2", "lmg", err);
    mgroupDominance(&fam_mgroups, &h2_mgroups, "fam", "h2", err);
    mgroupDominance(&h2_mgroups, &fam_mgroups, "h2", "fam", err);
    mgroupDominance(&fam_mgroups, &lmg_mgroups, "fam", "lmg", err);
    mgroupDominance(&lmg_mgroups, &fam_mgroups, "lmg", "fam", err);


    pddlMGroupsFree(&lmg_mgroups);
    pddlMGroupsFree(&h2_mgroups);
    pddlMGroupsFree(&fam_mgroups);
    pddlLiftedMGroupsFree(&lifted_mgroups);
    pddlLiftedMGroupsFree(&monotonicity_invariants);
}
