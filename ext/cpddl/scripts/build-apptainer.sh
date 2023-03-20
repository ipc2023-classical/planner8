#!/bin/bash

if [ "$1" = "" ]; then
    echo "Usage: $0 [--cplex ibm_studio_topdir] [alpine,debian-{bullseye,buster,stretch},ubuntu-{jammy,focal,bionic},fedora]"
    exit -1
fi

SETUP="
%setup
    cp -r ./ \$APPTAINER_ROOTFS/cpddl
"

cplex_suff=
if [ "$1" = "--cplex" ]; then
    cplex_suff="-cplex"
    shift
    cplex_dir="${1%%/}"
    SETUP="$SETUP
    rsync -avm --exclude 'python' \\
              --exclude 'examples' \\
              --exclude 'opl/*' \\
              --include '*/' \\
              --include '*.h' \\
              --include '*.a' \\
              --include '*.so' \\
              --exclude '*' \\
              $cplex_dir/ \$APPTAINER_ROOTFS/cplex/
    find \$APPTAINER_ROOTFS/cplex
"
    shift
fi

MAKE="
    cd /cpddl
    rm -f Makefile.config
    [ -d /cplex ] && echo \"IBM_CPLEX_ROOT = /cplex\" >>Makefile.config
    [ -f /usr/bin/clang ] \\
        && echo \"CC = clang\" >>Makefile.config \\
        && echo \"CXX = clang++\" >>Makefile.config
    make help
    make mrproper
    make -j8 third-party
    make -j8
    make -j8 bin
    mv /cpddl/bin/pddl /
    [ -d /cplex ] && find /cplex -type f -not -name '*.so' -exec rm '{}' ';'
    rm -rf /cpddl
"

RUN="
%runscript
    /pddl "\$@"

%labels
    Daniel Fiser <danfis@danfis.cz>
"

function build_alpine(){
    local name="${1}${cplex_suff}"
    local base="$2"
    cat >Apptainer.${name} <<EOF
Bootstrap: docker
From: $base

$SETUP

%post
    apk update
    apk upgrade
    apk add make gcc g++ autoconf automake git glpk glpk-dev bash libstdc++
    $MAKE
    apk del make gcc g++ autoconf automake git glpk-dev bash
$RUN
EOF
    sudo apptainer build cpddl-${name}.img Apptainer.${name}
}

function build_debian(){
    local name="${1}${cplex_suff}"
    local base="$2"
    cat >Apptainer.${name} <<EOF
Bootstrap: docker
From: $base

$SETUP

%post
    apt update -y
    apt upgrade -y
    apt install -y make gcc g++ autoconf automake git libglpk-dev libglpk40 libstdc++6
    $MAKE
    apt purge -y make gcc g++ autoconf automake git libglpk-dev
    apt purge -y manpages openssh-client
    apt autoremove -y
    apt-get clean -y

$RUN
EOF
    sudo apptainer build cpddl-${name}.img Apptainer.${name}
}

function build_fedora(){
    local name="${1}${cplex_suff}"
    local base="$2"
    cat >Apptainer.${name} <<EOF
Bootstrap: docker
From: $base

$SETUP

%post
    dnf -y update
    dnf -y install make gcc g++ autoconf automake git glpk-devel glpk libstdc++
    $MAKE
    dnf -y remove make gcc g++ autoconf automake git glpk-devel
$RUN
EOF
    sudo apptainer build cpddl-${name}.img Apptainer.${name}
}

if [ "$1" = "alpine" ]; then
    build_alpine alpine alpine:latest

elif [ "$1" = "debian" ] || [ "$1" = "debian-bullseye" ]; then
    build_debian debian-bullseye debian:bullseye-slim
elif [ "$1" = "debian-buster" ]; then
    build_debian debian-buster debian:buster-slim
elif [ "$1" = "debian-stretch" ]; then
    build_debian debian-stretch debian:stretch-slim

elif [ "$1" = "ubuntu" ] || [ "$1" = "ubuntu-jammy" ]; then
    build_debian ubuntu-jammy ubuntu:jammy
elif [ "$1" = "ubuntu-focal" ]; then
    build_debian ubuntu-focal ubuntu:focal
elif [ "$1" = "ubuntu-bionic" ]; then
    build_debian ubuntu-bionic ubuntu:bionic

elif [ "$1" = "fedora" ]; then
    build_fedora fedora fedora:36
fi
