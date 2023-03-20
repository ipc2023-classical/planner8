#!/bin/bash

set -e

if [ "$1" = "-h" ]; then
    echo "Usage: $0 [-h/--check/--check-all] ..."
    echo "Example: $0 debian bullseye cplex clang"
    exit

elif [ "$1" = "--check" ]; then
    shift
    RUN_CHECK="RUN cd /cpddl && make check"

elif [ "$1" = "--check-all" ]; then
    shift
    RUN_CHECK="RUN cd /cpddl && make check-all"
fi


CPLEX="/opt/cplex/v22.1.0"

COPY="
COPY ./ /cpddl
"

function gen_make(){
    local m=""
    local werror=""
    while [ "$1" != "" ]; do
        if [ "$1" = "cplex" ]; then
            m="
RUN cd /cpddl && echo \"IBM_CPLEX_ROOT = /cpddl/.cplex\" >>Makefile.config
$m
"
        elif [ "$1" = "third-party" ]; then
            m="$m
RUN cd /cpddl && make -j8 third-party
"
        elif [ "$1" = "clang" ]; then
            m="
RUN cd /cpddl && echo \"CC = clang\" >>Makefile.config
RUN cd /cpddl && echo \"CXX = clang++\" >>Makefile.config
$m
"

        elif [ "$1" = "debian:buster" ]; then
            werror="yes"

        fi
        shift
    done

    pre="
RUN cd /cpddl && rm -f Makefile.config && make mrproper && make help
"
    if [ "$werror" != "no" ]; then
        pre="
$pre
RUN cd /cpddl && echo \"WERROR = yes\" >Makefile.config
"
    fi
    m="$pre
$m
RUN cd /cpddl && make help
RUN cd /cpddl && make -j8
RUN cd /cpddl && make -j8 bin
"
    echo "$m"
}

function alpine(){
    local m="$(gen_make $@)"
    local run=
    while [ "$1" != "" ]; do
        if [ "$1" = "glpk" ]; then
            run="$run
RUN apk add glpk-dev"
        elif [ "$1" = "clang" ]; then
            run="$run
RUN apk add clang"
        fi
        shift
    done

    cat >Dockerfile <<EOF
FROM alpine:3.16.0
LABEL cpddl=test-build

RUN apk update
RUN apk upgrade
RUN apk add make gcc g++ autoconf automake git bash libstdc++
$run

$COPY

$m
EOF
    if [ "$RUN_CHECK" != "" ]; then
        echo "RUN apk add python3 diffutils" >>Dockerfile
        echo "$RUN_CHECK" >>Dockerfile
    fi
}

function debian(){
    local m="$(gen_make "debian:$1" $@)"
    local run=
    local from="$1"
    shift
    while [ "$1" != "" ]; do
        if [ "$1" = "glpk" ]; then
            run="$run
RUN apt install -y libglpk-dev"
        elif [ "$1" = "clang" ]; then
            run="$run
RUN apt install -y clang"
        fi
        shift
    done

    cat >Dockerfile <<EOF
FROM debian:${from}-slim
LABEL cpddl=test-build

RUN apt update -y
RUN apt upgrade -y
RUN apt install -y make gcc g++ autoconf automake git
$run

$COPY

$m
EOF
    if [ "$RUN_CHECK" != "" ]; then
        echo "RUN apt install -y python3 diffutils" >>Dockerfile
        echo "$RUN_CHECK" >>Dockerfile
    fi
}

function ubuntu(){
    local m="$(gen_make "ubuntu:$1" $@)"
    local run=
    local from="$1"
    shift
    while [ "$1" != "" ]; do
        if [ "$1" = "glpk" ]; then
            run="$run
RUN apt install -y libglpk-dev"
        elif [ "$1" = "clang" ]; then
            run="$run
RUN apt install -y clang"
        fi
        shift
    done

    cat >Dockerfile <<EOF
FROM ubuntu:${from}
LABEL cpddl=test-build

RUN apt update -y
RUN apt upgrade -y
RUN apt install -y make gcc g++ autoconf automake git
$run

$COPY

$m
EOF
    if [ "$RUN_CHECK" != "" ]; then
        echo "RUN apt install -y python3 diffutils" >>Dockerfile
        echo "$RUN_CHECK" >>Dockerfile
    fi
}

function fedora(){
    local m="$(gen_make $@)"
    local run=
    local from="$1"
    shift
    while [ "$1" != "" ]; do
        if [ "$1" = "glpk" ]; then
            run="$run
RUN dnf -y install glpk-devel"
        elif [ "$1" = "clang" ]; then
            run="$run
RUN dnf -y install clang"
        fi
        shift
    done

    cat >Dockerfile <<EOF
FROM fedora:${from}
LABEL cpddl=test-build

RUN dnf -y update
RUN dnf -y install make gcc g++ autoconf automake git findutils
$run

$COPY

$m
EOF
    if [ "$RUN_CHECK" != "" ]; then
        echo "RUN dnf -y install python3 diffutils" >>Dockerfile
        echo "$RUN_CHECK" >>Dockerfile
    fi
}

function run(){
    cat Dockerfile
    docker build --force-rm .
}

rm -rf .cplex
if [ -d $CPLEX ]; then
    rsync -avm --exclude 'python' \
               --exclude 'examples' \
               --exclude 'opl/*' \
               --include '*/' \
               --include '*.h' \
               --include '*.a' \
               --include '*.so' \
               --exclude '*' \
               $CPLEX/ .cplex/
fi

rm -f test.log

if [ "$1" != "" ]; then
    $@ 2>&1 | tee -a test.log
    run 2>&1 | tee -a test.log
    exit
fi

for clang in "" "clang"; do
for lp in "" "cplex" "glpk"; do
for third_party in "" "third-party"; do
    if [ ! -d .cplex ] && [ "$lp" = "cplex" ]; then
        continue
    fi

    echo "=============="
    [ "$clang" = "clang" ] && echo "==== clang"
    [ "$lp" = "" ] && echo "==== no-LP"
    [ "$lp" = "cplex" ] && echo "==== cplex"
    [ "$lp" = "glpk" ] && echo "==== glpk"
    [ "$third_party" = "" ] && echo "==== no-third-party"
    [ "$third_party" != "" ] && echo "==== third-party"
    echo "=============="
    if [ "$lp" != "cplex" ]; then
        echo
        echo "=== alpine"
        alpine $clang $lp $third_party
        run
    fi
    
    echo
    echo "=== debian-bullseye"
    debian bullseye $clang $lp $third_party
    run

    echo
    echo "=== debian-buster"
    debian buster $clang $lp $third_party
    run

    echo
    echo "=== debian-stretch"
    debian stretch $clang $lp $third_party
    run

    echo
    echo "=== ubuntu-jammy"
    ubuntu jammy $clang $lp $third_party
    run

    echo
    echo "=== ubuntu-focal"
    ubuntu focal $clang $lp $third_party
    run

    echo
    echo "=== ubuntu-bionic"
    ubuntu bionic $clang $lp $third_party
    run

    echo
    echo "=== fedora 36"
    fedora 36 $clang $lp $third_party
    run

    echo
    echo "=== fedora 34"
    fedora 34 $clang $lp $third_party
    run
done
done
done 2>&1 | tee -a test.log

rm -rf .cplex
rm -f Dockerfile
yes | docker image prune --filter label=cpddl=test-build
