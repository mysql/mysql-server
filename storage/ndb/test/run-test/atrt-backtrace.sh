#!/bin/sh

# Copyright (c) 2008, 2023, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
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
echo "bt" > $bt
echo "thread apply all bt" >> $bt
echo "quit" >> $bt

out=`dirname $core`/bt.txt

#
# get binary
#
exe=`file $core | sed 's/.*execfn//' | cut -d"'" -f 2`

if [ -x "$exe" ]
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
