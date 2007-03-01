#!/bin/sh

set -e

mkdir -p result
cd result
rm -rf *

while [ $# -gt 0 ]
do
  rsync -a --exclude='BACKUP' --exclude='ndb_*_fs' "$1" .
  shift
done



