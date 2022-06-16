#! /usr/bin/env python

import os
import shutil

import project


REPO = project.get_repo_base()
BENCHMARKS_DIR = os.environ["AUTOSCALE_BENCHMARKS_AGILE"]
if project.REMOTE:
    SUITE = project.SUITE_SATISFICING
    ENV = project.BaselSlurmEnvironment(email="augusto.blaascorrea@unibas.ch", partition="infai_2")
else:
    SUITE = ["depots:p01.pddl", "grid:p01.pddl", "gripper:p01.pddl"]
    ENV = project.LocalEnvironment(processes=2)

CONFIGS = [
    ("01-ff", [
        "--evaluator", "hff=ff(transform=adapt_costs(one))",
        "--search", """lazy(single(hff), cost_type=one, reopen_closed=false)"""]),
    #("ff-typed", [
    #    "--evaluator", "hff=ff(transform=adapt_costs(one))",
    #    "--search", """lazy(alt([single(hff), type_based([hff, g()])]), cost_type=one, reopen_closed=false)"""]),
] + [
    (f"ff-novelty-{novelty}{'-random' if random else ''}{'-reset' if reset else ''}-{handle_progress}", [
        "--evaluator", "hff=ff(transform=adapt_costs(one))",
        "--search", f"""lazy(alt([single(hff), novelty_open_list(novelty(width={novelty}, consider_only_novel_states=true, reset_after_progress={reset}), break_ties_randomly={random}, handle_progress={handle_progress})]), cost_type=one, reopen_closed=false)"""])
    for novelty in [1, 2]
    for reset in [True, False]
    for handle_progress in ["clear", "move"]
    for random in [False]
]
BUILD_OPTIONS = []
DRIVER_OPTIONS = ["--overall-time-limit", "5m"]
REVS = [
    ("41d35d664", ""),
]
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

# if not project.REMOTE:
#     exp.add_step(
#         "remove-eval-dir", shutil.rmtree, exp.eval_dir, ignore_errors=True
#     )
#     project.add_scp_step(exp, SCP_LOGIN, REMOTE_REPOS_DIR)

project.add_absolute_report(
    exp, attributes=ATTRIBUTES, filter=[project.add_evaluations_per_time]
)

from downward.reports.compare import ComparativeReport
exp.add_report(ComparativeReport([
        ("ff-novelty-1", "ff-novelty-1-random"),
        ("ff-novelty-2", "ff-novelty-2-random"),
        ("ff-novelty-1-clear", "ff-novelty-1-random-clear"),
        ("ff-novelty-2-clear", "ff-novelty-2-random-clear"),
        ("ff-novelty-1-reset", "ff-novelty-1-random-reset"),
        ("ff-novelty-2-reset", "ff-novelty-2-random-reset"),
        ("ff-novelty-1-reset-clear", "ff-novelty-1-random-reset-clear"),
        ("ff-novelty-2-reset-clear", "ff-novelty-2-random-reset-clear"),
    ],
    attributes=ATTRIBUTES, filter=[project.add_evaluations_per_time]
), name=f"{exp.name}-random")

exp.add_report(ComparativeReport([
        ("ff-novelty-1", "ff-novelty-1-reset"),
        ("ff-novelty-2", "ff-novelty-2-reset"),
        ("ff-novelty-1-clear", "ff-novelty-1-reset-clear"),
        ("ff-novelty-2-clear", "ff-novelty-2-reset-clear"),
        ("ff-novelty-1-random", "ff-novelty-1-random-reset"),
        ("ff-novelty-2-random", "ff-novelty-2-random-reset"),
        ("ff-novelty-1-random-clear", "ff-novelty-1-random-reset-clear"),
        ("ff-novelty-2-random-clear", "ff-novelty-2-random-reset-clear"),
    ],
    attributes=ATTRIBUTES, filter=[project.add_evaluations_per_time]
), name=f"{exp.name}-reset")

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
    ("map:02-ff-typed", "map:03-ff-typed-novelty-1"),
    ("map:02-ff-typed", "map:04-ff-typed-novelty-2"),
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
