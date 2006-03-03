#!/bin/sh
# Copyright (C) 2002-2003 MySQL AB
# For a more info consult the file COPYRIGHT distributed with this file.

# Runs mysqlcheck --check-upgrade in case it has not been done on this
# major MySQL version

# This script should always be run when upgrading from one major version
# to another (ie: 4.1 -> 5.0 -> 5.1)

#
# Note that in most cases one have to use '--password' as
# arguments as these needs to be passed on to the mysqlcheck command


user=root

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
      --basedir=*) MY_BASEDIR_VERSION=`echo "$arg" | sed -e 's/^[^=]*=//'` ;;
      --user=*) user=`echo "$arg" | sed -e 's/^[^=]*=//'` ;;
      --ldata=*|--data=*|--datadir=*) DATADIR=`echo "$arg" | sed -e 's/^[^=]*=//'` ;;
      --force) force=1 ;;
      --verbose) verbose=1 ;;
      --help) help_option=1 ;;
      *)
        if test -n "$pick_args"
        then
          # This sed command makes sure that any special chars are quoted,
          # so the arg gets passed exactly to the server.
          args="$args "`echo "$arg" | sed -e 's,\([^a-zA-Z0-9_.=-]\),\\\\\1,g'`
        fi
        ;;
    esac
  done
}

#
# Find where my_print_defaults is
#

find_my_print_defaults () {
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
}

find_my_print_defaults

# Get first arguments from the my.cfg file, groups [mysqld] and
# [mysql_upgrade], and then merge with the command line arguments

args=
DATADIR=
bindir=
MY_BASEDIR_VERSION=
verbose=0
force=0
help_option=0

parse_arguments `$print_defaults $defaults mysqld mysql_upgrade`
parse_arguments PICK-ARGS-FROM-ARGV "$@"

if test $help_option = 1
then
  echo "MySQL utility script to upgrade database to the current server version"
  echo ""
  echo "It takes the following arguments:"
  echo "  --help     Show this help message"
  echo "  --basedir  Specifies the directory where MySQL is installed"
  echo "  --datadir  Specifies the data directory"
  echo "  --force    Mysql_upgrade.info file will be ignored"
  echo "  --user     Username for server login if not current user"
  echo "  --verbose  Display more output about the process"
  echo ""

  exit 0
fi

#
# Try to find where binaries are installed
#

MY_PWD=`pwd`
# Check for the directories we would expect from a binary release install
if test -z "$MY_BASEDIR_VERSION"
then
  if test -f ./share/mysql/english/errmsg.sys -a -x ./bin/mysqld
  then
    MY_BASEDIR_VERSION=$MY_PWD            # Where bin, share and data are
    bindir="$MY_BASEDIR_VERSION/bin"
  # Check for the directories we would expect from a source install
  elif test -f ./share/mysql/english/errmsg.sys -a -x ./libexec/mysqld
  then
    MY_BASEDIR_VERSION=$MY_PWD            # Where libexec, share and var are
    bindir="$MY_BASEDIR_VERSION/bin"
# Since we didn't find anything, used the compiled-in defaults
  else
    MY_BASEDIR_VERSION=@prefix@
    bindir=@bindir@
  fi
else
  bindir="$MY_BASEDIR_VERSION/bin"
fi

#
# Try to find the data directory
#

if test -z "$DATADIR"
then
  # Try where the binary installs put it
  if test -d $MY_BASEDIR_VERSION/data/mysql
  then
    DATADIR=$MY_BASEDIR_VERSION/data
  # Next try where the source installs put it
  elif test -d $MY_BASEDIR_VERSION/var/mysql
  then
    DATADIR=$MY_BASEDIR_VERSION/var
  # Or just give up and use our compiled-in default
  else
    DATADIR=@localstatedir@
  fi
fi

if test ! -x "$bindir/mysqlcheck"
then
  echo "Can't find program '$bindir/mysqlcheck'"
  echo "Please restart with --basedir=mysql-install-directory"
  exit 1
fi

if test ! -f "$DATADIR/mysql/user.frm"
then
  echo "Can't find data directory. Please restart with --datadir=path-to-data-dir"
  exit 1
fi

CHECK_FILE=$DATADIR/mysql_upgrade.info

if test -f $CHECK_FILE -a $force = 0
then
  version=`cat $CHECK_FILE`
  if test "$version" = "@MYSQL_BASE_VERSION@"
  then
    if test $verbose = 1
    then
       echo "mysql_upgrade already done for this version"
    fi
    $bindir/mysql_fix_privilege_tables --silent $args
    exit 0
  fi
fi

#
# Run the upgrade
#

check_args="--check-upgrade --all-databases --auto-repair --user=$user"

if test $verbose = 1
then
  echo "Running $bindir/mysqlcheck $args $check_args"
fi

$bindir/mysqlcheck $check_args $args
if [ $? = 0 ]
then
  # Remember base version so that we don't run this script again on the
  # same base version
  echo "@MYSQL_BASE_VERSION@" > $CHECK_FILE
fi

$bindir/mysql_fix_privilege_tables --silent --user=$user $args
