#!/bin/sh
# Copyright Abandoned 1996 TCX DataKonsult AB & Monty Program KB & Detron HB
# This file is public domain and comes with NO WARRANTY of any kind

# Mysql daemon start/stop script.

# Usually this is put in /etc/init.d (at least on machines SYSV R4
# based systems) and linked to /etc/rc3.d/S99mysql and /etc/rc0.d/S01mysql.
# When this is done the mysql server will be started when the machine is started
# and shut down when the systems goes down.

# Comments to support chkconfig on RedHat Linux
# chkconfig: 2345 90 90
# description: A very fast and reliable SQL database engine.

PATH=/sbin:/usr/sbin:/bin:/usr/bin
basedir=@prefix@
bindir=@bindir@
datadir=@localstatedir@
pid_file=@localstatedir@/mysqld.pid
log_file=@localstatedir@/mysqld.log
# Run mysqld as this user.
mysql_daemon_user=@MYSQLD_USER@
export PATH

mode=$1

if test -w /             # determine if we should look at the root config file
then                     # or user config file
  conf=/etc/my.cnf
else
  conf=$HOME/.my.cnf	# Using the users config file
fi

# The following code tries to get the variables safe_mysqld needs from the
# config file.  This isn't perfect as this ignores groups, but it should
# work as the options doesn't conflict with anything else.

if test -f "$conf"       # Extract those fields we need from config file.
then
  if grep "^datadir" $conf >/dev/null
  then
    datadir=`grep "^datadir" $conf | cut -f 2 -d= | tr -d ' '`
  fi
  if grep "^user" $conf >/dev/null
  then
    mysql_daemon_user=`grep "^user" $conf | cut -f 2 -d= | tr -d ' ' | head -1`
  fi
  if grep "^pid-file" $conf >/dev/null
  then
    pid_file=`grep "^pid-file" $conf | cut -f 2 -d= | tr -d ' '`
  else
    if test -d "$datadir"
    then
      pid_file=$datadir/`hostname`.pid
    fi
  fi
  if grep "^basedir" $conf >/dev/null
  then
    basedir=`grep "^basedir" $conf | cut -f 2 -d= | tr -d ' '`
    bindir=$basedir/bin
  fi
  if grep "^bindir" $conf >/dev/null
  then
    bindir=`grep "^bindir" $conf | cut -f 2 -d= | tr -d ' '`
  fi
  if grep "^log[ \t]*=" $conf >/dev/null
  then
    log_file=`grep "log[ \t]*=" $conf | cut -f 2 -d= | tr -d ' '`
  fi
fi


# Safeguard (relative paths, core dumps..)
cd $basedir

case "$mode" in
  'start')
    # Start daemon

    if test -x $bindir/safe_mysqld
    then
      # Give extra arguments to mysqld with the my.cnf file. This script may
      # be overwritten at next upgrade.
      $bindir/safe_mysqld \
	--user=$mysql_daemon_user --datadir=$datadir --pid-file=$pid_file --log=$log_file  &
    else
      echo "Can't execute $bindir/safe_mysqld"
    fi
    ;;

  'stop')
    # Stop daemon. We use a signal here to avoid having to know the
    # root password.
    if test -f "$pid_file"
    then
      mysqld_pid=`cat $pid_file`
      echo "Killing mysqld with pid $mysqld_pid"
      kill $mysqld_pid
      # mysqld should remove the pid_file when it exits, so wait for it.

      sleep 1
      while [ -s $pid_file -a "$flags" != aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa ]
	do  [ -z "$flags" ] && echo "Wait for mysqld to exit\c" || echo ".\c"
	    flags=a$flags
	    sleep 1
      done
      if [ -s $pid_file ]
         then echo " gave up waiting!"
      elif [ -n "$flags" ]
         then echo " done"
      fi
    else
      echo "No mysqld pid file found. Looked for $pid_file."
    fi
    ;;

  *)
    # usage
    echo "usage: $0 start|stop"
    exit 1
    ;;
esac
