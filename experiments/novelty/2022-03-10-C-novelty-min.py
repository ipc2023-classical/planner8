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
    ("00-novelty", [
        "--search", """lazy_greedy([novelty(width=2, aggregate=min)],cost_type=one,reopen_closed=false)"""]),
    ("01-ff", [
        "--evaluator", "hff=ff(transform=adapt_costs(one))",
        "--search", """lazy_greedy([hff],preferred=[hff],cost_type=one,reopen_closed=false)"""]),
    ("02-ff-novelty", [
        "--evaluator", "hff=ff(transform=adapt_costs(one))",
        "--search", """lazy_greedy([hff,novelty(width=2, aggregate=min)],preferred=[hff],
                                cost_type=one,reopen_closed=false)"""]),
    ("03-ff-lmc", [
        "--evaluator",
        "hlm=lmcount(lm_factory=lm_reasonable_orders_hps(lm_rhw()),transform=adapt_costs(one),pref=false)",
        "--evaluator", "hff=ff(transform=adapt_costs(one))",
        "--search", """lazy_greedy([hff,hlm],preferred=[hff,hlm],
                                cost_type=one,reopen_closed=false)"""]),
    ("04-ff-lmc-novelty", [
        "--evaluator",
        "hlm=lmcount(lm_factory=lm_reasonable_orders_hps(lm_rhw()),transform=adapt_costs(one),pref=false)",
        "--evaluator", "hff=ff(transform=adapt_costs(one))",
        "--search", """lazy_greedy([hff,hlm,novelty(width=2, aggregate=min)],preferred=[hff,hlm],
                                cost_type=one,reopen_closed=false)"""]),
]
BUILD_OPTIONS = []
DRIVER_OPTIONS = ["--overall-time-limit", "5m"]
REVS = [
    ("novelty", ""),
]
ATTRIBUTES = [
    "error",
    "run_dir",
    "search_start_time",
    "search_start_memory",
    "total_time",
    "h_values",
    "coverage",
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

attributes = ["expansions"]
pairs = [
    ("novelty:01-cg", "novelty:02-ff"),
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
                filter=[project.add_evaluations_per_time],
                format="tex" if project.TEX else "png",
            ),
            name=f"{exp.name}-{algo1}-vs-{algo2}-{attr}{suffix}",
        )

exp.run_steps()
