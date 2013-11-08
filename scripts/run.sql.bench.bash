#!/usr/bin/env bash

function usage() {
    echo "run the sql bench tests"
    echo "--mysqlbuild=$mysqlbuild"
    echo "--commit=$commit"
}

function retry() {
    local cmd
    local retries
    local exitcode
    cmd=$*
    let retries=0
    while [ $retries -le 10 ] ; do
        echo `date` $cmd
        bash -c "$cmd"
        exitcode=$?
        echo `date` $cmd $exitcode $retries
        let retries=retries+1
        if [ $exitcode -eq 0 ] ; then break; fi
        sleep 10
    done
    test $exitcode = 0
}

svnserver=https://svn.tokutek.com/tokudb
basedir=$HOME/svn.build
builddir=$basedir/mysql.build
mysqlbuild=
mysqlserver=`hostname`
commit=0
engine=tokudb
socket=/tmp/mysql.sock
system=`uname -s | tr [:upper:] [:lower:]`
arch=`uname -m | tr [:upper:] [:lower:]`

# parse the command line
while [ $# -gt 0 ] ; do
    arg=$1; shift
    if [[ $arg =~ --(.*)=(.*) ]] ; then
        eval ${BASH_REMATCH[1]}=${BASH_REMATCH[2]}
    else
        usage; exit 1
    fi
done

if [[ $mysqlbuild =~ (.*)-(tokudb-.*)-(linux)-(x86_64) ]] ; then
    mysql=${BASH_REMATCH[1]}
    tokudb=${BASH_REMATCH[2]}
    system=${BASH_REMATCH[3]}
    arch=${BASH_REMATCH[4]}
else
    echo $mysqlbuild is not a tokudb build
fi

# goto the base directory
if [ ! -d $basedir ] ; then mkdir $basedir; fi
pushd $basedir

# update the build directory
if [ ! -d $builddir ] ; then mkdir $builddir; fi

date=`date +%Y%m%d`
testresultsdir=$builddir/$date
pushd $builddir
while [ ! -d $date ] ; do
    svn mkdir $svnserver/mysql.build/$date -m ""
    svn checkout -q $svnserver/mysql.build/$date
    if [ $? -ne 0 ] ; then rm -rf $date; fi
done
popd

# run the tests
pushd /usr/local/mysql/sql-bench

tracefile=sql-bench-$engine-$mysqlbuild-$mysqlserver.trace
summaryfile=sql-bench-$engine-$mysqlbuild-$mysqlserver.summary

function mydate() {
    date +"%Y%m%d %H:%M:%S"
}

function runtests() {
    testargs=$*
    for testname in test-* ; do
        chmod +x ./$testname
        echo `mydate` $testname $testargs
        ./$testname $testargs
        exitcode=$?
        echo `mydate`
        if [ $exitcode != 0 ] ; then
            # assume that the test failure due to a crash.  allow mysqld to restart.
            sleep 60
        fi
    done
}

>$testresultsdir/$tracefile

runtests --create-options=engine=$engine --socket=$socket --verbose --small-test         >>$testresultsdir/$tracefile 2>&1
runtests --create-options=engine=$engine --socket=$socket --verbose --small-test --fast  >>$testresultsdir/$tracefile 2>&1
runtests --create-options=engine=$engine --socket=$socket --verbose                      >>$testresultsdir/$tracefile 2>&1
runtests --create-options=engine=$engine --socket=$socket --verbose              --fast  >>$testresultsdir/$tracefile 2>&1
runtests --create-options=engine=$engine --socket=$socket --verbose              --fast --fast-insert >>$testresultsdir/$tracefile 2>&1
runtests --create-options=engine=$engine --socket=$socket --verbose              --fast --lock-tables >>$testresultsdir/$tracefile 2>&1

popd

# summarize the results
python ~/bin/sql.bench.summary.py <$testresultsdir/$tracefile >$testresultsdir/$summaryfile

testresult=""
pf=`mktemp`
egrep "^PASS" $testresultsdir/$summaryfile >$pf 2>&1
if [ $? -eq 0 ] ; then testresult="PASS=`cat $pf | wc -l` $testresult"; fi
egrep "^FAIL" $testresultsdir/$summaryfile >$pf 2>&1
if [ $? -eq 0 ] ; then testresult="FAIL=`cat $pf | wc -l` $testresult"; fi
rm $pf
if [ "$testresult" = "" ] ; then testresult="?"; fi

# commit the results
pushd $testresultsdir
if [ $commit != 0 ] ; then
    svn add $tracefile $summaryfile
    retry svn commit -m \"$testresult sql-bench $mysqlbuild $mysqlserver\" $tracefile $summaryfile
fi
popd

popd

if [[ $testresult =~ "PASS" ]] ; then exitcode=0; else exitcode=1; fi
exit $exitcode



