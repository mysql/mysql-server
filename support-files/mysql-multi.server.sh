#!/bin/sh
# Copyright Abandoned 1996 TCX DataKonsult AB & Monty Program KB & Detron HB
# This file is public domain and comes with NO WARRANTY of any kind

# Mysql daemon start/stop script. Multi-instance enhancements by Tim Bunce.

# Usually this is put in /etc/init.d (at least on machines SYSV R4
# based systems) and linked to
#     /etc/rc3.d/S99mysql.svr1
# and /etc/rc0.d/S01mysql.svr1
# When this is done the mysql server will be started when the machine is
# started and shut down when the systems goes down. The '.svr1' suffix can
# be used to identify one of a number of servers. Multiple symlinks can be
# created, one per instance. The 'svrN' suffix can then be used to
# prefix configuration variables in a seperate section of /etc/my.cnf.
# See example below.
#
# A typical multi-instance /etc/my.cnf file would look like:
# [mysqld]
# basedir=...
# set-variable = key_buffer_size=16M
# set-variable = max_allowed_packet=1M
# [mysql_multi_server]
# svr1-datadir=/foo1/bar
# svr2-datadir=/foo2/bar
#
# and then the /foo1/bar/my.cnf and /foo2/bar/my.cnf files
# would contain all the *instance specific* configurations.
#
# This script can also be run manually in which case the server instance
# is identified by an extra argument, for example:
#     /etc/init.d/mysql stop svr3
#

PATH=/sbin:/usr/sbin:/bin:/usr/bin
export PATH

mode=$1    # start or stop
svr=$2     # eg 'svr1' (optional)
if [ "$2" = "" ]
then name=`basename $0`
else name=$2
fi

# Extract identity of the server we are working with
svr=`echo "$name" | sed -e 's/.*\<\(svr[1-9][0-9]*\)\>.*/\1/'`
if [ "$svr" = "" ]
then
  echo "Can't determine database svr number from name '$name'"
  exit 1
fi

echo "mysqld $svr $mode"

parse_arguments() {
  for arg do
    case "$arg" in
      --basedir=*|--${svr}-basedir=*)
        basedir=`echo "$arg" | sed -e 's/^[^=]*=//'` ;;
      --datadir=*|--${svr}-basedir=*)
        datadir=`echo "$arg" | sed -e 's/^[^=]*=//'` ;;
      --pid-file=*|--${svr}-basedir=*)
        pid_file=`echo "$arg" | sed -e 's/^[^=]*=//'` ;;
    esac
  done
}

# Get arguments from the my.cfg file, groups [mysqld], [mysql_server],
# and mysql_multi_server
if test -x ./bin/my_print_defaults
then
  print_defaults="./bin/my_print_defaults"
elif test -x @bindir@/my_print_defaults
then
  print_defaults="@bindir@/my_print_defaults"
elif test -x @bindir@/mysql_print_defaults
then
  print_defaults="@bindir@/mysql_print_defaults"
else
  # Try to find basedir in /etc/my.cnf
  conf=/etc/my.cnf
  print_defaults=
  if test -r $conf
  then
    subpat='^[^=]*basedir[^=]*=\(.*\)$'
    dirs=`sed -e "/$subpat/!d" -e 's//\1/' $conf
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

datadir=@localstatedir@
basedir=
pid_file=
parse_arguments `$print_defaults $defaults mysqld mysql_server mysql_multi_server`

if test -z "$basedir"
then
  basedir=@prefix@
  bindir=@bindir@
else
  bindir="$basedir/bin"
fi
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

    if test -x $bindir/safe_mysqld
    then
      # We only need to specify datadir and pid-file here and we
      # get all other instance-specific config from $datadir/my.cnf.
      # We have to explicitly pass --defaults-extra-file because it
      # reads the config files before the command line options.
      # Also it must be first because of the way safe_mysqld works.
      $bindir/safe_mysqld --defaults-extra-file=$datadir/my.cnf \
                          --datadir=$datadir --pid-file=$pid_file &
      # Make lock for RedHat / SuSE
      if test -d /var/lock/subsys
      then
        touch /var/lock/subsys/mysql
      fi
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
      echo "Killing mysqld $svr with pid $mysqld_pid"
      kill $mysqld_pid
      # mysqld should remove the pid_file when it exits, so wait for it.

      sleep 1
      while [ -s $pid_file -a "$flags" != aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa ]
      do
        [ -z "$flags" ] && echo "Wait for mysqld $svr to exit\c" || echo ".\c"
        flags=a$flags
        sleep 1
      done
      if [ -s $pid_file ]
         then echo " gave up waiting!"
      elif [ -n "$flags" ]
         then echo " done"
      fi
      # delete lock for RedHat / SuSE
      if test -e /var/lock/subsys/mysql
      then
        rm /var/lock/subsys/mysql
      fi
    else
      echo "No mysqld pid file found. Looked for $pid_file."
    fi
    ;;

  *)
    # usage
    echo "usage: $0 start|stop [ svrN ]"
    exit 1
    ;;
esac
