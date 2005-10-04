#!/bin/sh

if ! test -f sql/mysqld.cc
then
  echo "You must run this script from the MySQL top-level directory"
  exit 1
fi

prefix_configs="--prefix=/usr/local/mysql"
just_print=
just_configure=
full_debug=
if test -n "$MYSQL_BUILD_PREFIX"
then
  prefix_configs="--prefix=$MYSQL_BUILD_PREFIX"
fi

while test $# -gt 0
do
  case "$1" in
  --prefix=* ) prefix_configs="$1"; shift ;;
  --with-debug=full ) full_debug="=full"; shift ;;
  -c | --just-configure ) just_configure=1; shift ;;
  -n | --just-print | --print ) just_print=1; shift ;;
  -h | --help ) cat <<EOF; exit 0 ;;
Usage: $0 [-h|-n] [configure-options]
  -h, --help              Show this help message.
  -n, --just-print        Don't actually run any commands; just print them.
  -c, --just-configure    Stop after running configure.
  --with-debug=full       Build with full debug.
  --prefix=path           Build with prefix 'path'.

Note:  this script is intended for internal use by MySQL developers.
EOF
  * )
    echo "Unknown option '$1'"
    exit 1
    break ;;
  esac
done

set -e

export AM_MAKEFLAGS
AM_MAKEFLAGS="-j 4"

# If you are not using codefusion add "-Wpointer-arith" to WARNINGS
# The following warning flag will give too many warnings:
# -Wshadow -Wunused  -Winline (The later isn't usable in C++ as
# __attribute()__ doesn't work with gnu C++)
global_warnings="-Wimplicit -Wreturn-type -Wswitch -Wtrigraphs -Wcomment -W -Wchar-subscripts -Wformat -Wparentheses -Wsign-compare -Wwrite-strings"
#debug_extra_warnings="-Wuninitialized"
c_warnings="$global_warnings -Wunused"
cxx_warnings="$global_warnings -Woverloaded-virtual -Wsign-promo -Wreorder -Wctor-dtor-privacy -Wnon-virtual-dtor -ansi"
base_max_configs="--with-innodb --with-berkeley-db --with-ndbcluster --with-archive-storage-engine --with-openssl --with-big-tables --with-blackhole-storage-engine --with-federated-storage-engine --with-csv-storage-engine"
base_max_no_ndb_configs="--with-innodb --with-berkeley-db --without-ndbcluster --with-archive-storage-engine --with-openssl --with-big-tables --with-blackhole-storage-engine --with-federated-storage-engine --with-csv-storage-engine"
max_leave_isam_configs="--with-innodb --with-berkeley-db --with-ndbcluster --with-archive-storage-engine --with-federated-storage-engine --with-blackhole-storage-engine --with-csv-storage-engine --with-openssl --with-embedded-server --with-big-tables"
max_configs="$base_max_configs --with-embedded-server"
max_no_ndb_configs="$base_max_no_ndb_configs --with-embedded-server"

path=`dirname $0`
. "$path/check-cpu"

alpha_cflags="$check_cpu_cflags -Wa,-m$cpu_flag"
amd64_cflags="$check_cpu_cflags"
pentium_cflags="$check_cpu_cflags"
pentium64_cflags="$check_cpu_cflags -m64"
ppc_cflags="$check_cpu_cflags"
sparc_cflags=""

# be as fast as we can be without losing our ability to backtrace
fast_cflags="-O3 -fno-omit-frame-pointer"
# this is one is for someone who thinks 1% speedup is worth not being
# able to backtrace
reckless_cflags="-O3 -fomit-frame-pointer "

debug_cflags="-DUNIV_MUST_NOT_INLINE -DEXTRA_DEBUG -DFORCE_INIT_OF_VARS -DSAFEMALLOC -DPEDANTIC_SAFEMALLOC -DSAFE_MUTEX"
debug_extra_cflags="-O1 -Wuninitialized"

base_cxxflags="-felide-constructors -fno-exceptions -fno-rtti"
amd64_cxxflags=""				# If dropping '--with-big-tables', add here  "-DBIG_TABLES"

base_configs="$prefix_configs --enable-assembler --with-extra-charsets=complex --enable-thread-safe-client --with-readline --with-big-tables"
static_link="--with-mysqld-ldflags=-all-static --with-client-ldflags=-all-static"
amd64_configs=""
alpha_configs=""	# Not used yet
pentium_configs=""
sparc_configs=""
# we need local-infile in all binaries for rpl000001
# if you need to disable local-infile in the client, write a build script
# and unset local_infile_configs
local_infile_configs="--enable-local-infile"

debug_configs="--with-debug$full_debug"
if [ -z "$full_debug" ]
then
  debug_cflags="$debug_cflags $debug_extra_cflags"
fi

if gmake --version > /dev/null 2>&1
then
  make=gmake
else
  make=make
fi

if test -z "$CXX" ; then
  CXX=gcc
fi

# If ccache (a compiler cache which reduces build time)
# (http://samba.org/ccache) is installed, use it.
# We use 'grep' and hope 'grep' will work as expected
# (returns 0 if finds lines)
if ccache -V > /dev/null 2>&1
then
  if ! (echo "$CC" | grep "ccache" > /dev/null)
  then
    CC="ccache $CC"
  fi
  if ! (echo "$CXX" | grep "ccache" > /dev/null)
  then
    CXX="ccache $CXX"
  fi
fi
