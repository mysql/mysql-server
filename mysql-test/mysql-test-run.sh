#! /bin/sh
# mysql-test-run - originally written by Matt Wagner <matt@mysql.com>
# modified by Sasha Pachev <sasha@mysql.com>
# Sligtly updated by Monty
# Cleaned up again by Matt

#++
# Access Definitions
#--
DB=test
DBUSER=test
DBPASSWD=
VERBOSE=""

# Are we on source or binary distribution?

if [ $0 = scripts/mysql-test-run ] ;
then
 BINARY_DIST=1
 cd mysql-test
else
 if [ -d mysql-test ] ; then
  cd mysql-test
 fi
  
 if [ -f ./mysql-test-run ] && [ -d ../sql ] ; then
 SOURCE_DIST=1
 else
  $ECHO "If you are using binary distribution, run from install root as"
  $ECHO "scripts/mysql-test-run. On source distribution run from source root"
  $ECHO "as mysql-test/mysql-test-run or from mysql-test as ./mysql-test-run"
  exit 1
 fi
fi

#++
# Misc. Definitions
#--

#BASEDIR is always one above mysql-test directory 
CWD=`pwd`
cd ..
BASEDIR=`pwd`
cd $CWD
MYSQL_TEST_DIR=$BASEDIR/mysql-test
STD_DATA=$MYSQL_TEST_DIR/std_data
SED=sed
  
TESTDIR="$MYSQL_TEST_DIR/t/"
TESTSUFFIX=test
TOT_PASS=0
TOT_FAIL=0
TOT_TEST=0
USERT=0
SYST=0
REALT=0
MYSQL_TMP_DIR=$MYSQL_TEST_DIR/var/tmp
TIMEFILE="$MYSQL_TMP_DIR/mysqltest-time"
RES_SPACE="      "
MYSQLD_SRC_DIRS="strings mysys include extra regex isam merge myisam \
 myisammrg heap sql"
GCOV_MSG=$MYSQL_TMP_DIR/mysqld-gcov.out
GCOV_ERR=$MYSQL_TMP_DIR/mysqld-gcov.err  

MASTER_RUNNING=0
SLAVE_RUNNING=0

#++
# mysqld Environment Parameters
#--
MYRUN_DIR=$MYSQL_TEST_DIR/var/run
MASTER_MYPORT=9306
MASTER_MYDDIR="$MYSQL_TEST_DIR/var/lib"
MASTER_MYSOCK="$MYSQL_TMP_DIR/mysql.sock"
MASTER_MYPID="$MYRUN_DIR/mysqld.pid"
MASTER_MYLOG="$MYSQL_TEST_DIR/var/log/mysqld.log"
MASTER_MYERR="$MYSQL_TEST_DIR/var/log/mysqld.err"

SLAVE_MYPORT=9307
SLAVE_MYDDIR="$MYSQL_TEST_DIR/var/slave-data"
SLAVE_MYSOCK="$MYSQL_TMP_DIR/mysql-slave.sock"
SLAVE_MYPID="$MYRUN_DIR/mysqld-slave.pid"
SLAVE_MYLOG="$MYSQL_TEST_DIR/var/log/mysqld-slave.log"
SLAVE_MYERR="$MYSQL_TEST_DIR/var/log/mysqld-slave.err"

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

#++
# Program Definitions
#--
BASENAME=`which basename | head -1`
CAT=/bin/cat
CUT=/usr/bin/cut
ECHO=echo # use internal echo if possible
EXPR=expr # use internal if possible
FIND=/usr/bin/find
GCOV=`which gcov | head -1`
PRINTF=/usr/bin/printf
RM=/bin/rm
TIME=/usr/bin/time
TR=/usr/bin/tr
XARGS=`which xargs | head -1`

[ -z "$COLUMNS" ] && COLUMNS=80
E=`$EXPR $COLUMNS - 8`
#DASH72=`expr substr '------------------------------------------------------------------------' 1 $E`
DASH72=`$ECHO '------------------------------------------------------------------------'|$CUT -c 1-$E`

# on source dist, we pick up freshly build executables
# on binary, use what is installed
if [ x$SOURCE_DIST = x1 ] ; then
 MYSQLD="$BASEDIR/sql/mysqld"
 MYSQL_TEST="$BASEDIR/client/mysqltest"
 MYSQLADMIN="$BASEDIR/client/mysqladmin"
 INSTALL_DB="./install_test_db"
else
 MYSQLD="$BASEDIR/bin/mysqld"
 MYSQL_TEST="$BASEDIR/bin/mysqltest"
 MYSQLADMIN="$BASEDIR/bin/mysqladmin"
 INSTALL_DB="./install_test_db -bin"
fi


SLAVE_MYSQLD=$MYSQLD #this will be changed later if we are doing gcov

MYSQL_TEST="$MYSQL_TEST --no-defaults --socket=$MASTER_MYSOCK --database=$DB --user=$DBUSER --password=$DBPASSWD --silent -v"
GDB_MASTER_INIT=$MYSQL_TMP_DIR/gdbinit.master
GDB_SLAVE_INIT=$MYSQL_TMP_DIR/gdbinit.slave

while test $# -gt 0; do
  case "$1" in
    --force ) FORCE=1 ;;
    --record ) RECORD=1 ;;
    --gcov )
      if [ x$BINARY_DIST = x1 ] ; then
	$ECHO "Cannot do coverage test without the source - please use source dist"
	exit 1
      fi
      DO_GCOV=1
      ;;
    --gdb )
      if [ x$BINARY_DIST = x1 ] ; then
	$ECHO "Note: you will get more meaningful output on a source distribution compiled with debugging option when running tests with -gdb option"
      fi
      DO_GDB=1
      ;;
    --ddd )
      if [ x$BINARY_DIST = x1 ] ; then
	$ECHO "Note: you will get more meaningful output on a source distribution compiled with debugging option when running tests with -gdb option"
      fi
      DO_DDD=1
      ;;
    --debug)
      EXTRA_MASTER_MYSQLD_OPT=--debug=d:t:O,$MYSQL_TMP_DIR/master.trace
      EXTRA_SLAVE_MYSQLD_OPT=--debug=d:t:O,$MYSQL_TMP_DIR/slave.trace
      ;;
    -- )  shift; break ;;
    --* ) $ECHO "Unrecognized option: $1"; exit 1 ;;
    * ) break ;;
  esac
  shift
done

#++
# Function Definitions
#--

prompt_user ()
{
 $ECHO $1
 read unused
}


error () {
    $ECHO  "Error:  $1"
    exit 1
}

error_is () {
    $ECHO `$CAT $TIMEFILE` | $SED -e 's/.* At line .*\: \(.*\)Command .*$/   \>\> Error: \1<\</'
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
    return 0
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
    cd $BASEDIR # for gcov
    # Remove old berkeley db log files that can confuse the server
    $RM -f $MASTER_MYDDIR/log.*	
    #start master
    master_args="--no-defaults --log-bin=master-bin \
    	    --server-id=1 \
            --basedir=$MY_BASEDIR \
	    --port=$MASTER_MYPORT \
	    --exit-info=256 \
            --datadir=$MASTER_MYDDIR \
	    --pid-file=$MASTER_MYPID \
	    --socket=$MASTER_MYSOCK \
            --log=$MASTER_MYLOG \
	    --language=english $EXTRA_MASTER_OPT $EXTRA_MASTER_MYSQLD_OPT"
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
  MASTER_RUNNING=1
}

start_slave()
{
    [ x$SKIP_SLAVE = x1 ] && return
    [ x$SLAVE_RUNNING = 1 ] && return
    if [ -z "$SLAVE_MASTER_INFO" ] ; then
      master_info="--master-user=root \
	    --master-connect-retry=1 \
	    --master-host=127.0.0.1 \
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
            --log=$SLAVE_MYLOG \
            --language=english $EXTRA_SLAVE_OPT $EXTRA_SLAVE_MYSQLD_OPT"
    if [ x$DO_DDD = x1 ]
    then
      $ECHO "set args $master_args" > $GDB_SLAVE_INIT
      ddd --debugger "gdb -x $GDB_SLAVE_INIT" $MYSQLD &
      prompt_user "Hit enter to continue after you've started the master"
    elif [ x$DO_GDB = x1 ]
    then
      $ECHO "set args $slave_args" > $GDB_SLAVE_INIT
      xterm -title "Slave" -e gdb -x $GDB_SLAVE_INIT $SLAVE_MYSQLD &
      prompt_user "Hit enter to continue after you've started the slave"
    else
      $SLAVE_MYSQLD $slave_args  >> $SLAVE_MYERR 2>&1 &
    fi
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
    $MYSQLADMIN --no-defaults --socket=$SLAVE_MYSOCK -u root shutdown
    if [ $? != 0 ] ; then # try harder!
     $ECHO "slave not cooperating with mysqladmin, will try manual kill"
     kill `cat $SLAVE_MYPID`
     sleep 2
     if [ -f $SLAVE_MYPID ] ; then
       $ECHO "slave refused to die, resorting to SIGKILL murder"
       kill -9 `cat $SLAVE_MYPID`
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
    $MYSQLADMIN --no-defaults --socket=$MASTER_MYSOCK -u root shutdown
    if [ $? != 0 ] ; then # try harder!
     $ECHO "master not cooperating with mysqladmin, will try manual kill"
     kill `cat $MASTER_MYPID`
     sleep 2
     if [ -f $MASTER_MYPID ] ; then
       $ECHO "master refused to die, resorting to SIGKILL murder"
       kill -9 `cat $MASTER_MYPID`
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
 $ECHO
 stop_master
 stop_slave
 return 1
}

mysql_restart () {

    mysql_stop
    mysql_start

    return 1
}

mysql_loadstd () {
    
    cp $STD_DATA/*.frm $STD_DATA/*.MRG $MASTER_MYDDIR/test  
    return 1
}

run_testcase ()
{
 tf=$1
 tname=`$BASENAME $tf .test`
 master_opt_file=$TESTDIR/$tname-master.opt
 slave_opt_file=$TESTDIR/$tname-slave.opt
 slave_master_info_file=$TESTDIR/$tname-slave-master-info.opt
 SKIP_SLAVE=`$EXPR \( $tname : rpl \) = 0`
 if [ x$RECORD = x1 ]; then
  extra_flags="-r"
 else
  extra_flags=""
 fi
  
 if [ -f $master_opt_file ] ;
 then
  EXTRA_MASTER_OPT=`cat $master_opt_file`
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
  EXTRA_SLAVE_OPT=`cat $slave_opt_file`
  do_slave_restart=1
 else
  if [ ! -z "$EXTRA_SLAVE_OPT" ] || [ x$SLAVE_RUNNING != x1 ] ;
  then
    EXTRA_SLAVE_OPT=""
    do_slave_restart=1    
  fi  
 fi

 if [ -f $slave_master_info_file ] ; then
   SLAVE_MASTER_INFO=`cat $slave_master_info_file`
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

 cd $MYSQL_TEST_DIR
  
 if [ -f $tf ] ; then
    $RM -f r/$tname.*reject
    mytime=`$TIME -p $MYSQL_TEST -R r/$tname.result $extra_flags \
     < $tf 2> $TIMEFILE`
    res=$?

    if [ $res = 0 ]; then
	mytime=`$CAT $TIMEFILE | $TR '\n' '-'`

	USERT=`$ECHO $mytime | $CUT -d - -f 2 | $CUT -d ' ' -f 2`
        USERT=`prefix_to_8 $USERT`
	SYST=`$ECHO $mytime | $CUT -d - -f 3 | $CUT -d ' ' -f 2`
        SYST=`prefix_to_8 $SYST`
	REALT=`$ECHO $mytime | $CUT -d - -f 1 | $CUT -d ' ' -f 2`
        REALT=`prefix_to_8 $REALT`
    else
	USERT="    ...."
	SYST="    ...."
	REALT="    ...."
    fi

	timestr="$USERT $SYST $REALT"
	pname=`$ECHO "$tname                 "|$CUT -c 1-16`
	$SETCOLOR_NORMAL && $ECHO -n "$pname          $timestr"



    if [ $res = 0 ]; then
      total_inc
      pass_inc
      $ECHO "$RES_SPACE [ pass ]"
    else
      if [ $res = 1 ]; then
	total_inc
        fail_inc
	$ECHO "$RES_SPACE [ fail ]"
        $ECHO
	error_is
	$ECHO
	if [ x$FORCE != x1 ] ; then
	 $ECHO "Aborting. To continue, re-run with '--force'."
	 $ECHO
	 mysql_stop
	 exit 1
	fi
	 
#	$ECHO "Restarting mysqld"
	mysql_restart
	$ECHO "Resuming Tests"
	$ECHO
      else
        pass_inc
	$ECHO "$RES_SPACE [ skipped ]"
      fi
    fi
  fi  
}


######################################################################
# Main script starts here
######################################################################

[ "$DO_GCOV" -a ! -x "$GCOV" ] && error "No gcov found"

[ "$DO_GCOV" ] && gcov_prepare 

# Ensure that no old mysqld test servers are running
$MYSQLADMIN --no-defaults --socket=$MASTER_MYSOCK -u root -O connect_timeout=5 shutdown > /dev/null 2>&1
$MYSQLADMIN --no-defaults --socket=$SLAVE_MYSOCK -u root -O connect_timeout=5 shutdown > /dev/null 2>&1

$ECHO "Installing Test Databases"
mysql_install_db

#do not automagically start deamons if we are in gdb or running only one test
#case
if [ -z "$DO_GDB" ] && [ -z "$1" ] && [ -z "$DO_DDD" ]
then
 mysql_start
fi

$ECHO  "Loading Standard Test Databases"
mysql_loadstd

$ECHO  "Starting Tests"

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

$RM -f $TIMEFILE

if [ -z "$DO_GDB" ] && [ -z "$DO_DDD" ]
then
    mysql_stop
fi

report_stats
$ECHO

[ "$DO_GCOV" ] && gcov_collect # collect coverage information

exit 0
