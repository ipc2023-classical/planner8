#include <pddl/pddl.h>
#include "report.h"

void reportLiftedMGroups(const pddl_t *pddl, pddl_err_t *err)
{
    pddl_lifted_mgroups_infer_limits_t limits
                = PDDL_LIFTED_MGROUPS_INFER_LIMITS_INIT;
    limits.max_candidates = 10000;
    limits.max_mgroups = 10000;

    pddl_lifted_mgroups_t fd, lmg, mono;
    pddlLiftedMGroupsInit(&fd);
    pddlLiftedMGroupsInit(&lmg);
    pddlLiftedMGroupsInit(&mono);

    PDDL_INFO(err, "FD Lifted Mutex Groups:");
    pddlLiftedMGroupsInferMonotonicity(pddl, &limits, &mono, &fd, err);
    PDDL_INFO(err, "Lifted Mutex Groups:");
    pddlLiftedMGroupsInferFAMGroups(pddl, &limits, &lmg, err);

    for (int li = 0; li < mono.mgroup_size; ++li){
        fprintf(stdout, "I:%d: ", li);
        pddlLiftedMGroupPrint(pddl, mono.mgroup + li, stdout);
    }
    for (int li = 0; li < fd.mgroup_size; ++li){
        fprintf(stdout, "F:%d: ", li);
        pddlLiftedMGroupPrint(pddl, fd.mgroup + li, stdout);
    }
    for (int li = 0; li < lmg.mgroup_size; ++li){
        fprintf(stdout, "M:%d: ", li);
        pddlLiftedMGroupPrint(pddl, lmg.mgroup + li, stdout);
    }

    pddlLiftedMGroupsFree(&fd);
    pddlLiftedMGroupsFree(&lmg);
    pddlLiftedMGroupsFree(&mono);
}
