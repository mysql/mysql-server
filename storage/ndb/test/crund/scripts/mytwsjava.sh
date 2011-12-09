#!/bin/bash

# Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

# usage: no args/options

touch out.txt
echo "" >> out.txt 2>&1
hwprefs -v cpu_count >> out.txt 2>&1
echo "" >> out.txt 2>&1
./restart_cluster.sh >> out.txt 2>&1
echo "" >> out.txt 2>&1
./load_shema.sh >> out.txt 2>&1
iostat 5 > iostat5.txt 2>&1 &
#vmstat 5 > vmstat5.txt 2>&1 &
pid=$!
echo "" >> out.txt 2>&1
( cd ../tws/tws_java/ ; ant run ) >> out.txt 2>&1
mkdir -p results/xxx
mv -v [a-z]*.txt results/xxx
mv -v ../tws/tws_java/log*.txt results/xxx
cp -v ../tws/tws_java/nbproject/configs/*.properties results/xxx
cp -v ../tws/tws_java/nbproject/private/*.properties results/xxx
cp -v ../tws/*.properties results/xxx
cp -v ../config.ini results/xxx
cp -v ../my.cnf results/xxx
mv -v results/xxx  results/xxx_tws_java
sleep 6
kill -9 $pid
