#/bin/sh

###############################################################################
#
# Author: Lasse Collin
#
# This file has been put into the public domain.
# You can do whatever you want with this file.
#
###############################################################################

for I in "$srcdir"/files/good-*.xz
do
	if ../src/xzdec/xzdec "$I" > /dev/null 2> /dev/null ; then
		:
	else
		echo "Good file failed: $I"
		(exit 1)
		exit 1
	fi
done

for I in "$srcdir"/files/bad-*.xz
do
	if ../src/xzdec/xzdec "$I" > /dev/null 2> /dev/null ; then
		echo "Bad file succeeded: $I"
		(exit 1)
		exit 1
	fi
done

(exit 0)
exit 0
