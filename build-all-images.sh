#! /bin/bash

set -euo pipefail

cd $(dirname "$0")

OUTDIR=images/
mkdir -p ${OUTDIR}

for recipe in Apptainer.* ; do
    name="${recipe##*.}"
    ./build-image.sh ${recipe} ${OUTDIR}/${name}.img
done

echo "Finished building images"
