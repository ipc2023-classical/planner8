#!/bin/bash

if [ "$1" = "" ]; then
    echo "Error: Path to libcplex.a must be specified"
    echo "Usage: $0 /path/to/libcplex.a targetlib.a"
    exit -1
fi
if [ ! -f "$1" ]; then
    echo "Error: Could not find '$1'"
    echo "Usage: $0 /path/to/libcplex.a targetlib.a"
    exit -1
fi

if [ "$2" = "" ]; then
    echo "Error: Target file must be specified"
    echo "Usage: $0 /path/to/libcplex.a targetlib.a"
    exit -1
fi

if [ -f "$2" ]; then
    echo "Error: File '$2' already exists"
    echo "Usage: $0 /path/to/libcplex.a targetlib.a"
    exit -1
fi

TMPFILE=.tmp.sanitize-libcplex
nm $1 | grep 'sqlite3_' | cut -f3 -d' ' | sort | uniq >$TMPFILE
objcopy --localize-symbols $TMPFILE $1 $2
rm -f $TMPFILE
