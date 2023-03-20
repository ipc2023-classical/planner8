#!/bin/bash
# Run in an empty directory.

#CPLEX_ROOT=/opt/cplex/v22.1.0/cplex

mkdir -p opt sat agl unsolve

function build-aidos(){
    local name="$1"
    local team_name="$2"
    local source_name="$3"
    local CPLEX_ROOT=/opt/cplex/v12.7.1.0/cplex
    if [ -f unsolve/${name}/planner.img ]; then
        return 0
    fi
    cd unsolve
    rm -rf $name
    mkdir $name
    cd $name
    cat >Singularity <<EOF
Bootstrap: docker
From: ubuntu:bionic

%setup
    cp -rv ../../repo/planners/${team_name}/source.tar.gz \$SINGULARITY_ROOTFS/
    cp -rv ../../Osi-0.107.9.tgz \$SINGULARITY_ROOTFS/
    mkdir \$SINGULARITY_ROOTFS/cplex
    cp -rv $CPLEX_ROOT/include \$SINGULARITY_ROOTFS/cplex/include
    cp -rv $CPLEX_ROOT/lib \$SINGULARITY_ROOTFS/cplex/lib
    cp -rv $CPLEX_ROOT/python \$SINGULARITY_ROOTFS/cplex/python
%post
    ## Install all necessary dependencies.
    export DEBIAN_FRONTEND="noninteractive"
    export TZ="Europe/London"
    apt-get update
    apt-get -y install cmake g++ g++-multilib make python autotools-dev automake ca-certificates git wget
    rm -rf /var/lib/apt/lists/*

    cd /
    tar xf /source.tar.gz
    mv /${source_name} /planner

    export DOWNWARD_CPLEX_ROOT=/cplex/
    export DOWNWARD_COIN_ROOT=/opt/coin/Osi-0.107.9
    export DOWNWARD_COIN_ROOT64=/opt/coin/Osi-0.107.9

    tar xvzf Osi-0.107.9.tgz
    cd Osi-0.107.9
    ./configure CC="gcc"  CFLAGS="-pthread -Wno-long-long" \\
                CXX="g++" CXXFLAGS="-pthread -Wno-long-long" \\
                LDFLAGS="-L\$DOWNWARD_CPLEX_ROOT/lib/x86-64_linux/static_pic -ldl" \\
                --without-lapack --enable-static=yes \\
                --prefix="\$DOWNWARD_COIN_ROOT" \\
                --disable-bzlib \\
                --with-cplex-incdir=\$DOWNWARD_CPLEX_ROOT/include/ilcplex \\
                --with-cplex-lib="-lcplex -lm -ldl"
    make
    make install
    cd ..
    rm -rf Osi-0.107.9
    rm Osi-0.107.9.tgz

    cd /planner
    sed -i 's/\${CMAKE_THREAD_LIBS_INIT}/\${CMAKE_THREAD_LIBS_INIT} dl/' src/cmake_modules/FindCplex.cmake
    sed -i 's/-Werror//g' src/cmake_modules/FastDownwardMacros.cmake
#sed -i 's/include <cmath>/include <math>/g' src/search/*.cc src/search/*/*.cc src/search/*.h src/search/*/*.h
    sed -i 's/ceil/std::ceil/g' src/search/*.cc src/search/*/*.cc
    sed -i '1i #include <cmath>' src/search/operator_counting/operator_counting_heuristic.cc
    ./build.py aidos_ipc -j12 VERBOSE=true

    find -name '*.o' -exec rm -f '{}' ';'
    find -name '*.a' -exec rm -f '{}' ';'
    find -name '*.cc' -exec rm -f '{}' ';'
    find -name '*.h' -exec rm -f '{}' ';'

    cd /cplex/python/2.7/x86-64_linux
    python setup.py install

%runscript
    DOMAINFILE=\$1
    PROBLEMFILE=\$2
    /planner/plan \$DOMAINFILE \$PROBLEMFILE
EOF
    sudo singularity build planner.img Singularity
    cd ..
    cd ..
}

function build-sympa(){
    name="$1"
    team_name="$2"
    source_name="$3"
    if [ -f unsolve/${name}/planner.img ]; then
        return 0
    fi
    cd unsolve
    rm -rf $name
    mkdir $name
    cd $name
    cat >Singularity <<EOF
Bootstrap: docker
From: ubuntu:trusty

%setup
    cp -rv ../../repo/planners/${team_name}/source.tar.gz \$SINGULARITY_ROOTFS/
%post
    ## Install all necessary dependencies.
    apt-get update
    apt-get -y install cmake g++ g++-multilib make python autotools-dev automake ca-certificates git wget time gawk
    rm -rf /var/lib/apt/lists/*

    cd /
    tar xf /source.tar.gz
    mv /${source_name} /planner

    cd /planner
    find -name '*.o' -exec rm '{}' ';'
    find -name '*.a' -exec rm '{}' ';'
    sed -i 's/searchDir <<   cout/searchDir/' src/search/sym/symba.cc
    ./build

    find -name '*.o' -exec rm -f '{}' ';'
    find -name '*.a' -exec rm -f '{}' ';'
    find -name '*.cc' -exec rm -f '{}' ';'
    find -name '*.h' -exec rm -f '{}' ';'

%runscript
    DOMAINFILE=\$1
    PROBLEMFILE=\$2
    /planner/plan \$DOMAINFILE \$PROBLEMFILE
EOF
    sudo singularity build planner.img Singularity
    cd ..
    cd ..
}

function build-ipc(){
    name="$1"
    repo="$2"
    dir="$3"
    if [ -f ${dir}/${name}/planner.img ]; then
        return 0
    fi
    cd $dir
    rm -rf $name
    mkdir $name
    cd $name
    wget https://bitbucket.org/ipc2018-classical/${repo}/raw/ipc-2018-seq-${dir}/Singularity
    sudo singularity build planner.img Singularity
    cd ..
    cd ..
}

if [ ! -d repo ]; then
    git clone https://github.com/AI-Planning/unsolve-ipc-2016 repo
fi
if [ ! -f Osi-0.107.9.tgz ]; then
    wget http://www.coin-or.org/download/source/Osi/Osi-0.107.9.tgz
fi

build-ipc SymbA symbolic-baseline-planners opt
build-ipc Complementary1 team9 opt
build-ipc Complementary2 team32 opt
build-ipc Delfi1 team23 opt
build-ipc Delfi2 team24 opt
build-ipc Planning-PDBs team40 opt
build-ipc Scorpion team44 opt

build-ipc FD-stonesoup team45 sat
build-ipc FD-remix team43 sat
build-ipc LAPKT-DUAL-BFWS team20 sat
build-ipc Saarplan team7 sat
build-ipc DecStar team2 sat
build-ipc Cerberus team15 sat
build-ipc Lama11 explicit-baseline-planners sat
build-ipc mercury2014 team6 sat
build-ipc freelunch-madagascar team4 sat
build-ipc freelunch-doubly-relaxed team34 sat

build-ipc BFWS-pref team47 agl
build-ipc Lama11 explicit-baseline-planners agl
build-ipc Saarplan team7 agl
build-ipc DUAL-BFWS team20 agl
build-ipc FD-remix team43 agl
build-ipc POLY-BFWS team30 agl
build-ipc DecStar team2 agl
build-ipc OLCFF team8 agl
build-ipc Cerberus team15 agl
build-ipc mercury2014 team6 agl
build-ipc freelunch-madagascar team4 agl
build-ipc freelunch-doubly-relaxed team34 agl

build-aidos Aidosv1 team9v1 seq-unsolvable-aidos-1
build-aidos Aidosv2 team9v2 seq-unsolvable-aidos-2
build-aidos Aidosv3 team9v3 seq-unsolvable-aidos-3

build-sympa SymPA team3v1 SymPA
build-sympa SymPA-irr team3v2 SymPA-irr
