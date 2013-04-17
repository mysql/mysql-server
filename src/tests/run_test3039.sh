#!/bin/bash

set -e

test $# -ge 1

bin=$1; shift

$bin -n 1000
$bin
