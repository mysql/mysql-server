#!/bin/sh
# Copyright (C) 2004 MySQL AB
# For a more info consult the file COPYRIGHT distributed with this file

# This scripts starts the table handler ndbcluster

# configurable parameters, make sure to change in mysqlcluterd as well
port_base="2200"
fsdir=`pwd`
# end configurable parameters

#BASEDIR is always one above mysql-test directory
CWD=`pwd`
cd ..
BASEDIR=`pwd`
cd $CWD

# Are we using a source or a binary distribution?
if [ -d ../sql ] ; then
   SOURCE_DIST=1
   ndbtop=$BASEDIR/ndb
   exec_ndb=$ndbtop/src/kernel/ndbd
   exec_mgmtsrvr=$ndbtop/src/mgmsrv/ndb_mgmd
   exec_waiter=$ndbtop/tools/ndb_waiter
   exec_mgmtclient=$ndbtop/src/mgmclient/ndb_mgm
else
   BINARY_DIST=1
   if test -x "$BASEDIR/libexec/ndbd"
   then
     exec_ndb=$BASEDIR/libexec/ndbd
     exec_mgmtsrvr=$BASEDIR/libexec/ndb_mgmd
   else
     exec_ndb=$BASEDIR/bin/ndbd
     exec_mgmtsrvr=$BASEDIR/bin/ndb_mgmd
   fi
   exec_waiter=$BASEDIR/bin/ndb_waiter
   exec_mgmtclient=$BASEDIR/bin/ndb_mgm
fi

pidfile=ndbcluster.pid
cfgfile=Ndb.cfg
stop_ndb=
initial_ndb=
status_ndb=
ndb_discless=0

ndb_con_op=100000
ndb_dmem=80M
ndb_imem=24M

while test $# -gt 0; do
  case "$1" in
    --stop)
     stop_ndb=1
     ;;
    --initial)
     flags_ndb=$flags_ndb" -i"
     initial_ndb=1
     ;;
    --status)
     status_ndb=1
     ;;
    --small)
     ndb_con_op=10000
     ndb_dmem=40M
     ndb_imem=12M
     ;;
    --discless)
     ndb_discless=1
     ;;
    --data-dir=*)
     fsdir=`echo "$1" | sed -e "s;--data-dir=;;"`
     ;;
    --port-base=*)
     port_base=`echo "$1" | sed -e "s;--port-base=;;"`
     ;;
    -- )  shift; break ;;
    --* ) $ECHO "Unrecognized option: $1"; exit 1 ;;
    * ) break ;;
  esac
  shift
done

fs_ndb=$fsdir/ndbcluster
fs_mgm_1=$fs_ndb/1.ndb_mgm
fs_ndb_2=$fs_ndb/2.ndb_db
fs_ndb_3=$fs_ndb/3.ndb_db
fs_name_2=$fs_ndb/node-2-fs-$port_base
fs_name_3=$fs_ndb/node-3-fs-$port_base

NDB_HOME=
export NDB_CONNECTSTRING
if [ ! -x $fsdir ]; then
  echo "$fsdir missing"
  exit 1
fi
if [ ! -x $exec_ndb ]; then
  echo "$exec_ndb missing"
  exit 1
fi
if [ ! -x $exec_mgmtsrvr ]; then
  echo "$exec_mgmtsrvr missing"
  exit 1
fi

start_default_ndbcluster() {

# do some checks

NDB_CONNECTSTRING=

if [ $initial_ndb ] ; then
  [ -d $fs_ndb ] || mkdir $fs_ndb
  [ -d $fs_mgm_1 ] || mkdir $fs_mgm_1
  [ -d $fs_ndb_2 ] || mkdir $fs_ndb_2
  [ -d $fs_ndb_3 ] || mkdir $fs_ndb_3
  [ -d $fs_name_2 ] || mkdir $fs_name_2
  [ -d $fs_name_3 ] || mkdir $fs_name_3
fi
if [ -d "$fs_ndb" -a -d "$fs_mgm_1" -a -d "$fs_ndb_2" -a -d "$fs_ndb_3" -a -d "$fs_name_2" -a -d "$fs_name_3" ]; then :; else
  echo "$fs_ndb filesystem directory does not exist"
  exit 1
fi

# set som help variables

ndb_host="localhost"
ndb_mgmd_port=$port_base
port_transporter=`expr $ndb_mgmd_port + 2`
NDB_CONNECTSTRING_BASE="host=$ndb_host:$ndb_mgmd_port;nodeid="


# Start management server as deamon

NDB_ID="1"
NDB_CONNECTSTRING=$NDB_CONNECTSTRING_BASE$NDB_ID

# Edit file system path and ports in config file

if [ $initial_ndb ] ; then
sed \
    -e s,"CHOOSE_MaxNoOfConcurrentOperations",$ndb_con_op,g \
    -e s,"CHOOSE_DataMemory",$ndb_dmem,g \
    -e s,"CHOOSE_IndexMemory",$ndb_imem,g \
    -e s,"CHOOSE_Discless",$ndb_discless,g \
    -e s,"CHOOSE_HOSTNAME_".*,"$ndb_host",g \
    -e s,"CHOOSE_FILESYSTEM_NODE_2","$fs_name_2",g \
    -e s,"CHOOSE_FILESYSTEM_NODE_3","$fs_name_3",g \
    -e s,"CHOOSE_PORT_MGM",$ndb_mgmd_port,g \
    -e s,"CHOOSE_PORT_TRANSPORTER",$port_transporter,g \
    < ndb/ndb_config_2_node.ini \
    > "$fs_mgm_1/config.ini"
fi

if ( cd $fs_mgm_1 ; echo $NDB_CONNECTSTRING > $cfgfile ; $exec_mgmtsrvr -d -c config.ini ) ; then :; else
  echo "Unable to start $exec_mgmtsrvr from `pwd`"
  exit 1
fi

cat `find $fs_ndb -name 'node*.pid'` > $pidfile

# Start database node 

NDB_ID="2"
NDB_CONNECTSTRING=$NDB_CONNECTSTRING_BASE$NDB_ID
echo "Starting ndbd connectstring=\""$NDB_CONNECTSTRING\"
( cd $fs_ndb_2 ; echo $NDB_CONNECTSTRING > $cfgfile ; $exec_ndb -d $flags_ndb & )

cat `find $fs_ndb -name 'node*.pid'` > $pidfile

# Start database node 

NDB_ID="3"
NDB_CONNECTSTRING=$NDB_CONNECTSTRING_BASE$NDB_ID
echo "Starting ndbd connectstring=\""$NDB_CONNECTSTRING\"
( cd $fs_ndb_3 ; echo $NDB_CONNECTSTRING > $cfgfile ; $exec_ndb -d $flags_ndb & )

cat `find $fs_ndb -name 'node*.pid'` > $pidfile

# test if Ndb Cluster starts properly

echo "Waiting for started..."
NDB_ID="11"
NDB_CONNECTSTRING=$NDB_CONNECTSTRING_BASE$NDB_ID
if ( $exec_waiter ) | grep "NDBT_ProgramExit: 0 - OK"; then :; else
  echo "Ndbcluster startup failed"
  exit 1
fi

echo $NDB_CONNECTSTRING > $cfgfile

cat `find $fs_ndb -name 'node*.pid'` > $pidfile

status_ndbcluster
}

status_ndbcluster() {
# Start management client

echo "show" | $exec_mgmtclient $ndb_host $ndb_mgmd_port
}

stop_default_ndbcluster() {

#if [ ! -f $pidfile ] ; then
#  exit 0
#fi

if [ ! -f $cfgfile ] ; then
  echo "$cfgfile missing"
  exit 1
fi

ndb_host=`cat $cfgfile | sed -e "s,.*host=\(.*\)\:.*,\1,1"`
ndb_mgmd_port=`cat $cfgfile | sed -e "s,.*host=$ndb_host\:\([0-9]*\).*,\1,1"`

# Start management client

exec_mgmtclient="$exec_mgmtclient --try-reconnect=1 $ndb_host $ndb_mgmd_port"

echo "$exec_mgmtclient"
echo "all stop" | $exec_mgmtclient

sleep 5

if [ -f $pidfile ] ; then
  kill `cat $pidfile` 2> /dev/null
  rm $pidfile
fi

}

if [ $status_ndb ] ; then
  status_ndbcluster
  exit 0
fi

if [ $stop_ndb ] ; then
  stop_default_ndbcluster
else
  start_default_ndbcluster
fi

exit 0
