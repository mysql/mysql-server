#!/usr/bin/env bash
# ident 4, no tabs

function usage() {
    echo "run the tokudb mysql tests"
    echo "--mysqlbuild=$mysqlbuild"
    echo "--commit=$commit"
    echo "--tests=$tests --engine=$engine"
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
mysql_basedir=/usr/local/mysql
mysqlserver=`hostname`
commit=0
tests="*"
engine=""
parallel=auto

while [ $# -gt 0 ] ; do
    arg=$1; shift
    if [[ $arg =~ --(.*)=(.*) ]] ; then
        eval ${BASH_REMATCH[1]}=${BASH_REMATCH[2]}
    else
        usage; exit 1
    fi
done

if [[ $mysqlbuild =~ (.*)-(tokudb\-.*)-(linux)-(x86_64) ]] ; then
    mysql=${BASH_REMATCH[1]}
    tokudb=${BASH_REMATCH[2]}
    system=${BASH_REMATCH[3]}
    arch=${BASH_REMATCH[4]}
else
    echo $mysqlbuild is not a tokudb build
fi

if [ -d $mysql_basedir/lib/mysql ] ; then
    export LD_LIBRARY_PATH=$mysql_basedir/lib/mysql
fi

# update the build directory
if [ ! -d $basedir ] ; then mkdir $basedir ; fi

pushd $basedir
if [ $? != 0 ] ; then exit 1; fi

if [ ! -d $builddir ] ; then mkdir $builddir; fi

# make the subversion directory that will hold the test results
date=`date +%Y%m%d`
testresultsdir=$builddir/$date
pushd $builddir
if [ $? = 0 ] ; then
    while [ ! -d $date ] ; do
        svn mkdir $svnserver/mysql.build/$date -m ""
        svn checkout -q $svnserver/mysql.build/$date
        if [ $? -ne 0 ] ; then rm -rf $date; fi
    done
    popd
fi

# generate a trace file name
if [ -z $engine ] ; then
    tracefile=mysql-test-$mysqlbuild-$mysqlserver
else
    tracefile=mysql-engine-$engine-$mysqlbuild-$mysqlserver
fi
echo >$testresultsdir/$tracefile

if [ -z $engine ] ; then

    # run all test suites including main
    teststorun_original="main"
    teststorun_tokudb=""
    pushd $mysql_basedir/mysql-test/suite
    if [ $? = 0 ] ; then
        for t in $tests ;  do
            if [[ $t =~ .*\.xfail$ ]] ; then continue; fi
            if [ $t = "perfschema_stress" ] ; then continue; fi
            if [ $t = "large_tests" ] ; then continue; fi
            if [ $t = "pbxt" ] ; then continue; fi
            if [ -d $t/t ] ; then 
                if [[ $t =~ tokudb* ]] ; then
                    if [ -z $teststorun_tokudb ] ; then teststorun_tokudb="$t" ; else teststorun_tokudb="$teststorun_tokudb,$t"; fi
                else
                    teststorun_original="$teststorun_original,$t";
                fi
            fi
        done
        popd
    fi
  
    # run the tests
    pushd $mysql_basedir/mysql-test
    if [ $? = 0 ] ; then
        if [[ $mysqlbuild =~ tokudb ]] ; then
            # run standard tests
            if [[ $mysqlbuild =~ 5\\.5 ]] ; then
                ./mysql-test-run.pl --suite=$teststorun_original --big-test --max-test-fail=0 --force --retry=1 --testcase-timeout=60 \
                    --mysqld=--default-storage-engine=myisam --mysqld=--sql-mode="" \
                    --mysqld=--loose-tokudb_debug=3072 \
                    --parallel=$parallel >>$testresultsdir/$tracefile 2>&1
            else
                ./mysql-test-run.pl --suite=$teststorun_original --big-test --max-test-fail=0 --force --retry=1 --testcase-timeout=60 \
                    --mysqld=--loose-tokudb_debug=3072 \
                    --parallel=$parallel >>$testresultsdir/$tracefile 2>&1
            fi

            # run tokudb tests
            ./mysql-test-run.pl --suite=$teststorun_tokudb --big-test --max-test-fail=0 --force --retry=1 --testcase-timeout=60 \
                --mysqld=--loose-tokudb_debug=3072 \
                --parallel=$parallel >>$testresultsdir/$tracefile 2>&1  
            # setup for engines tests
            engine="tokudb"
        else
            ./mysql-test-run.pl --suite=$teststorun_original --big-test --max-test-fail=0 --force --retry=1 --testcase-timeout=60 \
                --parallel=$parallel >>$testresultsdir/$tracefile 2>&1
        fi
        popd
    fi
fi

if [ ! -z $engine ] ; then
    teststorun="engines/funcs,engines/iuds"
    pushd $mysql_basedir/mysql-test
    if [ $? = 0 ] ; then
        if [[ $mysqlbuild =~ 5\\.6 ]] ; then
            ./mysql-test-run.pl --suite=$teststorun --force --retry-failure=0 --max-test-fail=0 --nowarnings --testcase-timeout=60 \
                --mysqld=--default-storage-engine=$engine --mysqld=--default-tmp-storage-engine=$engine \
                --parallel=$parallel >>$testresultsdir/$tracefile 2>&1
        else
            ./mysql-test-run.pl --suite=$teststorun --force --retry-failure=0 --max-test-fail=0 --nowarnings --testcase-timeout=60 \
                --mysqld=--default-storage-engine=$engine \
                --parallel=$parallel >>$testresultsdir/$tracefile 2>&1
        fi
        popd
    fi
fi

# summarize the results
let tests_failed=0
let tests_passed=0
while read line ; do
    if [[ "$line" =~ (Completed|Timeout):\ Failed\ ([0-9]+)\/([0-9]+) ]] ; then
            # failed[2]/total[3]
        let tests_failed=tests_failed+${BASH_REMATCH[2]}
        let tests_passed=tests_passed+${BASH_REMATCH[3]}-${BASH_REMATCH[2]}
    elif [[ "$line" =~ Completed:\ All\ ([0-9]+)\ tests ]] ; then
            # passed[1]
        let tests_passed=tests_passed+${BASH_REMATCH[1]}
    fi
done <$testresultsdir/$tracefile

# commit the results
if [ $tests_failed = 0 ] ; then
    testresult="PASS=$tests_passed"
else
    testresult="FAIL=$tests_failed PASS=$tests_passed"
fi
pushd $testresultsdir
if [ $? = 0 ] ; then
    if [ $commit != 0 ] ; then
        svn add $tracefile 
        if [[ $tracefile =~ "mysql-test" ]] ; then test=mysql-test; else test=mysql-engine-$engine; fi
        retry svn commit -m \"$testresult $test $mysqlbuild $mysqlserver\" $tracefile 
    fi
    popd
fi

popd # $basedir

if [[ $testresult =~ "PASS" ]] ; then exitcode=0; else exitcode=1; fi
exit $exitcode


