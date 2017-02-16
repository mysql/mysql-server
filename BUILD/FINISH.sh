# Copyright (c) 2000, 2010, Oracle and/or its affiliates. All rights reserved.
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
# Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
# MA 02110-1301, USA

cflags="$c_warnings $extra_flags $EXTRA_FLAGS $EXTRA_CFLAGS"
cxxflags="$cxx_warnings $base_cxxflags $extra_flags $EXTRA_FLAGS $EXTRA_CXXFLAGS"
extra_configs="$extra_configs $local_infile_configs $EXTRA_CONFIGS"

configure="./configure $base_configs $extra_configs"

if test "$just_print" = "1" -a "$just_configure" = "1"
then
  just_print=""
  configure="$configure --print"
fi

if test "$AM_EXTRA_MAKEFLAGS" = "VERBOSE=1" -o "$verbose_make" = "1"
then
  configure="$configure --verbose"
fi

commands="\
/bin/rm -rf configure;
/bin/rm -rf CMakeCache.txt CMakeFiles/

path=`dirname $0`
. \"$path/autorun.sh\""

if [ -z "$just_clean" ]
then
commands="$commands
CC=\"$CC\" CFLAGS=\"$cflags\" CXX=\"$CXX\" CXXFLAGS=\"$cxxflags\" CXXLDFLAGS=\"$CXXLDFLAGS\" $configure"
fi

if [ -z "$just_configure" -a -z "$just_clean" ]
then
  commands="$commands

$make $AM_MAKEFLAGS $AM_EXTRA_MAKEFLAGS"

  if [ "x$strip" = "xyes" ]
  then
    commands="$commands

mkdir -p tmp
nm --numeric-sort sql/mysqld  > tmp/mysqld.sym
objdump -d sql/mysqld > tmp/mysqld.S
strip sql/mysqld"
  fi
fi

if test -z "$just_print"
then
  eval "set -x; $commands"
else
  echo "$commands"
fi
