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
builddir=""
ldata="@localstatedir@"
srcdir=""

args=""
defaults=""
mysqld_opt=""
user=""

force=0
in_rpm=0
ip_only=0
windows=0

usage()
{
  cat <<EOF
Usage: $0 [OPTIONS]
  --basedir=path       The path to the MySQL installation directory.
  --builddir=path      If using --srcdir with out-of-directory builds, you
                       will need to set this to the location of the build
                       directory where built files reside.
  --datadir=path       The path to the MySQL data directory.
  --force              Causes mysql_install_db to run even if DNS does not
                       work.  In that case, grant table entries that normally
                       use hostnames will use IP addresses.
  --ldata=path         The path to the MySQL data directory. Same as --datadir.
  --rpm                For internal use.  This option is used by RPM files
                       during the MySQL installation process.
  --skip-name-resolve  Use IP addresses rather than hostnames when creating
                       grant table entries.  This option can be useful if
                       your DNS does not work.
  --srcdir=path        The path to the MySQL source directory.  This option
                       uses the compiled binaries and support files within the
                       source tree, useful for if you don't want to install
                       MySQL yet and just want to create the system tables.
  --user=user_name     The login username to use for running mysqld.  Files
                       and directories created by mysqld will be owned by this
                       user.  You must be root to use this option.  By default
                       mysqld runs using your current login name and files and
                       directories that it creates will be owned by you.
  --windows            For internal use.  This option is used for creating
                       Windows distributions.

All other options are passed to the mysqld program

EOF
  exit 1
}

s_echo()
{
  if test "$in_rpm" -eq 0 -a "$windows" -eq 0
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
      --builddir=*) builddir=`parse_arg "$arg"` ;;
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

cannot_find_file()
{
  echo
  echo "FATAL ERROR: Could not find $*"
  echo
  echo "If you compiled from source, you need to run 'make install' to"
  echo "copy the software into the correct location ready for operation."
  echo
  echo "If you are using a binary release, you must either be at the top"
  echo "level of the extracted archive, or pass the --basedir option"
  echo "pointing to that location."
  echo
}

# Ok, let's go.  We first need to parse arguments which are required by
# my_print_defaults so that we can execute it first, then later re-parse
# the command line to add any extra bits that we need.
parse_arguments PICK-ARGS-FROM-ARGV "$@"

#
# We can now find my_print_defaults.  This script supports:
#
#   --srcdir=path pointing to compiled source tree
#   --basedir=path pointing to installed binary location
#
# or default to compiled-in locations.
#
if test -n "$srcdir" && test -n "$basedir"
then
  echo "ERROR: Specify either --basedir or --srcdir, not both."
  exit 1
fi
if test -n "$srcdir"
then
  if test -z "$builddir"
  then
    builddir="$srcdir"
  fi
  print_defaults="$builddir/extra/my_print_defaults"
elif test -n "$basedir"
then
  print_defaults=`find_in_basedir my_print_defaults bin extra`
else
  print_defaults="@bindir@/my_print_defaults"
fi

if test ! -x "$print_defaults"
then
  cannot_find_file "$print_defaults"
  exit 1
fi

# Now we can get arguments from the groups [mysqld] and [mysql_install_db]
# in the my.cfg file, then re-run to merge with command line arguments.
parse_arguments `$print_defaults $defaults mysqld mysql_install_db`
parse_arguments PICK-ARGS-FROM-ARGV "$@"

# Configure paths to support files
if test -n "$srcdir"
then
  basedir="$builddir"
  bindir="$basedir/client"
  extra_bindir="$basedir/extra"
  mysqld="$basedir/sql/mysqld"
  mysqld_opt="--language=$srcdir/sql/share/english"
  pkgdatadir="$srcdir/scripts"
  scriptdir="$srcdir/scripts"
elif test -n "$basedir"
then
  bindir="$basedir/bin"
  extra_bindir="$bindir"
  mysqld=`find_in_basedir mysqld libexec sbin bin`
  pkgdatadir=`find_in_basedir --dir fill_help_tables.sql share share/mysql`
  scriptdir="$basedir/scripts"
else
  basedir="@prefix@"
  bindir="@bindir@"
  extra_bindir="$bindir"
  mysqld="@libexecdir@/mysqld"
  pkgdatadir="@pkgdatadir@"
  scriptdir="@scriptdir@"
fi

# Set up paths to SQL scripts required for bootstrap
fill_help_tables="$pkgdatadir/fill_help_tables.sql"
create_system_tables="$pkgdatadir/mysql_system_tables.sql"
fill_system_tables="$pkgdatadir/mysql_system_tables_data.sql"

for f in $fill_help_tables $create_system_tables $fill_system_tables
do
  if test ! -f "$f"
  then
    cannot_find_file "$f"
    exit 1
  fi
done

if test ! -x "$mysqld"
then
  cannot_find_file "$mysqld"
  exit 1
fi

# Try to determine the hostname
hostname=`@HOSTNAME@`

# Check if hostname is valid
if test "$windows" -eq 0 -a "$in_rpm" -eq 0 -a "$force" -eq 0
then
  resolved=`$extra_bindir/resolveip $hostname 2>&1`
  if test $? -ne 0
  then
    resolved=`$extra_bindir/resolveip localhost 2>&1`
    if test $? -ne 0
    then
      echo "Neither host '$hostname' nor 'localhost' could be looked up with"
      echo "$extra_bindir/resolveip"
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

# Create database directories
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

# Configure mysqld command line
mysqld_bootstrap="${MYSQLD_BOOTSTRAP-$mysqld}"
mysqld_install_cmd_line="$mysqld_bootstrap $defaults $mysqld_opt --bootstrap \
  --basedir=$basedir --datadir=$ldata --log-warnings=0 --loose-skip-innodb \
  --loose-skip-ndbcluster $args --max_allowed_packet=8M \
  --net_buffer_length=16K"

# Create the system and help tables by passing them to "mysqld --bootstrap"
s_echo "Installing MySQL system tables..."
if `(echo "use mysql;"; cat $create_system_tables $fill_system_tables) | $mysqld_install_cmd_line`
then
  s_echo "OK"
else
  echo
  echo "Installation of system tables failed!  Examine the logs in"
  echo "$ldata for more information."
  echo
  echo "You can try to start the mysqld daemon with:"
  echo
  echo "    shell> $mysqld --skip-grant &"
  echo
  echo "and use the command line tool $bindir/mysql"
  echo "to connect to the mysql database and look at the grant tables:"
  echo
  echo "    shell> $bindir/mysql -u root mysql"
  echo "    mysql> show tables"
  echo
  echo "Try 'mysqld --help' if you have problems with paths.  Using --log"
  echo "gives you a log in $ldata that may be helpful."
  echo
  echo "The latest information about MySQL is available on the web at"
  echo "http://www.mysql.com/.  Please consult the MySQL manual section"
  echo "'Problems running mysql_install_db', and the manual section that"
  echo "describes problems on your OS.  Another information source are the"
  echo "MySQL email archives available at http://lists.mysql.com/."
  echo
  echo "Please check all of the above before mailing us!  And remember, if"
  echo "you do mail us, you MUST use the $scriptdir/mysqlbug script!"
  echo
  exit 1
fi

s_echo "Filling help tables..."
if `(echo "use mysql;"; cat $fill_help_tables) | $mysqld_install_cmd_line`
then
  s_echo "OK"
else
  echo
  echo "WARNING: HELP FILES ARE NOT COMPLETELY INSTALLED!"
  echo "The \"HELP\" command might not work properly."
fi

# Don't output verbose information if running inside bootstrap or using
# --srcdir for testing.
if test "$windows" -eq 0 && test -z "$srcdir"
then
  s_echo
  s_echo "To start mysqld at boot time you have to copy"
  s_echo "support-files/mysql.server to the right place for your system"

  echo
  echo "PLEASE REMEMBER TO SET A PASSWORD FOR THE MySQL root USER !"
  echo "To do so, start the server, then issue the following commands:"
  echo
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

  if test "$in_rpm" -eq 0
  then
    echo
    echo "You can start the MySQL daemon with:"
    echo "cd $basedir ; $bindir/mysqld_safe &"
    echo
    echo "You can test the MySQL daemon with mysql-test-run.pl"
    echo "cd $basedir/mysql-test ; perl mysql-test-run.pl"
  fi

  echo
  echo "Please report any problems with the $scriptdir/mysqlbug script!"
  echo
  echo "The latest information about MySQL is available at http://www.mysql.com/"
  echo "Support MySQL by buying support/licenses from http://shop.mysql.com/"
  echo
fi

exit 0
