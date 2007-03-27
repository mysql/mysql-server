#!/bin/sh
# Copyright (C) 2002-2003 MySQL AB
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

# This scripts creates the MySQL Server system tables
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

s_echo()
{
  if test "$in_rpm" -eq 0 -a "$windows" -eq 0
  then
    echo $1
  fi
}

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
      --srcdir=*)  srcdir=`echo "$arg" | sed -e 's/^[^=]*=//'` ;;
      --ldata=*|--datadir=*) ldata=`echo "$arg" | sed -e 's/^[^=]*=//'` ;;
      --user=*)
        # Note that the user will be passed to mysqld so that it runs
        # as 'user' (crucial e.g. if log-bin=/some_other_path/
        # where a chown of datadir won't help)
	 user=`echo "$arg" | sed -e 's/^[^=]*=//'` ;;
      --skip-name-resolve) ip_only=1 ;;
      --verbose) verbose=1 ;; # Obsolete
      --rpm) in_rpm=1 ;;

      --windows)
	# This is actually a "cross bootstrap" argument used when
        # building the MySQL system tables on a different host
        # than the target. The platform independent
        # files that are created in --datadir on the host can
        # be copied to the target system, the most common use for
        # this feature is in the windows installer which will take
        # the files from datadir and include them as part of the install
        # package.
         windows=1 ;;

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
srcdir=
force=0

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

# Check that no previous MySQL installation exist
if test -f "$ldata/mysql/db.frm"
then
  echo "FATAL ERROR: Found already existing MySQL system tables"
  echo "in $ldata."
  echo "If you are upgrading from a previous MySQL version you"
  echo "should run '$bindir/mysql_upgrade', "
  echo "to upgrade all tables for this version of MySQL"
  exit 1;
fi

# Find SQL scripts needed for bootstrap
fill_help_tables="fill_help_tables.sql"
create_system_tables="mysql_system_tables.sql"
fill_system_tables="mysql_system_tables_data.sql"
if test -n "$srcdir"
then
  fill_help_tables=$srcdir/scripts/$fill_help_tables
  create_system_tables=$srcdir/scripts/$create_system_tables
  fill_system_tables=$srcdir/scripts/$fill_system_tables
else
  for i in $basedir/support-files $basedir/share $basedir/share/mysql \
           $basedir/scripts `pwd` `pwd`/scripts @pkgdatadir@
  do
    if test -f $i/$fill_help_tables
    then
      pkgdatadir=$i
    fi
  done

  fill_help_tables=$pkgdatadir/$fill_help_tables
  create_system_tables=$pkgdatadir/$create_system_tables
  fill_system_tables=$pkgdatadir/$fill_system_tables
fi

if test ! -f $create_system_tables
then
  echo "FATAL ERROR: Could not find SQL file '$create_system_tables' in"
  echo "@pkgdatadir@ or inside $basedir"
  exit 1;
fi

if test ! -f $fill_help_tables
then
  echo "FATAL ERROR: Could not find help file '$fill_help_tables' in"
  echo "@pkgdatadir@ or inside $basedir"
  exit 1;
fi

if test ! -f $fill_system_tables
then
  echo "FATAL ERROR: Could not find help file '$fill_system_tables' in"
  echo "@pkgdatadir@ or inside $basedir"
  exit 1;
fi

# Find executables and paths
mysqld=$execdir/mysqld
mysqld_opt=""
scriptdir=$bindir

if test "$windows" = 1
then
  mysqld="./sql/mysqld"
  if test -n "$srcdir" -a -f $srcdir/sql/share/english/errmsg.sys
  then
    langdir=$srcdir/sql/share/english
  else
    langdir=./sql/share/english
  fi
  mysqld_opt="--language=$langdir"
  scriptdir="./scripts"
fi

if test ! -x $mysqld
then
  if test "$in_rpm" = 1
  then
    echo "FATAL ERROR $mysqld not found!"
    exit 1
  else
    echo "FATAL ERROR Didn't find $mysqld"
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
      echo "Please configure the 'hostname' command to return a correct"
      echo "hostname."
      echo "If you want to solve this at a later stage, restart this script"
      echo "with the --force option"
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
if test ! -d $ldata; then
  mkdir $ldata;
  chmod 700 $ldata ;
fi
if test ! -d $ldata/mysql; then
  mkdir $ldata/mysql;
  chmod 700 $ldata/mysql ;
fi
if test ! -d $ldata/test; then
  mkdir $ldata/test;
  chmod 700 $ldata/test ;
fi
if test -w / -a ! -z "$user"; then
  chown $user $ldata $ldata/mysql $ldata/test;
fi

if test -n "$user"; then
  args="$args --user=$user"
fi

# Peform the install of system tables
mysqld_bootstrap="${MYSQLD_BOOTSTRAP-$mysqld}"
mysqld_install_cmd_line="$mysqld_bootstrap $defaults $mysqld_opt --bootstrap \
--basedir=$basedir --datadir=$ldata --skip-innodb \
--skip-bdb --skip-ndbcluster $args --max_allowed_packet=8M \
--net_buffer_length=16K"

# Pipe mysql_system_tables.sql to "mysqld --bootstrap"
s_echo "Installing MySQL system tables..."
if `(echo "use mysql;"; cat $create_system_tables $fill_system_tables) | $mysqld_install_cmd_line`
then
  s_echo "OK"

  if test -n "$fill_help_tables"
  then
    s_echo "Filling help tables..."
    # Pipe fill_help_tables.sql to "mysqld --bootstrap"
    if `(echo "use mysql;"; cat $fill_help_tables) | $mysqld_install_cmd_line`
    then
      # Fill suceeded
      s_echo "OK"
    else
      echo ""
      echo "WARNING: HELP FILES ARE NOT COMPLETELY INSTALLED!"
      echo "The \"HELP\" command might not work properly"
      echo ""
    fi
  fi

  s_echo ""
  s_echo "To start mysqld at boot time you have to copy"
  s_echo "support-files/mysql.server to the right place for your system"
  s_echo

  if test "$windows" -eq 0
  then
    # A root password should of course also be set on Windows!
    # The reason for not displaying these prompts here is that when
    # executing this script with the --windows argument the script
    # is used to generate system tables mainly used by the
    # windows installer. And thus the password should not be set until
    # those files has been copied to the target system
    echo "PLEASE REMEMBER TO SET A PASSWORD FOR THE MySQL root USER !"
    echo "To do so, start the server, then issue the following commands:"
    echo "$bindir/mysqladmin -u root password 'new-password'"
    echo "$bindir/mysqladmin -u root -h $hostname password 'new-password'"
    echo "See the manual for more instructions."

    if test "$in_rpm" = "0"
    then
      echo "You can start the MySQL daemon with:"
      echo "cd @prefix@ ; $bindir/mysqld_safe &"
      echo
      echo "You can test the MySQL daemon with mysql-test-run.pl"
      echo "cd mysql-test ; perl mysql-test-run.pl"
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
  echo "You can try to start the mysqld daemon with:"
  echo "$mysqld --skip-grant &"
  echo "and use the command line tool"
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
