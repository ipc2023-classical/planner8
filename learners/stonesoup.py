#! /usr/bin/env python

import argparse
from collections import defaultdict
import json
import lzma
import sys
import time

import numpy as np


EPSILON = 0.0001 # Don't care about this little time.


def read_properties(path):
    open_func = lzma.open if path.endswith(".xz") else open
    with open_func(path) as f:
        return json.load(f)


def dump_portfolio(track, configs, timeouts, results, **kwargs):
    outfile = kwargs.pop("outfile", None)
    if outfile:
        stdout = sys.stdout
        sys.stdout = open(outfile, "w")

    print('"""')
    kwargs.setdefault("runtime", "{}s".format(sum(timeouts)))
    for key, value in sorted(kwargs.items()):
        if value is not None:
            print("{}: {}".format(key.capitalize(), value))
    print('"""')
    print()
    print("TRACK = \"{}\"".format(track))
    print("CONFIGS = [")

    assert len(configs) == len(timeouts)
    for config, timeout in zip(configs, timeouts):
        if timeout > 0:
            print("    # {}".format(config))
            print("    ({}, {}),".format(timeout, results.config_to_options[config]))
    print("]")

    if outfile:
        sys.stdout.close()
        sys.stdout = stdout


class Results(object):
    def __init__(self, props, track, timeout):
        self.config_to_options, self.problems, data = self._parse_entries(props, track, timeout)
        self.configs = sorted(self.config_to_options.keys())
        assert len(data) == len(self.configs) * len(self.problems)
        self.runtimes, self.scores = self._get_arrays(self.configs, self.problems, data)

    def _parse_entries(self, runs, track, timeout):
        configs = {}
        problems = set()
        data = {}
        best_costs = {}
        costs = defaultdict(set)
        for entry_ in runs:
            entry = self._massage_entry(entry_, timeout)

            config = entry["config"]
            problem = entry["problem"]
            cost = entry["cost"]
            options = entry["options"]

            configs[config] = options
            problems.add(problem)
            if cost is not None:
                costs[problem].add(cost)
                if problem not in best_costs:
                    best_costs[problem] = cost
                else:
                    best_costs[problem] = min(best_costs[problem], cost)
            data[config, problem] = (entry["time"], cost)

        for problem, all_costs in sorted(costs.items()):
            print("{}: {} different costs from {} to {}".format(
                problem, len(all_costs), min(all_costs), max(all_costs)))

        for (config, problem), (time, cost) in data.items():
            score = 0.
            if time is not None:
                if track == "agl":
                    # We set agile scores with the update_agile_scores method.
                    pass
                elif track in ["cbo", "sat"]:
                    best_cost = best_costs[problem]
                    assert cost is not None
                    assert best_cost <= cost
                    if cost == 0:
                        assert best_cost == 0
                        score = 1.
                    else:
                        score = float(best_cost) / cost
                elif track == "opt":
                    assert cost == best_costs[problem], (problem, config)
                    score = 1.
                else:
                    assert False, track
            data[config, problem] = (time, score)
        return configs, sorted(problems), data

    def _massage_entry(self, entry, timeout):
        algorithm = entry["algorithm"]
        if ":" in algorithm:
            config = algorithm.split(":", 1)[1]
        else:
            config = algorithm
        domain = entry["domain"]
        problem = entry["problem"]
        coverage = entry.get("coverage", 0)
        assert coverage in [0, 1], entry

        if coverage:
            assert "total_time" in entry, entry
            assert "cost" in entry, entry
            time = float(entry["total_time"])
            cost = int(entry["cost"])
            if time >= timeout + EPSILON:
                time = None
                cost = None
        else:
            assert "cost" not in entry, entry
            time = None
            cost = None
        return dict(
            config=config,
            options=entry.get("component_options", [config]),
            problem="%s:%s" % (domain, problem),
            time=time,
            cost=cost)

    def _get_arrays(self, configs, problems, data):
        runtimes = np.zeros((len(problems), len(configs)))
        scores = np.zeros((len(problems), len(configs)))
        for p, problem in enumerate(self.problems):
            for c, config in enumerate(self.configs):
                time, score = data[config, problem]
                if time is None:
                    assert score == 0.
                    time = np.inf
                runtimes[p][c] = time
                scores[p][c] = score
        return runtimes, scores

    def update_agile_scores(self, elapsed_time, portfolio_time):
        times = self.runtimes + elapsed_time
        self.scores = np.zeros(self.runtimes.shape)
        self.scores += np.where(times <= 1., 1., 0.)
        self.scores += np.where(
            (1. < times) & (times <= portfolio_time),
            1. - (np.log10(times) / np.log10(portfolio_time)),
            0.)

    def solution_times(self, problem_id):
        result = {}
        for c, config in enumerate(self.configs):
            time = self.runtimes[problem_id][c]
            if time != np.inf:
                result[config] = time
        return result

    def _total_solved(self):
        return np.sum(np.any(self.runtimes != np.inf, axis=1))

    def dump_statistics(self):
        print(len(self.problems), "problems")
        print(len(self.configs), "configs")
        print(len(self.configs) * len(self.problems), "results")
        print(self._total_solved(), "problems solved by someone")
        max_num_solved = -1
        max_score = -1
        for c, config in enumerate(self.configs):
            num_solved = np.sum(self.runtimes[:,c] != np.inf)
            print(num_solved, "problems solved by", config)
            max_num_solved = max(max_num_solved, num_solved)
        for c, config in enumerate(self.configs):
            score = np.sum(self.scores[:,c])
            print("%.2f" % score, "score by", config)
            max_score = max(max_score, score)
        print("Max problems solved by single config:", max_num_solved)
        print("Max score by single config:", max_score)


class Portfolio(object):
    def __init__(self, results, timeouts):
        self.results = results
        self.timeouts = timeouts
        self.configs = sorted(self.timeouts.keys())
        self.timeouts_array = np.array([self.timeouts[config] for config in self.configs])

    def solves_problem(self, problem_id):
        solution_times = self.results.solution_times(problem_id)
        for solver, time in solution_times.items():
            if time < self.timeouts[solver] + EPSILON:
                return True
        return False

    def total_timeout(self):
        return sum(self.timeouts.values())

    def num_solved(self):
        return sum(1 for p in range(len(self.results.problems))
                   if self.solves_problem(p))

    def ipc_score(self):
        # Set score to 0 if config fails to solve problem within timeout.
        censored = np.where(self.results.runtimes < self.timeouts_array, self.results.scores, 0.)
        # Maximum score per problem.
        maxima = np.amax(censored, axis=1)
        # Sum over maximum scores for all problems.
        return np.sum(maxima)

    def successors(self, granularity):
        for config in self.configs:
            succ_timeouts = self.timeouts.copy()
            succ_timeouts[config] += granularity
            yield Portfolio(self.results, succ_timeouts)

    def all_successors(self, granularity, timeout):
        used_time = self.total_timeout()
        unused_time = timeout - used_time
        for additional_time in range(granularity, unused_time + 1, granularity):
            for config in self.configs:
                succ_timeouts = self.timeouts.copy()
                succ_timeouts[config] += additional_time
                yield Portfolio(self.results, succ_timeouts)

    def reduce_score_based(self, granularity):
        score = self.ipc_score()
        for c, config in enumerate(self.configs):
            # Reduce timeout for this config until we hit zero or lose
            # a problem.
            while self.timeouts[config] > granularity + EPSILON:
                self.timeouts[config] -= granularity
                self.timeouts_array[c] -= granularity
                if self.ipc_score() < score:
                    # Undo last change and stop reducing this timeout.
                    self.timeouts[config] += granularity
                    self.timeouts_array[c] += granularity
                    break

    def dump_marginal_contributions(self):
        print("Marginal contributions:")
        num_solved = self.num_solved()
        score = self.ipc_score()
        for config in self.configs:
            if self.timeouts[config] > EPSILON:
                succ_timeouts = self.timeouts.copy()
                succ_timeouts[config] = 0
                succ = Portfolio(self.results, succ_timeouts)
                num_lost = num_solved - succ.num_solved()
                score_lost = score - succ.ipc_score()
                print("   %3d problems / %.2f score from %s" % (num_lost,
                        score_lost, config))

    def dump(self):
        print("portfolio for %.2f seconds solves %d problems with score %.2f:" % (
            self.total_timeout(), self.num_solved(), self.ipc_score()))
        for config in self.configs:
            print("   %7.2f seconds for %s" % (self.timeouts[config], config))

    def dump_unsolved(self):
        print("Unsolved problems:")
        count = 0
        for p, problem in enumerate(self.results.problems):
            solution_times = self.results.solution_times(p)
            if solution_times and not self.solves_problem(p):
                formatted_solution_times = "{%s}" % ", ".join(
                    "%s: %.2f" % pair
                    for pair in sorted(solution_times.items()))
                print("   %-35s %s" % (problem, formatted_solution_times))
                count += 1
        print("(%d problems)" % count)

def compute_portfolio_using_ipc_scores(results, granularity, timeout):
    portfolio = Portfolio(results, dict.fromkeys(results.configs, 0))
    while portfolio.total_timeout() + granularity <= timeout:
        portfolio.dump()
        print()

        def score(portfolio):
            return portfolio.ipc_score()

        best_successor = max(portfolio.successors(granularity), key=score)
        portfolio = best_successor
    return portfolio

def sort_configs_by_decreasing_coverage(configs, results):
    def get_coverage(c):
        return np.sum(results.scores[:,c] != 0)

    sorted_indices = sorted(list(range(len(configs))), key=get_coverage, reverse=True)
    return [configs[c] for c in sorted_indices]

def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("properties_file", help="Path to Downward Lab properties file")
    parser.add_argument("granularity", type=int, help="Time granularity in seconds")
    parser.add_argument(
        "--portfolio-time", default=1800, type=int,
        help="Portfolio time limit in seconds (default %(default)s)")
    parser.add_argument(
        "--track", choices=["cbo", "opt", "sat"], default="sat",
        help="IPC track (default %(default)s)")
    return parser.parse_args()

def main():
    args = parse_args()
    props = read_properties(args.properties_file)
    results = Results(list(props.values()), track=args.track, timeout=args.portfolio_time)
    results.dump_statistics()
    print("Computing portfolio using IPC scores...")
    start_time = time.process_time()
    portfolio = compute_portfolio_using_ipc_scores(
        results, granularity=args.granularity, timeout=args.portfolio_time)
    portfolio.dump()
    print()
    print("Time for computing portfolio: {:.02f}s".format(time.process_time() - start_time))
    print()
    print("Reducing portfolio...")
    portfolio.reduce_score_based(granularity=1)
    portfolio.dump()
    print()
    summary = {
        "Final score": portfolio.ipc_score(),
        "Configs": sum(1 if time > 0 else 0 for time in list(portfolio.timeouts.values())),
        "Min time": min(val for val in list(portfolio.timeouts.values()) if val > 0),
        "Max time": max(portfolio.timeouts.values()),
    }

    print("Sort by decreasing coverage")
    configs = sort_configs_by_decreasing_coverage(results.configs, results)

    dump_portfolio(
        args.track,
        configs,
        [portfolio.timeouts[config] for config in configs],
        results,
        granularity="{}s".format(args.granularity),
        coverage=portfolio.num_solved(),
        score=portfolio.ipc_score())
    print()
    portfolio.dump_unsolved()
    print()
    portfolio.dump_marginal_contributions()
    print()
    print("    ".join(f"{key}: {value}" for key, value in summary.items()))


if __name__ == "__main__":
    main()
