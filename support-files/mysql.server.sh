#!/bin/sh
# Copyright (C) 2005 MySQL AB
# This file is public domain and comes with NO WARRANTY of any kind

# MySQL server management daemon start/stop script.

# Usually this is put in /etc/init.d (at least on machines SYSV R4 based
# systems) and linked to /etc/rc3.d/S99mysqlmanager and
# /etc/rc0.d/K01mysqlmanager
# When this is done the mysql server will be started when the machine is
# started and shut down when the systems goes down.

# description: MySQL database server Instance Manager

# Comments to support LSB init script conventions
### BEGIN INIT INFO
# Provides: mysqlmanager
# Required-Start: $local_fs $network $remote_fs
# Required-Stop: $local_fs $network $remote_fs
# Default-Start:  2 3 4 5
# Default-Stop: 0 1 6
# Short-Description: start and stop MySQL Instance Manager
# Description: MySQL Instance Manager is used to start/stop/status/monitor
# MySQL server instances
### END INIT INFO
 
basedir=

# The following variables are only set for letting mysql.server find things.

# Set some defaults
datadir=@localstatedir@
pid_file=
if test -z "$basedir"
then
  basedir=@prefix@
  bindir=@bindir@
  sbindir=@sbindir@
else
  bindir="$basedir/bin"
  sbindir="$basedir/sbin"
fi

PATH=/sbin:/usr/sbin:/bin:/usr/bin:$basedir/bin
export PATH

mode=$1    # start or stop

case `echo "testing\c"`,`echo -n testing` in
    *c*,-n*) echo_n=   echo_c=     ;;
    *c*,*)   echo_n=-n echo_c=     ;;
    *)       echo_n=   echo_c='\c' ;;
esac

parse_server_arguments() {
  for arg do
    case "$arg" in
      --basedir=*)  basedir=`echo "$arg" | sed -e 's/^[^=]*=//'` ;;
      --datadir=*)  datadir=`echo "$arg" | sed -e 's/^[^=]*=//'` ;;
    esac
  done
}

parse_manager_arguments() {
  for arg do
    case "$arg" in
      --pid-file=*) pid_file=`echo "$arg" | sed -e 's/^[^=]*=//'` ;;
    esac
  done
}

# Get arguments from the my.cnf file,
# groups [mysqld] [mysql_server] and [mysql.server]
if test -x ./bin/my_print_defaults
then
  print_defaults="./bin/my_print_defaults"
elif test -x $bindir/my_print_defaults
then
  print_defaults="$bindir/my_print_defaults"
elif test -x $bindir/mysql_print_defaults
then
  print_defaults="$bindir/mysql_print_defaults"
else
  # Try to find basedir in /etc/my.cnf
  conf=/etc/my.cnf
  print_defaults=
  if test -r $conf
  then
    subpat='^[^=]*basedir[^=]*=\(.*\)$'
    dirs=`sed -e "/$subpat/!d" -e 's//\1/' $conf`
    for d in $dirs
    do
      d=`echo $d | sed -e 's/[ 	]//g'`
      if test -x "$d/bin/my_print_defaults"
      then
        print_defaults="$d/bin/my_print_defaults"
        break
      fi
      if test -x "$d/bin/mysql_print_defaults"
      then
        print_defaults="$d/bin/mysql_print_defaults"
        break
      fi
    done
  fi

  # Hope it's in the PATH ... but I doubt it
  test -z "$print_defaults" && print_defaults="my_print_defaults"
fi

#
# Test if someone changed datadir;  In this case we should also read the
# default arguments from this directory
#

extra_args=""
if test "$datadir" != "@localstatedir@"
then
  extra_args="-e $datadir/my.cnf"
fi

parse_server_arguments `$print_defaults $extra_args mysqld server mysql_server mysql.server`

parse_manager_arguments `$print_defaults manager`

#
# Set pid file if not given
#
if test -z "$pid_file"
then
  pid_file=$datadir/mysqlmanager-`@HOSTNAME@`.pid
else
  case "$pid_file" in
    /* ) ;;
    * )  pid_file="$datadir/$pid_file" ;;
  esac
fi

user=@MYSQLD_USER@
USER_OPTION="--user=$user"

# Safeguard (relative paths, core dumps..)
cd $basedir

case "$mode" in
  'start')
    # Start daemon

    if test -x $sbindir/mysqlmanager
    then
      # Give extra arguments to mysqlmanager with the my.cnf file. This script may
      # be overwritten at next upgrade.
      $sbindir/mysqlmanager "--pid-file=$pid_file" $USER_OPTION --run-as-service >/dev/null 2>&1 &
      # Make lock for RedHat / SuSE
      if test -w /var/lock/subsys
      then
        touch /var/lock/subsys/mysqlmanager
      fi
    else
      echo "Can't execute $sbindir/mysqlmanager from dir $basedir"
    fi
    ;;

  'stop')
    # Stop daemon. We use a signal here to avoid having to know the
    # root password.
    if test -s "$pid_file"
    then
      mysqlmanager_pid=`cat $pid_file`
      echo "Killing mysqlmanager with pid $mysqlmanager_pid"
      kill $mysqlmanager_pid
      # mysqlmanager should remove the pid_file when it exits, so wait for it.

      sleep 1
      while [ -s $pid_file -a "$flags" != aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa ]
      do
	[ -z "$flags" ] && echo $echo_n "Wait for mysqlmanager to exit$echo_c" || echo $echo_n ".$echo_c"
        flags=a$flags
        sleep 1
      done
      if [ -s $pid_file ]
         then echo " gave up waiting!"
      elif [ -n "$flags" ]
         then echo " done"
      fi
      # delete lock for RedHat / SuSE
      if test -f /var/lock/subsys/mysqlmanager
      then
        rm -f /var/lock/subsys/mysqlmanager
      fi
    else
      echo "No mysqlmanager pid file found. Looked for $pid_file."
    fi
    ;;

  'restart')
    # Stop the service and regardless of whether it was
    # running or not, start it again.
    $0 stop
    $0 start
		;;

  *)
    # usage
    echo "Usage: $0 start|stop|restart"
    exit 1
    ;;
esac
