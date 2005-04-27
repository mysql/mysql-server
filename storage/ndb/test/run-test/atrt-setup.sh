#!/bin/sh

set -e

ssh $1 mkdir -p $3
rsync -a --delete --force --ignore-errors $2 $1:$3
