#! /bin/sh
# mysql-test-run - originally written by Matt Wagner <matt@mysql.com>
# modified by Sasha Pachev <sasha@mysql.com>
# Sligtly updated by Monty
# Cleaned up again by Matt
# Fixed by Sergei
# :-)

#++
# Access Definitions
#--
DB=test
DBPASSWD=
VERBOSE=""
NO_MANAGER=""
TZ=GMT-3; export TZ # for UNIX_TIMESTAMP tests to work

#++
# Program Definitions
#--

PATH=/bin:/usr/bin:/usr/local/bin:/usr/bsd:/usr/X11R6/bin

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
    echo "which: no $file in ($PATH)"
    exit 1
  done
  IFS="$save_ifs"
}


# No paths below as we can't be sure where the program is!

BASENAME=`which basename | head -1`
DIFF=`which diff | head -1`
CAT=cat
CUT=cut
TAIL=tail
ECHO=echo # use internal echo if possible
EXPR=expr # use internal if possible
FIND=find
GCOV=`which gcov | head -1`
PRINTF=printf
RM=rm
TIME=time
TR=tr
XARGS=`which xargs | head -1`
SED=sed

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
MYSQL_TMP_DIR=$MYSQL_TEST_DIR/var/tmp
SLAVE_LOAD_TMPDIR=../../var/tmp #needs to be same length to test logging
RES_SPACE="      "
MYSQLD_SRC_DIRS="strings mysys include extra regex isam merge myisam \
 myisammrg heap sql"
#
# Set LD_LIBRARY_PATH if we are using shared libraries
#
LD_LIBRARY_PATH="$BASEDIR/lib:$LD_LIBRARY_PATH"
export LD_LIBRARY_PATH

MASTER_RUNNING=0
MASTER_MYPORT=9306
SLAVE_RUNNING=0
SLAVE_MYPORT=9307
MYSQL_MANAGER_PORT=9305 # needs to be out of the way of slaves
MYSQL_MANAGER_PW_FILE=$MYSQL_TEST_DIR/var/tmp/manager.pwd
MYSQL_MANAGER_LOG=$MYSQL_TEST_DIR/var/log/manager.log
MYSQL_MANAGER_USER=root
NO_SLAVE=0

EXTRA_MASTER_OPT=""
EXTRA_MYSQL_TEST_OPT=""
USE_RUNNING_SERVER=1
DO_GCOV=""
DO_GDB=""
DO_DDD=""
DO_CLIENT_GDB=""
SLEEP_TIME=2
CHARACTER_SET=latin1
DBUSER=""
START_WAIT_TIMEOUT=3
STOP_WAIT_TIMEOUT=3

while test $# -gt 0; do
  case "$1" in
    --user=*) DBUSER=`$ECHO "$1" | $SED -e "s;--user=;;"` ;;
    --force)  FORCE=1 ;;
    --verbose-manager)  MANAGER_QUIET_OPT="" ;;
    --local)   USE_RUNNING_SERVER="" ;;
    --tmpdir=*) MYSQL_TMP_DIR=`$ECHO "$1" | $SED -e "s;--tmpdir=;;"` ;;
    --master_port=*) MASTER_MYPORT=`$ECHO "$1" | $SED -e "s;--master_port=;;"` ;;
    --slave_port=*) SLAVE_MYPORT=`$ECHO "$1" | $SED -e "s;--slave_port=;;"` ;;
    --manager-port=*) MYSQL_MANAGER_PORT=`$ECHO "$1" | $SED -e "s;--manager_port=;;"` ;;
    --with-openssl)
     EXTRA_MASTER_MYSQLD_OPT="$EXTRA_MASTER_MYSQLD_OPT \
     --ssl-ca=$BASEDIR/SSL/cacert.pem \
     --ssl-cert=$BASEDIR/SSL/server-cert.pem \
     --ssl-key=$BASEDIR/SSL/server-key.pem"
     EXTRA_SLAVE_MYSQLD_OPT="$EXTRA_SLAVE_MYSQLD_OPT \
     --ssl-ca=$BASEDIR/SSL/cacert.pem \
     --ssl-cert=$BASEDIR/SSL/server-cert.pem \
     --ssl-key=$BASEDIR/SSL/server-key.pem" ;;
    --no-manager)
     NO_MANAGER=1
     ;;
    --skip-innobase)
     EXTRA_MASTER_MYSQLD_OPT="$EXTRA_MASTER_MYSQLD_OPT --skip-innobase"
     EXTRA_SLAVE_MYSQLD_OPT="$EXTRA_SLAVE_MYSQLD_OPT --skip-innobase" ;;
    --skip-bdb)
     EXTRA_MASTER_MYSQLD_OPT="$EXTRA_MASTER_MYSQLD_OPT --skip-bdb"
     EXTRA_SLAVE_MYSQLD_OPT="$EXTRA_SLAVE_MYSQLD_OPT --skip-bdb" ;;
    --skip-rpl) NO_SLAVE=1 ;;
    --skip-test=*) SKIP_TEST=`$ECHO "$1" | $SED -e "s;--skip-test=;;"`;;
    --do-test=*) DO_TEST=`$ECHO "$1" | $SED -e "s;--do-test=;;"`;;
    --wait-timeout=*)
     START_WAIT_TIMEOUT=`$ECHO "$1" | $SED -e "s;--wait-timeout=;;"`
     STOP_WAIT_TIMEOUT=$START_WAIT_TIMEOUT;;
    --record)
      RECORD=1;
      EXTRA_MYSQL_TEST_OPT="$EXTRA_MYSQL_TEST_OPT $1" ;;
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
      SLEEP_TIME=`$ECHO "$1" | $SED -e "s;--sleep=;;"`
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
      USE_RUNNING_SERVER=""
      ;;
    --client-gdb )
      if [ x$BINARY_DIST = x1 ] ; then
	$ECHO "Note: you will get more meaningful output on a source distribution compiled with debugging option when running tests with --client-gdb option"
      fi
      DO_CLIENT_GDB=1
      ;;
    --ddd )
      if [ x$BINARY_DIST = x1 ] ; then
	$ECHO "Note: you will get more meaningful output on a source distribution compiled with debugging option when running tests with --ddd option"
      fi
      DO_DDD=1
      USE_RUNNING_SERVER=""
      ;;
    --skip-*)
      EXTRA_MASTER_MYSQLD_OPT="$EXTRA_MASTER_MYSQLD_OPT $1"
      EXTRA_SLAVE_MYSQLD_OPT="$EXTRA_SLAVE_MYSQLD_OPT $1"
      ;;
    --debug)
      EXTRA_MASTER_MYSQLD_OPT="$EXTRA_MASTER_MYSQLD_OPT \
       --debug=d:t:i:O,$MYSQL_TEST_DIR/var/log/master.trace"
      EXTRA_SLAVE_MYSQLD_OPT="$EXTRA_SLAVE_MYSQLD_OPT \
       --debug=d:t:i:O,$MYSQL_TEST_DIR/var/log/slave.trace"
      EXTRA_MYSQL_TEST_OPT="$EXTRA_MYSQL_TEST_OPT --debug"
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
MASTER_MYDDIR="$MYSQL_TEST_DIR/var/master-data"
MASTER_MYSOCK="$MYSQL_TMP_DIR/master.sock"
MASTER_MYPID="$MYRUN_DIR/mysqld.pid"
MASTER_MYLOG="$MYSQL_TEST_DIR/var/log/mysqld.log"
MASTER_MYERR="$MYSQL_TEST_DIR/var/log/mysqld.err"

SLAVE_MYDDIR="$MYSQL_TEST_DIR/var/slave-data"
SLAVE_MYSOCK="$MYSQL_TMP_DIR/slave.sock"
SLAVE_MYPID="$MYRUN_DIR/mysqld-slave.pid"
SLAVE_MYLOG="$MYSQL_TEST_DIR/var/log/mysqld-slave.log"
SLAVE_MYERR="$MYSQL_TEST_DIR/var/log/mysqld-slave.err"

SMALL_SERVER="-O key_buffer_size=1M -O sort_buffer=256K -O max_heap_table_size=1M"

export MASTER_MYPORT
export SLAVE_MYPORT

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

[ -z "$COLUMNS" ] && COLUMNS=80
E=`$EXPR $COLUMNS - 8`
#DASH72=`$EXPR substr '------------------------------------------------------------------------' 1 $E`
DASH72=`$ECHO '------------------------------------------------------------------------'|$CUT -c 1-$E`

# on source dist, we pick up freshly build executables
# on binary, use what is installed
if [ x$SOURCE_DIST = x1 ] ; then
 MYSQLD="$BASEDIR/sql/mysqld"
 if [ -f "$BASEDIR/client/.libs/lt-mysqltest" ] ; then
   MYSQL_TEST="$BASEDIR/client/.libs/lt-mysqltest"
 else
   MYSQL_TEST="$BASEDIR/client/mysqltest"
 fi
 MYSQLADMIN="$BASEDIR/client/mysqladmin"
 MYSQL_MANAGER_CLIENT="$BASEDIR/client/mysqlmanagerc"
 MYSQL_MANAGER="$BASEDIR/tools/mysqlmanager"
 MYSQL_MANAGER_PWGEN="$BASEDIR/client/mysqlmanager-pwgen"
 MYSQL="$BASEDIR/client/mysql"
 LANGUAGE="$BASEDIR/sql/share/english/"
 CHARSETSDIR="$BASEDIR/sql/share/charsets"
 INSTALL_DB="./install_test_db"
else
 MYSQLD="$BASEDIR/bin/mysqld"
 MYSQL_TEST="$BASEDIR/bin/mysqltest"
 MYSQLADMIN="$BASEDIR/bin/mysqladmin"
 MYSQL_MANAGER="$BASEDIR/bin/mysqlmanager"
 MYSQL_MANAGER_CLIENT="$BASEDIR/bin/mysqlmanagerc"
 MYSQL_MANAGER_PWGEN="$BASEDIR/bin/mysqlmanager-pwgen"
 MYSQL="$BASEDIR/bin/mysql"
 INSTALL_DB="./install_test_db -bin"
 if test -d "$BASEDIR/share/mysql/english" 
 then
   LANGUAGE="$BASEDIR/share/mysql/english/"
   CHARSETSDIR="$BASEDIR/share/mysql/charsets"
 else
   LANGUAGE="$BASEDIR/share/english/"
   CHARSETSDIR="$BASEDIR/share/charsets"
  fi
fi

# If we should run all tests cases, we will use a local server for that

if [ -z "$1" ]
then
   USE_RUNNING_SERVER=""
fi
if [ -n "$USE_RUNNING_SERVER" ]
then
   MASTER_MYSOCK="/tmp/mysql.sock"
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


MYSQL_TEST_ARGS="--no-defaults --socket=$MASTER_MYSOCK --database=$DB \
 --user=$DBUSER --password=$DBPASSWD --silent -v \
 --tmpdir=$MYSQL_TMP_DIR"
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
TIMEFILE="$MYSQL_TMP_DIR/mysqltest-time"
SLAVE_MYSQLD=$MYSQLD #this can be changed later if we are doing gcov
XTERM=`which xterm`

#++
# Function Definitions
#--
wait_for_server_start ()
{
   $MYSQLADMIN --no-defaults -u $DBUSER --silent -O connect_timeout=10 -w2 --host=$hostname --port=$1  ping >/dev/null 2>&1
}

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
    echo "http://www.mysql.com/doc/R/e/Reporting_mysqltest_bugs.html"
    echo "to find the reason to this problem and how to report this."
  fi  
}

do_gdb_test ()
{
  mysql_test_args="$MYSQL_TEST_ARGS $1"
  $ECHO "set args $mysql_test_args < $2" > $GDB_CLIENT_INIT
  echo "Set breakpoints ( if needed) and type 'run' in gdb window"
  #this xterm should not be backgrounded
  xterm -title "Client" -e gdb -x $GDB_CLIENT_INIT $MYSQL_TEST_BIN
}

error () {
    $ECHO  "Error:  $1"
    exit 1
}

error_is () {
    $TR "\n" " " < $TIMEFILE | $SED -e 's/.* At line \(.*\)\: \(.*\)Command .*$/   \>\> Error at line \1: \2<\</'
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
        $ECHO "The log files in $MYSQL_TEST_DIR/var/log may give you some hint"
	$ECHO "of what when wrong."
	$ECHO "If you want to report this error, please read first the documentation at"
        $ECHO "http://www.mysql.com/doc/M/y/MySQL_test_suite.html"
    fi
}

mysql_install_db () {
    $ECHO "Removing Stale Files"
    $RM -rf $MASTER_MYDDIR $SLAVE_MYDDIR $SLAVE_MYLOG $MASTER_MYLOG \
     $SLAVE_MYERR $MASTER_MYERR
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
     rm -rf var/slave$slave_num-data/
     mkdir -p var/slave$slave_num-data/mysql
     mkdir -p var/slave$slave_num-data/test
     cp var/slave-data/mysql/* var/slave$slave_num-data/mysql
    done
    # Give mysqld some time to die.
    sleep $SLEEP_TIME
    return 0
}

gprof_prepare ()
{
 rm -rf $GPROF_DIR
 mkdir -p $GPROF_DIR 
}

gprof_collect ()
{
 if [ -f $MASTER_MYDDIR/gmon.out ]; then
   gprof $MYSQLD $MASTER_MYDDIR/gmon.out > $GPROF_MASTER
   echo "Master execution profile has been saved in $GPROF_MASTER"
 fi
 if [ -f $SLAVE_MYDDIR/gmon.out ]; then
   gprof $MYSQLD $SLAVE_MYDDIR/gmon.out > $GPROF_SLAVE
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
 if [ -n "$NO_MANAGER" ] ; then
  echo "Manager disabled, skipping manager start. Tests requiring manager will\
 be skipped"
  return
 fi
 MYSQL_MANAGER_PW=`$MYSQL_MANAGER_PWGEN -u $MYSQL_MANAGER_USER \
 -o $MYSQL_MANAGER_PW_FILE`
 $MYSQL_MANAGER --log=$MYSQL_MANAGER_LOG --port=$MYSQL_MANAGER_PORT \
  --password-file=$MYSQL_MANAGER_PW_FILE
  abort_if_failed "Could not start MySQL manager"
  mysqltest_manager_args="--manager-host=localhost \
  --manager-user=$MYSQL_MANAGER_USER \
  --manager-password=$MYSQL_MANAGER_PW \
  --manager-port=$MYSQL_MANAGER_PORT \
  --manager-wait-timeout=$START_WAIT_TIMEOUT"
  MYSQL_TEST="$MYSQL_TEST $mysqltest_manager_args"
  MYSQL_TEST_ARGS="$MYSQL_TEST_ARGS $mysqltest_manager_args"
  
}

stop_manager()
{
 if [ -n "$NO_MANAGER" ] ; then
  return
 fi
 $MYSQL_MANAGER_CLIENT $MANAGER_QUIET_OPT -u$MYSQL_MANAGER_USER \
  -p$MYSQL_MANAGER_PW -P $MYSQL_MANAGER_PORT <<EOF
shutdown
EOF
}

manager_launch()
{
  ident=$1
  shift
  if [ -n "$NO_MANAGER" ] ; then
   $@  >$CUR_MYERR 2>&1  &
   sleep 2 #hack 
   return
  fi
  $MYSQL_MANAGER_CLIENT $MANAGER_QUIET_OPT --user=$MYSQL_MANAGER_USER \
   --password=$MYSQL_MANAGER_PW  --port=$MYSQL_MANAGER_PORT <<EOF
def_exec $ident $@
set_exec_stdout $ident $CUR_MYERR
set_exec_stderr $ident $CUR_MYERR
set_exec_con $ident root localhost $CUR_MYSOCK
start_exec $ident $START_WAIT_TIMEOUT
EOF
 abort_if_failed "Could not execute manager command"
}

manager_term()
{
  ident=$1
  shift
  if [ -n "$NO_MANAGER" ] ; then
   $MYSQLADMIN --no-defaults -uroot --socket=$MYSQL_TMP_DIR/$ident.sock -O \
   connect_timeout=5 shutdown >/dev/null 2>&1
   return
  fi
  $MYSQL_MANAGER_CLIENT $MANAGER_QUIET_OPT --user=$MYSQL_MANAGER_USER \
   --password=$MYSQL_MANAGER_PW  --port=$MYSQL_MANAGER_PORT <<EOF
stop_exec $ident $STOP_WAIT_TIMEOUT
EOF
 abort_if_failed "Could not execute manager command"
}


start_master()
{
    [ x$MASTER_RUNNING = 1 ] && return
    # Remove old berkeley db log files that can confuse the server
    $RM -f $MASTER_MYDDIR/log.*
    # Remove stale binary logs
    $RM -f $MYSQL_TEST_DIR/var/log/master-bin.*
    #run master initialization shell script if one exists
    if [ -f "$master_init_script" ] ;
    then
        /bin/sh $master_init_script
    fi
    cd $BASEDIR # for gcov
    #start master
    if [ -z "$DO_BENCH" ]
    then
      master_args="--no-defaults --log-bin=$MYSQL_TEST_DIR/var/log/master-bin \
    	    --server-id=1 --rpl-recovery-rank=1 \
            --basedir=$MY_BASEDIR --init-rpl-role=master \
	    --port=$MASTER_MYPORT \
	    --exit-info=256 \
            --datadir=$MASTER_MYDDIR \
	    --pid-file=$MASTER_MYPID \
	    --socket=$MASTER_MYSOCK \
            --log=$MASTER_MYLOG \
	    --character-sets-dir=$CHARSETSDIR \
	    --default-character-set=$CHARACTER_SET \
	    --tmpdir=$MYSQL_TMP_DIR \
	    --language=$LANGUAGE \
            --innodb_data_file_path=ibdata1:50M \
	     $SMALL_SERVER \
	     $EXTRA_MASTER_OPT $EXTRA_MASTER_MYSQLD_OPT"
    else
      master_args="--no-defaults --log-bin=$MYSQL_TEST_DIR/var/log/master-bin \
	    --server-id=1 --rpl-recovery-rank=1 \
            --basedir=$MY_BASEDIR --init-rpl-role=master \
	    --port=$MASTER_MYPORT \
            --datadir=$MASTER_MYDDIR \
	    --pid-file=$MASTER_MYPID \
	    --socket=$MASTER_MYSOCK \
	    --character-sets-dir=$CHARSETSDIR \
            --default-character-set=$CHARACTER_SET \
	    --core \
	    --tmpdir=$MYSQL_TMP_DIR \
	    --language=$LANGUAGE \
            --innodb_data_file_path=ibdata1:50M \
	     $SMALL_SERVER \
	     $EXTRA_MASTER_OPT $EXTRA_MASTER_MYSQLD_OPT"
    fi
    
    CUR_MYERR=$MASTER_MYERR
    CUR_MYSOCK=$MASTER_MYSOCK
    
    if [ x$DO_DDD = x1 ]
    then
      $ECHO "set args $master_args" > $GDB_MASTER_INIT
      manager_launch master ddd -display $DISPLAY --debugger \
      "gdb -x $GDB_MASTER_INIT" $MYSQLD 
    elif [ x$DO_GDB = x1 ]
    then
      $ECHO "set args $master_args" > $GDB_MASTER_INIT
      manager_launch master $XTERM -display :0 -title "Master" -e gdb -x \
       $GDB_MASTER_INIT $MYSQLD 
    else	    
      manager_launch master $MYSQLD $master_args  
    fi  
  MASTER_RUNNING=1
}

start_slave()
{
    [ x$SKIP_SLAVE = x1 ] && return
    eval "this_slave_running=\$SLAVE$1_RUNNING"
    [ x$this_slave_running = 1 ] && return
    #when testing fail-safe replication, we will have more than one slave
    #in this case, we start secondary slaves with an argument
    slave_ident="slave$1"
    if [ -n "$1" ] ;
    then
     slave_server_id=`$EXPR 2 + $1`
     slave_rpl_rank=$slave_server_id
     slave_port=`expr $SLAVE_MYPORT + $1`
     slave_log="$SLAVE_MYLOG.$1"
     slave_err="$SLAVE_MYERR.$1"
     slave_datadir="var/$slave_ident-data/"
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
    # Remove stale binary logs
    $RM -f $MYSQL_TEST_DIR/var/log/$slave_ident-bin.*
    
    #run slave initialization shell script if one exists
    if [ -f "$slave_init_script" ] ;
    then
	  /bin/sh $slave_init_script
    fi
    
    if [ -z "$SLAVE_MASTER_INFO" ] ; then
      master_info="--master-user=root \
	    --master-connect-retry=1 \
	    --master-host=127.0.0.1 \
	    --master-password= \
	    --master-port=$MASTER_MYPORT \
	    --server-id=$slave_server_id --rpl-recovery-rank=$slave_rpl_rank"
   else
     master_info=$SLAVE_MASTER_INFO
   fi	    
    
    $RM -f $slave_datadir/log.*	
    slave_args="--no-defaults $master_info \
    	    --exit-info=256 \
	    --log-bin=$MYSQL_TEST_DIR/var/log/$slave_ident-bin \
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
	    --skip-innodb --skip-slave-start \
	    --slave-load-tmpdir=$SLAVE_LOAD_TMPDIR \
	    --report-host=127.0.0.1 --report-user=root \
	    --report-port=$slave_port \
	    --master-retry-count=5 \
	     $SMALL_SERVER \
             $EXTRA_SLAVE_OPT $EXTRA_SLAVE_MYSQLD_OPT"
    CUR_MYERR=$slave_err
    CUR_MYSOCK=$slave_sock
  
    if [ x$DO_DDD = x1 ]
    then
      $ECHO "set args $master_args" > $GDB_SLAVE_INIT
      manager_launch $slave_ident ddd -display $DISPLAY --debugger \
       "gdb -x $GDB_SLAVE_INIT" $SLAVE_MYSQLD 
    elif [ x$DO_GDB = x1 ]
    then
      $ECHO "set args $slave_args" > $GDB_SLAVE_INIT
      manager_launch $slave_ident $XTERM -display $DISPLAY -title "Slave" -e gdb -x \
       $GDB_SLAVE_INIT $SLAVE_MYSQLD 
    else
      manager_launch $slave_ident $SLAVE_MYSQLD $slave_args
    fi
    eval "SLAVE$1_RUNNING=1"
}

mysql_start () {
    $ECHO "Starting MySQL daemon"
    start_master
    start_slave
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
    manager_term $slave_ident
    if [ $? != 0 ] && [ -f $slave_pid ]
    then # try harder!
     $ECHO "slave not cooperating with mysqladmin, will try manual kill"
     kill `$CAT $slave_pid`
     sleep $SLEEP_TIME
     if [ -f $SLAVE_MYPID ] ; then
       $ECHO "slave refused to die. Sending SIGKILL"
       kill -9 `$CAT $slave_pid`
       $RM -f $slave_pid
     else
      $ECHO "slave responded to SIGTERM " 
     fi
    fi
    eval "SLAVE$1_RUNNING=0"
  fi  
}

stop_master ()
{
  if [ x$MASTER_RUNNING = x1 ]
  then
    manager_term master
    if [ $? != 0 ] && [ -f $MASTER_MYPID ]
    then # try harder!
     $ECHO "master not cooperating with mysqladmin, will try manual kill"
     kill `$CAT $MASTER_MYPID`
     sleep $SLEEP_TIME
     if [ -f $MASTER_MYPID ] ; then
       $ECHO "master refused to die. Sending SIGKILL"
       kill -9 `$CAT $MASTER_MYPID`
       $RM -f $MASTER_MYPID
     else
      $ECHO "master responded to SIGTERM " 
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

mysql_restart () {

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
 slave_master_info_file=$TESTDIR/$tname-slave-master-info.opt
 SKIP_SLAVE=`$EXPR \( $tname : rpl \) = 0`
 if [ -z "$NO_MANAGER" ] ; then
  many_slaves=`$EXPR \( $tname : rpl_failsafe \) != 0`
 fi 
 
 if [ -n "$SKIP_TEST" ] ; then 
   SKIP_THIS_TEST=`$EXPR \( $tname : "$SKIP_TEST" \) != 0`
   if [ x$SKIP_THIS_TEST = x1 ] ;
   then
    return;
   fi
  fi

 if [ -n "$DO_TEST" ] ; then 
   DO_THIS_TEST=`$EXPR \( $tname : "$DO_TEST" \) != 0`
   if [ x$DO_THIS_TEST = x0 ] ;
   then
    return;
   fi
  fi


 if [ x${NO_SLAVE}x$SKIP_SLAVE = x1x0 ] ;
 then
   USERT="    ...."
   SYST="    ...."
   REALT="    ...."
   timestr="$USERT $SYST $REALT"
   pname=`$ECHO "$tname                        "|$CUT -c 1-24`
   RES="$pname  $timestr"
   skip_inc
   $ECHO "$RES$RES_SPACE [ skipped ]"
   return
 fi

 if [ -z "$USE_RUNNING_SERVER" ] ;
 then
   if [ -f $master_opt_file ] ;
   then
     EXTRA_MASTER_OPT=`$CAT $master_opt_file`
     stop_master
     start_master
   else
     if [ ! -z "$EXTRA_MASTER_OPT" ] || [ x$MASTER_RUNNING != x1 ] ;
     then
       EXTRA_MASTER_OPT=""
       stop_master
       start_master
     fi  
   fi
   do_slave_restart=0
 
   if [ -f $slave_opt_file ] ;
   then
     EXTRA_SLAVE_OPT=`$CAT $slave_opt_file`
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
     start_slave
   fi
   if [ x$many_slaves = x1 ]; then
    start_slave 1
    start_slave 2
   fi
 fi
 cd $MYSQL_TEST_DIR
  
 if [ -f $tf ] ; then
    $RM -f r/$tname.*reject
    mysql_test_args="-R r/$tname.result $EXTRA_MYSQL_TEST_OPT"
    if [ -z "$DO_CLIENT_GDB" ] ; then
      mytime=`$TIME -p $MYSQL_TEST  $mysql_test_args < $tf 2> $TIMEFILE`
    else
      do_gdb_test "$mysql_test_args" "$tf"
    fi
     
    res=$?

    if [ $res = 0 ]; then
	mytime=`$CAT $TIMEFILE | $TAIL -3 | $TR '\n' ':'`

	USERT=`$ECHO $mytime | $CUT -d : -f 2 | $CUT -d ' ' -f 2`
        USERT=`prefix_to_8 $USERT`
	SYST=`$ECHO $mytime | $CUT -d : -f 3 | $CUT -d ' ' -f 2`
        SYST=`prefix_to_8 $SYST`
	REALT=`$ECHO $mytime | $CUT -d : -f 1 | $CUT -d ' ' -f 2`
        REALT=`prefix_to_8 $REALT`
    else
	USERT="    ...."
	SYST="    ...."
	REALT="    ...."
    fi

    timestr="$USERT $SYST $REALT"
    pname=`$ECHO "$tname                        "|$CUT -c 1-24`
    RES="$pname  $timestr"
    
    if [ x$many_slaves = x1 ] ; then
     stop_slave 1
     stop_slave 2
    fi
    
    if [ $res = 0 ]; then
      total_inc
      pass_inc
      $ECHO "$RES$RES_SPACE [ pass ]"
    else
      # why the following ``if'' ? That is why res==1 is special ?
      if [ $res = 2 ]; then
        skip_inc
	$ECHO "$RES$RES_SPACE [ skipped ]"
      else
	total_inc
        fail_inc
	$ECHO "$RES$RES_SPACE [ fail ]"
        $ECHO
	error_is
	show_failed_diff $tname
	$ECHO
	if [ x$FORCE != x1 ] ; then
	 $ECHO "Aborting. To continue, re-run with '--force'."
	 $ECHO
         if [ -z "$DO_GDB" ] && [ -z "$USE_RUNNING_SERVER" ] && [ -z "$DO_DDD" ]
	 then
	   mysql_stop
	   stop_manager
   	 fi
	 exit 1
	fi
	 
        if [ -z "$DO_GDB" ] && [ -z "$USE_RUNNING_SERVER" ] && [ -z "$DO_DDD" ]
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

# Ensure that no old mysqld test servers are running
if [ -z "$USE_RUNNING_SERVER" ]
then
  $MYSQLADMIN --no-defaults --socket=$MASTER_MYSOCK -u root -O connect_timeout=5 shutdown > /dev/null 2>&1
  $MYSQLADMIN --no-defaults --socket=$SLAVE_MYSOCK -u root -O connect_timeout=5 shutdown > /dev/null 2>&1
  $ECHO "Installing Test Databases"
  mysql_install_db
  $ECHO "Starting MySQL Manager"
  start_manager
#do not automagically start deamons if we are in gdb or running only one test
#case
  if [ -z "$DO_GDB" ] && [ -z "$DO_DDD" ]
  then
    mysql_start
  fi
  $ECHO  "Loading Standard Test Databases"
  mysql_loadstd
fi


$ECHO  "Starting Tests"

if [ "$DO_BENCH" = 1 ]
then
 BENCHDIR=$BASEDIR/sql-bench/
 savedir=`pwd`
 cd $BENCHDIR
 if [ -z "$1" ]
 then
  ./run-all-tests --socket=$MASTER_MYSOCK --user=root
 else
 if [ -x "./$1" ]
  then
   ./$1 --socket=$MASTER_MYSOCK --user=root
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
$ECHO " TEST                         USER   SYSTEM  ELAPSED        RESULT"
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
tname=`$BASENAME $1 .test`
 tf=$TESTDIR/$tname.$TESTSUFFIX
 if [ -f $tf ] ; then
  run_testcase $tf
 else
   $ECHO "Test case $tf does not exist."
 fi
fi

$ECHO $DASH72
$ECHO

if [ -z "$DO_GDB" ] && [ -z "$USE_RUNNING_SERVER" ] && [ -z "$DO_DDD" ]
then
    mysql_stop
fi

stop_manager
report_stats
$ECHO

[ "$DO_GCOV" ] && gcov_collect # collect coverage information
[ "$DO_GPROF" ] && gprof_collect # collect coverage information

exit 0
