import sys
import re

import clingo
from clingo.control import Control
from clingox.program import Program, ProgramObserver, Remapping

import pddl
import timers

class ClingoApp(object):
    def __init__(self, name, no_show=False, ground_guess=False, ground=False, map_actions=dict()):
        self.program_name = name
        self.sub_doms = {}
        self.no_show = no_show
        self.ground_guess = ground_guess
        self.ground = ground
        self.map_actions = map_actions

        self.relevant_atoms = []
        self.prg = Program()

    def main(self, program):
        ctl_insts = Control()
        ctl_insts.register_observer(ProgramObserver(self.prg))
        ctl_insts.add("base", [], program)
        with timers.timing("Gringo grounding", block=True):
            ctl_insts.ground([("base", [])])

        with timers.timing("Parsing Clingo model into our model...", block=True):
            for f in self.prg.facts:
                name = f.symbol.name
                v = self.map_actions.get(name)
                if v:
                    # if it is an action predicate, then predicate is the object
                    predicate = v
                else:
                    # if it is not an action, than predicate is a simple string
                    predicate = name.replace('___', '-')
                args = (a.name for a in f.symbol.arguments)
                self.relevant_atoms.append(pddl.Atom(predicate, args))
            print("Size of the model:", len(self.prg.facts))


def _addToSubdom(sub_doms, var, value):
    if var.startswith('_dom_'):
        var = var[5:]
    else:
        return

    if var not in sub_doms:
        sub_doms[var] = []
        sub_doms[var].append(value)
    elif value not in sub_doms[var]:
        sub_doms[var].append(value)


def main(program, map_actions):
    no_show = False
    ground_guess = True
    ground = True

    clingo_app = ClingoApp(sys.argv[0], no_show, ground_guess, ground, map_actions)
    clingo_app.main(program)

    return clingo_app.relevant_atoms
