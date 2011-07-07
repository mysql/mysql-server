# Copyright (c) 2000-2007 MySQL AB, 2008 Sun Microsystems, Inc.
# Use is subject to license terms.
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

cflags="$c_warnings $extra_flags"
cxxflags="$cxx_warnings $base_cxxflags $extra_flags"
extra_configs="$extra_configs $local_infile_configs"
configure="./configure $base_configs $extra_configs"

commands="\
$make -k maintainer-clean || true 
/bin/rm -rf */.deps/*.P configure config.cache storage/*/configure storage/*/config.cache autom4te.cache storage/*/autom4te.cache;

path=`dirname $0`
. \"$path/autorun.sh\""

if [ -z "$just_clean" ]
then
commands="$commands
CC=\"$CC\" CFLAGS=\"$cflags\" CXX=\"$CXX\" CXXFLAGS=\"$cxxflags\" CXXLDFLAGS=\"$CXXLDFLAGS\" \
$configure"
fi

if [ -z "$just_configure" -a -z "$just_clean" ]
then
  commands="$commands

$make $AM_MAKEFLAGS"

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
