# split_rules: Split rules whose conditions fall into different "connected
# components" (where to conditions are related if they share a variabe) into
# several rules, one for each connected component and one high-level rule.

from pddl_to_prolog import Rule, get_variables

import decompositions
import graph
import greedy_join
import options
import pddl

def get_connected_conditions(conditions):
    agraph = graph.Graph(conditions)
    var_to_conditions = {var: [] for var in get_variables(conditions)}
    for cond in conditions:
        for var in cond.args:
            if var[0] == "?":
                var_to_conditions[var].append(cond)

    # Connect conditions with a common variable
    for var, conds in var_to_conditions.items():
        for cond in conds[1:]:
            agraph.connect(conds[0], cond)
    return sorted(map(sorted, agraph.connected_components()))

def project_rule(rule, conditions, name_generator):
    predicate = next(name_generator)
    effect_variables = set(rule.effect.args) & get_variables(conditions)
    effect = pddl.Atom(predicate, sorted(effect_variables))
    projected_rule = Rule(conditions, effect)
    return projected_rule


def get_rule_type(rule):
    if len(rule.conditions) == 1:
        return 'project'
    assert len(rule.conditions) == 2
    left_args = rule.conditions[0].args
    right_args = rule.conditions[1].args
    eff_args = rule.effect.args
    left_vars = {v for v in left_args if isinstance(v, int) or v[0] == "?"}
    right_vars = {v for v in right_args if isinstance(v, int) or v[0] == "?"}
    eff_vars = {v for v in eff_args if isinstance(v, int) or v[0] == "?"}
    if left_vars & right_vars:
            return 'join'
    return 'product'

def split_rule(rule, name_generator):
    important_conditions, trivial_conditions = [], []
    for cond in rule.conditions:
        for arg in cond.args:
            if arg[0] == "?":
                important_conditions.append(cond)
                break
            else:
                trivial_conditions.append(cond)

    # important_conditions = [cond for cond in rule.conditions if cond.args]
    # trivial_conditions = [cond for cond in rule.conditions if not cond.args]

    non_normalized_rules = []
    components = get_connected_conditions(important_conditions)

    if len(components) == 1 and not trivial_conditions:
        decomposed_rules = decompositions.split_into_hypertree(rule, name_generator)
        if True:
            # TODO maybe we only need this?
            return decomposed_rules
        else:
            split_rules = []
            for r in decomposed_rules:
                if len(r.conditions) > 2:
                    split_rules += split_into_binary_rules(r, name_generator)
                else:
                    r.type = get_rule_type(r)
                    split_rules.append(r)
            return split_rules

    projected_rules = [project_rule(rule, conditions, name_generator)
                       for conditions in components]

    result = []
    for proj_rule in projected_rules:
        if len(proj_rule.conditions) == 1:
            proj_rule.type = get_rule_type(proj_rule)
            result += [proj_rule]
            non_normalized_rules += [proj_rule]
            continue
        new_proj_rules = decompositions.split_into_hypertree(proj_rule, name_generator)
        non_normalized_rules += new_proj_rules
        for r in new_proj_rules:
            if len(r.conditions) > 2:
                result += split_into_binary_rules(r, name_generator)
            else:
                r.type = get_rule_type(r)
                result += [r]

    conditions = ([proj_rule.effect for proj_rule in projected_rules] +
                  trivial_conditions)
    combining_rule = Rule(conditions, rule.effect)
    if len(conditions) >= 2:
        combining_rule.type = "product"
    else:
        combining_rule.type = "project"
    result.append(combining_rule)
    non_normalized_rules.append(combining_rule)

    return result

def split_into_binary_rules(rule, name_generator):
    if len(rule.conditions) <= 1:
        rule.type = "project"
        return [rule]
    return greedy_join.greedy_join(rule, name_generator)
