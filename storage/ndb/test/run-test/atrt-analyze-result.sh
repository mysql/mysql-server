#!/bin/sh

p="`pwd`"
bt=`find result -name bt.sh`
if [ "$bt" ]
then
    for i in $bt
    do
	cd `dirname $i`
	sh ./bt.sh || true
	cd "$p"
    done
fi

f=`find result -name 'log.out' | xargs grep "NDBT_ProgramExit: " | grep -c "Failed"`
o=`find result -name 'log.out' | xargs grep "NDBT_ProgramExit: " | grep -c "OK"`

if [ $o -gt 0 -a $f -eq 0 ]
then
    exit 0
fi

exit 1

