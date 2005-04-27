#!/bin/sh

DbCreate -l
ret=$?
if [ $ret -ne 0 ]
then
	echo "DbCreate failed"
	exit $ret
fi

DbAsyncGenerator -time 300 -p 10 $*
ret=$?
exit $ret

