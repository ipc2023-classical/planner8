#!/bin/bash

MODULE="$1"
MODULE_CAP="$2"
TYPE="$3"
PREFIX_STRUCT="$4"
PREFIX_FUNC="$5"
PREFIX_MACRO="$6"
if [ "$PREFIX_FUNC" = "" ]; then
    PREFIX_FUNC=$(echo "$PREFIX" | tr '[:lower:]' '[:upper:]')
fi
if [ "$PREFIX_MACRO" = "" ]; then
    PREFIX_MACRO=$(echo "$PREFIX" | tr '[:lower:]' '[:upper:]')
fi

MODULE_UP=$(echo "$MODULE" | tr '[:lower:]' '[:upper:]')

sed "s/TYPE/${TYPE}/g" \
    | sed "s/pddl_${MODULE}/pddl_${PREFIX_STRUCT}${MODULE}/g" \
    | sed "s/pddl${MODULE_CAP}/pddl${PREFIX_FUNC}${MODULE_CAP}/g" \
    | sed "s/PDDL_${MODULE_UP}/PDDL_${PREFIX_MACRO}${MODULE_UP}/g" \
    | sed "s/pddl\/_${MODULE}\.h/pddl\/${PREFIX_STRUCT}${MODULE}.h/g" \
    | sed "s/pddl\/${MODULE}\.h/pddl\/${PREFIX_STRUCT}${MODULE}.h/g"
