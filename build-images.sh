#! /usr/bin/env bash

echo "Building image for Scorpion Novelty - Satisficing Track"
apptainer build scorpion-novelty-sat.sif Apptainer.sn_sat

echo "Building image for Scorpion Novelty - Agile Track"
apptainer build scorpion-novelty-agl.sif Apptainer.sn_agl

echo "Building image for Powerlifted - Satisficing Track"
apptainer build powerlifted-sat.sif Apptainer.powerlifted_sat

echo "Building image for Powerlifted - Agile Track"
apptainer build powerlifted-agl.sif Apptainer.powerlifted_agl

echo "Building image for Levitron - Satisficing Track"
apptainer build levitron-sat.sif Apptainer.levitron_sat

echo "Building image for Levitron - Agile Track"
apptainer build levitron-agl.sif Apptainer.levitron_agl
