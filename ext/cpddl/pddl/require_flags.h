/***
 * Copyright (c)2022 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

#ifndef __PDDL_REQUIRE_FLAGS_H__
#define __PDDL_REQUIRE_FLAGS_H__

#include <pddl/err.h>
#include <pddl/common.h>
#include <pddl/lisp.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct pddl_require_flags {
    unsigned strips:1;
    unsigned typing:1;
    unsigned negative_pre:1;
    unsigned disjunctive_pre:1;
    unsigned equality:1;
    unsigned existential_pre:1;
    unsigned universal_pre:1;
    unsigned conditional_eff:1;
    unsigned numeric_fluent:1;
    unsigned object_fluent:1;
    unsigned durative_action:1;
    unsigned duration_inequality:1;
    unsigned continuous_eff:1;
    unsigned derived_pred:1;
    unsigned timed_initial_literal:1;
    unsigned preference:1;
    unsigned constraint:1;
    unsigned action_cost:1;
    unsigned multi_agent:1;
    unsigned unfactored_privacy:1;
    unsigned factored_privacy:1;
};
typedef struct pddl_require_flags pddl_require_flags_t;

/**
 * Parses :requirements from domain pddl.
 */
int pddlRequireFlagsParse(pddl_t *pddl, pddl_err_t *err);

/**
 * Transform flags to uint mask
 */
unsigned pddlRequireFlagsToMask(const pddl_require_flags_t *flags);

/**
 * Print requirements in PDDL format.
 */
void pddlRequireFlagsPrintPDDL(const pddl_require_flags_t *flags, FILE *fout);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_REQUIRE_FLAGS_H__ */
