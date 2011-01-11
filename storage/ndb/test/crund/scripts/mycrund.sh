#!/bin/bash

#for f in run.ndbapi.opt run.ndbjtie.opt run.clusterj.opt run.mysql.opt run.openjpa.mysql.opt run.openjpa.clusterj.opt ; do
#for f in run.ndbapi.opt run.ndbjtie.opt run.clusterj.opt run.mysql.opt ; do
#for f in run.ndbjtie.opt ; do
#for f in run.ndbapi.opt ; do
for f in run.clusterj.opt ; do
  echo testing $f
  ./mycrundjava.sh $f
  mv -v results/xxx  results/xxx_$f
done
