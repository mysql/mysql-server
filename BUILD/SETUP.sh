#!/bin/sh

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
  --with-debug=full       Build with full debug.
  --warning-mode=[old|pedantic]
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
    -c | --just-configure)
      just_configure=1;;
    -n | --just-print | --print)
      just_print=1;;
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
full_debug=
warning_mode=

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

export AM_MAKEFLAGS
AM_MAKEFLAGS="-j 6"

# SSL library to use.--with-ssl will select our bundled yaSSL
# implementation of SSL. To use openSSl you will nee too point out
# the location of openSSL headers and lbs on your system.
# Ex --with-ssl=/usr
SSL_LIBRARY=--with-ssl

if [ "x$warning_mode" != "xpedantic" ]; then
# Both C and C++ warnings
  warnings="-Wall -Wextra -Wunused -Wwrite-strings"

# For more warnings, uncomment the following line
# warnings="$warnings -Wshadow"

# C warnings
  c_warnings="$warnings"
# C++ warnings
  cxx_warnings="$warnings -Wno-unused-parameter"
# cxx_warnings="$cxx_warnings -Woverloaded-virtual -Wsign-promo"
  cxx_warnings="$cxx_warnings -Wctor-dtor-privacy -Wnon-virtual-dtor"
# Added unless --with-debug=full
  debug_extra_cflags="-O0 -g3 -gdwarf-2"
else
  warnings="-W -Wall -ansi -pedantic -Wno-long-long -Wno-unused -D_POSIX_SOURCE"
  c_warnings="$warnings"
  cxx_warnings="$warnings -std=c++98"
# NOTE: warning mode should not influence optimize/debug mode.
# Please feel free to add a separate option if you don't feel it's an overkill.
  debug_extra_cflags="-O0"
# Reset CPU flags (-mtune), they don't work in -pedantic mode
  check_cpu_cflags=""
fi

# Set flags for various build configurations.
# Used in -valgrind builds
# Override -DFORCE_INIT_OF_VARS from debug_cflags. It enables the macro
# LINT_INIT(), which is only useful for silencing spurious warnings
# of static analysis tools. We want LINT_INIT() to be a no-op in Valgrind.
valgrind_flags="-USAFEMALLOC -UFORCE_INIT_OF_VARS -DHAVE_purify "
valgrind_flags="$valgrind_flags -DMYSQL_SERVER_SUFFIX=-valgrind-max"
valgrind_configs="--with-valgrind"
#
# Used in -debug builds
debug_cflags="-DUNIV_MUST_NOT_INLINE -DEXTRA_DEBUG -DFORCE_INIT_OF_VARS "
debug_cflags="$debug_cflags -DSAFEMALLOC -DPEDANTIC_SAFEMALLOC -DSAFE_MUTEX"
error_inject="--with-error-inject "
#
# Base C++ flags for all builds
base_cxxflags="-felide-constructors -fno-exceptions -fno-rtti"
#
# Flags for optimizing builds.
# Be as fast as we can be without losing our ability to backtrace.
fast_cflags="-O3 -fno-omit-frame-pointer"

debug_configs="--with-debug$full_debug"
if [ -z "$full_debug" ]
then
  debug_cflags="$debug_cflags $debug_extra_cflags"
fi

#
# Configuration options.
#
base_configs="--prefix=$prefix --enable-assembler "
base_configs="$base_configs --with-extra-charsets=complex "
base_configs="$base_configs --enable-thread-safe-client "
base_configs="$base_configs --with-big-tables"

if test -d "$path/../cmd-line-utils/readline"
then
    base_configs="$base_configs --with-readline"
elif test -d "$path/../cmd-line-utils/libedit"
then
    base_configs="$base_configs --with-libedit"
fi

static_link="--with-mysqld-ldflags=-all-static "
static_link="$static_link --with-client-ldflags=-all-static"
# we need local-infile in all binaries for rpl000001
# if you need to disable local-infile in the client, write a build script
# and unset local_infile_configs
local_infile_configs="--enable-local-infile"


max_no_embedded_configs="$SSL_LIBRARY --with-plugins=max"
max_no_ndb_configs="$SSL_LIBRARY --with-plugins=max-no-ndb --with-embedded-server"
max_configs="$SSL_LIBRARY --with-plugins=max --with-embedded-server"

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
  CXX=gcc
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

gcov_configs="--disable-shared"

# gprof

gprof_compile_flags="-O2 -pg -g"

gprof_link_flags="--disable-shared $static_link"

