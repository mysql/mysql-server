#!/usr/bin/env bash

if [[ $# -ne 1 ]]; then exit 1; fi

bin=$1; shift

set -e

$bin -a -thread_stack 16384
$bin -a -thread_stack 16384 -resume
