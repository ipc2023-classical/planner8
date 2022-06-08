#! /usr/bin/env python

from pathlib import Path

from downward.reports.compare import ComparativeReport

from lab.experiment import Experiment

import project


ATTRIBUTES = [
    "error",
    "run_dir",
    "planner_time",
    "initial_h_value",
    "coverage",
    "cost",
    "evaluations",
    "expansions",
    "memory",
    project.EVALUATIONS_PER_TIME,
]

exp = Experiment()
exp.add_step(
    "remove-combined-properties", project.remove_file, Path(exp.eval_dir) / "properties"
)

project.fetch_algorithm(exp, "2022-05-14-C-typed-bfs-with-novelty-open-list", "open:04-ff-typed-novelty-2", new_algo="novelty-2-open")
project.fetch_algorithm(exp, "2022-05-14-D-typed-bfs-with-novelty-fact-pair-map", "map:04-ff-typed-novelty-2", new_algo="novelty-2-map")

filters = [project.add_evaluations_per_time]

project.add_absolute_report(
    exp, attributes=ATTRIBUTES, filter=filters, name=f"{exp.name}"
)

exp.add_report(ComparativeReport([
        ("novelty-2-open", "novelty-2-map"),
    ],
    attributes=ATTRIBUTES, filter=[project.add_evaluations_per_time]
))


exp.run_steps()
