#!/bin/sh
# Copyright (C) 2004 MySQL AB
# For a more info consult the file COPYRIGHT distributed with this file

# This scripts stops the table handler ndbcluster

#BASEDIR is always one above mysql-test directory
CWD=`pwd`
cd ..
BASEDIR=`pwd`
cd $CWD

# Are we using a source or a binary distribution?
if [ -d ../sql ] ; then
   SOURCE_DIST=1
   ndbtop=$BASEDIR/ndb
   exec_ndb=$ndbtop/src/kernel/ndb-main/ndb
   exec_mgmtsrvr=$ndbtop/src/mgmsrv/mgmtsrvr
   exec_waiter=$ndbtop/tools/ndb_waiter
   exec_mgmtclient=$ndbtop/src/mgmclient/mgmtclient
else
   BINARY_DIST=1
   if test -x "$BASEDIR/libexec/ndb"
   then
     exec_ndb=$BASEDIR/libexec/ndb
     exec_mgmtsrvr=$BASEDIR/libexec/mgmtsrvr
   else
     exec_ndb=$BASEDIR/bin/ndb
     exec_mgmtsrvr=$BASEDIR/bin/mgmtsrvr
   fi
   exec_waiter=$BASEDIR/bin/ndb_waiter
   exec_mgmtclient=$BASEDIR/bin/mgmtclient
fi

pidfile=ndbcluster.pid
cfgfile=Ndb.cfg

while test $# -gt 0; do
  case "$1" in
    --port-base=*)
     port_base=`echo "$1" | sed -e "s;--port-base=;;"`
     ;;
    -- )  shift; break ;;
    --* ) $ECHO "Unrecognized option: $1"; exit 1 ;;
    * ) break ;;
  esac
  shift
done

stop_default_ndbcluster() {

#if [ ! -f $pidfile ] ; then
#  exit 0
#fi

if [ ! -f $cfgfile ] ; then
  echo "$cfgfile missing"
  exit 1
fi

ndb_host=`cat $cfgfile | sed -e "s,.*host=\(.*\)\:.*,\1,1"`
ndb_port=`cat $cfgfile | sed -e "s,.*host=$ndb_host\:\([0-9]*\).*,\1,1"`

# Start management client

exec_mgmtclient="$exec_mgmtclient --try-reconnect=1 $ndb_host $ndb_port"

echo "$exec_mgmtclient"
echo "all stop" | $exec_mgmtclient

sleep 5

if [ -f $pidfile ] ; then
  kill `cat $pidfile`
  rm $pidfile
fi

}

stop_default_ndbcluster

exit 0
