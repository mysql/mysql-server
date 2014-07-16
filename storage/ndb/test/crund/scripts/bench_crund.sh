#!/bin/bash

# Copyright (c) 2010, 2014, Oracle and/or its affiliates. All rights reserved.
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

echo "--> $0"

outdir="logs"
newdir="$outdir/xxx"
dstdir="$outdir/$1"
out="$newdir/out.txt"

if [ $# -ne 1 ] || [[ "$1" = *"help" ]] ; then
    echo "usage: $0 <run.c++crund|run.jcrund|...>"
    exit 1
fi

driver=$1
case $driver in
    run.c++crund|run.jcrund)
	;;
    "")
	echo "missing argument: <driver>"; exit 1
	;;
    *)
	echo "unknown <driver>: $driver"; exit 1
	;;
esac

if [ -d "$newdir" ] ; then
    echo "dir already exists: $newdir"
    exit 1
fi
if [ -d "$dstdir" ] ; then
    echo "dir already exists: $dstdir"
    exit 1
fi

mkdir -p "$newdir"
touch "$out"
echo "" | tee -a "$out"
if command -v hwprefs >> "$out" 2>&1 ; then
    hwprefs -v os_type machine_type cpu_type cpu_freq cpu_bus_freq cpu_count \
	2>&1 | tee -a "$out"
fi
echo "" | tee -a "$out"
#./restart_cluster.sh 2>&1 | tee -a "$out"

#vmstat 5 > vmstat5.txt 2>&1 &
iostat 5 > iostat5.txt 2>&1 &
pid=$!

echo "" | tee -a "$out"
./mysql.sh -v < ../src/crund_schema.sql 2>&1 | tee -a "$out"

echo "RUNNING CRUND ..." | tee -a "$out"
case $driver in
    run.c++crund)
	( cd .. ; make run.crund ) >> "$out" 2>&1
	;;
    run.jcrund)
	( cd .. ; ant run.crund.opt ) >> "$out" 2>&1
	;;
    *)
	echo "unknown <driver>: $driver" | tee -a "$out"
	;;
esac
echo "... DONE CRUND" | tee -a "$out"

sleep 6
kill -9 $pid

mv -v "$out" *stat*.txt ../log*.txt "$newdir"
cp -v ../*.properties ../build.xml ../config.ini ../my.cnf "$newdir"
mv -v "$newdir" "$dstdir"

echo
echo "<-- $0"
