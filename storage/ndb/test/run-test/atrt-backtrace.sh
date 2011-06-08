#!/bin/sh

# Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

# Does not work on Windows (gcc only)
if [ `uname | grep -ic cygwin || true` -ne 0 ]
then
    exit
fi

core=$1
out=$2

if [ ! -f "$core" ]
then
    exit
fi

if [ -f `dirname $core`/env.sh ]
then
    . `dirname $core`/env.sh
fi

#
# gdb command file
#
bt=`dirname $core`/bt.gdb
echo "thread apply all bt" > $bt
echo "quit" >> $bt

#
# Fix output
#
if [ -z "$out" ]
then
    out=`dirname $core`/bt.txt
fi
outarg=">> $out 2>&1"
if [ "$out" = "-" ]
then
    outarg=""
fi

#
# get binary
#
tmp=`echo ${core}.tmp`
gdb -c $core -x $bt -q 2>&1 | grep "Core was generated" | awk '{ print $5;}' | sed 's!`!!' | sed `echo "s/'\.//"` > $tmp
exe=`cat $tmp`
rm -f $tmp

if [ -x $exe ]
then
    echo "*** $exe - $core" >> $out
    gdb -q -batch -x $bt -c $core $exe >> $out 2>&1
elif [ -x "`which $exe 2> /dev/null`" ]
then
    exe=`which $exe`
    echo "*** $exe - $core" >> $out
    gdb -q -batch -x $bt -c $core $exe >> $out 2>&1
else
    echo "*** $core : cant find exe: $exe" >> $out
fi
rm -f $bt
