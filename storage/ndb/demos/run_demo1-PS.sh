#!/bin/sh
if [ -z "$MYSQLCLUSTER_TOP" ]; then
  echo "MYSQLCLUSTER_TOP not set"
  exit 1
fi
if [ -d "$MYSQLCLUSTER_TOP/ndb" ]; then :; else
  echo "$MYSQLCLUSTER_TOP/ndb directory does not exist"
  exit 1
fi
NDB_CONNECTSTRING=
NDB_HOME=
NDB_DEMO=$MYSQLCLUSTER_TOP/ndb/demos/1-node-PS

NDB_PORT_BASE="102"
NDB_REP_ID="5"
NDB_EXTREP_ID="4"

NDB_DEMO_NAME="Demo 1-PS MySQL Cluster"
NDB_HOST1=$1
NDB_HOST2=$2
if [ -z "$NDB_HOST1" ]; then
  NDB_HOST1=localhost
fi
if [ -z "$NDB_HOST2" ]; then
  NDB_HOST2=localhost
fi
NDB_HOST=$NDB_HOST1
NDB_EXTHOST=$NDB_HOST2

source $MYSQLCLUSTER_TOP/ndb/demos/run_demo1-PS-SS_common.sh
