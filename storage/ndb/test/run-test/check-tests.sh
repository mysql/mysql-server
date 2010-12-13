#!/bin/sh

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
