#!/bin/sh
# Copyright Abandoned 1996 TCX DataKonsult AB & Monty Program KB & Detron HB
# This file is public domain and comes with NO WARRANTY of any kind

# MySQL daemon start/stop script.

# Usually this is put in /etc/init.d (at least on machines SYSV R4 based
# systems) and linked to /etc/rc3.d/S99mysql and /etc/rc0.d/K01mysql.
# When this is done the mysql server will be started when the machine is
# started and shut down when the systems goes down.

# Comments to support chkconfig on RedHat Linux
# chkconfig: 2345 90 20
# description: A very fast and reliable SQL database engine.

# Comments to support LSB init script conventions
### BEGIN INIT INFO
# Provides: mysql
# Required-Start: $local_fs $network $remote_fs
# Required-Stop: $local_fs $network $remote_fs
# Default-Start:  2 3 4 5
# Default-Stop: 0 1 6
# Short-Description: start and stop MySQL
# Description: MySQL is a very fast and reliable SQL database engine.
### END INIT INFO
 
# If you install MySQL on some other places than @prefix@, then you
# have to do one of the following things for this script to work:
#
# - Run this script from within the MySQL installation directory
# - Create a /etc/my.cnf file with the following information:
#   [mysqld]
#   basedir=<path-to-mysql-installation-directory>
# - Add the above to any other configuration file (for example ~/.my.ini)
#   and copy my_print_defaults to /usr/bin
# - Add the path to the mysql-installation-directory to the basedir variable
#   below.
#
# If you want to affect other MySQL variables, you should make your changes
# in the /etc/my.cnf, ~/.my.cnf or other MySQL configuration files.

# If you change base dir, you must also change datadir

basedir=
datadir=

# The following variables are only set for letting mysql.server find things.

# Set some defaults
pid_file=
if test -z "$basedir"
then
  basedir=@prefix@
  bindir=@bindir@
  datadir=@localstatedir@
else
  bindir="$basedir/bin"
fi

PATH=/sbin:/usr/sbin:/bin:/usr/bin:$basedir/bin
export PATH

mode=$1    # start or stop

case `echo "testing\c"`,`echo -n testing` in
    *c*,-n*) echo_n=   echo_c=     ;;
    *c*,*)   echo_n=-n echo_c=     ;;
    *)       echo_n=   echo_c='\c' ;;
esac

parse_arguments() {
  for arg do
    case "$arg" in
      --basedir=*)  basedir=`echo "$arg" | sed -e 's/^[^=]*=//'` ;;
      --datadir=*)  datadir=`echo "$arg" | sed -e 's/^[^=]*=//'` ;;
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

parse_arguments `$print_defaults $extra_args mysqld server mysql_server mysql.server`

#
# Set pid file if not given
#
if test -z "$pid_file"
then
  pid_file=$datadir/`@HOSTNAME@`.pid
else
  case "$pid_file" in
    /* ) ;;
    * )  pid_file="$datadir/$pid_file" ;;
  esac
fi

# Safeguard (relative paths, core dumps..)
cd $basedir

case "$mode" in
  'start')
    # Start daemon

    if test -x $bindir/mysqld_safe
    then
      # Give extra arguments to mysqld with the my.cnf file. This script may
      # be overwritten at next upgrade.
      $bindir/mysqld_safe --datadir=$datadir --pid-file=$pid_file >/dev/null 2>&1 &
      # Make lock for RedHat / SuSE
      if test -w /var/lock/subsys
      then
        touch /var/lock/subsys/mysql
      fi
    else
      echo "Can't execute $bindir/mysqld_safe from dir $basedir"
    fi
    ;;

  'stop')
    # Stop daemon. We use a signal here to avoid having to know the
    # root password.
    if test -s "$pid_file"
    then
      mysqld_pid=`cat $pid_file`
      echo "Killing mysqld with pid $mysqld_pid"
      kill $mysqld_pid
      # mysqld should remove the pid_file when it exits, so wait for it.

      sleep 1
      while [ -s $pid_file -a "$flags" != aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa ]
      do
	[ -z "$flags" ] && echo $echo_n "Wait for mysqld to exit$echo_c" || echo $echo_n ".$echo_c"
        flags=a$flags
        sleep 1
      done
      if [ -s $pid_file ]
         then echo " gave up waiting!"
      elif [ -n "$flags" ]
         then echo " done"
      fi
      # delete lock for RedHat / SuSE
      if test -f /var/lock/subsys/mysql
      then
        rm -f /var/lock/subsys/mysql
      fi
    else
      echo "No mysqld pid file found. Looked for $pid_file."
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
