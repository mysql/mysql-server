#!/bin/sh

# Copyright (c) 2009, 2018, Oracle and/or its affiliates. All rights reserved.
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

set -e

files="daily-basic-tests.txt daily-devel-tests.txt upgrade-tests.txt"

die(){
    echo "error at $1 : $2"
    exit 1
}

check_state(){
    if  [ $1 != $2 ]
    then
	die $3 $4
    fi
}

check_file(){
    file=$1
    lineno=0
    testcase=0

    echo -n "-- checking $file..."
    cat $file | awk '{ print "^" $0 "$";}' > /tmp/ct.$$
    while read line
    do
	lineno=$(expr $lineno + 1)
	if [ $(echo $line | grep -c "^^#") -ne 0 ]
	then
	    continue
	fi
	
	case "$line" in
	    ^max-time:*)
		testcase=$(expr $testcase + 1);;
	    ^cmd:*)
		if [ $(echo $line | wc -w) -ne 2 ]
		then
		    die $file $lineno
		fi
		testcase=$(expr $testcase + 2);;
	    ^args:*)
		testcase=$(expr $testcase + 4);;
	    ^type:*)
		;;
	    ^max-retries:*)
		;;
	    ^$) 
                if [ $testcase -ne 7 ]
		then
		    die $file $lineno
		else
		    testcase=0
		    cnt=$(expr $cnt + 1)
		fi;;
	    *)
	        die $file $lineno
	esac
   done < /tmp/ct.$$
   rm -f /tmp/ct.$$
   if [ $testcase -ne 0 ]
   then
   	echo "Missing newline in end-of-file"
	die $file $lineno
   fi

   echo "ok"
}
   
for file in $files
do
    check_file $file
done
