#ifndef __PDDL_PROCESS_STRIPS_H__
#define __PDDL_PROCESS_STRIPS_H__

#include <pddl/strips.h>
#include <pddl/mgroup.h>
#include <pddl/mutex_pair.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** Forward declaration */
typedef struct pddl_process_strips_step pddl_process_strips_step_t;

struct pddl_process_strips {
    pddl_list_t steps;
    pddl_iset_t rm_op;
    pddl_iset_t rm_fact;
    int rm_not_unreachable_or_dead_end;
    int removed_op;
    int removed_fact;

    pddl_strips_t *strips;
    pddl_mgroups_t *mgroups;
    pddl_mutex_pairs_t *mutex;

    pddl_process_strips_step_t *open_fixpoint;
};
typedef struct pddl_process_strips pddl_process_strips_t;

void pddlProcessStripsInit(pddl_process_strips_t *prune);
void pddlProcessStripsFree(pddl_process_strips_t *prune);

int pddlProcessStripsExecute(pddl_process_strips_t *prune,
                             pddl_strips_t *strips,
                             pddl_mgroups_t *mgroups,
                             pddl_mutex_pairs_t *mutex,
                             pddl_err_t *err);


void pddlProcessStripsAddIrrelevance(pddl_process_strips_t *prune);
void pddlProcessStripsAddIrrelevanceOps(pddl_process_strips_t *prune);
void pddlProcessStripsAddRemoveUselessDelEffs(pddl_process_strips_t *prune);
void pddlProcessStripsAddUnreachableOps(pddl_process_strips_t *prune);
void pddlProcessStripsAddRemoveOpsEmptyAddEff(pddl_process_strips_t *prune);
void pddlProcessStripsAddFAMGroupsDeadEndOps(pddl_process_strips_t *prune);
void pddlProcessStripsAddH2Fw(pddl_process_strips_t *prune,
                              float time_limit_in_s);
void pddlProcessStripsAddH2FwBw(pddl_process_strips_t *prune,
                                float time_limit_in_s);
void pddlProcessStripsAddH3Fw(pddl_process_strips_t *prune,
                              float time_limit_in_s,
                              size_t excess_memory);
void pddlProcessStripsAddDeduplicateOps(pddl_process_strips_t *prune);
void pddlProcessStripsAddSortOps(pddl_process_strips_t *prune);

void pddlProcessStripsAddOpMutex(pddl_process_strips_t *prune,
                                 int ts, int op_fact, int hm_op,
                                 int no_prune, int prune_method,
                                 float prune_time_limit,
                                 const char *out);
void pddlProcessStripsAddEndomorphFDR(pddl_process_strips_t *prune,
                                      const pddl_endomorphism_config_t *cfg);
void pddlProcessStripsAddEndomorphTS(pddl_process_strips_t *prune,
                                     const pddl_endomorphism_config_t *cfg);
void pddlProcessStripsAddEndomorphFDRTS(pddl_process_strips_t *prune,
                                        const pddl_endomorphism_config_t *cfg);

void pddlProcessStripsAddPrintPddlDomain(pddl_process_strips_t *ps,
                                         const char *fn);
void pddlProcessStripsAddPrintPddlProblem(pddl_process_strips_t *ps,
                                          const char *fn);

void pddlProcessStripsFixpointStart(pddl_process_strips_t *ps);
void pddlProcessStripsFixpointFinalize(pddl_process_strips_t *ps);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_PROCESS_STRIPS_H__ */
