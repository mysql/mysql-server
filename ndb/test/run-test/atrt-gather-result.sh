#!/bin/sh

set -e

mkdir -p result
cd result
rm -rf *

while [ $# -gt 0 ]
do
  rsync -a "$1" .
  shift
done



