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

cflags="-64 -mips4"
config_args=
if [ "$#" != 0 ]; then
  case "$1" in
    --help)
      echo "Usage: $0 [options]"
      echo "Options:" 
      echo "--help                print this message"
      echo "-32                   build 32-bit binary"
      echo "-64                   build 64-bit binary [default]"
      exit 0
      ;;
    -64)
      echo "Building 64-bit binary"
      ;;
    -32)
      echo "Building 32-bit binary"
      cflags=""
      ;;
    *)
      config_args="$config_args $1"; shift
      ;;
  esac
else
  echo "Building 64-bit binary"
fi

set -x
make maintainer-clean

path=`dirname $0`
. "$path/autorun.sh"

# C options:
# -apo          - auto-parallize for multiprocessors (implies -mp)
# -mp           - generate multiprocessor code
# These two common optimization options apparently use 'sproc' model of
# threading, which is not compatible with PTHREADS: don't add them unless you
# know what you're doing.
#
# -c99          - enable C features standardized in C99, such as long long,
#                 strtoll, stroull etc.
#                 This option is vital to compile MySQL.
# -woff         - turn off some warnings 
# -64           - generate 64 bit object (implies -mips4)
# -mips4        - produce code for MIPS R10000, MIPS R12000 and further 64 bit
#                 processors
# -OPT:Olimit=0 - no limits exists to size of function for compiler to optimize
#                 it
nowarn="-woff 1064,1188,1460,1552,1681,1682,3303"
cflags="$cflags $nowarn -O3 -c99 -OPT:Olimit=0"

# C++ only options:
# -LANG:exceptions=OFF            - don't generate exception handling code
#                                   MySQL doesn't use exceptions.
# -LANG:std=OFF                   - don't link standard C++ library, such as
#                                   <iostream>, <complex>, etc. 
# -LANG:libc_in_namespace_std=OFF - libstdc functions can be  
#                                   declared in namespace 'std', when included
#                                   into C++ code. Switch this feature off.
#                                   This option is vital to compile MySQL

cxxflags="$cflags -LANG:exceptions=OFF -LANG:std=OFF"
cxxflags="$cxxflags -LANG:libc_in_namespace_std=OFF"

CC=cc CXX=CC CFLAGS="$cflags" CXXFLAGS="$cxxflags" \
./configure --prefix=/usr/local/mysql --disable-shared \
            --with-extra-charsets=complex --enable-thread-safe-client \
            --without-extra-tools --disable-dependency-tracking \
            $config_args

make 
