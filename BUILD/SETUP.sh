#!/bin/sh

# Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; version 2
# of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the Free
# Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
# MA 02110-1301, USA

########################################################################

get_key_value()
{
  echo "$1" | sed 's/^--[a-zA-Z_-]*=//'
}

usage()
{
cat <<EOF 
Usage: $0 [-h|-n] [configure-options]
  -h, --help              Show this help message.
  -n, --just-print        Don't actually run any commands; just print them.
  -c, --just-configure    Stop after running configure.
  --extra-configs=xxx     Add this to configure options
  --extra-flags=xxx       Add this C and CXX flags
  --extra-cflags=xxx      Add this to C flags
  --extra-cxxflags=xxx    Add this to CXX flags
  --verbose               Print out full compile lines
  --with-debug=full       Build with full debug(no optimizations, keep call stack).
  --warning-mode=[old|pedantic|maintainer]
                          Influences the debug flags. Old is default.
  --prefix=path           Build with prefix 'path'.

Note: this script is intended for internal use by MySQL developers.
EOF
}

parse_options()
{
  while test $# -gt 0
  do
    case "$1" in
    --prefix=*)
      prefix=`get_key_value "$1"`;;
    --with-debug=full)
      full_debug="=full";;
    --warning-mode=*)
      warning_mode=`get_key_value "$1"`;;
    --extra-flags=*)
      EXTRA_FLAGS=`get_key_value "$1"`;;
    --extra-cflags=*)
      EXTRA_CFLAGS=`get_key_value "$1"`;;
    --extra-cxxflags=*)
      EXTRA_CXXFLAGS=`get_key_value "$1"`;;
    --extra-configs=*)
      EXTRA_CONFIGS=`get_key_value "$1"`;;
    -c | --just-configure)
      just_configure=1;;
    -n | --just-print | --print)
      just_print=1;;
    --verbose)
      verbose_make=1;;
    -h | --help)
      usage
      exit 0;;
    *)
      echo "Unknown option '$1'"
      exit 1;;
    esac
    shift
  done
}

########################################################################

if test ! -f sql/mysqld.cc
then
  echo "You must run this script from the MySQL top-level directory"
  exit 1
fi

prefix="/usr/local/mysql"
just_print=
just_configure=
warning_mode=
maintainer_mode=
full_debug=
verbose_make=

parse_options "$@"

if test -n "$MYSQL_BUILD_PREFIX"
then
  prefix="$MYSQL_BUILD_PREFIX"
fi

set -e

#
# Check for the CPU and set up CPU specific flags. We may reset them
# later.
# 
path=`dirname $0`
. "$path/check-cpu"
. "$path/util.sh"

get_make_parallel_flag

# SSL library to use.--with-ssl will select our bundled yaSSL
# implementation of SSL. To use OpenSSL you will need to specify
# the location of OpenSSL headers and libs on your system.
# Ex --with-ssl=/usr
SSL_LIBRARY=--with-ssl

if [ "x$warning_mode" = "xpedantic" ]; then
  warnings="-W -Wall -ansi -pedantic -Wno-long-long -Wno-unused -D_POSIX_SOURCE"
  c_warnings="$warnings"
  cxx_warnings="$warnings -std=c++98"
# NOTE: warning mode should not influence optimize/debug mode.
# Please feel free to add a separate option if you don't feel it's an overkill.
  debug_extra_cflags="-O0"
# Reset CPU flags (-mtune), they don't work in -pedantic mode
  check_cpu_cflags=""
elif [ "x$warning_mode" = "xmaintainer" ]; then
  c_warnings="-Wall -Wextra"
  cxx_warnings="$c_warnings -Wno-unused-parameter"
  maintainer_mode="--enable-mysql-maintainer-mode"
  debug_extra_cflags="-g3"
else
# Both C and C++ warnings
  warnings="-Wall -Wextra -Wunused -Wwrite-strings -Wno-uninitialized"

# For more warnings, uncomment the following line
# warnings="$warnings -Wshadow"

# C warnings
  c_warnings="$warnings"
# C++ warnings
  cxx_warnings="$warnings -Wno-unused-parameter"
# cxx_warnings="$cxx_warnings -Woverloaded-virtual -Wsign-promo"
  cxx_warnings="$cxx_warnings -Wnon-virtual-dtor"
  debug_extra_cflags="-O0 -g3 -gdwarf-2"
fi

# Set flags for various build configurations.
# Used in -valgrind builds
# Override -DFORCE_INIT_OF_VARS from debug_cflags. It enables the macro
# LINT_INIT(), which is only useful for silencing spurious warnings
# of static analysis tools. We want LINT_INIT() to be a no-op in Valgrind.
valgrind_flags="-DHAVE_valgrind -USAFEMALLOC"
valgrind_flags="$valgrind_flags -UFORCE_INIT_OF_VARS -Wno-uninitialized"
valgrind_flags="$valgrind_flags -DMYSQL_SERVER_SUFFIX=-valgrind-max"
valgrind_configs="--with-valgrind"
#
# Used in -debug builds
debug_cflags="-DUNIV_MUST_NOT_INLINE -DEXTRA_DEBUG"
debug_cflags="$debug_cflags -DSAFE_MUTEX -DSAFEMALLOC"
error_inject="--with-error-inject "
#
# Base C++ flags for all builds
base_cxxflags="-felide-constructors -fno-exceptions -fno-rtti"
#
# Flags for optimizing builds.
# Be as fast as we can be without losing our ability to backtrace.
fast_cflags="-O3 -fno-omit-frame-pointer"

debug_configs="--with-debug"
if [ -z "$full_debug" ]
then
  debug_cflags="$debug_cflags $debug_extra_cflags"
fi

# we need local-infile in all binaries for rpl000001
# if you need to disable local-infile in the client, write a build script
# and unset local_infile_configs
local_infile_configs="--enable-local-infile"

#
# Configuration options.
#
base_configs="--prefix=$prefix --enable-assembler "
base_configs="$base_configs --with-extra-charsets=complex "
base_configs="$base_configs --enable-thread-safe-client "
base_configs="$base_configs --with-big-tables $maintainer_mode"
base_configs="$base_configs --with-plugin-aria --with-aria-tmp-tables"

if test -d "$path/../cmd-line-utils/readline"
then
    base_configs="$base_configs --with-readline"
elif test -d "$path/../cmd-line-utils/libedit"
then
    base_configs="$base_configs --with-libedit"
fi

max_no_embedded_configs="$SSL_LIBRARY --with-plugins=max"
max_no_qc_configs="$SSL_LIBRARY --with-plugins=max --without-query-cache"
max_no_ndb_configs="$SSL_LIBRARY --with-plugins=max-no-ndb --with-embedded-server --with-libevent"
max_configs="$SSL_LIBRARY --with-plugins=max --with-embedded-server --with-libevent"
all_configs="$SSL_LIBRARY --with-plugins=max --with-plugin-ndbcluster --with-embedded-server --with-innodb_plugin --with-libevent"

#
# CPU and platform specific compilation flags.
#
alpha_cflags="$check_cpu_cflags -Wa,-m$cpu_flag"
amd64_cflags="$check_cpu_cflags"
amd64_cxxflags=""  # If dropping '--with-big-tables', add here  "-DBIG_TABLES"
pentium_cflags="$check_cpu_cflags"
pentium64_cflags="$check_cpu_cflags -m64"
ppc_cflags="$check_cpu_cflags"
sparc_cflags=""

if gmake --version > /dev/null 2>&1
then
  make=gmake
else
  make=make
fi

if test -z "$CC" ; then
  CC=gcc
fi

if test -z "$CXX" ; then
  CXX=g++
fi


#
# Set -Wuninitialized to debug flags for gcc 4.4 and above
# because it is allowed there without -O
#
if test `$CC -v 2>&1 | tail -1 | sed 's/ .*$//'` = 'gcc' ; then
  GCCVERSION=`$CC -v 2>&1 | tail -1 | \
    sed 's/^[a-zA-Z][a-zA-Z]* [a-zA-Z][a-zA-Z]* //' | sed 's/ .*$//'`
  GCCV1=`echo $GCCVERSION | sed 's/\..*$//'`
  GCCV2=`echo $GCCVERSION | sed 's/[0-9][0-9]*\.//'|sed 's/\..*$//'`
  if test '(' "$GCCV1" -gt '4' ')' -o \
    '(' '(' "$GCCV1" -eq '4' ')' -a '(' "$GCCV2" -ge '4' ')' ')'
  then
    debug_cflags="$debug_cflags -DFORCE_INIT_OF_VARS -Wuninitialized"
  fi
fi


# If ccache (a compiler cache which reduces build time)
# (http://samba.org/ccache) is installed, use it.
# We use 'grep' and hope 'grep' will work as expected
# (returns 0 if finds lines)
if test "$USING_GCOV" != "1"
then
  # Not using gcov; Safe to use ccache
  CCACHE_GCOV_VERSION_ENABLED=1
fi

if ccache -V > /dev/null 2>&1 && test "$CCACHE_GCOV_VERSION_ENABLED" = "1"
then
  echo "$CC" | grep "ccache" > /dev/null || CC="ccache $CC"
  echo "$CXX" | grep "ccache" > /dev/null || CXX="ccache $CXX"
fi

# gcov

# The  -fprofile-arcs and -ftest-coverage options cause GCC to instrument the
# code with profiling information used by gcov.
# The -DDISABLE_TAO_ASM is needed to avoid build failures in Yassl.
# The -DHAVE_gcov enables code to write out coverage info even when crashing.

gcov_compile_flags="-fprofile-arcs -ftest-coverage"
gcov_compile_flags="$gcov_compile_flags -DDISABLE_TAO_ASM"
gcov_compile_flags="$gcov_compile_flags -DMYSQL_SERVER_SUFFIX=-gcov -DHAVE_gcov"

# GCC4 needs -fprofile-arcs -ftest-coverage on the linker command line (as well
# as on the compiler command line), and this requires setting LDFLAGS for BDB.

gcov_link_flags="-fprofile-arcs -ftest-coverage"

gcov_configs="--with-gcov"

# gprof

gprof_compile_flags="-O2 -pg -g"

gprof_link_flags="--disable-shared $static_link"

