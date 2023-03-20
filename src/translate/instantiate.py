#! /usr/bin/env python3
import uuid
from collections import defaultdict

from subprocess import Popen, PIPE

import options
import pddl_to_prolog
import pddl
import timers

import random
import sys

def sanitize_predicate_name(name):
    for rep in ((' ', ''), ('()', ''), ('__', '_DOUBLEUNDERSCORE_'), ('-', '_HYPHEN_'), ('=', 'equals')):
        name = name.replace(*rep)
    return name

def get_fluent_predicates(task):
    fluent_predicates = set()
    for action in task.actions:
        for effect in action.effects:
            fluent_predicates.add(effect.literal.predicate)
    for axiom in task.axioms:
        fluent_predicates.add(axiom.name)
    return fluent_predicates

def get_fluent_facts(task, model):
    fluent_predicates = set()
    for action in task.actions:
        for effect in action.effects:
            fluent_predicates.add(effect.literal.predicate)
    for axiom in task.axioms:
        fluent_predicates.add(axiom.name)
    return {fact for fact in model
            if fact.predicate in fluent_predicates}

def get_objects_by_type(typed_objects, types):
    result = defaultdict(list)
    supertypes = {}
    for type in types:
        supertypes[type.name] = type.supertype_names
    for obj in typed_objects:
        result[obj.type_name].append(obj.name)
        for type in supertypes[obj.type_name]:
            result[type].append(obj.name)
    return result

def instantiate_goal(goal, init_facts, fluent_facts):
    # With the way this module is designed, we need to "instantiate"
    # the goal to make sure we properly deal with static conditions,
    # in particular flagging unreachable negative static goals as
    # impossible. See issue1055.
    #
    # This returns None for goals that are impossible due to static
    # facts.

    # HACK! The implementation of this probably belongs into
    # pddl.condition or a similar file, not here. The `instantiate`
    # method of conditions with its slightly weird interface and the
    # existence of the `Impossible` exceptions should perhaps be
    # implementation details of `pddl`.
    result = []
    try:
        goal.instantiate({}, init_facts, fluent_facts, result)
    except pddl.conditions.Impossible:
        return None
    return result

def transform_clingo_fact_into_atom(name, args):
    return pddl.Atom(name, args)

def instantiate(task, model):
    relaxed_reachable = False
    fluent_facts = get_fluent_facts(task, model)

    init_facts = set()
    init_assignments = {}
    for element in task.init:
        if isinstance(element, pddl.Assign):
            init_assignments[element.fluent] = element.expression
        else:
            init_facts.add(element)

    type_to_objects = get_objects_by_type(task.objects, task.types)

    instantiated_actions = []
    instantiated_axioms = []
    reachable_action_parameters = defaultdict(list)

    #random.Random(options.random_seed).shuffle(model)
    iter = 0
    with timers.timing("Main loop...", block=True):
        for atom in model:
            if isinstance(atom.predicate, pddl.Action):
                iter +=1
                action = atom.predicate
                parameters = action.parameters
                inst_parameters = atom.args[:len(parameters)]
                # Note: It's important that we use the action object
                # itself as the key in reachable_action_parameters (rather
                # than action.name) since we can have multiple different
                # actions with the same name after normalization, and we
                # want to distinguish their instantiations.
                reachable_action_parameters[action].append(inst_parameters)
                variable_mapping = {par.name: arg
                                    for par, arg in zip(parameters, atom.args)}
                inst_action = action.instantiate(
                    variable_mapping, init_facts, init_assignments,
                    fluent_facts, type_to_objects,
                    task.use_min_cost_metric)
                if inst_action:
                    instantiated_actions.append(inst_action)
            elif isinstance(atom.predicate, pddl.Axiom):
                 axiom = atom.predicate
                 variable_mapping = {par.name: arg
                                     for par, arg in zip(axiom.parameters, atom.args)}
                 inst_axiom = axiom.instantiate(variable_mapping, init_facts, fluent_facts)
                 if inst_axiom:
                     instantiated_axioms.append(inst_axiom)
            elif atom.predicate == "goal_reachable":
                relaxed_reachable = True
        print("iterations: %d" % iter)

    #sys.exit()
    instantiated_goal = instantiate_goal(task.goal, init_facts, fluent_facts)

    return (relaxed_reachable, fluent_facts,
            instantiated_actions, instantiated_goal,
            sorted(instantiated_axioms), reachable_action_parameters)

def explore(task):
    with timers.timing("Creating program...", block=True):
        prog, map_actions = pddl_to_prolog.translate(task)

        fluent_predicates = get_fluent_predicates(task)
        sanitized_predicates_to_original = defaultdict()

        program_description = prog.dump_sanitized()

        # List of tuples (N, A) where N is the non-sanitized name of the predicate (as in the PDDL
        # task) and A is the action associated to it (or None if there's no such action). This is
        # used to parse the model back to the PDDL names.
        for p in task.predicates:
            name = p.name
            if p.name not in fluent_predicates:
                continue
            name = sanitize_predicate_name(name)
            sanitized_predicates_to_original[name] = p.name
            program_description += "#show %s/%d." % (name, len(p.arguments))
        for name, action in map_actions.items():
            program_description += "#show %s/%d." % (name, len(action.parameters))
        sanitized_predicates_to_original["goal_reachable"] = "goal_reachable"
        program_description += "#show goal_reachable/0."

    with timers.timing("Computing model..."):
        with open("output.theory", "w") as file:
             print(f"Saving Datalog program to {file.name}")
             file.write(program_description)
        gringo = Popen(['gringo'], stdout=PIPE, stdin=PIPE, stderr=PIPE, text=True)
        gringo_output = gringo.communicate(input=program_description)[0]

    with timers.timing("Parsing Clingo model into our model..."):
        model = []
        for line in gringo_output.splitlines():
            words = line.split()
            if words[0]  == '4':
                # This is a relevant ground atom
                atom = words[2]
                if '(' in atom:
                    predicate = atom.split('(')[0]
                    terms = atom.split('(', 1)[1].split(')')[0]
                    args = terms.split(sep=',')
                    for idx, name in enumerate(args):
                        args[idx] = name.replace("_HYPHEN_", "-")
                        args[idx] = args[idx].replace("_DOUBLEUNDERSCORE_", "__")
                else:
                    predicate = atom
                    args = []
                possible_action = map_actions.get(predicate)
                if possible_action:
                    model.append(pddl.Atom(possible_action, args))
                else:
                    predicate = sanitized_predicates_to_original[predicate]
                    model.append(pddl.Atom(predicate, args))

    #old_model = build_model.compute_model(prog)

    with timers.timing("Completing instantiation"):
        return instantiate(task, model)

if __name__ == "__main__":
    import pddl_parser
    task = pddl_parser.open()
    relaxed_reachable, atoms, actions, goals, axioms, _ = explore(task)
    print("goal relaxed reachable: %s" % relaxed_reachable)
    print("%d atoms:" % len(atoms))
    for atom in atoms:
        print(" ", atom)
    print()
    print("%d actions:" % len(actions))
    for action in actions:
        action.dump()
        print()
    print("%d axioms:" % len(axioms))
    for axiom in axioms:
        axiom.dump()
        print()
    print()
    if goals is None:
        print("impossible goal")
    else:
        print("%d goals:" % len(goals))
        for literal in goals:
            literal.dump()
