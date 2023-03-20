#!/bin/bash

VAL="$1"

find -name plan.out \
     -or -name 'plan.out.*' \
     -or -name sas_plan \
     -or -name 'sas_plan.*' | \
while read plan; do
    dir=$(dirname "$plan")
    base=$(basename "$plan")
    if [ ! -f "$dir/domain.pddl" ]; then
        continue
    fi
    domain="$dir/domain.pddl"
    problem="$dir/problem.pddl"
    echo "Validating $plan"
    "$VAL" "$domain" "$problem" "$plan" >"$dir/validate.${base}" 2>&1
    if [ "$?" != 0 ]; then
        echo "$plan : PLAN FAILED!"
        echo "valid_plan = false" >"$dir/validate.prop"
    else
        echo "valid_plan = true" >"$dir/validate.prop"
    fi
done
