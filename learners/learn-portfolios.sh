#! /bin/bash

set -euo pipefail

POWERLIFTED_DATA=../experiments/ipc2023/data/01-powerlifted-eval/properties-hardest.json.xz
MAIDU_DATA=../experiments/ipc2023/data/02-maidu-eval/properties-hardest.json.xz

#./batch-stonesoup.sh ${POWERLIFTED_DATA} sat 1800 | tee batch-stonesoup-powerlifted-sat.txt
./stonesoup.py --track sat ${POWERLIFTED_DATA} 40 | tee stonesoup-powerlifted-sat.txt

#./batch-stonesoup.sh ${MAIDU_DATA} sat 1800 | tee batch-stonesoup-maidu-sat.txt
./stonesoup.py --track sat ${MAIDU_DATA} 40 | tee stonesoup-maidu-sat.txt

./greedy.py --portfolio-time 300 --track agl ${POWERLIFTED_DATA} | tee greedy-powerlifted-agl.txt
./greedy.py --portfolio-time 300 --track agl ${MAIDU_DATA} | tee greedy-maidu-agl.txt
