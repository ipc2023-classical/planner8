#! /usr/bin/env python

import os
from pathlib import Path
import shutil

from downward import suites
from downward.cached_revision import CachedFastDownwardRevision
from downward.experiment import (
    _DownwardAlgorithm,
    _get_solver_resource_name,
    FastDownwardRun,
)
from lab.experiment import Experiment, get_default_data_dir

import project


REPO = project.get_repo_base()
BENCHMARKS_DIR = "/infai/blaas/projects/ipc/fd-submission/benchmarks/"
SCP_LOGIN = "infai"
REMOTE_REPOS_DIR = "/infai/myuser/repos"
SUITE = ["miconic-strips:0-p01.pddl"]
try:
    REVISION_CACHE = Path(os.environ["DOWNWARD_REVISION_CACHE"])
except KeyError:
    REVISION_CACHE = Path(get_default_data_dir()) / "revision-cache"
if project.REMOTE:
    ENV = BaselSlurmEnvironment(
        partition='infai_3',
        email="augusto.blaascorrea@unibas.ch",
        memory_per_cpu="4G",
        extra_options='#SBATCH --cpus-per-task=2')
    SUITE = project.SUITE_STRIPS_AND_ADL
else:
    BENCHMARKS_DIR = "/home/blaas/work/projects/ipc/fd-submission/benchmarks/"
    ENV = project.LocalEnvironment(processes=2)


CONFIGS = [("lama-first",
            ["--evaluator","hlm=lmcount(lm_factory=lm_reasonable_orders_hps(lm_rhw()),transform=adapt_costs(one),pref=false)",
            "--evaluator", "hff=ff(transform=adapt_costs(one))",
            "--search", """lazy(alt([single(hff), single(hff, pref_only=true), single(hlm), single(hlm, pref_only=true)], boost=1000),preferred=[hff,hlm],
            cost_type=one,reopen_closed=false)"""])
] + [(f"lama-novelty-{novelty}{'-random' if random else ''}{'-reset' if reset else ''}-{handle_progress}-prefops", [
        "--evaluator",
        "hlm=lmcount(lm_factory=lm_reasonable_orders_hps(lm_rhw()),transform=adapt_costs(one),pref=false)",
        "--evaluator", "hff=ff(transform=adapt_costs(one))",
        "--search", f"""lazy(alt([single(hff), single(hff, pref_only=true), single(hlm), single(hlm, pref_only=true), novelty_open_list(novelty(width={novelty}, consider_only_novel_states=true, reset_after_progress={reset}), break_ties_randomly={random}, handle_progress={handle_progress})], boost=1000), preferred=[hff,hlm], cost_type=one,reopen_closed=false)"""])
    for novelty in [1,2]
    for reset in [True]
    for handle_progress in ["clear", "move"]
    for random in [False]
] + [
    (f"lama-type-based-prefops", [
        "--evaluator",
        "hlm=lmcount(lm_factory=lm_reasonable_orders_hps(lm_rhw()),transform=adapt_costs(one),pref=false)",
        "--evaluator", "hff=ff(transform=adapt_costs(one))",
        "--search", """lazy(alt([single(hff), single(hff, pref_only=true), single(hlm), single(hlm, pref_only=true), type_based([hff, g()])], boost=1000),preferred=[hff,hlm], cost_type=one,reopen_closed=false)"""])
] + [
    (f"lama-type-based-novelty-{novelty}{'-random' if random else ''}{'-reset' if reset else ''}-{handle_progress}-prefops",
     ["--evaluator",
        "hlm=lmcount(lm_factory=lm_reasonable_orders_hps(lm_rhw()),transform=adapt_costs(one),pref=false)",
        "--evaluator", "hff=ff(transform=adapt_costs(one))",
        "--search", f"""lazy(alt([single(hff), single(hff, pref_only=true), single(hlm), single(hlm, pref_only=true), type_based([hff, g()]), novelty_open_list(novelty(width={novelty}, consider_only_novel_states=true, reset_after_progress={reset}), break_ties_randomly={random}, handle_progress={handle_progress})], boost=1000),preferred=[hff,hlm], cost_type=one,reopen_closed=false)"""])
    for novelty in [1,2]
    for reset in [True]
    for handle_progress in ["move"]
    for random in [False]
]



BUILD_OPTIONS = []
DRIVER_OPTIONS = [
    "--validate",
    # Higher time limits probably don't make sense since we're building sequential portfolios.
    "--overall-time-limit", "10m",
    # Same memory limit as in competition.
    "--overall-memory-limit", "8G",
]
# Pairs of revision identifier and revision nick.
REVS = [
    ("c19db50", ""),
]
ATTRIBUTES = [
    "error",
    "run_dir",
    #"search_start_time",
    #"search_start_memory",
    #"score_total_time",
    "total_time",
    "coverage",
    "expansions",
    #"memory",
]

exp = Experiment(environment=ENV)
for rev, rev_nick in REVS:
    cached_rev = CachedFastDownwardRevision(REPO, rev, BUILD_OPTIONS)
    cached_rev.cache(REVISION_CACHE)
    cache_path = REVISION_CACHE / cached_rev.name
    dest_path = Path(f"code-{cached_rev.name}")
    exp.add_resource("", cache_path, dest_path)
    # Overwrite the script to set an environment variable.
    exp.add_resource(
        _get_solver_resource_name(cached_rev),
        cache_path / "fast-downward.py",
        dest_path / "fast-downward.py",
    )
    for config_nick, config in CONFIGS:
        algo_name = f"{rev_nick}:{config_nick}" if rev_nick else config_nick

        for task in suites.build_suite(BENCHMARKS_DIR, SUITE):
            algo = _DownwardAlgorithm(
                algo_name,
                cached_rev,
                DRIVER_OPTIONS,
                config,
            )
            run = FastDownwardRun(exp, algo, task)
            exp.add_run(run)


exp.add_parser(project.FastDownwardExperiment.EXITCODE_PARSER)
#exp.add_parser(project.FastDownwardExperiment.TRANSLATOR_PARSER)
exp.add_parser(project.FastDownwardExperiment.SINGLE_SEARCH_PARSER)
exp.add_parser(project.DIR / "parser.py")
#exp.add_parser(project.FastDownwardExperiment.PLANNER_PARSER)

exp.add_step("build", exp.build)
exp.add_step("start", exp.start_runs)
exp.add_fetcher(name="fetch")

if not project.REMOTE:
    exp.add_step("remove-eval-dir", shutil.rmtree, exp.eval_dir, ignore_errors=True)
    project.add_scp_step(exp, SCP_LOGIN, REMOTE_REPOS_DIR)

project.add_absolute_report(
    exp,
    attributes=ATTRIBUTES,
)

exp.run_steps()
