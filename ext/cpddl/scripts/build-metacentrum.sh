#!/bin/bash

# This script builds the project on the Czech National Grid.
# Submit this task from the top directory of this repository as follows:
# qsub -l walltime=8:0:0 -l select=1:ncpus=8:mem=64gb:scratch_local=30gb:cluster=zenon ./scripts/build-metacentrum.sh

set -x

HOME_ROOT=$PBS_O_WORKDIR
ROOT=$SCRATCHDIR/repo
NCPUS=$PBS_NCPUS

mkdir $ROOT
rsync -av $HOME_ROOT/ $ROOT/

CPLEX_LIBDIR=/software/cplex/12.8.0/cplex/lib/x86-64_linux/static_pic
CPLEX_INCDIR=/software/cplex/12.8.0/cplex/include/
cd $ROOT
cat >Makefile.config <<EOF
CFLAGS = -march=native
CPLEX_CFLAGS = -I$CPLEX_INCDIR
CPLEX_LDFLAGS = -L$CPLEX_LIBDIR -lcplex -ldl
EOF

make mrproper
make -j$NCPUS opts
make -j$NCPUS bliss
make -j$NCPUS cudd
make -j$NCPUS sqlite
make -j$NCPUS
make -j$NCPUS -C bin

rsync -av --include="pddl-*" --exclude="*.o" $ROOT/bin/ $HOME_ROOT/bin/
rm -rf $SCRATCHDIR/*
