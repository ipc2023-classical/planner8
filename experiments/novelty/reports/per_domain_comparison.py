from collections import defaultdict
import itertools
import logging

from lab.reports import CellFormatter, Table

from downward.reports import PlanningReport


class PerDomainComparison(PlanningReport):
    def __init__(self, sstddev=None, sort=False, **kwargs):
        """
        If given, *stddev* must be a dictionary mapping from algorithm
        names to standard deviation values.

        If *sort* is True, sort algorithms from "weakest" to "strongest".
        The "strength" of an algorithm A is the number of other algorithms
        against which A "wins" in a per-domain comparison.

        You can control the decimal precision by passing an `Attribute`
        object with an adjusted value for `digits`.

        """
        kwargs.setdefault("attributes", ["coverage"])
        PlanningReport.__init__(self, **kwargs)
        self.sstddev = sstddev or {}
        self.sort = sort
        if len(self.attributes) != 1:
            logging.critical("report needs exactly one attribute")
        self.attribute = self.attributes[0]
        self.digits = self.attribute.digits

    def get_markup(self):
        domain_and_algorithm_to_coverage = defaultdict(int)
        for (domain, problem), runs in self.problem_runs.items():
            for run in runs:
                domain_and_algorithm_to_coverage[(run["domain"], run["algorithm"])] += run[self.attribute]

        algorithms = self.algorithms
        domain_groups = sorted(set([group for group, _ in domain_and_algorithm_to_coverage.keys()]))
        print("{} domains: {}".format(len(domain_groups), domain_groups))

        num_domains_better = defaultdict(int)
        for algo1, algo2 in itertools.combinations(algorithms, 2):
            for domain in domain_groups:
                coverage1 = domain_and_algorithm_to_coverage[(domain, algo1)]
                coverage2 = domain_and_algorithm_to_coverage[(domain, algo2)]
                if coverage1 > coverage2:
                    num_domains_better[(algo1, algo2)] += 1
                elif coverage2 > coverage1:
                    num_domains_better[(algo2, algo1)] += 1

        def get_coverage(algo):
            return sum(domain_and_algorithm_to_coverage[(domain, algo)] for domain in domain_groups)

        def get_wins(algo1):
            num_wins = 0
            num_strict_wins = 0
            for algo2 in self.algorithms:
                if algo1 == algo2:
                    continue
                num_algo1_better = 0
                num_algo2_better = 0
                for domain in domain_groups:
                    coverage1 = domain_and_algorithm_to_coverage[(domain, algo1)]
                    coverage2 = domain_and_algorithm_to_coverage[(domain, algo2)]
                    if coverage1 > coverage2:
                        num_algo1_better += 1
                    elif coverage2 > coverage1:
                        num_algo2_better += 1

                if num_domains_better[(algo1, algo2)] >= num_domains_better[(algo2, algo1)]:
                    num_wins += 1
                if num_domains_better[(algo1, algo2)] > num_domains_better[(algo2, algo1)]:
                    num_strict_wins += 1
            return num_wins, num_strict_wins

        def get_wins_and_coverage(algo):
            return (get_wins(algo), get_coverage(algo))

        if self.sort:
            algorithms = sorted(algorithms, key=get_wins_and_coverage)

        comparison_table = Table()
        comparison_table.set_row_order(algorithms)
        comparison_table.set_column_order(algorithms + ["Coverage"])
        comparison_table.row_min_wins["Coverage"] = False
        for algo1, algo2 in itertools.permutations(algorithms, 2):
            num_algo1_better = num_domains_better[(algo1, algo2)]
            num_algo2_better = num_domains_better[(algo2, algo1)]
            comparison_table.add_cell(algo1, algo2, num_algo1_better)
            if num_algo1_better >= num_algo2_better:
                comparison_table.cell_formatters[algo1][algo2] = CellFormatter(bold=True, align_right=True)
        for algo in algorithms:
            comparison_table.add_cell(algo, algo, "''--''")
            comparison_table.cell_formatters[algo][algo] = CellFormatter(align_right=True)

        total_coverage = dict(
            (algo, sum(domain_and_algorithm_to_coverage[(domain, algo)] for domain in domain_groups))
            for algo in algorithms)

        def print_line(cells):
            print(" & ".join(str(c) for c in cells) + r" \\")

        def format_value(val):
            if isinstance(val, int):
                return str(val)
            else:
                return f"{float(val):.{self.digits}f}"

        include_sstddev = bool(self.sstddev)
        max_coverage = format_value(max(get_coverage(algo) for algo in algorithms))
        print(r"\newcommand{\bc}[1]{\textbf{#1}}")
        print(r"\renewcommand{\arraystretch}{1.2}")
        print(r"\setlength{\tabcolsep}{3pt}")
        print(r"\setlength{\cmidrulekern}{8pt}")
        print(r"\begin{tabular}{@{}l%s@{\hskip 8pt}r@{}}" % ('r' * len(algorithms)))
        line = [""] + [r"\rot{%s}" % c for c in algorithms] + [fr"\rot{{{self.attribute}}}"]
        if include_sstddev:
            line.append(r"\rot{Stddev.}")
        print_line(line)
        offsets = tuple(offset + len(algorithms) for offset in (1, 2, 3 if include_sstddev else 2))
        print(r"\cmidrule[\lightrulewidth](r){1-%d} \cmidrule[\lightrulewidth]{%d-%d}" % offsets)
        for algo1 in algorithms:
            total_coverage = format_value(get_coverage(algo1))
            if total_coverage == max_coverage:
                total_coverage = fr"\bc{{{total_coverage}}}"
            line = []
            for algo2 in algorithms:
                num_algo1_better = 0
                num_algo2_better = 0
                for domain in domain_groups:
                    coverage1 = domain_and_algorithm_to_coverage[(domain, algo1)]
                    coverage2 = domain_and_algorithm_to_coverage[(domain, algo2)]
                    if coverage1 > coverage2:
                        num_algo1_better += 1
                    elif coverage2 > coverage1:
                        num_algo2_better += 1

                if algo1 == algo2:
                    entry = "--"
                elif num_algo1_better >= num_algo2_better:
                    entry = fr"\bc{{{num_algo1_better}}}"
                else:
                    entry = str(num_algo1_better)
                line.append(entry)
            line = [algo1] + line + [total_coverage]
            if include_sstddev:
                sstddev = self.sstddev.get(algo1)
                line.append(format_value(sstddev) if sstddev is not None else "--")
            print_line(line)
        print(r"\end{tabular}")

        return str(comparison_table)
