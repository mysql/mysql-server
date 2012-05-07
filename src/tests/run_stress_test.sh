#!/bin/bash

if [[ $# -lt 3 ]]; then exit 1; fi

bin=$1; shift
size=$1; shift
time=$1; shift

$bin --only_create --num_elements $size
$bin --only_stress --num_elements $size --num_seconds $time
