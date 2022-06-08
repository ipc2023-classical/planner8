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

project.fetch_algorithm(exp, "2022-06-07-A-incomplete-novelty-open-list", "03-ff-typed-incomplete-novelty-1", new_algo="novelty-1-fifo")
project.fetch_algorithm(exp, "2022-06-07-A-incomplete-novelty-open-list", "04-ff-typed-incomplete-novelty-2", new_algo="novelty-2-fifo")
#project.fetch_algorithm(exp, "2022-06-07-B-incomplete-novelty-open-list-reset", "03-ff-typed-incomplete-novelty-1-reset", new_algo="novelty-1-reset-fifo")
#project.fetch_algorithm(exp, "2022-06-07-B-incomplete-novelty-open-list-reset", "04-ff-typed-incomplete-novelty-2-reset", new_algo="novelty-2-reset-fifo")
project.fetch_algorithm(exp, "2022-06-07-C-incomplete-novelty-open-list-randomize-clear", "ff-typed-novelty-1", new_algo="novelty-1-random")
project.fetch_algorithm(exp, "2022-06-07-C-incomplete-novelty-open-list-randomize-clear", "ff-typed-novelty-2", new_algo="novelty-2-random")

filters = [project.add_evaluations_per_time]

project.add_absolute_report(
    exp, attributes=ATTRIBUTES, filter=filters, name=f"{exp.name}"
)

exp.add_report(ComparativeReport([
        ("novelty-1-fifo", "novelty-1-random"),
        ("novelty-2-fifo", "novelty-2-random"),
    ],
    attributes=ATTRIBUTES, filter=[project.add_evaluations_per_time]
))

def add_missing_coverage(run):
    run.setdefault("coverage", 0)
    return run

exp.add_report(project.PerDomainComparison(sort=True, filter=add_missing_coverage))

exp.run_steps()
