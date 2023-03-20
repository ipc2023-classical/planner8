#ifndef REPORT_H
#define REPORT_H
void reportLiftedMGroups(const pddl_t *pddl, pddl_err_t *err);
void reportMGroups(const pddl_t *pddl,
                   const pddl_strips_t *strips,
                   pddl_err_t *err);
#endif
