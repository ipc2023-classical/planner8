#! /bin/bash

set -euo pipefail

PROPERTIES=${1}
TRACK=${2}
PORTFOLIO_TIME=${3}

for granularity in 10 15 20 30 40 50 60 70 80 90 100 \
    110 120 130 140 150 160 170 180 190 200 220 240 260 280 300 \
    330 360 390 420 450 500 550 600 900; do
    echo -n "Granularity: ${granularity}s    "
    ./stonesoup.py --track ${TRACK} --portfolio-time ${PORTFOLIO_TIME} ${PROPERTIES} $granularity | grep Final
done
