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

# The following variables are only set for letting mysql.server find things
# if you want to affect other MySQL variables, you should make your changes
# in the /etc/my.cnf or other configuration files

PATH=/sbin:/usr/sbin:/bin:/usr/bin
basedir=@prefix@
bindir=@bindir@
sbindir=@sbindir@
datadir=@localstatedir@
pid_file=@localstatedir@/mysqld.pid

export PATH

mode=$1

GetCNF () {

VARIABLES="basedir bindir sbindir datadir pid-file"
CONFIG_FILES="/etc/my.cnf $basedir/my.cnf $HOME/.my.cnf"

for c in $CONFIG_FILES
do
   if [ -f $c ]
   then
      #echo "Processing $c..."
      for v in $VARIABLES
      do
         # This method assumes last of duplicate $variable entries will be the
         # value set ([mysqld])
         # This could easily be rewritten to gather [xxxxx]-specific entries,
         # but for now it looks like only the mysqld ones are needed for
         # server startup scripts
         thevar=""
         eval `sed -n -e '/^$/d' -e '/^#/d' -e 's,[ 	],,g' -e '/=/p' $c |\
         awk -F= -v v=$v '{if ($1 == v) printf ("thevar=\"%s\"\n", $2)}'`

         # it would be easier if the my.cnf and variable values were
         # all matched, but since they aren't we need to map them here.
         case $v in
         pid-file) v=pid_file ;;
              log) v=log_file ;;
         esac

         # As long as $thevar isn't blank, use it to set or override current
         # value
         [ "$thevar" != "" ] && eval $v=$thevar
            
      done
   #else
   #   echo "No $c config file."
   fi
done
}

# run function to get config values
GetCNF

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
      # delete lock for RedHat / SuSE
      if test -d /var/lock/subsys
      then
        rm /var/lock/subsys/mysql
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
