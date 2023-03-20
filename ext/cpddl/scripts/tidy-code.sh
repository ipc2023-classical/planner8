#!/bin/bash

if [ "$1" = "" ]; then
    echo "Usage: $0 filename"
fi

IN="$1"

cat "$IN" \
    | awk '/^ *}$/{prev = $0; if (getline == 1){
                    if ($0 ~ /^ *else/) printf "%s", prev; else print prev;}}
           {print}' >tmp.tidy-code && mv tmp.tidy-code "$IN"
sed -i 's/} *else *{/}else{/' "$IN"
sed -i 's/} *else if/}else if/' "$IN"
sed -i 's/) *{$/){/' "$IN"
sed -i 's/ if(/ if (/' "$IN"
