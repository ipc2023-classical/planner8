#!/bin/bash

FORCE=0
if [ "$1" = "-f" ] || [ "$1" = "--force" ]; then
    FORCE=1
    shift
fi

while [ "$1" != "" ]; do
    if [ "$FORCE" = "1" ] && [ -f "$1/submitted" ]; then
        rm $1/submitted
        echo "Resubmitting $1"
    else
        echo "Submitting $1"
    fi

    bash $1/submit.sh

    shift
done

