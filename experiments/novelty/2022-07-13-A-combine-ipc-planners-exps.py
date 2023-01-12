#! /usr/bin/env python

import os
import shutil

import project

from lab.experiment import Experiment

ATTRIBUTES = [
    "cost",
    "error",
    "plan_length",
    "run_dir",
    "search_start_time",
    "search_start_memory",
    "search_time",
    "total_time",
    "h_values",
    "coverage",
    "evaluations",
    "expansions",
    "memory",
    "time_for_computing_novelty",
    project.EVALUATIONS_PER_TIME,
]


exp = Experiment('data/2022-07-13-A-combine-ipc-planners-exp')
exp.add_fetcher('data/2022-07-11-F-novelty-boost-autoscale-eval')
exp.add_fetcher('data/2022-07-11-G-pareto-autoscale-eval')
for exp_name in ['2022-07-12-A-dual-bfws-eval',
                 '2022-07-12-B-olcff-eval',
                 '2022-07-12-C-merwin-eval',
                 '2022-07-13-A-bfws-pref-eval',
                 '2022-07-14-A-fickert-icaps2020-eval']:
    exp.add_fetcher(f'../ipc-planners/data/{exp_name}/')

project.add_absolute_report(
    exp,
    attributes=ATTRIBUTES,
    filter=[project.add_evaluations_per_time],
    filter_algorithm=['ipc2018_agl_lapkt_dual_bfws',
                      'ipc2018_agl_lapkt_bfws_pref',
                      'ipc2018_agl_merwin',
 	              'ipc2018_agl_olcff',
                      'fickert_icaps2020',
 	              'lama-first',
 	              'lama-novelty-2-reset-move-prefops',
                      'alt-ff-pareto-novelty-2-lmcount-reset-move-prefops']
)

project.add_absolute_report(
    exp,
    attributes=["coverage", "expansions", "memory", "total_time"],
    filter=[project.add_evaluations_per_time],
    filter_algorithm=['ipc2018_agl_lapkt_dual_bfws',
 	              'lama-first',
 	              'lama-novelty-2-reset-move-prefops',
                      'alt-ff-pareto-novelty-2-lmcount-reset-move-prefops'],
    name="expansions.html"
)


exp.run_steps()
