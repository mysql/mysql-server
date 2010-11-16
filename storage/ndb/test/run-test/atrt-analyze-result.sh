#!/bin/sh

cores=`find result -name 'core*'`
if [ "$cores" ]
then
    for i in "$cores"
    do
	atrt-backtrace.sh $i
    done
fi

f=`find result -name 'log.out' | xargs grep "NDBT_ProgramExit: " | grep -c "Failed"`
o=`find result -name 'log.out' | xargs grep "NDBT_ProgramExit: " | grep -c "OK"`

if [ $o -gt 0 -a $f -eq 0 ]
then
    exit 0
fi

exit 1

