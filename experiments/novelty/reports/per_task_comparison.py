from collections import defaultdict
import itertools

from lab.reports import Table

from downward.reports import PlanningReport

class PerTaskComparison(PlanningReport):
    """
    Make a per-task, pairwise comparison of a single attribute.

    The entry in row r and column c shows the numbers of tasks where
    algorithm r yields a better value than algorithm c. We only consider
    the subset of tasks for which all algorithms report the attribute.
    We highlight the maximum of the entries (r, c) and (c, r) in bold.

    """
    def __init__(self, sort=False, **kwargs):
        """
        If *sort* is True, sort algorithms from "weakest" to
        "strongest". The "strength" of an algorithm A is the number of
        other algorithms against which A "wins" in a per-domain
        comparison.

        """
        PlanningReport.__init__(self, **kwargs)
        if len(self.attributes) != 1:
            raise ValueError("Report needs exactly one attribute")
        self.attribute = self.attributes[0]
        self.sort = sort
        if self.attribute.min_wins is None:
            raise ValueError(
                "Report needs an Attribute object with min_wins={True,False}")

    def get_markup(self):
        num_tasks_better = defaultdict(int)
        num_tasks = 0
        for (domain, problem), runs in sorted(self.problem_runs.items()):
            if all(self.attribute in run for run in runs):
                num_tasks += 1
            else:
                continue

            for run1, run2 in itertools.combinations(runs, 2):
                algo1 = run1["algorithm"]
                algo2 = run2["algorithm"]
                val1 = run1.get(self.attribute)
                val2 = run2.get(self.attribute)
                if val1 is not None and val2 is not None:
                    order = None
                    if self.attribute.min_wins:
                        if val1 < val2:
                            order = (algo1, algo2)
                        elif val1 > val2:
                            order = (algo2, algo1)
                    else:
                        assert not self.attribute.min_wins
                        if val1 > val2:
                            order = (algo1, algo2)
                        elif val1 < val2:
                            order = (algo2, algo1)
                    if order is not None:
                        num_tasks_better[order] += 1

        def get_wins(algo1):
            num_wins = 0
            for algo2 in self.algorithms:
                if algo1 == algo2:
                    continue
                num_algo1_better = num_tasks_better[(algo1, algo2)]
                num_algo2_better = num_tasks_better[(algo2, algo1)]
                if num_algo1_better >= num_algo2_better:
                    num_wins += 1
            return num_wins

        algorithms = self.algorithms[:]
        if self.sort:
            algorithms.sort(key=get_wins)

        comparison_table = Table()
        comparison_table.set_row_order(algorithms)
        comparison_table.set_column_order(algorithms)
        for algo1, algo2 in itertools.permutations(algorithms, 2):
            num_algo1_better = num_tasks_better[(algo1, algo2)]
            num_algo2_better = num_tasks_better[(algo2, algo1)]
            if num_algo1_better >= num_algo2_better:
                if self.output_format == "tex":
                    content = r" ''\textbf{{{}}}''".format(num_algo1_better)
                else:
                    content = r" ''<b>{}</b>''".format(num_algo1_better)
            else:
                content = num_algo1_better
            comparison_table.add_cell(algo1, algo2, content)
        for algo in algorithms:
            comparison_table.add_cell(algo, algo, " ''--''")

        print("Number of tasks for which all algorithms report {}: {}".format(
            self.attribute, num_tasks))

        return str(comparison_table)
