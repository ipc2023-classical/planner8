#! /usr/bin/env python

import os
import shutil

import project


REPO = project.get_repo_base()
BENCHMARKS_DIR = os.environ["DOWNWARD_BENCHMARKS"]
SCP_LOGIN = "seipp@login-infai.scicore.unibas.ch"
REMOTE_REPOS_DIR = "/infai/seipp/projects"
if project.REMOTE:
    SUITE = project.SUITE_SATISFICING
    ENV = project.BaselSlurmEnvironment(email="jendrik.seipp@liu.se", partition="infai_2")
else:
    SUITE = ["depot:p01.pddl", "grid:prob01.pddl", "gripper:prob01.pddl"]
    ENV = project.LocalEnvironment(processes=2)

CONFIGS = [
    #("01-ff", [
    #    "--evaluator", "hff=ff(transform=adapt_costs(one))",
    #    "--search", """lazy(single(hff), cost_type=one, reopen_closed=false)"""]),
    ("02-ff-typed", [
        "--evaluator", "hff=ff(transform=adapt_costs(one))",
        "--search", """lazy(alt([single(hff), type_based([hff, g()])]), cost_type=one, reopen_closed=false)"""]),
    ("03-ff-typed-novelty-1", [
        "--evaluator", "hff=ff(transform=adapt_costs(one))",
        "--search", """lazy(alt([single(hff), tbbf([hff, g()], width=1)]), cost_type=one, reopen_closed=false)"""]),
    ("04-ff-typed-novelty-2", [
        "--evaluator", "hff=ff(transform=adapt_costs(one))",
        "--search", """lazy(alt([single(hff), tbbf([hff, g()], width=2)]), cost_type=one, reopen_closed=false)"""]),
]
BUILD_OPTIONS = []
DRIVER_OPTIONS = ["--overall-time-limit", "5m"]
REVS = [
    ("fd8c52efb", "map"),
]
ATTRIBUTES = [
    "error",
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
    project.EVALUATIONS_PER_TIME,
]

exp = project.FastDownwardExperiment(environment=ENV)
for config_nick, config in CONFIGS:
    for rev, rev_nick in REVS:
        algo_name = f"{rev_nick}:{config_nick}" if rev_nick else config_nick
        exp.add_algorithm(
            algo_name,
            REPO,
            rev,
            config,
            build_options=BUILD_OPTIONS,
            driver_options=DRIVER_OPTIONS,
        )
exp.add_suite(BENCHMARKS_DIR, SUITE)

exp.add_parser(exp.EXITCODE_PARSER)
exp.add_parser(exp.TRANSLATOR_PARSER)
exp.add_parser(exp.SINGLE_SEARCH_PARSER)
exp.add_parser(project.DIR / "parser.py")
exp.add_parser(exp.PLANNER_PARSER)

exp.add_step("build", exp.build)
exp.add_step("start", exp.start_runs)
exp.add_fetcher(name="fetch")

if not project.REMOTE:
    exp.add_step(
        "remove-eval-dir", shutil.rmtree, exp.eval_dir, ignore_errors=True
    )
    project.add_scp_step(exp, SCP_LOGIN, REMOTE_REPOS_DIR)

project.add_absolute_report(
    exp, attributes=ATTRIBUTES, filter=[project.add_evaluations_per_time]
)

from downward.reports.compare import ComparativeReport
exp.add_report(ComparativeReport([
        ("01-ff", "04-ff-novelty-alt"),
        ("01-ff", "05-ff-novelty-tb"),
    ],
    attributes=ATTRIBUTES, filter=[project.add_evaluations_per_time]
))

def add_missing_coverage(run):
    run.setdefault("coverage", 0)
    return run

exp.add_report(project.PerDomainComparison(sort=True, filter=add_missing_coverage))
exp.add_report(project.PerTaskComparison(sort=True, attributes=["expansions"]))

def filter_zero_expansions(run):
    if run.get("expansions") == 0:
        run["expansions"] = None
    return run

attributes = ["expansions"]
pairs = [
    ("rnd:02-ff-typed", "rnd:03-ff-typed-novelty-1"),
    ("rnd:02-ff-typed", "rnd:04-ff-typed-novelty-2"),
    ("rnd:02-ff-typed", "rnd:05-ff-typed-counting-1"),
    ("rnd:02-ff-typed", "rnd:06-ff-typed-counting-2"),
]
suffix = "-rel" if project.RELATIVE else ""
for algo1, algo2 in pairs:
    for attr in attributes:
        exp.add_report(
            project.ScatterPlotReport(
                relative=project.RELATIVE,
                get_category=None if project.TEX else lambda run1, run2: run1["domain"],
                attributes=[attr],
                filter_algorithm=[algo1, algo2],
                filter=[project.add_evaluations_per_time,filter_zero_expansions],
                format="tex" if project.TEX else "png",
            ),
            name=f"{exp.name}-{algo1}-vs-{algo2}-{attr}{suffix}",
        )

exp.run_steps()
