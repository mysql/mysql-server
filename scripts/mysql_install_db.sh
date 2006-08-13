#!/bin/sh
# Copyright (C) 2002-2003 MySQL AB
# For a more info consult the file COPYRIGHT distributed with this file.

# This scripts creates the privilege tables db, host, user, tables_priv,
# columns_priv, procs_priv in the mysql database, as well as the func table.
#
# All unrecognized arguments to this script are passed to mysqld.

in_rpm=0
windows=0
defaults=""
user=""

case "$1" in
    --no-defaults|--defaults-file=*|--defaults-extra-file=*)
      defaults="$1"; shift
      ;;
esac

parse_arguments() {
  # We only need to pass arguments through to the server if we don't
  # handle them here.  So, we collect unrecognized options (passed on
  # the command line) into the args variable.
  pick_args=
  if test "$1" = PICK-ARGS-FROM-ARGV
  then
    pick_args=1
    shift
  fi

  for arg do
    case "$arg" in
      --force) force=1 ;;
      --basedir=*) basedir=`echo "$arg" | sed -e 's/^[^=]*=//'` ;;
      --ldata=*|--datadir=*) ldata=`echo "$arg" | sed -e 's/^[^=]*=//'` ;;
      --user=*)
        # Note that the user will be passed to mysqld so that it runs
        # as 'user' (crucial e.g. if log-bin=/some_other_path/
        # where a chown of datadir won't help)
	 user=`echo "$arg" | sed -e 's/^[^=]*=//'` ;;
      --skip-name-resolve) ip_only=1 ;;
      --verbose) verbose=1 ;;
      --rpm) in_rpm=1 ;;
      --windows) windows=1 ;;
      *)
        if test -n "$pick_args"
        then
          # This sed command makes sure that any special chars are quoted,
          # so the arg gets passed exactly to the server.
          args="$args "`echo "$arg" | sed -e 's,\([^a-zA-Z0-9_.-]\),\\\\\1,g'`
        fi
        ;;
    esac
  done
}

# Get first arguments from the my.cfg file, groups [mysqld] and
# [mysql_install_db], and then merge with the command line arguments
if test -x ./bin/my_print_defaults
then
  print_defaults="./bin/my_print_defaults"
elif test -x ./extra/my_print_defaults
then
  print_defaults="./extra/my_print_defaults"
elif test -x @bindir@/my_print_defaults
then
  print_defaults="@bindir@/my_print_defaults"
elif test -x @bindir@/mysql_print_defaults
then
  print_defaults="@bindir@/mysql_print_defaults"
else
  print_defaults="my_print_defaults"
fi

args=
ldata=
execdir=
bindir=
basedir=
force=0
verbose=0
fill_help_tables=""

parse_arguments `$print_defaults $defaults mysqld mysql_install_db`
parse_arguments PICK-ARGS-FROM-ARGV "$@"

test -z "$ldata" && ldata=@localstatedir@
if test -z "$basedir"
then
  basedir=@prefix@
  bindir=@bindir@
  execdir=@libexecdir@
  pkgdatadir=@pkgdatadir@
else
  bindir="$basedir/bin"
  if test -x "$basedir/libexec/mysqld"
  then
    execdir="$basedir/libexec"
  elif test -x "$basedir/sbin/mysqld"
  then
    execdir="$basedir/sbin"
  else
    execdir="$basedir/bin"
  fi
fi

# find fill_help_tables.sh
for i in $basedir/support-files $basedir/share $basedir/share/mysql $basedir/scripts `pwd` `pwd`/scripts @pkgdatadir@
do
  if test -f $i/fill_help_tables.sql
  then
    pkgdatadir=$i
  fi
done

if test -f $pkgdatadir/fill_help_tables.sql
then
  fill_help_tables=$pkgdatadir/fill_help_tables.sql
else
  echo "Could not find help file 'fill_help_tables.sql' in @pkgdatadir@ or inside $basedir".
  exit 1;
fi

mdata=$ldata/mysql
mysqld=$execdir/mysqld
mysqld_opt=""
scriptdir=$bindir

if test "$windows" = 1
then
  mysqld="./sql/mysqld"
  mysqld_opt="--language=./sql/share/english"
  scriptdir="./scripts"
fi

if test ! -x $mysqld
then
  if test "$in_rpm" = 1
  then
    echo "FATAL ERROR $mysqld not found!"
    exit 1
  else
    echo "Didn't find $mysqld"
    echo "You should do a 'make install' before executing this script"
    exit 1
  fi
fi

# Try to determine the hostname
hostname=`@HOSTNAME@`

# Check if hostname is valid
if test "$windows" = 0 -a "$in_rpm" = 0 -a $force = 0
then
  resolved=`$bindir/resolveip $hostname 2>&1`
  if [ $? -ne 0 ]
  then
    resolved=`$bindir/resolveip localhost 2>&1`
    if [ $? -ne 0 ]
    then
      echo "Neither host '$hostname' nor 'localhost' could be looked up with"
      echo "$bindir/resolveip"
      echo "Please configure the 'hostname' command to return a correct hostname."
      echo "If you want to solve this at a later stage, restart this script with"
      echo "the --force option"
      exit 1
    fi
    echo "WARNING: The host '$hostname' could not be looked up with resolveip."
    echo "This probably means that your libc libraries are not 100 % compatible"
    echo "with this binary MySQL version. The MySQL daemon, mysqld, should work"
    echo "normally with the exception that host name resolving will not work."
    echo "This means that you should use IP addresses instead of hostnames"
    echo "when specifying MySQL privileges !"
  fi
fi

if test "$ip_only" = "1"
then
  ip=`echo "$resolved" | awk '/ /{print $6}'`
  hostname=$ip
fi

# Create database directories mysql & test

  if test ! -d $ldata; then mkdir $ldata; chmod 700 $ldata ; fi
  if test ! -d $ldata/mysql; then mkdir $ldata/mysql;  chmod 700 $ldata/mysql ; fi
  if test ! -d $ldata/test; then mkdir $ldata/test;  chmod 700 $ldata/test ; fi
  if test -w / -a ! -z "$user"; then
    chown $user $ldata $ldata/mysql $ldata/test;
  fi

if test ! -f $mdata/db.frm
then
  c_d="yes"
fi

if test $verbose = 1
then
  create_option="verbose"
else
  create_option="real"
fi

if test -n "$user"; then
  args="$args --user=$user"
fi

if test "$in_rpm" -eq 0 -a "$windows" -eq 0
then
  echo "Installing all prepared tables"
fi
mysqld_install_cmd_line="$mysqld $defaults $mysqld_opt --bootstrap \
--skip-grant-tables --basedir=$basedir --datadir=$ldata --skip-innodb \
--skip-bdb --skip-ndbcluster $args --max_allowed_packet=8M --net_buffer_length=16K"
if $scriptdir/mysql_create_system_tables $create_option $mdata $hostname $windows \
   | eval "$mysqld_install_cmd_line" 
then
  if test -n "$fill_help_tables"
  then
    if test "$in_rpm" -eq 0 -a "$windows" -eq 0
    then
      echo "Fill help tables"
    fi
    (echo "use mysql;"; cat $fill_help_tables) | eval "$mysqld_install_cmd_line"
    res=$?
    if test $res != 0
    then
      echo ""
      echo "WARNING: HELP FILES ARE NOT COMPLETELY INSTALLED!"
      echo "The \"HELP\" command might not work properly"
      echo ""
    fi
  fi
  if test "$in_rpm" = 0 -a "$windows" = 0
  then
    echo ""
    echo "To start mysqld at boot time you have to copy support-files/mysql.server"
    echo "to the right place for your system"
    echo
  fi
  if test "$windows" -eq 0
  then
  echo "PLEASE REMEMBER TO SET A PASSWORD FOR THE MySQL root USER !"
  echo "To do so, start the server, then issue the following commands:"
  echo "$bindir/mysqladmin -u root password 'new-password'"
  echo "$bindir/mysqladmin -u root -h $hostname password 'new-password'"
  echo "See the manual for more instructions."
  #
  # Print message about upgrading unless we have created a new db table.
  if test -z "$c_d"
  then
    echo
    echo "NOTE:  If you are upgrading from a MySQL <= 3.22.10 you should run"
    echo "the $bindir/mysql_fix_privilege_tables. Otherwise you will not be"
    echo "able to use the new GRANT command!"
  fi
  echo
  if test "$in_rpm" = "0"
  then
    echo "You can start the MySQL daemon with:"
    echo "cd @prefix@ ; $bindir/mysqld_safe &"
    echo
    echo "You can test the MySQL daemon with the benchmarks in the 'sql-bench' directory:"
    echo "cd sql-bench ; perl run-all-tests"
    echo
  fi
  echo "Please report any problems with the @scriptdir@/mysqlbug script!"
  echo
  echo "The latest information about MySQL is available on the web at"
  echo "http://www.mysql.com"
  echo "Support MySQL by buying support/licenses at http://shop.mysql.com"
  fi
  exit 0
else
  echo "Installation of system tables failed!"
  echo
  echo "Examine the logs in $ldata for more information."
  echo "You can also try to start the mysqld daemon with:"
  echo "$mysqld --skip-grant &"
  echo "You can use the command line tool"
  echo "$bindir/mysql to connect to the mysql"
  echo "database and look at the grant tables:"
  echo
  echo "shell> $bindir/mysql -u root mysql"
  echo "mysql> show tables"
  echo
  echo "Try 'mysqld --help' if you have problems with paths. Using --log"
  echo "gives you a log in $ldata that may be helpful."
  echo
  echo "The latest information about MySQL is available on the web at"
  echo "http://www.mysql.com"
  echo "Please consult the MySQL manual section: 'Problems running mysql_install_db',"
  echo "and the manual section that describes problems on your OS."
  echo "Another information source is the MySQL email archive."
  echo "Please check all of the above before mailing us!"
  echo "And if you do mail us, you MUST use the @scriptdir@/mysqlbug script!"
  exit 1
fi
