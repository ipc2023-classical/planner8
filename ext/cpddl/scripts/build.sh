#!/bin/bash

set -e

make mrproper
make third-party
make
make -C bin
