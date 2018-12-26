#!/bin/sh

# Copyright (C) 2000, 2007 MySQL AB
# Use is subject to license terms
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

#
# Execute some simple basic test on MyISAM libary to check if things
# works at all.

valgrind="valgrind --alignment=8 --leak-check=yes"
silent="-s"

if test -f mi_test1$MACH ; then suffix=$MACH ; else suffix=""; fi
./mi_test1$suffix $silent
./myisamchk$suffix -se test1
./mi_test1$suffix $silent -N -S
./myisamchk$suffix -se test1
./mi_test1$suffix $silent -P --checksum
./myisamchk$suffix -se test1
./mi_test1$suffix $silent -P -N -S
./myisamchk$suffix -se test1
./mi_test1$suffix $silent -B -N -R2
./myisamchk$suffix -sm test1
./mi_test1$suffix $silent -a -k 480 --unique
./myisamchk$suffix -sm test1
./mi_test1$suffix $silent -a -N -S -R1
./myisamchk$suffix -sm test1
./mi_test1$suffix $silent -p -S
./myisamchk$suffix -sm test1
./mi_test1$suffix $silent -p -S -N --unique
./myisamchk$suffix -sm test1
./mi_test1$suffix $silent -p -S -N --key_length=127 --checksum
./myisamchk$suffix -sm test1
./mi_test1$suffix $silent -p -S -N --key_length=128
./myisamchk$suffix -sm test1
./mi_test1$suffix $silent -p -S --key_length=480
./myisamchk$suffix -sm test1
./mi_test1$suffix $silent -a -B
./myisamchk$suffix -sm test1
./mi_test1$suffix $silent -a -B --key_length=64  --unique
./myisamchk$suffix -sm test1
./mi_test1$suffix $silent -a -B -k 480 --checksum
./myisamchk$suffix -sm test1
./mi_test1$suffix $silent -a -B -k 480 -N  --unique --checksum
./myisamchk$suffix -sm test1
./mi_test1$suffix $silent -a -m
./myisamchk$suffix -sm test1
./mi_test1$suffix $silent -a -m -P --unique --checksum
./myisamchk$suffix -sm test1
./mi_test1$suffix $silent -a -m -P --key_length=480 --key_cache
./myisamchk$suffix -sm test1
./mi_test1$suffix $silent -m -p
./myisamchk$suffix -sm test1
./mi_test1$suffix $silent -w -S --unique
./myisamchk$suffix -sm test1
./mi_test1$suffix $silent -a -w --key_length=64 --checksum
./myisamchk$suffix -sm test1
./mi_test1$suffix $silent -a -w -N --key_length=480
./myisamchk$suffix -sm test1
./mi_test1$suffix $silent -a -w -S --key_length=480 --checksum
./myisamchk$suffix -sm test1
./mi_test1$suffix $silent -a -b -N
./myisamchk$suffix -sm test1
./mi_test1$suffix $silent -a -b --key_length=480
./myisamchk$suffix -sm test1
./mi_test1$suffix $silent -p -B --key_length=480
./myisamchk$suffix -sm test1

./mi_test1$suffix $silent --checksum
./myisamchk$suffix -se test1
./myisamchk$suffix -rs test1
./myisamchk$suffix -se test1
./myisamchk$suffix -rqs test1
./myisamchk$suffix -se test1
./myisamchk$suffix -rs --correct-checksum test1
./myisamchk$suffix -se test1
./myisamchk$suffix -rqs --correct-checksum test1
./myisamchk$suffix -se test1
./myisamchk$suffix -ros --correct-checksum test1
./myisamchk$suffix -se test1
./myisamchk$suffix -rqos --correct-checksum test1
./myisamchk$suffix -se test1

# check of myisampack / myisamchk
./myisampack$suffix --force -s test1
# Ignore error for index file
./myisamchk$suffix -es test1 2>&1 >& /dev/null
./myisamchk$suffix -rqs test1
./myisamchk$suffix -es test1
./myisamchk$suffix -rs test1
./myisamchk$suffix -es test1
./myisamchk$suffix -rus test1
./myisamchk$suffix -es test1

./mi_test1$suffix $silent --checksum -S
./myisamchk$suffix -se test1
./myisamchk$suffix -ros test1
./myisamchk$suffix -rqs test1
./myisamchk$suffix -se test1

./myisampack$suffix --force -s test1
./myisamchk$suffix -rqs test1
./myisamchk$suffix -es test1
./myisamchk$suffix -rus test1
./myisamchk$suffix -es test1

./mi_test1$suffix $silent --checksum --unique
./myisamchk$suffix -se test1
./mi_test1$suffix $silent --unique -S
./myisamchk$suffix -se test1


./mi_test1$suffix $silent --key_multiple -N -S
./myisamchk$suffix -sm test1
./mi_test1$suffix $silent --key_multiple -a -p --key_length=480
./myisamchk$suffix -sm test1
./mi_test1$suffix $silent --key_multiple -a -B --key_length=480
./myisamchk$suffix -sm test1
./mi_test1$suffix $silent --key_multiple -P -S
./myisamchk$suffix -sm test1

./mi_test2$suffix $silent -L -K -W -P
./myisamchk$suffix -sm test2
./mi_test2$suffix $silent -L -K -W -P -A
./myisamchk$suffix -sm test2
./mi_test2$suffix $silent -L -K -W -P -S -R1 -m500
echo "mi_test2$suffix $silent -L -K -R1 -m2000 ;  Should give error 135"
./myisamchk$suffix -sm test2
./mi_test2$suffix $silent -L -K -R1 -m2000
./myisamchk$suffix -sm test2
./mi_test2$suffix $silent -L -K -P -S -R3 -m50 -b1000000
./myisamchk$suffix -sm test2
./mi_test2$suffix $silent -L -B
./myisamchk$suffix -sm test2
./mi_test2$suffix $silent -D -B -c
./myisamchk$suffix -sm test2
./mi_test2$suffix $silent -m10000 -e8192 -K
./myisamchk$suffix -sm test2
./mi_test2$suffix $silent -m10000 -e16384 -E16384 -K -L
./myisamchk$suffix -sm test2

./mi_test2$suffix $silent -L -K -W -P -m50 -l
./myisamlog$suffix
./mi_test2$suffix $silent -L -K -W -P -m50 -l -b100
./myisamlog$suffix
time ./mi_test2$suffix $silent
time ./mi_test2$suffix $silent -K -B
time ./mi_test2$suffix $silent -L -B
time ./mi_test2$suffix $silent -L -K -B
time ./mi_test2$suffix $silent -L -K -W -B
time ./mi_test2$suffix $silent -L -K -W -S -B
time ./mi_test2$suffix $silent -D -K -W -S -B
