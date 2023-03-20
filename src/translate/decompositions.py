import os
import pddl
import pddl_to_prolog
import subprocess
import sys

def subset(l1, l2):
    for e in l1:
        if e not in l2:
            return False
    return True

class Hypertree:
    def __init__(self) -> None:
        self.bag = []
        self.cover = []
        self.parent = None
        self.children = []

    def set_bag(self, vertices):
        self.bag = vertices

    def set_cover(self, edges):
        self.cover = edges

    def add_child(self, child):
        self.children.append(child)
        child.parent = self

    def del_child(self, child):
        self.children.remove(child)
        child.bag = None
        child.cover = None
        child.parent = None
        child.children = None

    def _upwards(self):
        if self.parent is not None:
            p = self.parent
            if subset(self.bag, p.bag):
                p.cover.extend(self.cover)
                p.cover = list(dict.fromkeys(p.cover))
                for c in self.children:
                    p.add_child(c)

                p.del_child(self)
                return True
        return False

    def _downwards(self):
        heir = None
        for c in self.children:
            if subset(self.bag, c.bag):
                heir = c
                break
        if heir is not None:
            heir.cover.extend(self.cover)
            heir.cover = list(dict.fromkeys(heir.cover))
            self.children.remove(heir)
            for s in self.children:
                heir.add_child(s)
            if self.parent is not None:
                self.parent.add_child(heir)
                self.parent.del_child(self)
            else:
                heir.parent = None
                self.children = None
            return True
        return False

def delete_previous_htd_files():
    delete_files(".htd")

def delete_files(extension):
    cwd = os.getcwd()
    files = os.listdir(cwd)
    for f in files:
        if f.endswith(extension):
            os.remove(os.path.join(cwd, f))


def is_ground(rule):
    if len(rule.effect.args) > 0:
        return False
    for c in rule.conditions:
        if len(c.args) > 0:
            return False
    return True

def generate_hypertree(rule):
    map_predicate_to_edge = dict()
    counter = 0
    f = open("rule-hypertree.ast", 'w')
    for idx, p in enumerate(rule.conditions):
        if len(p.args) == 0 or isinstance(p, pddl.InequalityAtom):
            continue
        atom_name = "{}-{}".format(p.predicate, str(counter))
        if p.predicate == '=':
            # special case
            atom_name = "EQUALPREDICATE-{}".format(p.predicate, str(counter))
        map_predicate_to_edge[atom_name] = (idx, p)
        counter = counter + 1
        terms = ','.join([x for x in p.args if x[0] == '?']).replace('?', 'Var_')
        f.write('%s(%s)\n' % (atom_name, terms))
        p.hyperedge = atom_name
    f.close()
    return f.name, map_predicate_to_edge

def compute_decompositions(file):
    decomp_file_name = file
    decomp_file_name = decomp_file_name.replace('.ast', '.htd')
    f = open(decomp_file_name, 'w')
    BALANCED_GO_CMD = ['BalancedGo',
                       '-bench',
                       '-approx', '10',
                       '-det',
                       '-graph', file,
                       '-cpu', '1',
                       '-gml', decomp_file_name]

    res = subprocess.run(BALANCED_GO_CMD, stdout=subprocess.PIPE,
                         check=True, universal_newlines=True)
    hd = []
    parents = []
    for line in res.stdout.splitlines():
        if 'Bag: {' in line:
            node = Hypertree()
            line = line.strip()[6:-1]
            node.set_bag([v.strip() for v in line.split(',')])
            hd.append(node)

            if len(parents):
                par = parents[-1]
                par.add_child(node)
        elif 'Cover: {' in line:
            line = line.strip()[8:-1]
            hd[-1].set_cover([v.strip() for v in line.split(',')])
            #hd[-1].covered = covered(hd[-1]) # Davide told to comment out this list
        elif 'Children:' in line:
            parents.append(hd[-1])
        elif ']' in line:
            parents = parents[:-1]
    return hd


def create_rule_dfs(node, associated_new_relation, not_joined_relations, important_variables, name_generator):
    rules = []
    effect_parent = associated_new_relation[node]
    conditions = [effect_parent]
    for child in node.children:
        rules += create_rule_dfs(child, associated_new_relation, not_joined_relations, important_variables, name_generator)
        effect_child = associated_new_relation[child]
        conditions.append(effect_child)
    bag_vars = get_variables_from_bag(node.bag)
    var_in_conditions = pddl_to_prolog.get_variables(conditions)
    important_vars_in_cond = important_variables.intersection(var_in_conditions)
    effect_variables = important_vars_in_cond.union(bag_vars)
    relations_to_be_joined = []
    for relation in not_joined_relations:
        vars_in_relation = pddl_to_prolog.get_variables([relation])
        if vars_in_relation.issubset(effect_variables):
            relations_to_be_joined.append(relation)
    not_joined_relations -= set(relations_to_be_joined)
    conditions += relations_to_be_joined
    new_effect = pddl.Atom(next(name_generator), effect_variables)
    associated_new_relation[node] = new_effect
    new_rule = pddl_to_prolog.Rule(conditions, new_effect)
    rules.append(new_rule)
    return rules

def get_variables_from_bag(bag):
    vars = set()
    for v in  bag:
        x = v.replace("Var_", "?")
        vars.add(x)
    return vars


def split_into_hypertree(rule, name_generator):
    #print("Rule : %s" % rule)
    #delete_previous_htd_files()
    important_variables = pddl_to_prolog.get_variables([rule.effect])
    if len(rule.conditions) == 1 or is_ground(rule):
        return [rule]
    file_name, map_predicate_to_edge = generate_hypertree(rule)
    htd = compute_decompositions(file_name)

    new_rules = []
    leaves = set()
    not_joined_relations = set(rule.conditions)
    associated_new_relation = dict()
    # First we create a new rules for cases with multiple bags
    # We simultaneously also collect the leaves of the tree
    for node in htd:
        if len(node.cover) > 1:
            conditions = []
            for c in node.cover:
                pos, _ = map_predicate_to_edge[c]
                condition = rule.conditions[pos]
                not_joined_relations.discard(condition)
                conditions.append(condition)
            bag_vars = get_variables_from_bag(node.bag)
            var_in_conditions = pddl_to_prolog.get_variables(conditions)
            important_vars_in_cond = important_variables.intersection(var_in_conditions)
            effect_variables = important_vars_in_cond.union(bag_vars)
            effect = pddl.Atom(next(name_generator), effect_variables)
            associated_new_relation[node] = effect
            new_rule = pddl_to_prolog.Rule(conditions, effect)
            new_rules.append(new_rule)
        else:
            pos, _ = map_predicate_to_edge[node.cover[0]] # single element in the cover
            condition = rule.conditions[pos]
            not_joined_relations.discard(condition)
            associated_new_relation[node] = condition


    new_rules += create_rule_dfs(htd[0], associated_new_relation, not_joined_relations, important_variables, name_generator)

    # HACK! change effect of last new_rule head to be the effect of the original rule
    if len(new_rules) > 0:
        root_rule = new_rules[-1]
        #print("Changing effect of %s" % str(root_rule), " to %s" % rule.effect)
        root_rule.effect = rule.effect

    delete_files(".htd")
    delete_files(".ast")

    return new_rules
