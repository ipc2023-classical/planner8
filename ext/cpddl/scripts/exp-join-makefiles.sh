#!/bin/bash

# Script for joining makefiles from different directories generated by exp

cat $@ | grep '^TASK'

echo
echo 'all: $(TASK)'
echo

cat $@ | grep -v '^TASK' | grep -v '^all:'
echo