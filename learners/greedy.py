#! /usr/bin/env python

import argparse
from collections import defaultdict
import time

import numpy as np

import stonesoup


class Portfolio(object):
    def __init__(self, results):
        self.results = results
        self.configs = []
        self.scores = np.zeros(len(self.results.problems))

    def add_config(self, timeout, config):
        self.configs.append((timeout, config))
        self.scores += self.get_added_scores(timeout, config)

    def total_timeout(self):
        return sum(t for t, _ in self.configs)

    def num_solved(self):
        return np.count_nonzero(self.scores)

    def total_score(self):
        return np.sum(self.scores)

    def get_added_scores(self, timeout, config):
        new_scores = np.where(
            self.results.runtimes[:,config] < timeout,
            self.results.scores[:,config],
            0.)
        return np.maximum(new_scores - self.scores, 0.)

    def get_total_added_score(self, timeout, config):
        return np.sum(self.get_added_scores(timeout, config))

    def uniquify_configs(self):
        print("Only keep longest time slice for each config, but preserve order of first occurences.")
        max_times = defaultdict(int)
        for timeout, config in self.configs:
            max_times[config] = max(max_times[config], timeout)
        self.configs = [(timeout, config) for config, timeout in max_times.items()]

    def dump(self):
        print("portfolio for %.2f seconds solves %d problems with score %.2f" % (
            self.total_timeout(), self.num_solved(), self.total_score()))

    def dump_portfolio(self, track, outfile):
        stonesoup.dump_portfolio(
            track,
            [self.results.configs[c] for t, c in self.configs],
            [t for t, c in self.configs],
            self.results,
            coverage=self.num_solved(),
            score=self.total_score(),
            outfile=outfile)


def get_candidates(num_configs, remaining_time):
        for timeout in range(1, remaining_time + 1):
            for config in range(num_configs):
                yield timeout, config


def compute_portfolio(results, track, portfolio_time):
    portfolio = Portfolio(results)
    portfolio.dump()
    print()
    while portfolio.total_timeout() < portfolio_time:
        if track == "agl":
            results.update_agile_scores(portfolio.total_timeout(), portfolio_time)

        def score(time_out_and_config):
            (timeout, config) = time_out_and_config
            additional_score = portfolio.get_total_added_score(timeout, config)
            return additional_score / timeout

        remaining_time = portfolio_time - portfolio.total_timeout()
        timeout, config = max(get_candidates(len(results.configs), remaining_time), key=score)
        if portfolio.get_total_added_score(timeout, config) > stonesoup.EPSILON:
            print(f"run config {config} for {timeout} seconds")
            portfolio.add_config(timeout, config)
            if track == "opt":
                portfolio.uniquify_configs()
            portfolio.dump()
            print()
        else:
            print("Improvement below {}".format(stonesoup.EPSILON))
            break
    return portfolio


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("properties_file")
    parser.add_argument("--portfolio-time", default=1800, type=int)
    parser.add_argument("--track", choices=["agl", "cbo", "opt", "sat"], default="sat")
    parser.add_argument("--outfile")
    return parser.parse_args()


def main():
    args = parse_args()
    props = stonesoup.read_properties(args.properties_file)
    results = stonesoup.Results(list(props.values()), track=args.track, timeout=args.portfolio_time)
    if args.track == "agl":
        results.update_agile_scores(0, args.portfolio_time)
    results.dump_statistics()
    print("Computing portfolio...")
    start_time = time.process_time()
    portfolio = compute_portfolio(results, track=args.track, portfolio_time=args.portfolio_time)
    portfolio.dump()
    print()
    print(f"Time for computing portfolio: {time.process_time() - start_time:.2f}s")
    print()
    print("Configs:", len(portfolio.configs))
    print("Unique configs:", len(set(c for t, c in portfolio.configs)))
    print("Min time limit:", min(t for t, c in portfolio.configs))
    print("Max time limit:", max(t for t, c in portfolio.configs))
    print("Final score: ", portfolio.total_score())
    print()
    portfolio.dump_portfolio(args.track, args.outfile)


if __name__ == "__main__":
    main()
