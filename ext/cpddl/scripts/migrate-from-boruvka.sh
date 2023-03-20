#!/bin/bash

if [ -f third-party/boruvka/boruvka/${1}.h ]; then
cat third-party/boruvka/boruvka/${1}.h \
        | sed 's/BOR/PDDL/g' \
        | sed 's/bor\([A-Z]\)/pddl\1/g' \
        | sed 's/bor_/pddl_/g' \
        | sed 's/PDDL_ALLOC/ALLOC/g' \
        | sed 's/PDDL_FREE/FREE/g' \
        | sed 's/PDDL_MALLOC/MALLOC/g' \
        | sed 's/PDDL_CALLOC/CALLOC/g' \
        | sed 's/PDDL_REALLOC/REALLOC/g' \
        | sed 's/Boruvka/cpddl/g' \
        | sed 's,boruvka/alloc.h,alloc.h,g' \
        | sed 's,boruvka/,pddl/,g' \
            >pddl/${1}.h
fi

if [ -f third-party/boruvka/src/${1}.c ]; then
cat third-party/boruvka/src/${1}.c \
        | sed 's/BOR/PDDL/g' \
        | sed 's/bor\([A-Z]\)/pddl\1/g' \
        | sed 's/bor_/pddl_/g' \
        | sed 's/PDDL_ALLOC/ALLOC/g' \
        | sed 's/PDDL_FREE/FREE/g' \
        | sed 's/PDDL_MALLOC/MALLOC/g' \
        | sed 's/PDDL_CALLOC/CALLOC/g' \
        | sed 's/PDDL_REALLOC/REALLOC/g' \
        | sed 's/Boruvka/cpddl/g' \
        | sed 's,boruvka/alloc.h,alloc.h,g' \
        | sed 's,boruvka/,pddl/,g' \
            >src/${1}.c
fi

if [ -f third-party/boruvka/src/${1}.h ]; then
cat third-party/boruvka/src/${1}.h \
        | sed 's/BOR/PDDL/g' \
        | sed 's/bor\([A-Z]\)/pddl\1/g' \
        | sed 's/bor_/pddl_/g' \
        | sed 's/PDDL_ALLOC/ALLOC/g' \
        | sed 's/PDDL_FREE/FREE/g' \
        | sed 's/PDDL_MALLOC/MALLOC/g' \
        | sed 's/PDDL_CALLOC/CALLOC/g' \
        | sed 's/PDDL_REALLOC/REALLOC/g' \
        | sed 's/Boruvka/cpddl/g' \
        | sed 's,boruvka/alloc.h,alloc.h,g' \
        | sed 's,boruvka/,pddl/,g' \
            >src/${1}.h
fi
