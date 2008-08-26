#!/bin/sh

core=$1
out=$2

if [ ! -f "$core" ]
then
    exit
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
    eval "gdb -q -batch -x bt.gdb -c $core $exe $outarg"
elif [ -x "`which $exe 2> /dev/null`" ]
then
    exe=`which $exe`
    echo "*** $exe - $core" >> $out
    eval "gdb -q -batch -x bt.gdb -c $core $exe $outarg"
else
    eval "echo \"*** $core : cant find exe: $exe\" $outarg"
fi
rm -f $bt
