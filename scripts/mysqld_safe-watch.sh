#!/bin/sh
# Copyright Abandoned 1996 TCX DataKonsult AB & Monty Program KB & Detron HB
# This file is public domain and comes with NO WARRANTY of any kind
#
# scripts to start the MySQL demon and restart it if it dies unexpectedly
#
# This should be executed in the MySQL base directory if you are using a
# binary installation that has other paths than you are using.
#
# mysql.server works by first doing a cd to the base directory and from there
# executing mysqld_safe

# Check if we are starting this relative (for the binary release)
if test -f ./data/mysql/db.frm -a -f ./share/mysql/english/errmsg.sys -a \
 -x ./bin/mysqld
then
  MY_BASEDIR_VERSION=`pwd`		# Where bin, share and data is
  DATADIR=$MY_BASEDIR_VERSION/data	# Where the databases are
  ledir=$MY_BASEDIR_VERSION/bin		# Where mysqld are
# Check if this is a 'moved install directory'
elif test -f ./var/mysql/db.frm -a -f ./share/mysql/english/errmsg.sys -a \
 -x ./libexec/mysqld
then
  MY_BASEDIR_VERSION=`pwd`		# Where libexec, share and var is
  DATADIR=$MY_BASEDIR_VERSION/var	# Where the databases are
  ledir=$MY_BASEDIR_VERSION/libexec	# Where mysqld are
else
  MY_BASEDIR_VERSION=/usr/local/mysql
  DATADIR=/usr/local/mysql/var
  ledir=/usr/local/mysql/libexec
fi

hostname=`@HOSTNAME@`
pidfile=$DATADIR/$hostname.pid
log=$DATADIR/$hostname.log
err=$DATADIR/$hostname.err
lockfile=$DATADIR/$hostname.lock

#
# If there exists an old pid file, check if the demon is already running
# Note: The switches to 'ps' may depend on your operating system

if test -f $pidfile
then
  PID=`cat $pidfile`
  if /bin/kill -0 $PID
  then
    if /bin/ps -p $PID | grep mysqld > /dev/null
    then    # The pid contains a mysqld process
      echo "A mysqld process already exists"
      echo "A mysqld process already exists at " `date` >> $log
      exit 1;
    fi
  fi
  rm -f $pidfile
  if test -f $pidfile
  then
    echo "Fatal error: Can't remove the pid file: $pidfile"
    echo "Fatal error: Can't remove the pid file: $pidfile at " `date` >> $log
    echo "Please remove it manually and start $0 again"
    echo "mysqld demon not started"
    exit 1;
  fi
fi

echo "Starting mysqld demon with databases from $DATADIR"

#Default communication ports
#MYSQL_TCP_PORT=3306
if test -z "$MYSQL_UNIX_PORT"
then
  MYSQL_UNIX_PORT="/tmp/mysql.sock"
  export MYSQL_UNIX_PORT    
fi
#export MYSQL_TCP_PORT

# Does this work on all systems?
#if type ulimit | grep "shell builtin" > /dev/null
#then
#  ulimit -n 256 > /dev/null 2>&1		# Fix for BSD and FreeBSD systems
#fi

echo "mysqld started on " `date` >> $log
bin/zap -f $lockfile < /dev/null > /dev/null 2>&1
rm -f $lockfile
$MY_BASEDIR_VERSION/bin/watchdog_mysqld $lockfile $pidfile $MY_BASEDIR_VERSION/bin $DATADIR 3 10 >> $err 2>&1 &
restart_pid=$!

while true
do
  rm -f $MYSQL_UNIX_PORT $pidfile	# Some extra safety
  lockfile -1 -r10 $lockfile >/dev/null 2>&1
  if test "$#" -eq 0
  then
    nohup $ledir/mysqld --basedir=$MY_BASEDIR_VERSION --datadir=$DATADIR \
     --skip-locking >> $err 2>&1 &
  else
    nohup $ledir/mysqld --basedir=$MY_BASEDIR_VERSION --datadir=$DATADIR \
     --skip-locking "$@" >> $err 2>&1 &
  fi
  pid=$!
  rm -f $lockfile
  wait $pid;

  lockfile -1 -r10 $lockfile >/dev/null 2>&1
  rm -f $lockfile
  if test ! -f $pidfile			# This is removed if normal shutdown
  then
    break;
  fi
  if true
  then
    # Test if one proces was hanging.
    # This is only a fix for Linux (running as base 3 mysqld processes)
    # but should work for the rest of the servers.
    # The only thing is ps x => redhat 5 gives warnings when using ps -x.
    # kill -9 is used or the proces won't react on the kill.
    numofproces=`ps x | grep -v "grep" | grep -c $ledir/mysqld`
    echo -e "\nNumber of processes running now: $numofproces" | tee -a $log
    I=1
    while test "$I" -le "$numofproces"
    do 
      PROC=`ps x | grep $ledir/mysqld | grep -v "grep" | tail -1` 
	for T in $PROC
	do
	  break
	done
	#    echo "TEST $I - $T **"
	if kill -9 $T
	then
	  echo "mysqld proces hanging, pid $T - killed" | tee -a $log
	else 
	  break
	fi
	I=`expr $I + 1`
    done
  fi
  echo "mysqld restarted" | tee -a $log
  # Check all tables and repair any wrong tables.
  $MY_BASEDIR_VERSION/bin/isamchk -sf $DATADIR/*/*.ISM >> $err 2>&1
done
if test $restart_pid -gt 0
then
  kill $restart_pid > /dev/null 2>&1
  sleep 1;
  kill -9 $restart_pid > /dev/null 2>&1
fi

echo -n "mysqld ended on " `date` >> $log
echo "mysqld demon ended"
