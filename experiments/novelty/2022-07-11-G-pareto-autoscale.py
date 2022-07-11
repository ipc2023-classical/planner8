#! /usr/bin/env python

import os
import shutil

import project


REPO = project.get_repo_base()
BENCHMARKS_DIR = os.environ["AUTOSCALE_BENCHMARKS_SAT"]
if project.REMOTE:
    SUITE = project.SUITE_AUTOSCALE_SAT
    ENV = project.BaselSlurmEnvironment(email="augusto.blaascorrea@unibas.ch", partition="infai_2")
else:
    SUITE = ["gripper:p01.pddl"]
    ENV = project.LocalEnvironment(processes=2)

CONFIGS = [(f"alt-pareto-ff-novelty-{novelty}-lmcount{'-random' if random else ''}{'-reset' if reset else ''}-{handle_progress}-prefops", [
    "--evaluator",
    "hlm=lmcount(lm_factory=lm_reasonable_orders_hps(lm_rhw()),transform=adapt_costs(one),pref=false)",
    "--evaluator", "hff=ff(transform=adapt_costs(one))",
    "--evaluator", f"novelty_eval=novelty(width={novelty}, consider_only_novel_states=true, reset_after_progress={reset})",
    "--search", f"""lazy(alt([pareto([hff, novelty_eval]), single(hff, pref_only=true), single(hlm), single(hlm, pref_only=true)], boost=1000), preferred=[hff,hlm], cost_type=one,reopen_closed=false)"""])
           for novelty in [2]
           for reset in [True]
           for handle_progress in ["move"]
           for random in [False]
] + [(f"alt-ff-pareto-novelty-{novelty}-lmcount{'-random' if random else ''}{'-reset' if reset else ''}-{handle_progress}-prefops", [
    "--evaluator",
    "hlm=lmcount(lm_factory=lm_reasonable_orders_hps(lm_rhw()),transform=adapt_costs(one),pref=false)",
    "--evaluator", "hff=ff(transform=adapt_costs(one))",
    "--evaluator", f"novelty_eval=novelty(width={novelty}, consider_only_novel_states=true, reset_after_progress={reset})",
    "--search", f"""lazy(alt([single(hff), single(hff, pref_only=true), pareto([hlm, novelty_eval]), single(hlm, pref_only=true)], boost=1000), preferred=[hff,hlm], cost_type=one,reopen_closed=false)"""])
           for novelty in [2]
           for reset in [True]
           for handle_progress in ["move"]
           for random in [False]
] + [(f"alt-pareto-ff-novelty-{novelty}-pareto-lmcount-novelty-{novelty}{'-random' if random else ''}{'-reset' if reset else ''}-{handle_progress}-prefops", [
    "--evaluator",
    "hlm=lmcount(lm_factory=lm_reasonable_orders_hps(lm_rhw()),transform=adapt_costs(one),pref=false)",
    "--evaluator", "hff=ff(transform=adapt_costs(one))",
    "--evaluator", f"novelty_eval=novelty(width={novelty}, consider_only_novel_states=true, reset_after_progress={reset})",
    "--search", f"""lazy(alt([pareto([hff, novelty_eval]), single(hff, pref_only=true), pareto([hlm, novelty_eval]), single(hlm, pref_only=true)], boost=1000), preferred=[hff,hlm], cost_type=one,reopen_closed=false)"""])
           for novelty in [2]
           for reset in [True]
           for handle_progress in ["move"]
           for random in [False]
]


BUILD_OPTIONS = []
DRIVER_OPTIONS = ["--overall-time-limit", "5m"]
REVS = [
    ("e7e4740a8", ""),
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

exp.run_steps()
