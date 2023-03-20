/***
 * Copyright (c)2022 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

#include "internal.h"
#include "pddl/pddl_struct.h"
#include "pddl/require_flags.h"


static int requireKw(int kw, pddl_require_flags_t *f)
{
    switch (kw){
        case PDDL_KW_STRIPS:
            f->strips = 1;
            break;
        case PDDL_KW_TYPING:
            f->typing = 1;
            break;
        case PDDL_KW_NEGATIVE_PRE:
            f->negative_pre = 1;
            break;
        case PDDL_KW_DISJUNCTIVE_PRE:
            f->disjunctive_pre = 1;
            break;
        case PDDL_KW_EQUALITY:
            f->equality = 1;
            break;
        case PDDL_KW_EXISTENTIAL_PRE:
            f->existential_pre = 1;
            break;
        case PDDL_KW_UNIVERSAL_PRE:
            f->universal_pre = 1;
            break;
        case PDDL_KW_CONDITIONAL_EFF:
            f->conditional_eff = 1;
            break;
        case PDDL_KW_NUMERIC_FLUENT:
            f->numeric_fluent = 1;
            break;
        case PDDL_KW_OBJECT_FLUENT:
            f->object_fluent = 1;
            break;
        case PDDL_KW_DURATIVE_ACTION:
            f->durative_action = 1;
            break;
        case PDDL_KW_DURATION_INEQUALITY:
            f->duration_inequality = 1;
            break;
        case PDDL_KW_CONTINUOUS_EFF:
            f->continuous_eff = 1;
            break;
        case PDDL_KW_DERIVED_PRED:
            f->derived_pred = 1;
            break;
        case PDDL_KW_TIMED_INITIAL_LITERAL:
            f->timed_initial_literal = 1;
            break;
        case PDDL_KW_PREFERENCE:
            f->preference = 1;
            break;
        case PDDL_KW_CONSTRAINT:
            f->constraint = 1;
            break;
        case PDDL_KW_ACTION_COST:
            f->action_cost = 1;
            break;
        case PDDL_KW_MULTI_AGENT:
            f->multi_agent = 1;
            break;
        case PDDL_KW_UNFACTORED_PRIVACY:
            f->unfactored_privacy = 1;
            break;
        case PDDL_KW_FACTORED_PRIVACY:
            f->factored_privacy = 1;
            break;
        case PDDL_KW_QUANTIFIED_PRE:
            f->existential_pre = 1;
            f->universal_pre = 1;
            break;
        case PDDL_KW_FLUENTS:
            f->numeric_fluent = 1;
            f->object_fluent = 1;
            break;
        case PDDL_KW_ADL:
            f->strips = 1;
            f->typing = 1;
            f->negative_pre = 1;
            f->disjunctive_pre = 1;
            f->equality = 1;
            f->existential_pre = 1;
            f->universal_pre = 1;
            f->conditional_eff = 1;
            break;
        default:
            return -1;
    }
    return 0;
}

int pddlRequireFlagsParse(pddl_t *pddl, pddl_err_t *err)
{
    ZEROIZE(&pddl->require);
    if (pddl->cfg.force_adl)
        requireKw(PDDL_KW_ADL, &pddl->require);

    const pddl_lisp_node_t *req_node;
    req_node = pddlLispFindNode(&pddl->domain_lisp->root, PDDL_KW_REQUIREMENTS);
    // No :requirements implies :strips
    if (req_node == NULL){
        requireKw(PDDL_KW_STRIPS, &pddl->require);
        return 0;
    }

    for (int i = 1; i < req_node->child_size; ++i){
        const pddl_lisp_node_t *n = req_node->child + i;
        if (n->value == NULL){
            PDDL_ERR_RET(err, -1, "Invalid :requirements definition in %s"
                         " on line %d.",
                         pddl->domain_lisp->filename, n->lineno);
        }
        if (requireKw(n->kw, &pddl->require) != 0){
            PDDL_ERR_RET(err, -1, "Invalid :requirements definition in %s"
                         " on line %d: Unknown keyword `%s'.",
                         pddl->domain_lisp->filename, n->lineno, n->value);
        }
    }

    return 0;
}

unsigned pddlRequireFlagsToMask(const pddl_require_flags_t *flags)
{
    unsigned mask = 0u;
    unsigned flag = 1u;
    if (flags->strips)
        mask |= flag;
    flag <<= 1;
    if (flags->typing)
        mask |= flag;
    flag <<= 1;
    if (flags->negative_pre)
        mask |= flag;
    flag <<= 1;
    if (flags->disjunctive_pre)
        mask |= flag;
    flag <<= 1;
    if (flags->equality)
        mask |= flag;
    flag <<= 1;
    if (flags->existential_pre)
        mask |= flag;
    flag <<= 1;
    if (flags->universal_pre)
        mask |= flag;
    flag <<= 1;
    if (flags->conditional_eff)
        mask |= flag;
    flag <<= 1;
    if (flags->numeric_fluent)
        mask |= flag;
    flag <<= 1;
    if (flags->object_fluent)
        mask |= flag;
    flag <<= 1;
    if (flags->durative_action)
        mask |= flag;
    flag <<= 1;
    if (flags->duration_inequality)
        mask |= flag;
    flag <<= 1;
    if (flags->continuous_eff)
        mask |= flag;
    flag <<= 1;
    if (flags->derived_pred)
        mask |= flag;
    flag <<= 1;
    if (flags->timed_initial_literal)
        mask |= flag;
    flag <<= 1;
    if (flags->preference)
        mask |= flag;
    flag <<= 1;
    if (flags->constraint)
        mask |= flag;
    flag <<= 1;
    if (flags->action_cost)
        mask |= flag;
    flag <<= 1;
    if (flags->multi_agent)
        mask |= flag;
    flag <<= 1;
    if (flags->unfactored_privacy)
        mask |= flag;
    flag <<= 1;
    if (flags->factored_privacy)
        mask |= flag;

    return mask;
}

void pddlRequireFlagsPrintPDDL(const pddl_require_flags_t *flags, FILE *fout)
{
    fprintf(fout, "(:requirements\n");
    if (flags->strips)
        fprintf(fout, "    :strips\n");
    if (flags->typing)
        fprintf(fout, "    :typing\n");
    if (flags->negative_pre)
        fprintf(fout, "    :negative-preconditions\n");
    if (flags->disjunctive_pre)
        fprintf(fout, "    :disjunctive-preconditions\n");
    if (flags->equality)
        fprintf(fout, "    :equality\n");
    if (flags->existential_pre)
        fprintf(fout, "    :existential-preconditions\n");
    if (flags->universal_pre)
        fprintf(fout, "    :universal-preconditions\n");
    if (flags->conditional_eff)
        fprintf(fout, "    :conditional-effects\n");
    if (flags->numeric_fluent)
        fprintf(fout, "    :numeric-fluents\n");
    if (flags->object_fluent)
        fprintf(fout, "    :object-fluents\n");
    if (flags->durative_action)
        fprintf(fout, "    :durative-actions\n");
    if (flags->duration_inequality)
        fprintf(fout, "    :duration-inequalities\n");
    if (flags->continuous_eff)
        fprintf(fout, "    :continuous-effects\n");
    if (flags->derived_pred)
        fprintf(fout, "    :derived-predicates\n");
    if (flags->timed_initial_literal)
        fprintf(fout, "    :timed-initial-literals\n");
    if (flags->preference)
        fprintf(fout, "    :preferences\n");
    if (flags->constraint)
        fprintf(fout, "    :constraints\n");
    if (flags->action_cost)
        fprintf(fout, "    :action-costs\n");
    if (flags->multi_agent)
        fprintf(fout, "    :multi-agent\n");
    if (flags->unfactored_privacy)
        fprintf(fout, "    :unfactored-privacy\n");
    if (flags->factored_privacy)
        fprintf(fout, "    :factored-privacy\n");
    fprintf(fout, ")\n");
}
