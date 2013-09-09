#!/usr/bin/env bash
# $Id$
# run a sequence of hotindexer undo tests.

tests=""
verbose=0
valgrind=""
exitcode=0

for arg in $* ; do 
    if [[ $arg =~ --(.*)=(.*) ]] ; then
	eval ${BASH_REMATCH[1]}=${BASH_REMATCH[2]}
    else
	tests="$tests $arg"
    fi
done

for t in $tests ; do
    testdir=`dirname $t`
    testfile=`basename $t`
    testname=""
    resultfile=""
    if [[ $testfile =~ (.*)\.test$ ]] ; then
	testname=${BASH_REMATCH[1]}
        resultfile=$testname.result
    else
	exit 1
    fi
    if [ $verbose != 0 ] ; then echo $testdir $testname $testfile $resultfile; fi

    $valgrind ./hotindexer-undo-do-test.tdb $testdir/$testfile >$testdir/$testname.run

    if [ -f $testdir/$resultfile ] ; then
	diff -q $testdir/$testname.run $testdir/$resultfile >/dev/null 2>&1
	exitcode=$?
    else
	exitcode=1
    fi
    if [ $verbose != 0 ] ; then
	echo $testname $exitcode
    else
	rm $testdir/$testname.run
    fi
    if [ $exitcode != 0 ] ; then break; fi
done

exit $exitcode
