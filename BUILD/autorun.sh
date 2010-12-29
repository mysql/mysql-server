#!/bin/sh

# Copyright (C) 2005 MySQL AB
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
# Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
# MA 02111-1307, USA

# Create MySQL autotools infrastructure

die() { echo "$@"; exit 1; }

# Handle "glibtoolize" (e.g., for native OS X autotools) as another
# name for "libtoolize". Use the first one, either name, found in PATH.
LIBTOOLIZE=libtoolize  # Default
IFS="${IFS=   }"; save_ifs="$IFS"; IFS=':'
for dir in $PATH
do
  if test -x $dir/glibtoolize
  then
    LIBTOOLIZE=glibtoolize
    break
  elif test -x $dir/libtoolize
  then
    break
  fi
done
IFS="$save_ifs"

aclocal || die "Can't execute aclocal" 
autoheader || die "Can't execute autoheader"
# --force means overwrite ltmain.sh script if it already exists 
$LIBTOOLIZE --automake --force --copy || die "Can't execute libtoolize"
  
# --add-missing instructs automake to install missing auxiliary files
# and --force to overwrite them if they already exist
automake --add-missing --force  --copy || die "Can't execute automake"
autoconf || die "Can't execute autoconf"
