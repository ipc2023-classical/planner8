#! /usr/bin/env bash

echo "Building image for Scorpion Novelty - Satisficing Track"
apptainer build scorpion-novelty-sat.img Apptainer.sn_sat

echo "Building image for Scorpion Novelty - Agile Track"
apptainer build scorpion-novelty-agl.img Apptainer.sn_agl

echo "Building image for Levitron - Satisficing Track"
apptainer build levitron-sat.img Apptainer.levitron_sat

echo "Building image for Levitron - Agile Track"
apptainer build levitron-agl.img Apptainer.levitron_agl
