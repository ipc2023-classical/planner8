#! /usr/bin/env bash

echo "Building image for Scorpion Maidu - Satisficing Track"
apptainer build maidu-sat.sif Apptainer.maidu_sat

echo "Building image for Scorpion Maidu - Agile Track"
apptainer build maidu-agl.sif Apptainer.maidu_agl

echo "Building image for Powerlifted - Satisficing Track"
apptainer build powerlifted-sat.sif Apptainer.powerlifted_sat

echo "Building image for Powerlifted - Agile Track"
apptainer build powerlifted-agl.sif Apptainer.powerlifted_agl

echo "Building image for Levitron - Satisficing Track"
apptainer build levitron-sat.sif Apptainer.levitron_sat

echo "Building image for Levitron - Agile Track"
apptainer build levitron-agl.sif Apptainer.levitron_agl
