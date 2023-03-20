#!/bin/bash
#SBATCH -p cpufast # partition (queue)
#SBATCH -N 1 # number of nodes
#SBATCH -n 8 # number of cores
#SBATCH -x n33 # exclude node n33
#SBATCH --mem 1G # memory pool for all cores
#SBATCH -t 0-2:00 # time (D-HH:MM)
#SBATCH -o build.%N.%j.out # STDOUT
#SBATCH -e build.%N.%j.err # STDERR

# This script builds the project on the RCI cluster.
# Submit this task from the top directory of this repository as follows:
# sbatch scripts/build-rci.sh

set -x

module load Autotools/20200321-GCCcore-10.2.0
module load Clang/11.0.1-GCCcore-10.2.0

NCPUS=8

#CPLEX_ROOT=/mnt/appl/software/CPLEX/12.9-foss-2018b
CPLEX_ROOT=/home/fiserdan/cplex/v22.1.0
#CPLEX_ROOT=/home/fiserdan/cplex/v12.10

cat >Makefile.config <<EOF
CC = clang
CXX = clang++
CFLAGS = -march=native
IBM_CPLEX_ROOT=$CPLEX_ROOT
#CPLEX_CFLAGS = -I$CPLEX_ROOT/cplex/include
#CPLEX_LDFLAGS = -L$CPLEX_ROOT/cplex/bin/x86-64_linux -Wl,-rpath=$CPLEX_ROOT/cplex/bin/x86-64_linux -lcplex1290
#CPOPTIMIZER_CPPFLAGS = -I$CPLEX_ROOT/cpoptimizer/include -I$CPLEX_ROOT/concert/include/ -I$CPLEX_ROOT/cplex/include/
#CPOPTIMIZER_LDFLAGS = -L$CPLEX_ROOT/cpoptimizer/lib/x86-64_linux/static_pic/ -lcp -L$CPLEX_ROOT/concert/lib/x86-64_linux/static_pic/ -lconcert -lstdc++
EOF


make mrproper
#make -j$NCPUS opts
make -j$NCPUS bliss
make -j$NCPUS cudd
make -j$NCPUS sqlite
make -j$NCPUS
make -j$NCPUS -C bin
