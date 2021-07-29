#!/bin/sh
# A simple script to excercise the sample dataset star2002.
if [ $# -gt 0 ]; then BIN="$1"; else BIN="../examples"; fi;
if [ ! -x ${BIN}/ibis ]; then
    echo $0 failed to locate command line tool ibis in ${BIN};
    exit -1;
fi
if [ $# -gt 1 ]; then
    DATADIR=$2;
else
    DATADIR="http://sdm.lbl.gov/fastbit/data";
fi
if [ ! -f star2002-1.csv ]; then
  if [ ! -f star2002-1.csv.gz ]; then
    wget ${DATADIR}/star2002-1.csv.gz
    if [ ! -f star2002-1.csv.gz ]; then
      echo $0 failed to retrieve star2002-1.csv.gz;
      exit -1;
    fi
  fi
  gunzip -v star2002-1.csv.gz
fi
#
if [ ! -f star2002-2.csv ]; then
  if [ ! -f star2002-2.csv.gz ]; then
    wget ${DATADIR}/star2002-2.csv.gz
    if [ ! -f star2002-2.csv.gz ]; then
      echo $0 failed to retrieve star2002-2.csv.gz;
      exit -1;
    fi
  fi
  gunzip -v star2002-2.csv.gz
fi
#
if [ ! -f star2002-3.csv ]; then
  if [ ! -f star2002-3.csv.gz ]; then
    wget ${DATADIR}/star2002-3.csv.gz
    if [ ! -f star2002-3.csv.gz ]; then
      echo $0 failed to retrieve star2002-3.csv.gz;
      exit -1;
    fi
  fi
  gunzip -v star2002-3.csv.gz
fi
${BIN}/ardea -d star2002 -m "antiNucleus:int, eventFile:int, eventNumber:int, eventTime:double, histFile:int, multiplicity:int, NaboveLb:int, NbelowLb:int, NLb:int, primaryTracks:int, prodTime:double, Pt:float, runNumber:int, vertexX:float, vertexY:float, vertexZ:float" -t star2002-1.csv -t star2002-2.csv -t star2002-3.csv -v
${BIN}/ibis -d star2002 -t 2 -v 2
echo "\n$0 ... DONE"
#
