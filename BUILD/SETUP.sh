if ! test -f sql/mysqld.cc
then
  echo "You must run this script from the MySQL top-level directory"
  exit 1
fi

nonono=
just_configure=
while test $# -gt 0
do
  case "$1" in
  -c | --just-configure ) just_configure=1; shift ;;
  -n | --just-print | --print ) nonono=1; shift ;;
  -h | --help ) cat <<EOF; exit 0 ;;
Usage: $0 [-h|-n] [configure-options]
  -h, --help              Show this help message.
  -n, --just-print        Don't actually run any commands; just print them.
  -c, --just-configure    Stop after running configure.

Any other options will be passed directly to configure.

Note:  this script is intended for internal use by MySQL developers.
EOF
  * ) break ;;
  esac
done

set -e

export AM_MAKEFLAGS
AM_MAKEFLAGS="-j 4"

# If you are not using codefusion add "-Wpointer-arith" to WARNINGS
# The following warning flag will give too many warnings:
# -Wshadow -Wunused  -Winline (The later isn't usable in C++ as
# __attribute()__ doesn't work with gnu C++)
global_warnings="-Wimplicit -Wreturn-type -Wid-clash-51 -Wswitch -Wtrigraphs -Wcomment -W -Wchar-subscripts -Wformat -Wimplicit-function-dec -Wimplicit-int -Wparentheses -Wsign-compare -Wwrite-strings"
debug_extra_warnings="-Wuninitialized"
c_warnings="$global_warnings -Wunused"
cxx_warnings="$global_warnings -Woverloaded-virtual -Wextern-inline -Wsign-promo -Wreorder -Wctor-dtor-privacy -Wnon-virtual-dtor"

alpha_cflags="-mcpu=ev6 -Wa,-mev6"	# Not used yet
pentium_cflags=""
sparc_cflags=""

# be as fast as we can be without losing our ability to backtrace
fast_cflags="-O3 -fno-omit-frame-pointer"
# this is one is for someone who thinks 1% speedup is worth not being
# able to backtrace
reckless_cflags="-O3 -fomit-frame-pointer "
debug_cflags="-DEXTRA_DEBUG -DFORCE_INIT_OF_VARS -DSAFEMALLOC -DSAFE_MUTEX -O2"

base_cxxflags="-felide-constructors -fno-exceptions -fno-rtti"

base_configs="--prefix=/usr/local/mysql --enable-assembler --with-extra-charsets=complex --enable-thread-safe-client --with-mysqld-ldflags=-all-static \
 --with-client-ldflags=-all-static"
alpha_configs=""	# Not used yet
pentium_configs=""
sparc_configs=""

debug_configs="--with-debug"

if gmake --version > /dev/null 2>&1
then
  make=gmake
else
  make=make
fi
