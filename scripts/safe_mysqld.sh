#!/bin/sh
# Copyright Abandoned 1996 TCX DataKonsult AB & Monty Program KB & Detron HB
# This file is public domain and comes with NO WARRANTY of any kind
#
# scripts to start the MySQL daemon and restart it if it dies unexpectedly
#
# This should be executed in the MySQL base directory if you are using a
# binary installation that has other paths than you are using.
#
# mysql.server works by first doing a cd to the base directory and from there
# executing safe_mysqld

trap '' 1 2 3 15			# we shouldn't let anyone kill us

defaults=
case "$1" in
    --no-defaults|--defaults-file=*|--defaults-extra-file=*)
      defaults="$1"; shift
      ;;
esac

# Parse arguments to see if caller wants the pid_file somewhere else.
parse_arguments() {
  for arg in "$@"; do
    case $arg in
      --datadir=*)  DATADIR=`echo "$arg" | sed -e "s;--datadir=;;"` ;;
      --pid-file=*) pid_file=`echo "$arg" | sed -e "s;--pid-file=;;"` ;;
      --socket=*)   MYSQL_UNIX_PORT=`echo "$arg" | sed -e "s;--socket=;;"` ;;
      --port=*)     MYSQL_TCP_PORT=`echo "$arg" | sed -e "s;--port=;;"` ;;
      --log=*)      log=`echo "$arg" | sed -e "s;--log=;;"` ;;
      --err-log=*)  err_log=`echo "$arg" | sed -e "s;--err-log=;;"` ;;
      --basedir=*)  MY_BASEDIR_VERSION=`echo "$arg" | sed -e "s;--basedir=;;"` ;;
      --ledir=*)    ledir=`echo "$arg" | sed -e "s;--ledir=;;"` ;;
      --user=*)     user=`echo "$arg" | sed -e "s;--user=;;"` ;;
    esac
  done
}

MY_PWD=`pwd`
# Check if we are starting this relative (for the binary release)
if test -d $MY_PWD/data/mysql -a -f ./share/mysql/english/errmsg.sys -a \
 -x ./bin/mysqld
then
  MY_BASEDIR_VERSION=$MY_PWD		# Where bin, share and data are
  ledir=$MY_BASEDIR_VERSION/bin		# Where mysqld is
  DATADIR=$MY_BASEDIR_VERSION/data
# Check if this is a 'moved install directory'
elif test -f ./var/mysql/db.frm -a -f ./share/mysql/english/errmsg.sys -a \
 -x ./libexec/mysqld
then
  MY_BASEDIR_VERSION=$MY_PWD		# Where libexec, share and var are
  ledir=$MY_BASEDIR_VERSION/libexec	# Where mysqld is
  DATADIR=$MY_BASEDIR_VERSION/var
else
  MY_BASEDIR_VERSION=@prefix@
  DATADIR=@localstatedir@
  ledir=@libexecdir@
fi

pid_file=$DATADIR/`@HOSTNAME@`.pid
MYSQL_UNIX_PORT=${MYSQL_UNIX_PORT:-@MYSQL_UNIX_ADDR@}
MYSQL_TCP_PORT=${MYSQL_TCP_PORT:-@MYSQL_TCP_PORT@}
log=$DATADIR/`@HOSTNAME@`.log
err_log=$DATADIR/`@HOSTNAME@`.err
user=@MYSQLD_USER@

# Get first arguments from the my.cfg file, groups [mysqld] and [safe_mysqld]
# and then merge with the command line arguments
if test -x ./bin/my_print_defaults; then
  print_defaults="./bin/my_print_defaults"
elif test -x @bindir@/my_print_defaults; then
  print_defaults="@bindir@/my_print_defaults"
elif test -x @bindir@/mysql_print_defaults; then
  print_defaults="@bindir@/mysql_print_defaults"
else
  print_defaults="my_print_defaults"
fi
parse_arguments `$print_defaults $defaults mysqld safe_mysqld`
parse_arguments "$@"

if test ! -x $ledir/mysqld
then
  echo "The file $ledir/mysqld doesn't exist or is not executable"
  echo "Please do a cd to the mysql installation directory and restart"
  echo "this script from there as follows:"
  echo "./bin/safe_mysqld".
  exit 1;
fi


NOHUP_NICENESS="nohup"
if test -w /
then
  NOHUP_NICENESS=`nohup nice 2>&1`
 if test $? -eq 0 && test x"$NOHUP_NICENESS" != x0 && nice --1 echo foo > /dev/null 2>&1; then
    NOHUP_NICENESS="nice --$NOHUP_NICENESS nohup"
  else
    NOHUP_NICENESS="nohup"
  fi
fi

export MYSQL_UNIX_PORT
export MYSQL_TCP_PORT
if test -w /
then
  # If we are root, change the err log to the right user.
  touch $err_log; chown $user $err_log
fi

#
# If there exists an old pid file, check if the daemon is already running
# Note: The switches to 'ps' may depend on your operating system
if test -f $pid_file
then
  PID=`cat $pid_file`
  if @CHECK_PID@
  then
    if @FIND_PROC@
    then    # The pid contains a mysqld process
      echo "A mysqld process already exists"
      echo "A mysqld process already exists at " `date` >> $err_log
      exit 1;
    fi
  fi
  rm -f $pid_file
  if test -f $pid_file
  then
    echo "Fatal error: Can't remove the pid file: $pid_file"
    echo "Fatal error: Can't remove the pid file: $pid_file at " `date` >> $err_log
    echo "Please remove it manually and start $0 again"
    echo "mysqld daemon not started"
    exit 1;
  fi
fi

#
# Uncomment the following lines if you want all tables to be automaticly
# checked and repaired at start
#
# echo "Checking tables in $DATADIR"
# $MY_BASEDIR_VERSION/bin/myisamchk --silent --force --fast --medium-check -O key_buffer=64M -O sort_buffer=64M $DATADIR/*/*.MYI
# $MY_BASEDIR_VERSION/bin/isamchk --silent --force -O sort_buffer=64M $DATADIR/*/*.ISM

echo "Starting mysqld daemon with databases from $DATADIR"

# Does this work on all systems?
#if type ulimit | grep "shell builtin" > /dev/null
#then
#  ulimit -n 256 > /dev/null 2>&1		# Fix for BSD and FreeBSD systems
#fi

echo "`date +'%y%m%d %H:%M:%S  mysqld started'`" >> $err_log
while true
do
  rm -f $MYSQL_UNIX_PORT $pid_file	# Some extra safety
  if test "$#" -eq 0
  then
    (trap "" 1 ; exec $NOHUP_NICENESS $ledir/mysqld $defaults --basedir=$MY_BASEDIR_VERSION --datadir=$DATADIR --user=$user --pid-file=$pid_file @MYSQLD_DEFAULT_SWITCHES@ >> $err_log 2>&1 )
  else
    (trap "" ; exec $NOHUP_NICENESS $ledir/mysqld $defaults --basedir=$MY_BASEDIR_VERSION --datadir=$DATADIR --user=$user --pid-file=$pid_file @MYSQLD_DEFAULT_SWITCHES@ "$@" >> $err_log 2>&1 )
  fi
  if test ! -f $pid_file		# This is removed if normal shutdown
  then
    break;
  fi
  if @IS_LINUX@
  then
    # Test if one process was hanging.
    # This is only a fix for Linux (running as base 3 mysqld processes)
    # but should work for the rest of the servers.
    # The only thing is ps x => redhat 5 gives warnings when using ps -x.
    # kill -9 is used or the process won't react on the kill.
    numofproces=`ps xa | grep -v "grep" | grep -c $ledir/mysqld`
    echo -e "\nNumber of processes running now: $numofproces" | tee -a $err_log
    I=1
    while test "$I" -le "$numofproces"
    do 
      PROC=`ps xa | grep $ledir/mysqld | grep -v "grep" | tail -1` 
	for T in $PROC
	do
	  break
	done
	#    echo "TEST $I - $T **"
	if kill -9 $T
	then
	  echo "mysqld process hanging, pid $T - killed" | tee -a $err_log
	else 
	  break
	fi
	I=`expr $I + 1`
    done
  fi
  echo "`date +'%y%m%d %H:%M:%S  mysqld restarted'`" | tee -a $err_log
done

echo "`date +'%y%m%d %H:%M:%S  mysqld ended\n'`" | tee -a $err_log
echo "" | tee -a $err_log
