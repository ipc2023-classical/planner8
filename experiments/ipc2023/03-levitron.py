#! /usr/bin/env python

import csv
from pathlib import Path

from lab.experiment import Experiment

import project


DIR = Path(__file__).resolve().parent
REPO = project.get_repo_base()
ATTRIBUTES = [
    "error",
    #"run_dir",
    "coverage",
    "search_time",
    "total_time",
]

exp = Experiment()
exp.add_step("remove-combined-properties", project.remove_properties, Path(exp.eval_dir))

domains_with_axioms = set()
domains_with_cond_effs = set()
with open("domain_properties.csv") as f:
    reader = csv.reader(f)
    next(reader)  # Skip header
    for row in reader:
        domain, axioms, cond_effs = row
        assert axioms in {"True", "False"}
        assert cond_effs in {"True", "False"}
        if axioms == "True":
            domains_with_axioms.add(domain)
        if cond_effs == "True":
            domains_with_cond_effs.add(domain)
print("Axioms:", sorted(domains_with_axioms))
print("Conditional effects:", sorted(domains_with_cond_effs))
print("Both:", sorted(domains_with_axioms & domains_with_cond_effs))

DOMAINS_WITH_AXIOMS = {
    'airport-adl', 'assembly-adl', 'miconic-fulladl-adl', 'openstacks-adl',
     'optical-telegraphs-adl', 'philosophers-adl', 'psr-large-adl', 'psr-middle-adl', 'trucks-adl'
}

def domain_has_no_axioms(run):
    return run["domain"] not in DOMAINS_WITH_AXIOMS

DOMAINS_WITH_COND_EFFS = {
    'assembly-adl', 'briefcaseworld-adl', 'caldera-adl', 'caldera-split-adl', 'cavediving-adl',
    'citycar-adl', 'flashfill-adl', 'fsc-blocks-strips', 'fsc-grid-a-strips', 'fsc-grid-r-strips',
    'fsc-hall-strips', 'fsc-visualmarker-strips', 'gedp-ds2ndp-adl', 'miconic-fulladl-adl',
     'miconic-simpleadl-adl', 'nurikabe-adl', 'psr-large-adl', 'psr-middle-adl', 'settlers-adl',
     't0-adder-adl', 't0-coins-adl', 't0-comm-adl', 't0-grid-dispose-adl', 't0-grid-push-adl',
     't0-grid-trash-adl', 't0-sortnet-adl', 't0-sortnet-alt-adl', 't0-uts-adl'
}

def domain_has_no_cond_effs(run):
    return run["domain"] not in DOMAINS_WITH_COND_EFFS

for expname in [
    "powerlifted-htg",
    "powerlifted-patricks-benchmark",
    "scorpion-novelty-htg",
    "scorpion-novelty-patricks-benchmark",
    #"fdss-patricks-benchmark",
]:
    project.fetch_algorithms(exp, expname, filters=[project.strip_properties, domain_has_no_axioms, domain_has_no_cond_effs])

project.add_absolute_report(
    exp, attributes=ATTRIBUTES, name=f"{exp.name}-full"
)

properties_hardest = Path(exp.eval_dir) / "properties-hardest.json"
exp.add_report(project.Hardest30Report(filter=[domain_has_no_axioms, domain_has_no_cond_effs]), outfile=properties_hardest, name="keep-only-30-hardest-tasks")
exp.add_step("compress-properties", project.compress, properties_hardest)
#project.add_absolute_report(exp, attributes=ATTRIBUTES, name=f"{exp.name}-hardest")

exp.run_steps()
