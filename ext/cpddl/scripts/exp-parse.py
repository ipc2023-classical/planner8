#!/usr/bin/env python3

import sys
import os
import re
import json
from optparse import OptionParser
from pprint import pprint

PROP_FILENAMES = ['cpddl.prop', 'prop.out']
LISAT_FILENAMES = ['lisat.out', 'lisat.err']
FD_FILENAMES = ['fd.out', 'fd.err']
FD_TRANSLATE_FILENAMES = ['fdtr.out', 'fdtr.err']
FF_TRANSLATE_FILENAMES = ['fftr.out', 'fftr.err']

FILENAMES = PROP_FILENAMES \
                + LISAT_FILENAMES \
                + FD_FILENAMES \
                + FD_TRANSLATE_FILENAMES \
                + FF_TRANSLATE_FILENAMES
FILENAMES = set(FILENAMES)

pat_task_dir = re.compile(r'^[0-9]+$')
pat_prop_line = re.compile(r'^([a-zA-Z][a-zA-Z0-9_.-]*) ?= *(.+)$')

def _parseProp(fn):
    with open(fn, 'r') as fin:
        for line in fin:
            line = line.strip()
            match = pat_prop_line.match(line)
            if match is None:
                raise Exception('Invalid .prop file: ' + line)
            key = match.group(1)
            if len(key) == 0:
                raise Exception('Invalid .prop file: ' + line)
            val = match.group(2)
            if val.startswith('"'):
                if not val.endswith('"'):
                    raise Exception('Invalid .prop file: ' + line)
                val = val[1:-1]

            elif val in ['true', 'True']:
                val = True

            elif val in ['false', 'False']:
                val = False

            elif '.' in val:
                val = float(val)

            else:
                val = int(val)

            yield key, val

def parseTaskProp(fn):
    data = {}
    for key, val in _parseProp(fn):
        data[key] = val
    return data

def parseProp(fn):
    data = {
        'num_types' : -1,
        'num_objs' : -1,
        'num_preds' : -1,
        'num_funcs' : -1,
        'num_actions' : -1,
        'num_lmgs' : -1,
        'compile_num_actions' : -1,
        'compile_time' : -1.,
        'compile_change' : False,
        'num_fdr_vars' : -1,
        'num_fdr_facts' : -1,
        'num_fdr_ops' : -1,
        'unsolvable' : False,
        'solved' : False,
        'plan_cost' : -1,
        'plan_len' : -1,
        'ground_unsolvable' : False,
    }

    for key, val in _parseProp(fn):
        if key.startswith('pddl.') and key.split('.')[1] in data:
            data[key.split('.')[1]] = val

        elif key == 'lmg_fam_group.found_mgroups':
            data['num_lmgs'] = val

        elif key == 'compile_in_lmg.out.actions' and data['compile_num_actions'] == -1:
            data['compile_num_actions'] = val

        elif key == 'compile_in_lmg.ctx_elapsed_time' and data['compile_time'] < 0.:
            data['compile_time'] = val

        elif key == 'fdr.num_vars':
            data['num_fdr_vars'] = val

        elif key == 'fdr.num_facts':
            data['num_fdr_facts'] = val

        elif key == 'fdr.num_ops':
            data['num_fdr_ops'] = val

        elif key == 'lplan.unsolvable':
            data['unsolvable'] = val

        elif key == 'lplan.found':
            data['solved'] = val

        elif key == 'lplan.plan_cost':
            data['plan_cost'] = val

        elif key == 'lplan.plan_length':
            data['plan_len'] = val

        elif key in ['compile_in_lmg.mutex.cond', 'compile_in_lmg.dead_end.cond']:
            if val not in ['(or)', '(or )']:
                data['compile_change'] = True

        elif key == 'ground_dl.goal_unreachable':
            data['ground_unsolvable'] = bool(val)
    return data

def parseFDTranslate(fn):
    data = { 
        'num_fdr_vars' : -1,
        'num_fdr_facts' : -1,
        'num_fdr_ops' : -1,
        'ground_unsolvable' : False,
    }
    with open(fn, 'r') as fin:
        for line in fin:
            line = line.strip()
            if line.startswith('Translator variables:'):
                data['num_fdr_vars'] = int(line.split()[-1])
            elif line.startswith('Translator facts:'):
                data['num_fdr_facts'] = int(line.split()[-1])
            elif line.startswith('Translator operators:'):
                data['num_fdr_ops'] = int(line.split()[-1])
            elif line.startswith('No relaxed solution!'):
                data['ground_unsolvable'] = True
    return data

def parseFFTranslate(fn):
    data = { 
        'num_fdr_vars' : -1,
        'num_fdr_facts' : -1,
        'num_fdr_ops' : -1,
    }
    with open(fn, 'r') as fin:
        for line in fin:
            line = line.strip()
            if line.startswith('Num actions:'):
                data['num_fdr_ops'] = int(line.split()[-1])
            elif line.startswith('Num relevant facts:'):
                data['num_fdr_facts'] = int(line.split()[-1])
    return data

def parseDir(dr):
    data = []
    for root, dirs, files in os.walk(dr):
        s = root.strip('/').split('/')
        if pat_task_dir.match(s[-1]) is not None:
            assert(len(s) >= 2)
            assert('task.prop' in files)
            if 'task.finished' not in files:
                print('{0:100s}'.format(root + ' not finished'), file = sys.stderr)
                continue
            print('{0:100s}'.format(root),
                  end = '\r', file = sys.stderr)
            d = parseProp(os.path.join(root, 'task.prop'))
            d['variant'] = s[-2]
            d['taskroot'] = str(root)
            d['plan_files'] = [f for f in files \
                    if f.startswith('plan.out') or f.startswith('sas_plan')]

            d['exit_status'] = None
            if 'task.status' in files:
                d['exit_status'] = \
                    int(open(os.path.join(root, 'task.status'), 'r').read().strip())
            d['timeout'] = ('task.timeout' in files)
            d['memout'] = ('task.memout' in files)
            d['signum'] = None
            if 'task.signum' in files:
                d['signum'] = \
                    int(open(os.path.join(root, 'task.signum'), 'r').read().strip())

            d['time'] = float(open(os.path.join(root, 'task.time'), 'r').read().strip())

            d.update(parseTaskProp(os.path.join(root, 'task.prop')))
            for fn in files:
                if fn in PROP_FILENAMES:
                    dp = parseProp(os.path.join(root, fn))
                    d.update(dp)

                if fn in LISAT_FILENAMES:
                    dp = parseLisat(os.path.join(root, fn))
                    d.update(dp)

                if fn in FD_FILENAMES:
                    dp = parseFD(os.path.join(root, fn))
                    d.update(dp)

                if fn in FD_TRANSLATE_FILENAMES:
                    dp = parseFDTranslate(os.path.join(root, fn))
                    d.update(dp)

                if fn in FF_TRANSLATE_FILENAMES:
                    dp = parseFFTranslate(os.path.join(root, fn))
                    d.update(dp)

            if len(FILENAMES & set(files)) == 0:
                print('Error: Nothing to parse in {0}'.format(root))
                sys.exit(-1)

            data += [d]

    print('{0:100s}'.format('Parsing of {0} DONE'.format(dr)), file = sys.stderr)
    return data

if __name__ == '__main__':
    data = []
    for dr in sys.argv[1:]:
        data += parseDir(dr)
    print(json.dumps(data))
