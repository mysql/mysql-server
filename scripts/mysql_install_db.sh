#!/bin/sh
# Copyright (C) 2002-2003 MySQL AB
# For a more info consult the file COPYRIGHT distributed with this file.

# This scripts creates the privilege tables db, host, user, tables_priv,
# columns_priv in the mysql database, as well as the func table.
#
# All unrecognized arguments to this script are passed to mysqld.

in_rpm=0
case "$1" in
    --rpm)
      in_rpm="1"; shift
      ;;
esac
windows=0
case "$1" in
    --windows)
      windows="1"; shift
      ;;
esac
defaults=
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
      --user=*) user=`echo "$arg" | sed -e 's/^[^=]*=//'` ;;
      --verbose) verbose=1 ;;
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
  elif test -x "@libexecdir@/mysqld"
  then
    execdir="@libexecdir@"
  else
    execdir="$basedir/bin"
  fi
fi

# find fill_help_tables.sh
for i in $basedir/support-files $basedir/share $basedir/share/mysql $basedir/scripts `pwd` @pkgdatadir@
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
  if test $verbose -eq 1
  then
    echo "Could not find help file 'fill_help_tables.sql' ;$pkgdatadir; ;$basedir;".
  fi
fi

mdata=$ldata/mysql

if test "$windows" -eq 0 -a ! -x $execdir/mysqld
then
  if test "$in_rpm" -eq 1
  then
    echo "FATAL ERROR $execdir/mysqld not found!"
    exit 1
  else
    echo "Didn't find $execdir/mysqld"
    echo "You should do a 'make install' before executing this script"
    exit 1
  fi
fi

hostname=`@HOSTNAME@`		# Install this too in the user table

# Check if hostname is valid
if test "$windows" -eq 0 -a "$in_rpm" -eq 0 -a $force -eq 0
then
  resolved=`$bindir/resolveip $hostname 2>&1`
  if [ $? -ne 0 ]
  then
    resolved=`$bindir/resolveip localhost 2>&1`
    if [ $? -eq 0 ]
    then
      echo "Neither host '$hostname' and 'localhost' could not be looked up with"
      echo "$bindir/resolveip"
      echo "Please configure the 'hostname' command to return a correct hostname."
      echo "If you want to solve this at a later stage, restart this script with"
      echo "the --force option"
      exit 1
    fi
    echo "WARNING: The host '$hostname' could not be looked up with resolveip."
    echo "This probably means that your libc libraries are not 100 % compatible"
    echo "with this binary MySQL version. The MySQL deamon, mysqld, should work"
    echo "normally with the exception that host name resolving will not work."
    echo "This means that you should use IP addresses instead of hostnames"
    echo "when specifying MySQL privileges !"
  fi
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

if test $verbose -eq 1
then
  create_option="verbose"
else
  create_option="real"
fi

echo "Installing all prepared tables"
if (
   mysql_create_system_tables $create_option $mdata $hostname $windows 
   if test -n "$fill_help_tables"
   then
     cat $fill_help_tables
   fi
) | eval "$execdir/mysqld $defaults --bootstrap --skip-grant-tables \
         --basedir=$basedir --datadir=$ldata --skip-innodb --skip-bdb $args" 
then
  echo ""
  if test "$in_rpm" -eq 0 || "$windows" -eq 0
  then
    echo "To start mysqld at boot time you have to copy support-files/mysql.server"
    echo "to the right place for your system"
    echo
  fi
  echo "PLEASE REMEMBER TO SET A PASSWORD FOR THE MySQL root USER !"
  echo "This is done with:"
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
  if test "$in_rpm" -eq 0 -a "$windows" -eq 0
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
  echo "Support MySQL by buying support/licenses at https://order.mysql.com"
  exit 0
else
  echo "Installation of grant tables failed!"
  echo
  echo "Examine the logs in $ldata for more information."
  echo "You can also try to start the mysqld daemon with:"
  echo "$execdir/mysqld --skip-grant &"
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
