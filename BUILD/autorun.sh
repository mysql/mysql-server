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

aclocal || die "Can't execute aclocal" 
autoheader || die "Can't execute autoheader"
# --force means overwrite ltmain.sh script if it already exists 
# Added glibtoolize reference to make native OSX autotools work
if test -f /usr/bin/glibtoolize  ; then
  glibtoolize --automake --force || die "Can't execute glibtoolize"
else
  libtoolize --automake --force || die "Can't execute libtoolize"
fi
  
# --add-missing instructs automake to install missing auxiliary files
# and --force to overwrite them if they already exist
automake --add-missing --force || die "Can't execute automake"
autoconf || die "Can't execute autoconf"
(cd bdb/dist && sh s_all)
(cd innobase && aclocal && autoheader && aclocal && automake && autoconf)
