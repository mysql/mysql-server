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

basedir=""
ldata=""
srcdir=""

args=""
defaults=""
mysqld_opt=""
user=""

force=0
in_rpm=0
ip_only=0
cross_bootstrap=0

usage()
{
  cat <<EOF
Usage: $0 [OPTIONS]
  --basedir=path       The path to the MySQL installation directory.
  --cross-bootstrap    For internal use.  Used when building the MySQL system
                       tables on a different host than the target.
  --datadir=path       The path to the MySQL data directory.
  --force              Causes mysql_install_db to run even if DNS does not
                       work.  In that case, grant table entries that normally
                       use hostnames will use IP addresses.
  --ldata=path         The path to the MySQL data directory.
  --rpm                For internal use.  This option is used by RPM files
                       during the MySQL installation process.
  --skip-name-resolve  Use IP addresses rather than hostnames when creating
                       grant table entries.  This option can be useful if
                       your DNS does not work.
  --srcdir=path        For internal use.  The directory under which
                       mysql_install_db looks for support files such as the
                       error message file and the file for popoulating the
                       help tables.
  --user=user_name     The login username to use for running mysqld.  Files
                       and directories created by mysqld will be owned by this
                       user.  You must be root to use this option.  By default
                       mysqld runs using your current login name and files and
                       directories that it creates will be owned by you.

All other options are passed to the mysqld program

EOF
  exit 1
}

s_echo()
{
  if test "$in_rpm" -eq 0 -a "$cross_bootstrap" -eq 0
  then
    echo "$1"
  fi
}

parse_arg()
{
  echo "$1" | sed -e 's/^[^=]*=//'
}

parse_arguments()
{
  # We only need to pass arguments through to the server if we don't
  # handle them here.  So, we collect unrecognized options (passed on
  # the command line) into the args variable.
  pick_args=
  if test "$1" = PICK-ARGS-FROM-ARGV
  then
    pick_args=1
    shift
  fi

  for arg
  do
    case "$arg" in
      --force) force=1 ;;
      --basedir=*) basedir=`parse_arg "$arg"` ;;
      --srcdir=*)  srcdir=`parse_arg "$arg"` ;;
      --ldata=*|--datadir=*) ldata=`parse_arg "$arg"` ;;
      --user=*)
        # Note that the user will be passed to mysqld so that it runs
        # as 'user' (crucial e.g. if log-bin=/some_other_path/
        # where a chown of datadir won't help)
	 user=`parse_arg "$arg"` ;;
      --skip-name-resolve) ip_only=1 ;;
      --verbose) verbose=1 ;; # Obsolete
      --rpm) in_rpm=1 ;;
      --help) usage ;;
      --no-defaults|--defaults-file=*|--defaults-extra-file=*)
        defaults="$arg" ;;

      --cross-bootstrap|--windows)
        # Used when building the MySQL system tables on a different host than
        # the target. The platform-independent files that are created in
        # --datadir on the host can be copied to the target system.
        #
        # The most common use for this feature is in the Windows installer
        # which will take the files from datadir and include them as part of
        # the install package.  See top-level 'dist-hook' make target.
        #
        # --windows is a deprecated alias
         cross_bootstrap=1 ;;

      *)
        if test -n "$pick_args"
        then
          # This sed command makes sure that any special chars are quoted,
          # so the arg gets passed exactly to the server.
          # XXX: This is broken; true fix requires using eval and proper
          # quoting of every single arg ($basedir, $ldata, etc.)
          #args="$args "`echo "$arg" | sed -e 's,\([^a-zA-Z0-9_.-]\),\\\\\1,g'`
          args="$args $arg"
        fi
        ;;
    esac
  done
}

# Try to find a specific file within --basedir which can either be a binary
# release or installed source directory and return the path.
find_in_basedir()
{
  case "$1" in
    --dir)
      return_dir=1; shift
      ;;
  esac

  file=$1; shift

  for dir in "$@"
  do
    if test -f "$basedir/$dir/$file"
    then
      if test -n "$return_dir"
      then
        echo "$basedir/$dir"
      else
        echo "$basedir/$dir/$file"
      fi
      break
    fi
  done
}

missing_in_basedir()
{
  echo "FATAL ERROR: Could not find $* inside --basedir"
  echo
  echo "When using --basedir you must point either into a MySQL binary"
  echo "distribution directory or a compiled tree previously populated"
  echo "by 'make install'"
}

# Ok, let's go.  We first need to parse arguments which are required by
# my_print_defaults so that we can execute it first, then later re-parse
# the command line to add any extra bits that we need.
parse_arguments PICK-ARGS-FROM-ARGV "$@"

# We can now find my_print_defaults, either in the supplied --basedir
# location or in the installed area.
if test -n "$basedir"
then
  print_defaults=`find_in_basedir my_print_defaults bin extra`
  if test ! -x "$print_defaults"
  then
    missing_in_basedir my_print_defaults
    exit 1
  fi
else
  print_defaults="@bindir@/my_print_defaults"
  if test ! -x "$print_defaults"
  then
    echo "FATAL ERROR: Could not find $print_defaults"
    echo
    echo "If you are using a binary release, you must run this script from"
    echo "within the directory the archive extracted into.  If you compiled"
    echo "MySQL yourself you must run 'make install' first."
    exit 1
  fi
fi

# Now we can get arguments from the groups [mysqld] and [mysql_install_db]
# in the my.cfg file, then re-run to merge with command line arguments.
parse_arguments `$print_defaults $defaults mysqld mysql_install_db`
parse_arguments PICK-ARGS-FROM-ARGV "$@"

# Path to MySQL installation directory
if test -z "$basedir"
then
  basedir="@prefix@"
  bindir="@bindir@"
  mysqld="@libexecdir@/mysqld"
  pkgdatadir="@pkgdatadir@"
else
  bindir="$basedir/bin"
  # We set up bootstrap-specific paths later, so skip this for now
  if test "$cross_bootstrap" -eq 0
  then
    pkgdatadir=`find_in_basedir --dir fill_help_tables.sql share share/mysql`
    if test -z "$pkgdatadir"
    then
      missing_in_basedir fill_help_tables.sql
      exit 1
    fi
    mysqld=`find_in_basedir mysqld libexec sbin bin`
    if test ! -x "$mysqld"
    then
      missing_in_basedir mysqld
      exit 1
    fi
  fi
fi

# Path to data directory
if test -z "$ldata"
then
  ldata="@localstatedir@"
fi

# Set up paths to SQL scripts required for bootstrap and ensure they exist.
if test -n "$srcdir"
then
  pkgdatadir="$srcdir/scripts"
fi

fill_help_tables="$pkgdatadir/fill_help_tables.sql"
create_system_tables="$pkgdatadir/mysql_system_tables.sql"
fill_system_tables="$pkgdatadir/mysql_system_tables_data.sql"

for f in $fill_help_tables $create_system_tables $fill_system_tables
do
  if test ! -f "$f"
  then
    echo "FATAL ERROR: Could not find SQL file '$f'"
    exit 1
  fi
done

# Set up bootstrap-specific paths
if test "$cross_bootstrap" -eq 1
then
  mysqld="./sql/mysqld"
  if test -n "$srcdir" -a -f "$srcdir/sql/share/english/errmsg.sys"
  then
    mysqld_opt="--language=$srcdir/sql/share/english"
  else
    mysqld_opt="--language=./sql/share/english"
  fi
fi

# Make sure mysqld is available in default location (--basedir option is
# already tested above).
if test ! -x "$mysqld"
then
  echo "FATAL ERROR: $mysqld not found!"
  exit 1
fi

# Try to determine the hostname
hostname=`@HOSTNAME@`

# Check if hostname is valid
if test "$cross_bootstrap" -eq 0 -a "$in_rpm" -eq 0 -a "$force" -eq 0
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

if test "$ip_only" -eq 1
then
  hostname=`echo "$resolved" | awk '/ /{print $6}'`
fi

# Create database directories mysql & test
for dir in $ldata $ldata/mysql $ldata/test
do
  if test ! -d $dir
  then
    mkdir -p $dir
    chmod 700 $dir
  fi
  if test -w / -a ! -z "$user"
  then
    chown $user $dir
  fi
done

if test -n "$user"
then
  args="$args --user=$user"
fi

# When doing a "cross bootstrap" install, no reference to the current
# host should be added to the system tables.  So we filter out any
# lines which contain the current host name.
if test $cross_bootstrap -eq 1
then
  filter_cmd_line="sed -e '/@current_hostname/d'"
else
  filter_cmd_line="cat"
fi

# Peform the install of system tables
mysqld_bootstrap="${MYSQLD_BOOTSTRAP-$mysqld}"
mysqld_install_cmd_line="$mysqld_bootstrap $defaults $mysqld_opt --bootstrap \
  --basedir=$basedir --datadir=$ldata --skip-innodb --skip-bdb \
  --skip-ndbcluster $args --max_allowed_packet=8M --net_buffer_length=16K"

# Pipe mysql_system_tables.sql to "mysqld --bootstrap"
s_echo "Installing MySQL system tables..."
if { echo "use mysql;"; cat $create_system_tables $fill_system_tables; } | eval "$filter_cmd_line" | $mysqld_install_cmd_line > /dev/null
then
  s_echo "OK"

  s_echo "Filling help tables..."
  # Pipe fill_help_tables.sql to "mysqld --bootstrap"
  if { echo "use mysql;"; cat $fill_help_tables; } | $mysqld_install_cmd_line > /dev/null
  then
    s_echo "OK"
  else
    echo
    echo "WARNING: HELP FILES ARE NOT COMPLETELY INSTALLED!"
    echo "The \"HELP\" command might not work properly"
    echo
  fi

  s_echo
  s_echo "To start mysqld at boot time you have to copy"
  s_echo "support-files/mysql.server to the right place for your system"
  s_echo

  if test "$cross_bootstrap" -eq 0
  then
    # This is not a true installation on a running system.  The end user must
    # set a password after installing the data files on the real host system.
    # At this point, there is no end user, so it does not make sense to print
    # this reminder.
    echo "PLEASE REMEMBER TO SET A PASSWORD FOR THE MySQL root USER !"
    echo "To do so, start the server, then issue the following commands:"
    echo "$bindir/mysqladmin -u root password 'new-password'"
    echo "$bindir/mysqladmin -u root -h $hostname password 'new-password'"
    echo
    echo "Alternatively you can run:"
    echo "$bindir/mysql_secure_installation"
    echo
    echo "which will also give you the option of removing the test"
    echo "databases and anonymous user created by default.  This is"
    echo "strongly recommended for production servers."
    echo
    echo "See the manual for more instructions."
    echo

    if test "$in_rpm" -eq 0
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
