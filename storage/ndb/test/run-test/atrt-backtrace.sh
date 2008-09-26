#!/bin/sh

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
