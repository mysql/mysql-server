#!/bin/sh

DbCreate
ret=$?
if [ $ret -ne 0 ]
then
	echo "DbCreate failed"
	exit $ret
fi

DbAsyncGenerator -time 300 $*
ret=$?
exit $ret

