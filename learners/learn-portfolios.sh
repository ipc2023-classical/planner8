#! /bin/bash

set -euo pipefail

POWERLIFTED_DATA=../experiments/ipc2023/data/01-powerlifted-eval/properties-hardest.json.xz

#./batch-stonesoup.sh ${POWERLIFTED_DATA} sat 1800 | tee batch-stonesoup-sat.txt
./stonesoup.py --track sat ${POWERLIFTED_DATA} 40 | tee stonesoup-sat.txt
#./greedy.py ${POWERLIFTED_DATA} --track sat | tee greedy-sat.txt
