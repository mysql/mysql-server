#!/bin/sh
# mysql-test-run - originally written by Matt Wagner <matt@mysql.com>
# modified by Sasha Pachev <sasha@mysql.com>
# Slightly updated by Monty
# Cleaned up again by Matt
# Fixed by Sergei
# :-)

#++
# Access Definitions
#--
DB=test
DBPASSWD=""
VERBOSE=""
USE_MANAGER=0
MY_TZ=GMT-3
TZ=$MY_TZ; export TZ # for UNIX_TIMESTAMP tests to work
LOCAL_SOCKET=@MYSQL_UNIX_ADDR@
MYSQL_TCP_PORT=@MYSQL_TCP_PORT@

# For query_cache test
case `uname` in
    SCO_SV | UnixWare | OpenUNIX )
        # do nothing (Causes strange behavior)
        ;;
    QNX)
        # do nothing (avoid error message)
        ;;
    * )
        ulimit -n 1024
        ;;
esac

#++
# Program Definitions
#--

LC_COLLATE=C
export LC_COLLATE
PATH=/bin:/usr/bin:/usr/local/bin:/usr/bsd:/usr/X11R6/bin:/usr/openwin/bin:/usr/bin/X11:$PATH
MASTER_40_ARGS="--rpl-recovery-rank=1 --init-rpl-role=master"

# Standard functions

which ()
{
  IFS="${IFS=   }"; save_ifs="$IFS"; IFS=':'
  for file
  do
    for dir in $PATH
    do
      if test -f $dir/$file
      then
        echo "$dir/$file"
        continue 2
      fi
    done
    echo "Fatal error: Cannot find program $file in $PATH" 1>&2
    exit 1
  done
  IFS="$save_ifs"
  exit 0
}


sleep_until_file_deleted ()
{
  pid=$1;
  file=$2
  loop=$SLEEP_TIME_FOR_DELETE
  while (test $loop -gt 0)
  do
    if [ ! -r $file ]
    then
      if test $pid != "0"
      then
        wait_for_pid $pid
      fi
      return
    fi
    sleep 1
    loop=`expr $loop - 1`
  done
}

sleep_until_file_created ()
{
  file=$1
  loop=$2
  org_time=$2
  while (test $loop -gt 0)
  do
    if [ -r $file ]
    then
      return 0
    fi
    sleep 1
    loop=`expr $loop - 1`
  done
  echo "ERROR: $file was not created in $org_time seconds;  Aborting"
  exit 1;
}

# For the future

wait_for_pid()
{
  pid=$1
  #$WAIT_PID pid $SLEEP_TIME_FOR_DELETE
}

# No paths below as we can't be sure where the program is!

SED=sed

BASENAME=`which basename`
if test $? != 0; then exit 1; fi
DIFF=`which diff | $SED q`
if test $? != 0; then exit 1; fi
CAT=cat
CUT=cut
HEAD=head
TAIL=tail
ECHO=echo # use internal echo if possible
EXPR=expr # use internal if possible
FIND=find
GREP=grep
if test $? != 0; then exit 1; fi
PRINTF=printf
RM=rm
if test $? != 0; then exit 1; fi
TR=tr
XARGS=`which xargs`
if test $? != 0; then exit 1; fi
SORT=sort

# Are we using a source or a binary distribution?

testdir=@testdir@
if [ -d bin/mysqld ] && [ -d mysql-test ] ; then
 cd mysql-test
else
 if [ -d $testdir/mysql-test ] ; then
   cd $testdir
 fi
fi

if [ ! -f ./mysql-test-run ] ; then
  $ECHO "Can't find the location for the mysql-test-run script"

  $ECHO "Go to to the mysql-test directory and execute the script as follows:"
  $ECHO "./mysql-test-run."
  exit 1
fi

#++
# Misc. Definitions
#--

if [ -d ../sql ] ; then
   SOURCE_DIST=1
else
   BINARY_DIST=1
fi

#BASEDIR is always one above mysql-test directory
CWD=`pwd`
cd ..
BASEDIR=`pwd`
cd $CWD
MYSQL_TEST_DIR=$BASEDIR/mysql-test
export MYSQL_TEST_DIR
STD_DATA=$MYSQL_TEST_DIR/std_data
hostname=`hostname`		# Installed in the mysql privilege table

MANAGER_QUIET_OPT="-q"
TESTDIR="$MYSQL_TEST_DIR/t"
TESTSUFFIX=test
TOT_SKIP=0
TOT_PASS=0
TOT_FAIL=0
TOT_TEST=0
USERT=0
SYST=0
REALT=0
FAST_START=""
MYSQL_TMP_DIR=$MYSQL_TEST_DIR/var/tmp
SLAVE_LOAD_TMPDIR=../../var/tmp #needs to be same length to test logging
RES_SPACE="      "
MYSQLD_SRC_DIRS="strings mysys include extra regex isam merge myisam \
 myisammrg heap sql"
MY_LOG_DIR="$MYSQL_TEST_DIR/var/log" 
#
# Set LD_LIBRARY_PATH if we are using shared libraries
#
LD_LIBRARY_PATH="$BASEDIR/lib:$BASEDIR/libmysql/.libs:$LD_LIBRARY_PATH"
DYLD_LIBRARY_PATH="$BASEDIR/lib:$BASEDIR/libmysql/.libs:$DYLD_LIBRARY_PATH"
export LD_LIBRARY_PATH DYLD_LIBRARY_PATH

MASTER_RUNNING=0
MASTER_MYPORT=9306
SLAVE_RUNNING=0
SLAVE_MYPORT=9307
MYSQL_MANAGER_PORT=9305 # needs to be out of the way of slaves
NDBCLUSTER_PORT=9350
MYSQL_MANAGER_PW_FILE=$MYSQL_TEST_DIR/var/tmp/manager.pwd
MYSQL_MANAGER_LOG=$MYSQL_TEST_DIR/var/log/manager.log
MYSQL_MANAGER_USER=root
NO_SLAVE=0
USER_TEST=

EXTRA_MASTER_OPT=""
EXTRA_MYSQL_TEST_OPT=""
EXTRA_MYSQLDUMP_OPT=""
EXTRA_MYSQLBINLOG_OPT=""
USE_RUNNING_SERVER=""
USE_NDBCLUSTER=""
USE_RUNNING_NDBCLUSTER=""
DO_GCOV=""
DO_GDB=""
MANUAL_GDB=""
DO_DDD=""
DO_CLIENT_GDB=""
SLEEP_TIME_AFTER_RESTART=1
SLEEP_TIME_FOR_DELETE=10
SLEEP_TIME_FOR_FIRST_MASTER=400		# Enough time to create innodb tables
SLEEP_TIME_FOR_SECOND_MASTER=30
SLEEP_TIME_FOR_FIRST_SLAVE=400
SLEEP_TIME_FOR_SECOND_SLAVE=30
CHARACTER_SET=latin1
DBUSER=""
START_WAIT_TIMEOUT=10
STOP_WAIT_TIMEOUT=10
MYSQL_TEST_SSL_OPTS=""
USE_EMBEDDED_SERVER=""
RESULT_EXT=""

while test $# -gt 0; do
  case "$1" in
    --embedded-server) USE_EMBEDDED_SERVER=1 ; USE_MANAGER=0 ; NO_SLAVE=1 ; \
      USE_RUNNING_SERVER="" RESULT_EXT=".es" ;;
    --user=*) DBUSER=`$ECHO "$1" | $SED -e "s;--user=;;"` ;;
    --force)  FORCE=1 ;;
    --verbose-manager)  MANAGER_QUIET_OPT="" ;;
    --old-master) MASTER_40_ARGS="";;
    --master-binary=*)
      MASTER_MYSQLD=`$ECHO "$1" | $SED -e "s;--master-binary=;;"` ;;
    --slave-binary=*)
      SLAVE_MYSQLD=`$ECHO "$1" | $SED -e "s;--slave-binary=;;"` ;;
    --local)   USE_RUNNING_SERVER="" ;;
    --extern)  USE_RUNNING_SERVER="1" ;;
    --with-ndbcluster)
      USE_NDBCLUSTER="--ndbcluster" ;;
    --ndbconnectstring=*)
      USE_NDBCLUSTER="--ndbcluster" ;
      USE_RUNNING_NDBCLUSTER=`$ECHO "$1" | $SED -e "s;--ndbconnectstring=;;"` ;;
    --tmpdir=*) MYSQL_TMP_DIR=`$ECHO "$1" | $SED -e "s;--tmpdir=;;"` ;;
    --local-master)
      MASTER_MYPORT=3306;
      EXTRA_MYSQL_TEST_OPT="$EXTRA_MYSQL_TEST_OPT --host=127.0.0.1 \
      --port=$MYSQL_MYPORT"
      LOCAL_MASTER=1 ;;
    --master_port=*) MASTER_MYPORT=`$ECHO "$1" | $SED -e "s;--master_port=;;"` ;;
    --slave_port=*) SLAVE_MYPORT=`$ECHO "$1" | $SED -e "s;--slave_port=;;"` ;;
    --manager-port=*) MYSQL_MANAGER_PORT=`$ECHO "$1" | $SED -e "s;--manager_port=;;"` ;;
    --ndbcluster_port=*) NDBCLUSTER_PORT=`$ECHO "$1" | $SED -e "s;--ndbcluster_port=;;"` ;;
    --with-openssl)
     EXTRA_MASTER_MYSQLD_OPT="$EXTRA_MASTER_MYSQLD_OPT \
     --ssl-ca=$BASEDIR/SSL/cacert.pem \
     --ssl-cert=$BASEDIR/SSL/server-cert.pem \
     --ssl-key=$BASEDIR/SSL/server-key.pem"
     EXTRA_SLAVE_MYSQLD_OPT="$EXTRA_SLAVE_MYSQLD_OPT \
     --ssl-ca=$BASEDIR/SSL/cacert.pem \
     --ssl-cert=$BASEDIR/SSL/server-cert.pem \
     --ssl-key=$BASEDIR/SSL/server-key.pem"
     MYSQL_TEST_SSL_OPTS="--ssl-ca=$BASEDIR/SSL/cacert.pem \
     --ssl-cert=$BASEDIR/SSL/client-cert.pem \
     --ssl-key=$BASEDIR/SSL/client-key.pem" ;;
    --no-manager | --skip-manager) USE_MANAGER=0 ;;
    --manager)
     USE_MANAGER=1
     USE_RUNNING_SERVER=
     ;;
    --start-and-exit)
     START_AND_EXIT=1
     ;;
    --socket=*) LOCAL_SOCKET=`$ECHO "$1" | $SED -e "s;--socket=;;"` ;;
    --skip-rpl) NO_SLAVE=1 ;;
    --skip-test=*) SKIP_TEST=`$ECHO "$1" | $SED -e "s;--skip-test=;;"`;;
    --do-test=*) DO_TEST=`$ECHO "$1" | $SED -e "s;--do-test=;;"`;;
    --start-from=* ) START_FROM=`$ECHO "$1" | $SED -e "s;--start-from=;;"` ;;
    --warnings | --log-warnings)
     EXTRA_MASTER_MYSQLD_OPT="$EXTRA_MASTER_MYSQLD_OPT --log-warnings"
     EXTRA_SLAVE_MYSQLD_OPT="$EXTRA_SLAVE_MYSQLD_OPT --log-warnings"
     ;;
    --wait-timeout=*)
     START_WAIT_TIMEOUT=`$ECHO "$1" | $SED -e "s;--wait-timeout=;;"`
     STOP_WAIT_TIMEOUT=$START_WAIT_TIMEOUT;;
    --record)
      RECORD=1;
      EXTRA_MYSQL_TEST_OPT="$EXTRA_MYSQL_TEST_OPT $1" ;;
    --small-bench)
      DO_SMALL_BENCH=1
      DO_BENCH=1
      NO_SLAVE=1
      ;;
    --bench)
      DO_BENCH=1
      NO_SLAVE=1
      ;;
    --big*)			# Actually --big-test
      EXTRA_MYSQL_TEST_OPT="$EXTRA_MYSQL_TEST_OPT $1" ;;
    --compress)
      EXTRA_MYSQL_TEST_OPT="$EXTRA_MYSQL_TEST_OPT $1" ;;
    --sleep=*)
      EXTRA_MYSQL_TEST_OPT="$EXTRA_MYSQL_TEST_OPT $1"
      SLEEP_TIME_AFTER_RESTART=`$ECHO "$1" | $SED -e "s;--sleep=;;"`
      ;;
    --user-test=*)
      USER_TEST=`$ECHO "$1" | $SED -e "s;--user-test=;;"`
      ;;
    --mysqld=*)
       TMP=`$ECHO "$1" | $SED -e "s;--mysqld=;;"`
       EXTRA_MASTER_MYSQLD_OPT="$EXTRA_MASTER_MYSQLD_OPT $TMP"
       EXTRA_SLAVE_MYSQLD_OPT="$EXTRA_SLAVE_MYSQLD_OPT $TMP"
       ;;
    --gcov )
      if [ x$BINARY_DIST = x1 ] ; then
	$ECHO "Cannot do coverage test without the source - please use source dist"
	exit 1
      fi
      DO_GCOV=1
      GCOV=`which gcov`
      ;;
    --gprof )
      DO_GPROF=1
      ;;
    --gdb )
      START_WAIT_TIMEOUT=300
      STOP_WAIT_TIMEOUT=300
      if [ x$BINARY_DIST = x1 ] ; then
	$ECHO "Note: you will get more meaningful output on a source distribution compiled with debugging option when running tests with --gdb option"
      fi
      DO_GDB=1
      EXTRA_MASTER_MYSQLD_OPT="$EXTRA_MASTER_MYSQLD_OPT --gdb"
      EXTRA_SLAVE_MYSQLD_OPT="$EXTRA_SLAVE_MYSQLD_OPT --gdb"
      # This needs to be checked properly
      # USE_MANAGER=1
      USE_RUNNING_SERVER=""
      ;;
    --client-gdb )
      if [ x$BINARY_DIST = x1 ] ; then
	$ECHO "Note: you will get more meaningful output on a source distribution compiled with debugging option when running tests with --client-gdb option"
      fi
      DO_CLIENT_GDB=1
      EXTRA_MASTER_MYSQLD_OPT="$EXTRA_MASTER_MYSQLD_OPT --gdb"
      EXTRA_SLAVE_MYSQLD_OPT="$EXTRA_SLAVE_MYSQLD_OPT --gdb"
      ;;
    --manual-gdb )
      DO_GDB=1
      MANUAL_GDB=1
      USE_RUNNING_SERVER=""
      EXTRA_MASTER_MYSQLD_OPT="$EXTRA_MASTER_MYSQLD_OPT --gdb"
      EXTRA_SLAVE_MYSQLD_OPT="$EXTRA_SLAVE_MYSQLD_OPT --gdb"
      ;;
    --ddd )
      if [ x$BINARY_DIST = x1 ] ; then
	$ECHO "Note: you will get more meaningful output on a source distribution compiled with debugging option when running tests with --ddd option"
      fi
      DO_DDD=1
      USE_RUNNING_SERVER=""
      EXTRA_MASTER_MYSQLD_OPT="$EXTRA_MASTER_MYSQLD_OPT --gdb"
      EXTRA_SLAVE_MYSQLD_OPT="$EXTRA_SLAVE_MYSQLD_OPT --gdb"
      ;;
    --valgrind | --valgrind-all)
      VALGRIND=`which valgrind` # this will print an error if not found
      # Give good warning to the user and stop
      if [ -z "$VALGRIND" ] ; then
        $ECHO "You need to have the 'valgrind' program in your PATH to run mysql-test-run with option --valgrind. Valgrind's home page is http://valgrind.kde.org ."
        exit 1
      fi
      # >=2.1.2 requires the --tool option, some versions write to stdout, some to stderr
      valgrind --help 2>&1 | grep "\-\-tool" > /dev/null && VALGRIND="$VALGRIND --tool=memcheck"
      VALGRIND="$VALGRIND --alignment=8 --leak-check=yes --num-callers=16"
      EXTRA_MASTER_MYSQLD_OPT="$EXTRA_MASTER_MYSQLD_OPT --skip-safemalloc --skip-bdb"
      EXTRA_SLAVE_MYSQLD_OPT="$EXTRA_SLAVE_MYSQLD_OPT --skip-safemalloc --skip-bdb"
      SLEEP_TIME_AFTER_RESTART=10
      SLEEP_TIME_FOR_DELETE=60
      USE_RUNNING_SERVER=""
      if test "$1" = "--valgrind-all"
      then
        VALGRIND="$VALGRIND -v --show-reachable=yes"
      fi
      ;;
    --valgrind-options=*)
      TMP=`$ECHO "$1" | $SED -e "s;--valgrind-options=;;"`
      VALGRIND="$VALGRIND $TMP"
      ;;
    --skip-*)
      EXTRA_MASTER_MYSQLD_OPT="$EXTRA_MASTER_MYSQLD_OPT $1"
      EXTRA_SLAVE_MYSQLD_OPT="$EXTRA_SLAVE_MYSQLD_OPT $1"
      ;;
    --strace-client )
      STRACE_CLIENT=1
      ;;
    --debug)
      EXTRA_MASTER_MYSQLD_OPT="$EXTRA_MASTER_MYSQLD_OPT \
       --debug=d:t:i:A,$MYSQL_TEST_DIR/var/log/master.trace"
      EXTRA_SLAVE_MYSQLD_OPT="$EXTRA_SLAVE_MYSQLD_OPT \
       --debug=d:t:i:A,$MYSQL_TEST_DIR/var/log/slave.trace"
      EXTRA_MYSQL_TEST_OPT="$EXTRA_MYSQL_TEST_OPT \
       --debug=d:t:A,$MYSQL_TEST_DIR/var/log/mysqltest.trace"
      EXTRA_MYSQLDUMP_OPT="$EXTRA_MYSQLDUMP_OPT \
       --debug=d:t:A,$MYSQL_TEST_DIR/var/log/mysqldump.trace"
      EXTRA_MYSQLBINLOG_OPT="$EXTRA_MYSQLBINLOG_OPT \
       --debug=d:t:A,$MYSQL_TEST_DIR/var/log/mysqlbinlog.trace"
      ;;
    --fast)
      FAST_START=1
      ;;
    -- )  shift; break ;;
    --* ) $ECHO "Unrecognized option: $1"; exit 1 ;;
    * ) break ;;
  esac
  shift
done

#++
# mysqld Environment Parameters
#--

MYRUN_DIR=$MYSQL_TEST_DIR/var/run
MANAGER_PID_FILE="$MYRUN_DIR/manager.pid"

MASTER_MYDDIR="$MYSQL_TEST_DIR/var/master-data"
MASTER_MYSOCK="$MYSQL_TMP_DIR/master.sock"
MASTER_MYPID="$MYRUN_DIR/master.pid"
MASTER_MYLOG="$MYSQL_TEST_DIR/var/log/master.log"
MASTER_MYERR="$MYSQL_TEST_DIR/var/log/master.err"

SLAVE_MYDDIR="$MYSQL_TEST_DIR/var/slave-data"
SLAVE_MYSOCK="$MYSQL_TMP_DIR/slave.sock"
SLAVE_MYPID="$MYRUN_DIR/slave.pid"
SLAVE_MYLOG="$MYSQL_TEST_DIR/var/log/slave.log"
SLAVE_MYERR="$MYSQL_TEST_DIR/var/log/slave.err"

CURRENT_TEST="$MYSQL_TEST_DIR/var/log/current_test"
SMALL_SERVER="--key_buffer_size=1M --sort_buffer=256K --max_heap_table_size=1M"

export MASTER_MYPORT SLAVE_MYPORT MYSQL_TCP_PORT MASTER_MYSOCK

if [ x$SOURCE_DIST = x1 ] ; then
 MY_BASEDIR=$MYSQL_TEST_DIR
else
 MY_BASEDIR=$BASEDIR
fi

# Create the directories

# This should be fixed to be not be dependent on the contence of MYSQL_TMP_DIR
# or MYRUN_DIR
# (mkdir -p is not portable)
[ -d $MYSQL_TEST_DIR/var ] || mkdir $MYSQL_TEST_DIR/var
[ -d $MYSQL_TEST_DIR/var/tmp ] || mkdir $MYSQL_TEST_DIR/var/tmp
[ -d $MYSQL_TEST_DIR/var/run ] || mkdir $MYSQL_TEST_DIR/var/run
[ -d $MYSQL_TEST_DIR/var/log ] || mkdir $MYSQL_TEST_DIR/var/log

if test ${COLUMNS:-0} -lt 80 ; then COLUMNS=80 ; fi
E=`$EXPR $COLUMNS - 8`
DASH72=`$ECHO '-------------------------------------------------------'|$CUT -c 1-$E`

# on source dist, we pick up freshly build executables
# on binary, use what is installed
if [ x$SOURCE_DIST = x1 ] ; then
 if [ "x$USE_EMBEDDED_SERVER" = "x1" ] ; then
   if [ -f "$BASEDIR/libmysqld/examples/mysqltest" ] ; then
     MYSQL_TEST="$VALGRIND $BASEDIR/libmysqld/examples/mysqltest"
   else
     echo "Fatal error: Cannot find embedded server 'mysqltest'" 1>&2
     exit 1
   fi
 else
   MYSQLD="$VALGRIND $BASEDIR/sql/mysqld"
   if [ -f "$BASEDIR/client/.libs/lt-mysqltest" ] ; then
     MYSQL_TEST="$BASEDIR/client/.libs/lt-mysqltest"
   elif [ -f "$BASEDIR/client/.libs/mysqltest" ] ; then
     MYSQL_TEST="$BASEDIR/client/.libs/mysqltest"
   else
     MYSQL_TEST="$BASEDIR/client/mysqltest"
   fi
 fi
 if [ -f "$BASEDIR/client/.libs/mysqldump" ] ; then
   MYSQL_DUMP="$BASEDIR/client/.libs/mysqldump"
 else
   MYSQL_DUMP="$BASEDIR/client/mysqldump"
 fi
 if [ -f "$BASEDIR/client/.libs/mysqlbinlog" ] ; then
   MYSQL_BINLOG="$BASEDIR/client/.libs/mysqlbinlog"
 else
   MYSQL_BINLOG="$BASEDIR/client/mysqlbinlog"
 fi
 if [ -n "$STRACE_CLIENT" ]; then
  MYSQL_TEST="strace -o $MYSQL_TEST_DIR/var/log/mysqltest.strace $MYSQL_TEST"
 fi

 CLIENT_BINDIR="$BASEDIR/client"
 MYSQLADMIN="$CLIENT_BINDIR/mysqladmin"
 WAIT_PID="$BASEDIR/extra/mysql_waitpid"
 MYSQL_MANAGER_CLIENT="$CLIENT_BINDIR/mysqlmanagerc"
 MYSQL_MANAGER="$BASEDIR/tools/mysqlmanager"
 MYSQL_MANAGER_PWGEN="$CLIENT_BINDIR/mysqlmanager-pwgen"
 MYSQL="$CLIENT_BINDIR/mysql"
 LANGUAGE="$BASEDIR/sql/share/english/"
 CHARSETSDIR="$BASEDIR/sql/share/charsets"
 INSTALL_DB="./install_test_db"
 MYSQL_FIX_SYSTEM_TABLES="$BASEDIR/scripts/mysql_fix_privilege_tables"
else
 if test -x "$BASEDIR/libexec/mysqld"
 then
   MYSQLD="$VALGRIND $BASEDIR/libexec/mysqld"
 else
   MYSQLD="$VALGRIND $BASEDIR/bin/mysqld"
 fi
 CLIENT_BINDIR="$BASEDIR/bin"
 MYSQL_TEST="$CLIENT_BINDIR/mysqltest"
 MYSQL_DUMP="$CLIENT_BINDIR/mysqldump"
 MYSQL_BINLOG="$CLIENT_BINDIR/mysqlbinlog"
 MYSQLADMIN="$CLIENT_BINDIR/mysqladmin"
 WAIT_PID="$CLIENT_BINDIR/mysql_waitpid"
 MYSQL_MANAGER="$CLIENT_BINDIR/mysqlmanager"
 MYSQL_MANAGER_CLIENT="$CLIENT_BINDIR/mysqlmanagerc"
 MYSQL_MANAGER_PWGEN="$CLIENT_BINDIR/mysqlmanager-pwgen"
 MYSQL="$CLIENT_BINDIR/mysql"
 INSTALL_DB="./install_test_db --bin"
 MYSQL_FIX_SYSTEM_TABLES="$CLIENT_BINDIR/mysql_fix_privilege_tables"
 if test -d "$BASEDIR/share/mysql/english"
 then
   LANGUAGE="$BASEDIR/share/mysql/english/"
   CHARSETSDIR="$BASEDIR/share/mysql/charsets"
 else
   LANGUAGE="$BASEDIR/share/english/"
   CHARSETSDIR="$BASEDIR/share/charsets"
  fi
fi

if [ -z "$MASTER_MYSQLD" ]
then
MASTER_MYSQLD=$MYSQLD
fi

if [ -z "$SLAVE_MYSQLD" ]
then
SLAVE_MYSQLD=$MYSQLD
fi

# If we should run all tests cases, we will use a local server for that

if [ -z "$1" ]
then
   USE_RUNNING_SERVER=""
fi
if [ -n "$USE_RUNNING_SERVER" ]
then
   MASTER_MYSOCK=$LOCAL_SOCKET;
   DBUSER=${DBUSER:-test}
else
   DBUSER=${DBUSER:-root}		# We want to do FLUSH xxx commands
fi

if [ -w / ]
then
  # We are running as root;  We need to add the --root argument
  EXTRA_MASTER_MYSQLD_OPT="$EXTRA_MASTER_MYSQLD_OPT --user=root"
  EXTRA_SLAVE_MYSQLD_OPT="$EXTRA_SLAVE_MYSQLD_OPT --user=root"
fi


MYSQL_DUMP="$MYSQL_DUMP --no-defaults -uroot --socket=$MASTER_MYSOCK --password=$DBPASSWD $EXTRA_MYSQLDUMP_OPT"
MYSQL_BINLOG="$MYSQL_BINLOG --no-defaults --local-load=$MYSQL_TMP_DIR $EXTRA_MYSQLBINLOG_OPT"
MYSQL_FIX_SYSTEM_TABLES="$MYSQL_FIX_SYSTEM_TABLES --no-defaults --host=localhost --port=$MASTER_MYPORT --socket=$MASTER_MYSOCK --user=root --password=$DBPASSWD --basedir=$BASEDIR --bindir=$CLIENT_BINDIR --verbose"
MYSQL="$MYSQL --host=localhost --port=$MASTER_MYPORT --socket=$MASTER_MYSOCK --user=root --password=$DBPASSWD"
export MYSQL MYSQL_DUMP MYSQL_BINLOG MYSQL_FIX_SYSTEM_TABLES CLIENT_BINDIR

MYSQL_TEST_ARGS="--no-defaults --socket=$MASTER_MYSOCK --database=$DB \
 --user=$DBUSER --password=$DBPASSWD --silent -v --skip-safemalloc \
 --tmpdir=$MYSQL_TMP_DIR --port=$MASTER_MYPORT --timer-file=$MY_LOG_DIR/timer \
 $MYSQL_TEST_SSL_OPTS"
MYSQL_TEST_BIN=$MYSQL_TEST
MYSQL_TEST="$MYSQL_TEST $MYSQL_TEST_ARGS"
GDB_CLIENT_INIT=$MYSQL_TMP_DIR/gdbinit.client
GDB_MASTER_INIT=$MYSQL_TMP_DIR/gdbinit.master
GDB_SLAVE_INIT=$MYSQL_TMP_DIR/gdbinit.slave
GCOV_MSG=$MYSQL_TMP_DIR/mysqld-gcov.out
GCOV_ERR=$MYSQL_TMP_DIR/mysqld-gcov.err
GPROF_DIR=$MYSQL_TMP_DIR/gprof
GPROF_MASTER=$GPROF_DIR/master.gprof
GPROF_SLAVE=$GPROF_DIR/slave.gprof
TIMEFILE="$MYSQL_TEST_DIR/var/log/mysqltest-time"
if [ -n "$DO_CLIENT_GDB" -o -n "$DO_GDB" ] ; then
  XTERM=`which xterm`
fi

#++
# Function Definitions
#--

prompt_user ()
{
 $ECHO $1
 read unused
}

# We can't use diff -u or diff -a as these are not portable

show_failed_diff ()
{
  reject_file=r/$1.reject
  result_file=r/$1.result
  eval_file=r/$1.eval

  # If we have an special externsion for result files we use it if we are recording
  # or a result file with that extension exists.
  if [ -n "$RESULT_EXT" -a \( x$RECORD = x1 -o -f "$result_file$RESULT_EXT" \) ]
  then
    result_file="$result_file$RESULT_EXT"
  fi

  if [ -f $eval_file ]
  then
    result_file=$eval_file
  fi

  if [ -x "$DIFF" ] && [ -f $reject_file ]
  then
    echo "Below are the diffs between actual and expected results:"
    echo "-------------------------------------------------------"
    $DIFF -c $result_file $reject_file
    echo "-------------------------------------------------------"
    echo "Please follow the instructions outlined at"
    echo "http://www.mysql.com/doc/en/Reporting_mysqltest_bugs.html"
    echo "to find the reason to this problem and how to report this."
    echo ""
  fi
}

do_gdb_test ()
{
  mysql_test_args="$MYSQL_TEST_ARGS $1"
  $ECHO "set args $mysql_test_args < $2" > $GDB_CLIENT_INIT
  echo "Set breakpoints ( if needed) and type 'run' in gdb window"
  #this xterm should not be backgrounded
  $XTERM -title "Client" -e gdb -x $GDB_CLIENT_INIT $MYSQL_TEST_BIN
}

error () {
    $ECHO  "Error:  $1"
    exit 1
}

error_is () {
    $ECHO "Errors are (from $TIMEFILE) :"
    $CAT < $TIMEFILE
    $ECHO "(the last lines may be the most important ones)"
}

prefix_to_8() {
 $ECHO "        $1" | $SED -e 's:.*\(........\)$:\1:'
}

pass_inc () {
    TOT_PASS=`$EXPR $TOT_PASS + 1`
}

fail_inc () {
    TOT_FAIL=`$EXPR $TOT_FAIL + 1`
}

skip_inc () {
    TOT_SKIP=`$EXPR $TOT_SKIP + 1`
}

total_inc () {
    TOT_TEST=`$EXPR $TOT_TEST + 1`
}


skip_test() {
   USERT="    ...."
   SYST="    ...."
   REALT="    ...."
   pname=`$ECHO "$1                        "|$CUT -c 1-24`
   RES="$pname"
   skip_inc
   $ECHO "$RES$RES_SPACE [ skipped ]"
}

report_stats () {
    if [ $TOT_FAIL = 0 ]; then
	$ECHO "All $TOT_TEST tests were successful."
    else
	xten=`$EXPR $TOT_PASS \* 10000`
	raw=`$EXPR $xten / $TOT_TEST`
	raw=`$PRINTF %.4d $raw`
	whole=`$PRINTF %.2s $raw`
	xwhole=`$EXPR $whole \* 100`
	deci=`$EXPR $raw - $xwhole`
	$ECHO  "Failed ${TOT_FAIL}/${TOT_TEST} tests, ${whole}.${deci}% successful."
	$ECHO ""
        $ECHO "The log files in $MY_LOG_DIR may give you some hint"
	$ECHO "of what when wrong."
	$ECHO "If you want to report this error, please read first the documentation at"
        $ECHO "http://www.mysql.com/doc/en/MySQL_test_suite.html"
    fi

    if test -z "$USE_RUNNING_SERVER"
    then

    # Report if there was any fatal warnings/errors in the log files
    #
    $RM -f $MY_LOG_DIR/warnings $MY_LOG_DIR/warnings.tmp
    # Remove some non fatal warnings from the log files
    $SED -e 's!Warning:  Table:.* on delete!!g' -e 's!Warning: Setting lower_case_table_names=2!!g' -e 's!Warning: One can only use the --user.*root!!g' \
        $MY_LOG_DIR/*.err \
        | $SED -e 's!Warning:  Table:.* on rename!!g' \
        > $MY_LOG_DIR/warnings.tmp

    found_error=0
    # Find errors
    for i in "^Warning:" "^Error:" "^==.* at 0x"
    do
      if $GREP "$i" $MY_LOG_DIR/warnings.tmp >> $MY_LOG_DIR/warnings
      then
        found_error=1
      fi
    done
    $RM -f $MY_LOG_DIR/warnings.tmp
    if [ $found_error = "1" ]
    then
      echo "WARNING: Got errors/warnings while running tests. Please examine"
      echo "$MY_LOG_DIR/warnings for details."
    fi
    fi
}

mysql_install_db () {
    $ECHO "Removing Stale Files"
    $RM -rf $MASTER_MYDDIR $SLAVE_MYDDIR $MY_LOG_DIR/* 
    $ECHO "Installing Master Databases"
    $INSTALL_DB
    if [ $? != 0 ]; then
	error "Could not install master test DBs"
	exit 1
    fi
    $ECHO "Installing Slave Databases"
    $INSTALL_DB -slave
    if [ $? != 0 ]; then
	error "Could not install slave test DBs"
	exit 1
    fi

    for slave_num in 1 2 ;
    do
      $RM -rf var/slave$slave_num-data
      mkdir -p var/slave$slave_num-data/mysql
      mkdir -p var/slave$slave_num-data/test
      cp var/slave-data/mysql/* var/slave$slave_num-data/mysql
    done
    return 0
}

gprof_prepare ()
{
 $RM -rf $GPROF_DIR
 mkdir -p $GPROF_DIR
}

gprof_collect ()
{
 if [ -f $MASTER_MYDDIR/gmon.out ]; then
   gprof $MASTER_MYSQLD $MASTER_MYDDIR/gmon.out > $GPROF_MASTER
   echo "Master execution profile has been saved in $GPROF_MASTER"
 fi
 if [ -f $SLAVE_MYDDIR/gmon.out ]; then
   gprof $SLAVE_MYSQLD $SLAVE_MYDDIR/gmon.out > $GPROF_SLAVE
   echo "Slave execution profile has been saved in $GPROF_SLAVE"
 fi
}

gcov_prepare () {
    $FIND $BASEDIR -name \*.gcov \
    -or -name \*.da | $XARGS $RM
}

gcov_collect () {
    $ECHO "Collecting source coverage info..."
    [ -f $GCOV_MSG ] && $RM $GCOV_MSG
    [ -f $GCOV_ERR ] && $RM $GCOV_ERR
    for d in $MYSQLD_SRC_DIRS; do
	cd $BASEDIR/$d
	for f in *.h *.cc *.c; do
	    $GCOV $f 2>>$GCOV_ERR  >>$GCOV_MSG
	done
	cd $MYSQL_TEST_DIR
    done

    $ECHO "gcov info in $GCOV_MSG, errors in $GCOV_ERR"
}

abort_if_failed()
{
 if [ ! $? = 0 ] ; then
  echo $1
  exit 1
 fi
}

start_manager()
{
 if [ $USE_MANAGER = 0 ] ; then
   echo "Manager disabled, skipping manager start."
   $RM -f $MYSQL_MANAGER_LOG
  return
 fi
 $ECHO "Starting MySQL Manager"
 if [ -f "$MANAGER_PID_FILE" ] ; then
    kill `cat $MANAGER_PID_FILE`
    sleep 1
    if [ -f "$MANAGER_PID_FILE" ] ; then
     kill -9 `cat $MANAGER_PID_FILE`
     sleep 1
    fi
 fi

 $RM -f $MANAGER_PID_FILE
 MYSQL_MANAGER_PW=`$MYSQL_MANAGER_PWGEN -u $MYSQL_MANAGER_USER \
 -o $MYSQL_MANAGER_PW_FILE`
 $MYSQL_MANAGER --log=$MYSQL_MANAGER_LOG --port=$MYSQL_MANAGER_PORT \
  --password-file=$MYSQL_MANAGER_PW_FILE --pid-file=$MANAGER_PID_FILE
  abort_if_failed "Could not start MySQL manager"
  mysqltest_manager_args="--manager-host=localhost \
  --manager-user=$MYSQL_MANAGER_USER \
  --manager-password=$MYSQL_MANAGER_PW \
  --manager-port=$MYSQL_MANAGER_PORT \
  --manager-wait-timeout=$START_WAIT_TIMEOUT"
  MYSQL_TEST="$MYSQL_TEST $mysqltest_manager_args"
  MYSQL_TEST_ARGS="$MYSQL_TEST_ARGS $mysqltest_manager_args"
  while [ ! -f $MANAGER_PID_FILE ] ; do
   sleep 1
  done
  echo "Manager started"
}

stop_manager()
{
 if [ $USE_MANAGER = 0 ] ; then
  return
 fi
 $MYSQL_MANAGER_CLIENT $MANAGER_QUIET_OPT -u$MYSQL_MANAGER_USER \
  -p$MYSQL_MANAGER_PW -P $MYSQL_MANAGER_PORT <<EOF
shutdown
EOF
 echo "Manager terminated"

}

manager_launch()
{
  ident=$1
  shift
  if [ $USE_MANAGER = 0 ] ; then
    echo $@ | /bin/sh  >> $CUR_MYERR 2>&1  &
    sleep 2 #hack
    return
  fi
  $MYSQL_MANAGER_CLIENT $MANAGER_QUIET_OPT --user=$MYSQL_MANAGER_USER \
   --password=$MYSQL_MANAGER_PW  --port=$MYSQL_MANAGER_PORT <<EOF
def_exec $ident "$@"
set_exec_stdout $ident $CUR_MYERR
set_exec_stderr $ident $CUR_MYERR
set_exec_con $ident root localhost $CUR_MYSOCK
start_exec $ident $START_WAIT_TIMEOUT
EOF
  abort_if_failed "Could not execute manager command"
}

manager_term()
{
  pid=$1
  ident=$2
  if [ $USE_MANAGER = 0 ] ; then
    # Shutdown time must be high as slave may be in reconnect
    $MYSQLADMIN --no-defaults -uroot --socket=$MYSQL_TMP_DIR/$ident.sock --connect_timeout=5 --shutdown_timeout=70 shutdown >> $MYSQL_MANAGER_LOG 2>&1
    res=$?
    # Some systems require an extra connect
    $MYSQLADMIN --no-defaults -uroot --socket=$MYSQL_TMP_DIR/$ident.sock --connect_timeout=1 ping >> $MYSQL_MANAGER_LOG 2>&1
    if test $res = 0
    then
      wait_for_pid $pid
    fi
    return $res
  fi
  $MYSQL_MANAGER_CLIENT $MANAGER_QUIET_OPT --user=$MYSQL_MANAGER_USER \
   --password=$MYSQL_MANAGER_PW  --port=$MYSQL_MANAGER_PORT <<EOF
stop_exec $ident $STOP_WAIT_TIMEOUT
EOF
 abort_if_failed "Could not execute manager command"
}

# The embedded server needs the cleanup so we do some of the start work
# but stop before actually running mysqld or anything.

start_master()
{
  if [ x$MASTER_RUNNING = x1 ] || [ x$LOCAL_MASTER = x1 ] ; then
    return
  fi
  # Remove stale binary logs except for 2 tests which need them
  if [ "$tname" != "rpl_crash_binlog_ib_1b" ] && [ "$tname" != "rpl_crash_binlog_ib_2b" ] && [ "$tname" != "rpl_crash_binlog_ib_3b" ] 
  then
    $RM -f $MYSQL_TEST_DIR/var/log/master-bin.*
  fi

  # Remove old master.info and relay-log.info files
  $RM -f $MYSQL_TEST_DIR/var/master-data/master.info $MYSQL_TEST_DIR/var/master-data/relay-log.info

  #run master initialization shell script if one exists

  if [ -f "$master_init_script" ] ;
  then
      /bin/sh $master_init_script
  fi
  cd $BASEDIR # for gcov
  if [ -z "$DO_BENCH" ]
  then
    master_args="--no-defaults --log-bin=$MYSQL_TEST_DIR/var/log/master-bin \
  	    --server-id=1  \
          --basedir=$MY_BASEDIR \
          --port=$MASTER_MYPORT \
          --local-infile \
          --exit-info=256 \
          --core \
          $USE_NDBCLUSTER \
          --datadir=$MASTER_MYDDIR \
          --pid-file=$MASTER_MYPID \
          --socket=$MASTER_MYSOCK \
          --log=$MASTER_MYLOG \
          --character-sets-dir=$CHARSETSDIR \
          --default-character-set=$CHARACTER_SET \
          --tmpdir=$MYSQL_TMP_DIR \
          --language=$LANGUAGE \
          --innodb_data_file_path=ibdata1:50M \
	  --open-files-limit=1024 \
	   $MASTER_40_ARGS \
           $SMALL_SERVER \
           $EXTRA_MASTER_OPT $EXTRA_MASTER_MYSQLD_OPT"
  else
    master_args="--no-defaults --log-bin=$MYSQL_TEST_DIR/var/log/master-bin \
          --server-id=1 --rpl-recovery-rank=1 \
          --basedir=$MY_BASEDIR --init-rpl-role=master \
          --port=$MASTER_MYPORT \
          --local-infile \
          --datadir=$MASTER_MYDDIR \
          --pid-file=$MASTER_MYPID \
          --socket=$MASTER_MYSOCK \
          --character-sets-dir=$CHARSETSDIR \
          --default-character-set=$CHARACTER_SET \
          --core \
          $USE_NDBCLUSTER \
          --tmpdir=$MYSQL_TMP_DIR \
          --language=$LANGUAGE \
          --innodb_data_file_path=ibdata1:50M \
	   $MASTER_40_ARGS \
           $SMALL_SERVER \
           $EXTRA_MASTER_OPT $EXTRA_MASTER_MYSQLD_OPT"
  fi

  CUR_MYERR=$MASTER_MYERR
  CUR_MYSOCK=$MASTER_MYSOCK

  # For embedded server we collect the server flags and return
  if [ "x$USE_EMBEDDED_SERVER" = "x1" ] ; then
    # Add a -A to each argument to pass it to embedded server
    EMBEDDED_SERVER_OPTS=""
    for opt in $master_args
    do
      EMBEDDED_SERVER_OPTS="$EMBEDDED_SERVER_OPTS -A $opt"
    done
    EXTRA_MYSQL_TEST_OPT="$EMBEDDED_SERVER_OPTS"
    return
  fi

  if [ x$DO_DDD = x1 ]
  then
    $ECHO "set args $master_args" > $GDB_MASTER_INIT
    manager_launch master ddd -display $DISPLAY --debugger \
    "gdb -x $GDB_MASTER_INIT" $MASTER_MYSQLD
  elif [ x$DO_GDB = x1 ]
  then
    if [ x$MANUAL_GDB = x1 ]
    then
      $ECHO "set args $master_args" > $GDB_MASTER_INIT
      $ECHO "To start gdb for the master , type in another window:"
      $ECHO "cd $CWD ; gdb -x $GDB_MASTER_INIT $MASTER_MYSQLD"
      wait_for_master=1500
    else
      ( $ECHO set args $master_args;
      if [ $USE_MANAGER = 0 ] ; then
    cat <<EOF
b mysql_parse
commands 1
disa 1
end
r
EOF
      fi )  > $GDB_MASTER_INIT
      manager_launch master $XTERM -display $DISPLAY \
      -title "Master" -e gdb -x $GDB_MASTER_INIT $MASTER_MYSQLD
    fi
  else
    manager_launch master $MASTER_MYSQLD $master_args
  fi
  sleep_until_file_created $MASTER_MYPID $wait_for_master
  wait_for_master=$SLEEP_TIME_FOR_SECOND_MASTER
  MASTER_RUNNING=1
}

start_slave()
{
  [ x$SKIP_SLAVE = x1 ] && return
  eval "this_slave_running=\$SLAVE$1_RUNNING"
  [ x$this_slave_running = 1 ] && return
  # When testing fail-safe replication, we will have more than one slave
  # in this case, we start secondary slaves with an argument
  slave_ident="slave$1"
  if [ -n "$1" ] ;
  then
   slave_server_id=`$EXPR 2 + $1`
   slave_rpl_rank=$slave_server_id
   slave_port=`expr $SLAVE_MYPORT + $1`
   slave_log="$SLAVE_MYLOG.$1"
   slave_err="$SLAVE_MYERR.$1"
   slave_datadir="$SLAVE_MYDDIR/../$slave_ident-data/"
   slave_pid="$MYRUN_DIR/mysqld-$slave_ident.pid"
   slave_sock="$SLAVE_MYSOCK-$1"
  else
   slave_server_id=2
   slave_rpl_rank=2
   slave_port=$SLAVE_MYPORT
   slave_log=$SLAVE_MYLOG
   slave_err=$SLAVE_MYERR
   slave_datadir=$SLAVE_MYDDIR
   slave_pid=$SLAVE_MYPID
   slave_sock="$SLAVE_MYSOCK"
 fi
  # Remove stale binary logs and old master.info files
  # except for too tests which need them
  if [ "$tname" != "rpl_crash_binlog_ib_1b" ] && [ "$tname" != "rpl_crash_binlog_ib_2b" ] && [ "$tname" != "rpl_crash_binlog_ib_3b" ]
  then
    $RM -f $MYSQL_TEST_DIR/var/log/$slave_ident-*bin.*
    $RM -f $slave_datadir/master.info $slave_datadir/relay-log.info
  fi

  #run slave initialization shell script if one exists
  if [ -f "$slave_init_script" ] ;
  then
        /bin/sh $slave_init_script
  fi

  if [ -z "$SLAVE_MASTER_INFO" ] ; then
    master_info="--master-user=root \
          --master-connect-retry=1 \
          --master-host=127.0.0.1 \
          --master-password="" \
          --master-port=$MASTER_MYPORT \
          --server-id=$slave_server_id --rpl-recovery-rank=$slave_rpl_rank"
 else
   master_info=$SLAVE_MASTER_INFO
 fi

  $RM -f $slave_datadir/log.*
  slave_args="--no-defaults $master_info \
  	    --exit-info=256 \
          --log-bin=$MYSQL_TEST_DIR/var/log/$slave_ident-bin \
          --relay-log=$MYSQL_TEST_DIR/var/log/$slave_ident-relay-bin \
          --log-slave-updates \
          --log=$slave_log \
          --basedir=$MY_BASEDIR \
          --datadir=$slave_datadir \
          --pid-file=$slave_pid \
          --port=$slave_port \
          --socket=$slave_sock \
          --character-sets-dir=$CHARSETSDIR \
          --default-character-set=$CHARACTER_SET \
          --core --init-rpl-role=slave \
          --tmpdir=$MYSQL_TMP_DIR \
          --language=$LANGUAGE \
          --skip-innodb --skip-ndbcluster --skip-slave-start \
          --slave-load-tmpdir=$SLAVE_LOAD_TMPDIR \
          --report-host=127.0.0.1 --report-user=root \
          --report-port=$slave_port \
          --master-retry-count=10 \
          -O slave_net_timeout=10 \
           $SMALL_SERVER \
           $EXTRA_SLAVE_OPT $EXTRA_SLAVE_MYSQLD_OPT"
  CUR_MYERR=$slave_err
  CUR_MYSOCK=$slave_sock

  if [ x$DO_DDD = x1 ]
  then
    $ECHO "set args $slave_args" > $GDB_SLAVE_INIT
    manager_launch $slave_ident ddd -display $DISPLAY --debugger \
     "gdb -x $GDB_SLAVE_INIT" $SLAVE_MYSQLD
  elif [ x$DO_GDB = x1 ]
  then
    if [ x$MANUAL_GDB = x1 ]
    then
      $ECHO "set args $slave_args" > $GDB_SLAVE_INIT
      echo "To start gdb for the slave, type in another window:"
      echo "cd $CWD ; gdb -x $GDB_SLAVE_INIT $SLAVE_MYSQLD"
      wait_for_slave=1500
    else
      ( $ECHO set args $slave_args;
      if [ $USE_MANAGER = 0 ] ; then
    cat <<EOF
b mysql_parse
commands 1
disa 1
end
r
EOF
      fi )  > $GDB_SLAVE_INIT
      manager_launch $slave_ident $XTERM -display $DISPLAY -title "Slave" -e \
      gdb -x $GDB_SLAVE_INIT $SLAVE_MYSQLD
    fi
  else
    manager_launch $slave_ident $SLAVE_MYSQLD $slave_args
  fi
  eval "SLAVE$1_RUNNING=1"
  sleep_until_file_created $slave_pid $wait_for_slave
  wait_for_slave=$SLEEP_TIME_FOR_SECOND_SLAVE
}

mysql_start ()
{
# We should not start the daemon here as we don't know the arguments
# for the test.  Better to let the test start the daemon

#  $ECHO "Starting MySQL daemon"
#  start_master
#  start_slave
  cd $MYSQL_TEST_DIR
  return 1
}

stop_slave ()
{
  eval "this_slave_running=\$SLAVE$1_RUNNING"
  slave_ident="slave$1"
  if [ -n "$1" ] ;
  then
   slave_pid="$MYRUN_DIR/mysqld-$slave_ident.pid"
  else
   slave_pid=$SLAVE_MYPID
  fi
  if [ x$this_slave_running = x1 ]
  then
    pid=`$CAT $slave_pid`
    manager_term $pid $slave_ident
    if [ $? != 0 ] && [ -f $slave_pid ]
    then # try harder!
      $ECHO "slave not cooperating with mysqladmin, will try manual kill"
      kill $pid
      sleep_until_file_deleted $pid $slave_pid
      if [ -f $slave_pid ] ; then
        $ECHO "slave refused to die. Sending SIGKILL"
        kill -9 `$CAT $slave_pid`
        $RM -f $slave_pid
      else
        $ECHO "slave responded to SIGTERM "
      fi
    else
      sleep $SLEEP_TIME_AFTER_RESTART
    fi
    eval "SLAVE$1_RUNNING=0"
  fi
}

stop_slave_threads ()
{
  eval "this_slave_running=\$SLAVE$1_RUNNING"
  slave_ident="slave$1"
  if [ x$this_slave_running = x1 ]
  then
    $MYSQLADMIN --no-defaults -uroot --socket=$MYSQL_TMP_DIR/$slave_ident.sock stop-slave > /dev/null 2>&1
  fi
}

stop_master ()
{
  if [ x$MASTER_RUNNING = x1 ]
  then
    # For embedded server we don't stop anyting but mark that
    # MASTER_RUNNING=0 to get cleanup when calling start_master().
    if [ x$USE_EMBEDDED_SERVER != x1 ] ; then
      pid=`$CAT $MASTER_MYPID`
      manager_term $pid master
      if [ $? != 0 ] && [ -f $MASTER_MYPID ]
      then # try harder!
	$ECHO "master not cooperating with mysqladmin, will try manual kill"
	kill $pid
	sleep_until_file_deleted $pid $MASTER_MYPID
	if [ -f $MASTER_MYPID ] ; then
	  $ECHO "master refused to die. Sending SIGKILL"
	  kill -9 `$CAT $MASTER_MYPID`
	  $RM -f $MASTER_MYPID
	else
	  $ECHO "master responded to SIGTERM "
	fi
      else
	sleep $SLEEP_TIME_AFTER_RESTART
      fi
    fi
    MASTER_RUNNING=0
  fi
}

mysql_stop ()
{
 $ECHO  "Ending Tests"
 $ECHO  "Shutting-down MySQL daemon"
 $ECHO  ""
 stop_master
 $ECHO "Master shutdown finished"
 stop_slave
 stop_slave 1
 stop_slave 2
 $ECHO "Slave shutdown finished"

 return 1
}

mysql_restart ()
{
  mysql_stop
  mysql_start
  return 1
}

mysql_loadstd () {

    # cp $STD_DATA/*.frm $STD_DATA/*.MRG $MASTER_MYDDIR/test
    return 1
}

run_testcase ()
{
 tf=$1
 tname=`$BASENAME $tf .test`
 master_opt_file=$TESTDIR/$tname-master.opt
 slave_opt_file=$TESTDIR/$tname-slave.opt
 master_init_script=$TESTDIR/$tname-master.sh
 slave_init_script=$TESTDIR/$tname-slave.sh
 slave_master_info_file=$TESTDIR/$tname.slave-mi
 tsrcdir=$TESTDIR/$tname-src
 result_file="r/$tname.result"
 echo $tname > $CURRENT_TEST
 SKIP_SLAVE=`$EXPR \( $tname : rpl \) = 0`
 if [ -n "$RESULT_EXT" -a \( x$RECORD = x1 -o -f "$result_file$RESULT_EXT" \) ] ; then
   result_file="$result_file$RESULT_EXT"
 fi
 if [ "$USE_MANAGER" = 1 ] ; then
   many_slaves=`$EXPR \( \( $tname : rpl_failsafe \) != 0 \) \| \( \( $tname : rpl_chain_temp_table \) != 0 \)`
 fi
 if $EXPR "$tname" '<' "$START_FROM" > /dev/null ; then
   #skip_test $tname
   return
 fi

 if [ "$SKIP_TEST" ] ; then
   if $EXPR \( "$tname" : "$SKIP_TEST" \) > /dev/null ; then
     skip_test $tname
     return
   fi
 fi

 if [ "$DO_TEST" ] ; then
   if $EXPR \( "$tname" : "$DO_TEST" \) > /dev/null ; then
     : #empty command to keep some shells happy
   else
     #skip_test $tname
     return
   fi
 fi

 if [ x${NO_SLAVE}x$SKIP_SLAVE = x1x0 ] ; then
   skip_test $tname
   return
 fi

 if [ "x$USE_EMBEDDED_SERVER" != "x1" ] ; then
   # Stop all slave threads, so that we don't have useless reconnection
   #  attempts and error messages in case the slave and master servers restart.
   stop_slave_threads
   stop_slave_threads 1
   stop_slave_threads 2
 fi

 # FIXME temporary solution, we will get a new C version of this
 # script soon anyway so it is not worth it spending the time
 if [ "x$USE_EMBEDDED_SERVER" = "x1" -a -z "$DO_TEST" ] ; then
   for t in \
	"bdb-deadlock" \
	"connect" \
	"flush_block_commit" \
	"grant2" \
	"grant_cache" \
	"grant" \
	"init_connect" \
	"innodb-deadlock" \
	"innodb-lock" \
	"mix_innodb_myisam_binlog" \
	"mysqlbinlog2" \
	"mysqlbinlog" \
	"mysqldump" \
	"mysql_protocols" \
	"ps_1general" \
	"rename" \
	"show_check" \
        "system_mysql_db_fix" \
	"user_var" \
	"variables"
   do
     if [ "$tname" = "$t" ] ; then
       skip_test $tname
       return
     fi
   done
 fi

 if [ -z "$USE_RUNNING_SERVER" ] ;
 then
   if [ -f $master_opt_file ] ;
   then
     EXTRA_MASTER_OPT=`$CAT $master_opt_file | $SED -e "s;\\$MYSQL_TEST_DIR;$MYSQL_TEST_DIR;"`
     case "$EXTRA_MASTER_OPT" in
       --timezone=*)
	 TZ=`$ECHO "$EXTRA_MASTER_OPT" | $SED -e "s;--timezone=;;"`
	 export TZ
	 # Note that this must be set to space, not "" for test-reset to work
	 EXTRA_MASTER_OPT=" "
	 ;;
       --result-file=*)
         result_file=`$ECHO "$EXTRA_MASTER_OPT" | $SED -e "s;--result-file=;;"`
         result_file="r/$result_file.result"
         if [ -n "$RESULT_EXT" -a \( x$RECORD = x1 -o -f "$result_file$RESULT_EXT" \) ] ; then
	   result_file="$result_file$RESULT_EXT"
	 fi
	 # Note that this must be set to space, not "" for test-reset to work
	 EXTRA_MASTER_OPT=" "
         ;;
     esac
     stop_master
     echo "CURRENT_TEST: $tname" >> $MASTER_MYERR
     start_master
     TZ=$MY_TZ; export TZ
   else
     # If we had extra master opts to the previous run
     # or there is no master running (FIXME strange.....)
     # or there is a master init script
     if [ ! -z "$EXTRA_MASTER_OPT" ] || [ x$MASTER_RUNNING != x1 ] || \
	[ -f $master_init_script ]
     then
       EXTRA_MASTER_OPT=""
       stop_master
       echo "CURRENT_TEST: $tname" >> $MASTER_MYERR
       start_master
     else
       echo "CURRENT_TEST: $tname" >> $MASTER_MYERR
     fi
   fi

   # We never start a slave if embedded server is used
   if [ "x$USE_EMBEDDED_SERVER" != "x1" ] ; then
     do_slave_restart=0
     if [ -f $slave_opt_file ] ;
     then
       EXTRA_SLAVE_OPT=`$CAT $slave_opt_file | $SED -e "s;\\$MYSQL_TEST_DIR;$MYSQL_TEST_DIR;"`
       do_slave_restart=1
     else
      if [ ! -z "$EXTRA_SLAVE_OPT" ] || [ x$SLAVE_RUNNING != x1 ] ;
      then
	EXTRA_SLAVE_OPT=""
	do_slave_restart=1
      fi
     fi

     if [ -f $slave_master_info_file ] ; then
       SLAVE_MASTER_INFO=`$CAT $slave_master_info_file`
       do_slave_restart=1
     else
       if [ ! -z "$SLAVE_MASTER_INFO" ] || [ x$SLAVE_RUNNING != x1 ] ;
       then
	 SLAVE_MASTER_INFO=""
	 do_slave_restart=1
       fi
     fi

     if [ x$do_slave_restart = x1 ] ; then
       stop_slave
       echo "CURRENT_TEST: $tname" >> $SLAVE_MYERR
       start_slave
     else
       echo "CURRENT_TEST: $tname" >> $SLAVE_MYERR
     fi
     if [ x$many_slaves = x1 ]; then
      start_slave 1
      start_slave 2
     fi
   fi
 fi
 cd $MYSQL_TEST_DIR

 if [ -f $tf ] ; then
    $RM -f r/$tname.*reject
    mysql_test_args="-R $result_file $EXTRA_MYSQL_TEST_OPT"
    if [ -z "$DO_CLIENT_GDB" ] ; then
      `$MYSQL_TEST  $mysql_test_args < $tf 2> $TIMEFILE`;
    else
      do_gdb_test "$mysql_test_args" "$tf"
    fi

    res=$?

    pname=`$ECHO "$tname                        "|$CUT -c 1-24`
    RES="$pname"

    if [ x$many_slaves = x1 ] ; then
     stop_slave 1
     stop_slave 2
    fi

    if [ $res = 0 ]; then
      total_inc
      pass_inc
      TIMER=""
      if [ -f "$MY_LOG_DIR/timer" ]; then
	TIMER=`cat $MY_LOG_DIR/timer`
	TIMER=`$PRINTF "%13s" $TIMER`
      fi
      $ECHO "$RES$RES_SPACE [ pass ]   $TIMER"
    else
      # why the following ``if'' ? That is why res==1 is special ?
      if [ $res = 2 ]; then
        skip_inc
	$ECHO "$RES$RES_SPACE [ skipped ]"
      else
        if [ $res -gt 2 ]; then
          $ECHO "mysqltest returned unexpected code $res, it has probably crashed" >> $TIMEFILE
        fi
	total_inc
        fail_inc
	$ECHO "$RES$RES_SPACE [ fail ]"
        $ECHO
	error_is
	show_failed_diff $tname
	$ECHO
	if [ x$FORCE != x1 ] ; then
	 $ECHO "Aborting: $tname failed. To continue, re-run with '--force'."
	 $ECHO
         if [ -z "$DO_GDB" ] && [ -z "$USE_RUNNING_SERVER" ] && \
	    [ -z "$DO_DDD" ] && [ -z "$USE_EMBEDDED_SERVER" ]
	 then
	   mysql_stop
	   stop_manager
   	 fi
	 exit 1
	fi

        if [ -z "$DO_GDB" ] && [ -z "$USE_RUNNING_SERVER" ] && \
	   [ -z "$DO_DDD" ] && [ -z "$USE_EMBEDDED_SERVER" ]
	then
	  mysql_restart
	fi
	$ECHO "Resuming Tests"
	$ECHO ""
      fi
    fi
  fi
}

######################################################################
# Main script starts here
######################################################################

[ "$DO_GCOV" -a ! -x "$GCOV" ] && error "No gcov found"

[ "$DO_GCOV" ] && gcov_prepare
[ "$DO_GPROF" ] && gprof_prepare

if [ -z "$USE_RUNNING_SERVER" ]
then
  if [ -z "$FAST_START" ]
  then
    # Ensure that no old mysqld test servers are running
    $MYSQLADMIN --no-defaults --socket=$MASTER_MYSOCK -u root -O connect_timeout=5 -O shutdown_timeout=20 shutdown > /dev/null 2>&1
    $MYSQLADMIN --no-defaults --socket=$SLAVE_MYSOCK -u root -O connect_timeout=5 -O shutdown_timeout=20 shutdown > /dev/null 2>&1
    $MYSQLADMIN --no-defaults --host=$hostname --port=$MASTER_MYPORT -u root -O connect_timeout=5 -O shutdown_timeout=20 shutdown > /dev/null 2>&1
    $MYSQLADMIN --no-defaults --host=$hostname --port=$SLAVE_MYPORT -u root -O connect_timeout=5 -O shutdown_timeout=20 shutdown > /dev/null 2>&1
    $MYSQLADMIN --no-defaults --host=$hostname --port=`expr $SLAVE_MYPORT + 1` -u root -O connect_timeout=5 -O shutdown_timeout=20 shutdown > /dev/null 2>&1
    sleep_until_file_deleted 0 $MASTER_MYPID
    sleep_until_file_deleted 0 $SLAVE_MYPID
  else
    rm $MASTER_MYPID $SLAVE_MYPID
  fi

  # Kill any running managers
  if [ -f "$MANAGER_PID_FILE" ]
  then
    kill `cat $MANAGER_PID_FILE`
    sleep 1
    if [ -f "$MANAGER_PID_FILE" ]
    then
      kill -9 `cat $MANAGER_PID_FILE`
      sleep 1
    fi
  fi

  if [ ! -z "$USE_NDBCLUSTER" ]
  then
  if [ -z "$USE_RUNNING_NDBCLUSTER" ]
  then
    # Kill any running ndbcluster stuff
    ./ndb/ndbcluster --data-dir=$MYSQL_TEST_DIR/var --port-base=$NDBCLUSTER_PORT --stop
  fi
  fi

  # Remove files that can cause problems
  $RM -rf $MYSQL_TEST_DIR/var/ndbcluster
  $RM -f $MYSQL_TEST_DIR/var/run/* $MYSQL_TEST_DIR/var/tmp/*

  # Remove old berkeley db log files that can confuse the server
  $RM -f $MASTER_MYDDIR/log.*

  wait_for_master=$SLEEP_TIME_FOR_FIRST_MASTER
  wait_for_slave=$SLEEP_TIME_FOR_FIRST_SLAVE
  $ECHO "Installing Test Databases"
  mysql_install_db

  if [ ! -z "$USE_NDBCLUSTER" ]
  then
  if [ -z "$USE_RUNNING_NDBCLUSTER" ]
  then
    echo "Starting ndbcluster"
    if [ "$DO_BENCH" = 1 ]
    then
      NDBCLUSTER_OPTS=""
    else
      NDBCLUSTER_OPTS="--small"
    fi
    ./ndb/ndbcluster --port-base=$NDBCLUSTER_PORT $NDBCLUSTER_OPTS --diskless --initial --data-dir=$MYSQL_TEST_DIR/var || exit 1
    USE_NDBCLUSTER="$USE_NDBCLUSTER --ndb-connectstring=\"host=localhost:$NDBCLUSTER_PORT\""
  else
    USE_NDBCLUSTER="$USE_NDBCLUSTER --ndb-connectstring=\"$USE_RUNNING_NDBCLUSTER\""
    echo "Using ndbcluster at $USE_NDBCLUSTER"
  fi
  fi

  start_manager

# Do not automagically start daemons if we are in gdb or running only one test
# case
  if [ -z "$DO_GDB" ] && [ -z "$DO_DDD" ]
  then
    mysql_start
  fi
  $ECHO  "Loading Standard Test Databases"
  mysql_loadstd
fi

if [ "x$START_AND_EXIT" = "x1" ] ; then
 echo "Servers started, exiting"
 exit
fi

$ECHO  "Starting Tests"

#
# This can probably be deleted
#
if [ "$DO_BENCH" = 1 ]
then
  start_master

  if [ "$DO_SMALL_BENCH" = 1 ]
  then
    EXTRA_BENCH_ARGS="--small-test --small-tables"
  fi

  if [ ! -z "$USE_NDBCLUSTER" ]
  then
    EXTRA_BENCH_ARGS="--create-options=TYPE=ndb $EXTRA_BENCH_ARGS"
  fi 

  BENCHDIR=$BASEDIR/sql-bench/
  savedir=`pwd`
  cd $BENCHDIR
  if [ -z "$1" ]
  then
    ./run-all-tests --socket=$MASTER_MYSOCK --user=root $EXTRA_BENCH_ARGS --log
  else
    if [ -x "./$1" ]
    then
       ./$1 --socket=$MASTER_MYSOCK --user=root $EXTRA_BENCH_ARGS
    else
      echo "benchmark $1 not found"
    fi
  fi
  cd $savedir
  mysql_stop
  stop_manager
  exit
fi

$ECHO
$ECHO "TEST                            RESULT        TIME (ms)"
$ECHO $DASH72

if [ -z "$1" ] ;
then
  if [ x$RECORD = x1 ]; then
    $ECHO "Will not run in record mode without a specific test case."
  else
    for tf in $TESTDIR/*.$TESTSUFFIX
    do
      run_testcase $tf
    done
    $RM -f $TIMEFILE	# Remove for full test
  fi
else
  while [ ! -z "$1" ]; do
    tname=`$BASENAME $1 .test`
    tf=$TESTDIR/$tname.$TESTSUFFIX
    if [ -f $tf ] ; then
      run_testcase $tf
    else
      $ECHO "Test case $tf does not exist."
    fi
    shift
  done
fi

$ECHO $DASH72
$ECHO

if [ -z "$DO_GDB" ] && [ -z "$USE_RUNNING_SERVER" ] && [ -z "$DO_DDD" ]
then
    mysql_stop
fi

if [ ! -z "$USE_NDBCLUSTER" ]
then
if [ -z "$USE_RUNNING_NDBCLUSTER" ]
then
  # Kill any running ndbcluster stuff
  ./ndb/ndbcluster --data-dir=$MYSQL_TEST_DIR/var --port-base=$NDBCLUSTER_PORT --stop
fi
fi

stop_manager
report_stats
$ECHO

[ "$DO_GCOV" ] && gcov_collect # collect coverage information
[ "$DO_GPROF" ] && gprof_collect # collect coverage information

exit 0
