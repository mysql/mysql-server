#!/bin/sh
# Copyright (C) 2004 MySQL AB
# For a more info consult the file COPYRIGHT distributed with this file

# This scripts starts the table handler ndbcluster

# configurable parameters, make sure to change in mysqlcluterd as well
port=@ndb_port@
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
   ndbtop=$BASEDIR/storage/ndb
   exec_ndb=$ndbtop/src/kernel/ndbd
   exec_mgmtsrvr=$ndbtop/src/mgmsrv/ndb_mgmd
   exec_waiter=$ndbtop/tools/ndb_waiter
   exec_test=$ndbtop/tools/ndb_test_platform
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
   exec_test=$BASEDIR/bin/ndb_test_platform
   exec_mgmtclient=$BASEDIR/bin/ndb_mgm
fi

if $exec_test ; then :; else
  echo "ndb not correctly compiled to support this platform"
  exit 1
fi

pidfile=ndbcluster.pid
cfgfile=Ndb.cfg
test_ndb=
stop_ndb=
initial_ndb=
status_ndb=
ndb_diskless=0

ndb_no_ord=512
ndb_con_op=105000
ndb_dmem=80M
ndb_imem=24M

NDB_MGM_EXTRA_OPTS=
NDB_MGMD_EXTRA_OPTS=
NDBD_EXTRA_OPTS=

while test $# -gt 0; do
  case "$1" in
    --test)
     test_ndb=1
     ;;
    --stop)
     stop_ndb=1
     ;;
    --initial)
     flags_ndb="$flags_ndb --initial"
     initial_ndb=1
     ;;
    --debug*)
     flags_ndb="$flags_ndb $1"
     ;;
    --status)
     status_ndb=1
     ;;
    --small)
     ndb_no_ord=128
     ndb_con_op=10000
     ndb_dmem=40M
     ndb_imem=12M
     ;;
    --diskless)
     ndb_diskless=1
     ;;
    --data-dir=*)
     fsdir=`echo "$1" | sed -e "s;--data-dir=;;"`
     ;;
    --port=*)
     port=`echo "$1" | sed -e "s;--port=;;"`
     ;;
    --port-base=*)
     $ECHO "--port-base option depricated. Ignored."
     ;;
    --ndb_mgm-extra-opts=*)
     NDB_MGM_EXTRA_OPTS=`echo "$1" | sed -e "s;--ndb_mgm-extra-opts=;;"`
     ;;
    --ndb_mgmd-extra-opts=*)
     NDB_MGMD_EXTRA_OPTS=`echo "$1" | sed -e "s;--ndb_mgmd-extra-opts=;;"`
     ;;
    --ndbd-extra-opts=*)
     NDBD_EXTRA_OPTS=`echo "$1" | sed -e "s;--ndbd-extra-opts=;;"`
     ;;
    -- )  shift; break ;;
    --* ) $ECHO "Unrecognized option: $1"; exit 1 ;;
    * ) break ;;
  esac
  shift
done

fs_ndb="$fsdir/ndbcluster-$port"

NDB_HOME=
if [ ! -x "$fsdir" ]; then
  echo "$fsdir missing"
  exit 1
fi
if [ ! -x "$exec_ndb" ]; then
  echo "$exec_ndb missing"
  exit 1
fi
if [ ! -x "$exec_mgmtsrvr" ]; then
  echo "$exec_mgmtsrvr missing"
  exit 1
fi
if [ ! -x "$exec_waiter" ]; then
  echo "$exec_waiter missing"
  exit 1
fi

exec_mgmtclient="$exec_mgmtclient --no-defaults $NDB_MGM_EXTRA_OPTS"
exec_mgmtsrvr="$exec_mgmtsrvr --no-defaults $NDB_MGMD_EXTRA_OPTS"
exec_ndb="$exec_ndb --no-defaults $NDBD_EXTRA_OPTS"
exec_waiter="$exec_waiter --no-defaults"

ndb_host="localhost"
ndb_mgmd_port=$port
NDB_CONNECTSTRING="host=$ndb_host:$ndb_mgmd_port"
export NDB_CONNECTSTRING

sleep_until_file_created () {
  file=$1
  loop=$2
  org_time=$2
  message=$3
  while (test $loop -gt 0)
  do
    if [ -r $file ]
    then
      return 0
    fi
    sleep 1
    loop=`expr $loop - 1`
  done
  if [ $message ]
  then
    echo $message
  fi
  echo "ERROR: $file was not created in $org_time seconds;  Aborting"
  return 1;
}

start_default_ndbcluster() {

# do some checks

if [ "$initial_ndb" ] ; then
  [ -d "$fs_ndb" ] || mkdir "$fs_ndb"
fi
if [ -d "$fs_ndb" ]; then :; else
  echo "$fs_ndb filesystem directory does not exist"
  exit 1
fi

# Start management server as deamon

# Edit file system path and ports in config file
if [ $initial_ndb ] ; then
  rm -f $fs_ndb/ndb_* 2>&1 | cat > /dev/null
  sed \
    -e s,"CHOOSE_MaxNoOfOrderedIndexes","$ndb_no_ord",g \
    -e s,"CHOOSE_MaxNoOfConcurrentOperations","$ndb_con_op",g \
    -e s,"CHOOSE_DataMemory","$ndb_dmem",g \
    -e s,"CHOOSE_IndexMemory","$ndb_imem",g \
    -e s,"CHOOSE_Diskless","$ndb_diskless",g \
    -e s,"CHOOSE_HOSTNAME_".*,"$ndb_host",g \
    -e s,"CHOOSE_FILESYSTEM","$fs_ndb",g \
    -e s,"CHOOSE_PORT_MGM","$ndb_mgmd_port",g \
    < ndb/ndb_config_2_node.ini \
    > "$fs_ndb/config.ini"
fi

rm -f "$cfgfile" 2>&1 | cat > /dev/null
rm -f "$fs_ndb/$cfgfile" 2>&1 | cat > /dev/null

if ( cd "$fs_ndb" ; $exec_mgmtsrvr -f config.ini ) ; then :; else
  echo "Unable to start $exec_mgmtsrvr from `pwd`"
  exit 1
fi
if sleep_until_file_created $fs_ndb/ndb_3.pid 120
then :; else
  exit 1
fi
cat `find "$fs_ndb" -name 'ndb_*.pid'` > "$fs_ndb/$pidfile"

# Start database node 

echo "Starting ndbd"
( cd "$fs_ndb" ; $exec_ndb $flags_ndb & )
if sleep_until_file_created $fs_ndb/ndb_1.pid 120
then :; else
  stop_default_ndbcluster
  exit 1
fi
cat `find "$fs_ndb" -name 'ndb_*.pid'` > "$fs_ndb/$pidfile"

# Start database node 

echo "Starting ndbd"
( cd "$fs_ndb" ; $exec_ndb $flags_ndb & )
if sleep_until_file_created $fs_ndb/ndb_2.pid 120
then :; else
  stop_default_ndbcluster
  exit 1
fi
cat `find "$fs_ndb" -name 'ndb_*.pid'` > "$fs_ndb/$pidfile"

# test if Ndb Cluster starts properly

echo "Waiting for started..."
if ( $exec_waiter ) | grep "NDBT_ProgramExit: 0 - OK"; then :; else
  echo "Ndbcluster startup failed"
  stop_default_ndbcluster
  exit 1
fi

cat `find "$fs_ndb" -name 'ndb_*.pid'` > $fs_ndb/$pidfile

status_ndbcluster
}

status_ndbcluster() {
  # Start management client
  $exec_mgmtclient -e show
}

stop_default_ndbcluster() {

# Start management client

exec_mgmtclient="$exec_mgmtclient --try-reconnect=1"

$exec_mgmtclient -e shutdown 2>&1 | cat > /dev/null

if [ -f "$fs_ndb/$pidfile" ] ; then
  kill_pids=`cat "$fs_ndb/$pidfile"`
  attempt=0
  while [ $attempt -lt 10 ] ; do
    new_kill_pid=""
    kill_pids2=""
    for p in $kill_pids ; do
      kill -0 $p 2> /dev/null
      if [ $? -eq 0 ] ; then
        new_kill_pid="$p $new_kill_pid"
        kill_pids2="-$p $kill_pids2"
      fi
    done
    kill_pids=$new_kill_pid
    if [ -z "$kill_pids" ] ; then
      break
    fi
    sleep 1
    attempt=`expr $attempt + 1`
  done
  if [ "$kill_pids2" != "" ] ; then
    echo "Failed to shutdown ndbcluster, executing kill "$kill_pids2
    kill          -9 -- $kill_pids2 2> /dev/null
    /bin/kill     -9 -- $kill_pids2 2> /dev/null
    /usr/bin/kill -9 -- $kill_pids2 2> /dev/null
    kill          -9    $kill_pids2 2> /dev/null
    /bin/kill     -9    $kill_pids2 2> /dev/null
    /usr/bin/kill -9    $kill_pids2 2> /dev/null
  fi
  rm "$fs_ndb/$pidfile"
fi
}

initialize_ndb_test ()
{
  fs_result=$fs_ndb/r
  rm -rf $fs_result
  mkdir $fs_result
  echo ------------------
  echo starting ndb tests
  echo ------------------
}

do_ndb_test ()
{
  test_name=$1

  clusterlog=$fs_ndb/ndb_3_cluster.log

  test_log_result=$fs_result/${test_name}_log.result
  test_log_reject=$fs_result/${test_name}_log.reject
  test_result=$fs_result/${test_name}.result
  test_reject=$fs_result/${test_name}.reject

  clean_log='s/.*\[MgmSrvr\]//'

  cat $clusterlog ndb/${test_name}_log.result | sed -e $clean_log > $test_log_result

  cp ndb/${test_name}.result $test_result

  cat ndb/${test_name}.test | $exec_mgmtclient > $test_reject
  cat $clusterlog | sed -e $clean_log > $test_log_reject

  t="pass"
  diff -C 5 $test_result $test_reject || t="fail"
  printf "ndb_mgm output    %20s [%s]\n" $test_name $t
  t="pass"
  diff -C 5 $test_log_result $test_log_reject || t="fail"
  printf "clusterlog output %20s [%s]\n" $test_name $t
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

if [ $test_ndb ] ; then
  initialize_ndb_test
  all_tests=`ls ndb/*.test | sed "s#ndb/##" | sed "s#.test##"`  
  for a in $all_tests ; do
    do_ndb_test $a
  done
  echo ------------------
  echo shutting down cluster
  stop_default_ndbcluster
fi

exit 0
