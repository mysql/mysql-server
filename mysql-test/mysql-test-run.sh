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
RES_SPACE="      "
MYSQLD_SRC_DIRS="strings mysys include extra regex isam merge myisam \
 myisammrg heap sql"

MASTER_RUNNING=0
MASTER_MYPORT=9306
SLAVE_RUNNING=0
SLAVE_MYPORT=9307
NO_SLAVE=0

EXTRA_MASTER_OPT=""
EXTRA_MYSQL_TEST_OPT=""
USE_RUNNING_SERVER=1
DO_GCOV=""
DO_GDB=""
DO_DDD=""
DO_CLIENT_GDB=""
SLEEP_TIME=2
DBUSER=""

while test $# -gt 0; do
  case "$1" in
    --user=*) DBUSER=`$ECHO "$1" | $SED -e "s;--user=;;"` ;;
    --force)  FORCE=1 ;;
    --local)   USE_RUNNING_SERVER="" ;;
    --tmpdir=*) MYSQL_TMP_DIR=`$ECHO "$1" | $SED -e "s;--tmpdir=;;"` ;;
    --master_port=*) MASTER_MYPORT=`$ECHO "$1" | $SED -e "s;--master_port=;;"` ;;
    --slave_port=*) SLAVE_MYPORT=`$ECHO "$1" | $SED -e "s;--slave_port=;;"` ;;
    --skip-innobase)
     EXTRA_MASTER_MYSQLD_OPT="$EXTRA_MASTER_MYSQLD_OPT --skip-innobase"
     EXTRA_SLAVE_MYSQLD_OPT="$EXTRA_SLAVE_MYSQLD_OPT --skip-innobase" ;;
    --skip-bdb)
     EXTRA_MASTER_MYSQLD_OPT="$EXTRA_MASTER_MYSQLD_OPT --skip-bdb"
     EXTRA_SLAVE_MYSQLD_OPT="$EXTRA_SLAVE_MYSQLD_OPT --skip-bdb" ;;
    --skip-rpl) NO_SLAVE=1 ;;
    --skip-test=*) SKIP_TEST=`$ECHO "$1" | $SED -e "s;--skip-test=;;"`;;
    --record)
      RECORD=1;
      EXTRA_MYSQL_TEST_OPT="$EXTRA_MYSQL_TEST_OPT $1" ;;
    --bench)
      DO_BENCH=1
      NO_SLAVE=1
      ;;  
    --big*)			# Actually --big-test
      EXTRA_MYSQL_TEST_OPT="$EXTRA_MYSQL_TEST_OPT $1" ;;
    --sleep=*)
      EXTRA_MYSQL_TEST_OPT="$EXTRA_MYSQL_TEST_OPT $1"
      SLEEP_TIME=`$ECHO "$1" | $SED -e "s;--sleep=;;"`
      ;;
    --mysqld=*)
       TMP=`$ECHO "$1" | $SED -e "s;--mysqld-=;"`
       EXTRA_MYSQL_TEST_OPT="$EXTRA_MYSQL_TEST_OPT $TMP"
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
       --debug=d:t:O,$MYSQL_TMP_DIR/master.trace"
      EXTRA_SLAVE_MYSQLD_OPT="$EXTRA_SLAVE_MYSQLD_OPT \
       --debug=d:t:O,$MYSQL_TMP_DIR/slave.trace"
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
MASTER_MYSOCK="$MYSQL_TMP_DIR/mysql-master.sock"
MASTER_MYPID="$MYRUN_DIR/mysqld.pid"
MASTER_MYLOG="$MYSQL_TEST_DIR/var/log/mysqld.log"
MASTER_MYERR="$MYSQL_TEST_DIR/var/log/mysqld.err"

SLAVE_MYDDIR="$MYSQL_TEST_DIR/var/slave-data"
SLAVE_MYSOCK="$MYSQL_TMP_DIR/mysql-slave.sock"
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
 MYSQL_TEST="$BASEDIR/client/mysqltest"
 MYSQLADMIN="$BASEDIR/client/mysqladmin"
 MYSQL="$BASEDIR/client/mysql"
 LANGUAGE="$BASEDIR/sql/share/english/"
 INSTALL_DB="./install_test_db"
else
 MYSQLD="$BASEDIR/bin/mysqld"
 MYSQL_TEST="$BASEDIR/bin/mysqltest"
 MYSQLADMIN="$BASEDIR/bin/mysqladmin"
 MYSQL="$BASEDIR/bin/mysql"
 INSTALL_DB="./install_test_db -bin"
 if test -d "$BASEDIR/share/mysql/english" 
 then
   LANGUAGE="$BASEDIR/share/mysql/english/"
 else
   LANGUAGE="$BASEDIR/share/english/"
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


MYSQL_TEST_ARGS="--no-defaults --socket=$MASTER_MYSOCK --database=$DB --user=$DBUSER --password=$DBPASSWD --silent -v --tmpdir=$MYSQL_TMP_DIR"
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

# We can't use diff -u as this isn't portable

show_failed_diff ()
{
  reject_file=r/$1.reject
  result_file=r/$1.result
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


start_master()
{
    [ x$MASTER_RUNNING = 1 ] && return
    #run master initialization shell script if one exists
    if [ -f "$master_init_script" ] ;
    then
        /bin/sh $master_init_script
    fi
    cd $BASEDIR # for gcov
    # Remove old berkeley db log files that can confuse the server
    $RM -f $MASTER_MYDDIR/log.*	
    #start master
    if [ -z "$DO_BENCH" ]
    then
      master_args="--no-defaults --log-bin=master-bin \
    	    --server-id=1 \
            --basedir=$MY_BASEDIR \
	    --port=$MASTER_MYPORT \
	    --exit-info=256 \
            --datadir=$MASTER_MYDDIR \
	    --pid-file=$MASTER_MYPID \
	    --socket=$MASTER_MYSOCK \
            --log=$MASTER_MYLOG --default-character-set=latin1 \
	    --tmpdir=$MYSQL_TMP_DIR \
	    --language=$LANGUAGE \
            --innodb_data_file_path=ibdata1:50M \
	     $SMALL_SERVER \
	     $EXTRA_MASTER_OPT $EXTRA_MASTER_MYSQLD_OPT"
    else
      master_args="--no-defaults --log-bin=master-bin --server-id=1 \
            --basedir=$MY_BASEDIR \
	    --port=$MASTER_MYPORT \
            --datadir=$MASTER_MYDDIR \
	    --pid-file=$MASTER_MYPID \
	    --socket=$MASTER_MYSOCK \
            --default-character-set=latin1 \
	    --core \
	    --tmpdir=$MYSQL_TMP_DIR \
	    --language=$LANGUAGE \
            --innodb_data_file_path=ibdata1:50M \
	     $SMALL_SERVER \
	     $EXTRA_MASTER_OPT $EXTRA_MASTER_MYSQLD_OPT"
    fi	     
    if [ x$DO_DDD = x1 ]
    then
      $ECHO "set args $master_args" > $GDB_MASTER_INIT
      ddd --debugger "gdb -x $GDB_MASTER_INIT" $MYSQLD &
      prompt_user "Hit enter to continue after you've started the master"
    elif [ x$DO_GDB = x1 ]
    then
      $ECHO "set args $master_args" > $GDB_MASTER_INIT
      xterm -title "Master" -e gdb -x $GDB_MASTER_INIT $MYSQLD &
      prompt_user "Hit enter to continue after you've started the master"
    else	    
      $MYSQLD $master_args  >> $MASTER_MYERR 2>&1 &
    fi  
  wait_for_server_start $MASTER_MYPORT
  MASTER_RUNNING=1
}

start_slave()
{
    [ x$SKIP_SLAVE = x1 ] && return
    [ x$SLAVE_RUNNING = 1 ] && return
    
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
	    --server-id=2"
   else
     master_info=$SLAVE_MASTER_INFO
   fi	    
    
    $RM -f $SLAVE_MYDDIR/log.*	
    slave_args="--no-defaults $master_info \
    	    --exit-info=256 \
	    --log-bin=slave-bin --log-slave-updates \
            --basedir=$MY_BASEDIR \
            --datadir=$SLAVE_MYDDIR \
	    --pid-file=$SLAVE_MYPID \
	    --port=$SLAVE_MYPORT \
	    --socket=$SLAVE_MYSOCK \
            --log=$SLAVE_MYLOG --default-character-set=latin1 \
	    --core \
	    --tmpdir=$MYSQL_TMP_DIR \
            --language=$LANGUAGE \
	    --skip-innodb --skip-slave-start \
	    --report-host=127.0.0.1 --report-user=root \
	    --report-port=$SLAVE_MYPORT \
	     $SMALL_SERVER \
             $EXTRA_SLAVE_OPT $EXTRA_SLAVE_MYSQLD_OPT"
    if [ x$DO_DDD = x1 ]
    then
      $ECHO "set args $master_args" > $GDB_SLAVE_INIT
      ddd --debugger "gdb -x $GDB_SLAVE_INIT" $SLAVE_MYSQLD &
      prompt_user "Hit enter to continue after you've started the master"
    elif [ x$DO_GDB = x1 ]
    then
      $ECHO "set args $slave_args" > $GDB_SLAVE_INIT
      xterm -title "Slave" -e gdb -x $GDB_SLAVE_INIT $SLAVE_MYSQLD &
      prompt_user "Hit enter to continue after you've started the slave"
    else
      $SLAVE_MYSQLD $slave_args  >> $SLAVE_MYERR 2>&1 &
    fi
    wait_for_server_start $SLAVE_MYPORT
    SLAVE_RUNNING=1
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
  if [ x$SLAVE_RUNNING = x1 ]
  then
    $MYSQLADMIN --no-defaults --socket=$SLAVE_MYSOCK -u root -O shutdown_timeout=10 shutdown
    if [ $? != 0 ] && [ -f $SLAVE_MYPID ]
    then # try harder!
     $ECHO "slave not cooperating with mysqladmin, will try manual kill"
     kill `$CAT $SLAVE_MYPID`
     sleep $SLEEP_TIME
     if [ -f $SLAVE_MYPID ] ; then
       $ECHO "slave refused to die. Sending SIGKILL"
       kill -9 `$CAT $SLAVE_MYPID`
       $RM -f $SLAVE_MYPID
     else
      $ECHO "slave responded to SIGTERM " 
     fi
    fi
    SLAVE_RUNNING=0
  fi  
}

stop_master ()
{
  if [ x$MASTER_RUNNING = x1 ]
  then
    $MYSQLADMIN --no-defaults --socket=$MASTER_MYSOCK -u root -O shutdown_timeout=10 shutdown
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
 if [ -n "$SKIP_TEST" ] ; then 
   SKIP_THIS_TEST=`$EXPR \( $tname : "$SKIP_TEST" \) != 0`
   if [ x$SKIP_THIS_TEST = x1 ] ;
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

    if [ $res = 0 ]; then
      total_inc
      pass_inc
      $ECHO "$RES$RES_SPACE [ pass ]"
    else
      if [ $res = 1 ]; then
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
   	 fi
	 exit 1
	fi
	 
        if [ -z "$DO_GDB" ] && [ -z "$USE_RUNNING_SERVER" ] && [ -z "$DO_DDD" ]
	then
	  mysql_restart
	fi
	$ECHO "Resuming Tests"
	$ECHO ""
      else
        skip_inc
	$ECHO "$RES$RES_SPACE [ skipped ]"
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

report_stats
$ECHO

[ "$DO_GCOV" ] && gcov_collect # collect coverage information
[ "$DO_GPROF" ] && gprof_collect # collect coverage information

exit 0
