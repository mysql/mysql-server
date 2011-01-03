#!/bin/sh

# Copyright (C) 2004, 2005 MySQL AB
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
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

if [ ! -f "sql/mysqld.cc" ]; then
  echo "You must run this script from the MySQL top-level directory."
  exit 1
fi

# -fast         Expand into a set of compiler options to result in
#               improved application run-time.  Options include: +O3,
#               +Onolooptransform, +Olibcalls, +FPD, +Oentryschedule,
#               +Ofastaccess.
# +O4           Perform level 3 as well as doing link time optimizations.
#               Also sends +Oprocelim and +Ofastaccess to the linker
#               (see ld(1)).

release_flags="-fast +O3"

# -z             Do not bind anything to address zero.  This option
#                allows runtime detection of null pointers.  See the
#                note on pointers below.
cflags="-g -z +O0"
cxxflags="-g0 -z +O0"
debug_configure_options="--with-debug"

while [ "$#" != 0 ]; do 
  case "$1" in
    --help)
      echo "Usage: $0 [options]"
      echo "Options:" 
      echo "--help                print this message"
      echo "--debug               build debug binary [default] "
      echo "--release             build optimised binary"
      echo "-32                   build 32 bit binary [default]"
      echo "-64                   build 64 bit binary"
      exit 0
      ;;
    --debug)
      echo "Building debug binary"
      ;;
    --release)
      echo "Building release binary"
      cflags="$release_flags"
      cxxflags="$release_flags"
      debug_configure_options=""
      ;;
    -32)
      echo "Building 32-bit binary"
      ;;
    -64)
      echo "Building 64-bit binary"
      cflags="$cflags +DA2.0W +DD64"
      cxxflags="$cxxflags +DA2.0W +DD64"
      ;;
    *)
      echo "$0: invalid option '$1'; use --help to show usage"
      exit 1
      ;;
  esac
  shift
done


set -x
make maintainer-clean

path=`dirname $0`
. "$path/autorun.sh"

CC=cc CXX=aCC CFLAGS="$cflags" CXXFLAGS="$cxxflags" \
./configure --prefix=/usr/local/mysql --disable-shared \
            --with-extra-charsets=complex --enable-thread-safe-client \
            --without-extra-tools $debug_configure_options \
						--disable-dependency-tracking

gmake 
