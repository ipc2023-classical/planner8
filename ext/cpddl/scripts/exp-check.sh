#!/bin/bash

SHORT=0

function expcheck() {
    topdir="$1"
    echo "DIR: ${topdir}"
    num_tasks=$(find "$topdir" -maxdepth 1 -type d -name '[0-9][0-9]*' | wc -l)
    num_finished=$(find "$topdir" -name task.finished | wc -l)
    num_timeout=$(find "$topdir" -name task.timeout | wc -l)
    num_memout=$(find "$topdir" -name task.memout | wc -l)
    num_segfault=$(find "$topdir" -name task.segfault | wc -l)
    echo "Num tasks: ${num_tasks}"
    echo "Finished: ${num_finished}"
    echo "Timeout: ${num_timeout}"
    echo "Memory out: ${num_memout}"
    echo "Segfaults: ${num_segfault}"

    if [ "$SHORT" = "1" ]; then
        echo
        return
    fi

    echo "Exit status:"
    find "$topdir" -name task.status -exec cat '{}' ';' -exec echo ';' | sort | uniq -c

    echo "Finished tasks with non-zero exit status:"
    for f in $(find "$topdir" -name task.finished | sort); do
        dir=${f%/task.finished}
        bench=$(cat ${dir}/task.prop | grep 'bench_name = ' | cut -f2 -d'"')
        domain=$(cat ${dir}/task.prop | grep 'domain_name = ' | cut -f2 -d'"')
        problem=$(cat ${dir}/task.prop | grep 'problem_name = ' | cut -f2 -d'"')

        st=""
        if [ -f ${dir}/task.timeout ]; then
            st="${st}T"
        fi
        if [ -f ${dir}/task.memout ]; then
            st="${st}M"
        fi
        if [ -f ${dir}/task.segfault ]; then
            st="${st}S"
        fi
        if [ -f ${dir}/task.status ]; then
            st="${st} Exit=$(cat ${dir}/task.status)"
        fi
        if [ -f ${dir}/task.signum ]; then
            st="${st} Sig=$(cat ${dir}/task.signum)"
        fi
        if [ ! -f ${dir}/task.status ] || [ "$(cat ${dir}/task.status)" != "0" ]; then
            echo $dir $bench/$domain/$problem "$st"
        fi
    done | column -t -s ' '

    echo
}

if [ "$1" = "-s" ] || [ "$1" = "--short" ]; then
    SHORT=1
    shift
fi

while [ "$1" != "" ]; do
    expcheck "$1"
    shift
done
