#!/usr/bin/env bash

function usage() {
    echo "run db-benchmark-test"
    echo "[--tokudb=$tokudb"
    echo "[--revision=$revision]"
    echo "[--branch=$branch]"
    echo "[--suffix=$suffix]"
    echo "[--commit=$commit]"
    echo "[--cc=$cc]"
    echo "[--n=$n]"
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

n=100
cc=gcc44
brtloader=cilk
branch=toku
revision=0
tokudb=tokudb
suffix=.
commit=0
svnserver=https://svn.tokutek.com/tokudb
basedir=$HOME/svn.build
builddir=$basedir/tokudb.build
system=`uname -s | tr [:upper:] [:lower:]`
arch=`uname -m | tr [:upper:] [:lower:]`
hostname=`hostname`
instancetype=""

# parse the command line
while [ $# -gt 0 ] ; do
    arg=$1; shift
    if [[ $arg =~ --(.*)=(.*) ]] ; then
        eval ${BASH_REMATCH[1]}=${BASH_REMATCH[2]}
    else
        usage; exit 1
    fi
done

# require a revision
if [ $revision -eq 0 ] ; then exit 1; fi

# setup the branchrevision string
if [ $branch = "toku" ] ; then
    branchrevision=$revision
else
    branchrevision=`basename $branch`-$revision
fi
if [ $suffix != "." ] ; then branchrevision=$branchrevision-$suffix; fi

# goto the base directory
if [ ! -d $basedir ] ; then mkdir $basedir; fi

pushd $basedir

# update the build directory
if [ ! -d $builddir ] ; then
    retry svn checkout -q $svnserver/tokudb.build
    exitcode=$?
    if [ $exitcode -ne 0 ] ; then exit 1; fi
else
    pushd $builddir
        retry svn update -q
        exitcode=$?
        if [ $exitcode -ne 0 ] ; then exit 1; fi
    popd
fi

date=`date +%Y%m%d`
testresultsdir=$builddir/$date
pushd $builddir
    while [ ! -d $date ] ; do
        mkdir $date
        svn add $date
        svn commit $date -m ""
        if [ $? -eq 0 ] ; then break; fi
        rm -rf $date
        svn remove $date
        svn update -q
    done
popd

gccversion=`$cc --version|head -1|cut -f3 -d" "`

runfile=$testresultsdir/db-benchmark-test-$branchrevision-$cc-$gccversion-$system-$arch-$hostname
if [ "$instancetype" != "" ] ; then runfile=$runfile-$instancetype; fi
rm -rf $runfile

testresult="PASS"
testdir=db-benchmark-test-$branchrevision
rm -rf $testdir

# checkout the tokudb branch
if [ $testresult = "PASS" ] ; then
    retry svn checkout -q https://svn.tokutek.com/tokudb/$branch/$tokudb
    exitcode=$?
    if [ $exitcode != 0 ] ; then testresult="FAIL"; fi
    mv $tokudb $testdir
fi

# build it
if [ $testresult = "PASS" ] ; then
    pushd $testdir/src
        make local -s CC=$cc GCCVERSION=$gccversion BRTLOADER=$brtloader >>$runfile 2>&1
	exitcode=$?
	if [ $exitcode != 0 ] ; then testresult="FAIL"; fi
    popd
    pushd $testdir/db-benchmark-test
        make build.tdb CC=$cc GCCVERSION=$gccversion -s >>$runfile 2>&1
	exitcode=$?
	if [ $exitcode != 0 ] ; then testresult="FAIL"; fi
    popd
fi

# run tests
if [ $testresult = "PASS" ] ; then
    let i=$n
    pushd $testdir/db-benchmark-test
        echo ./db-benchmark-test-tokudb -x $i >>$runfile 2>&1
        ./db-benchmark-test-tokudb -x $i >>$runfile 2>&1
	exitcode=$?
	if [ $exitcode != 0 ] ; then testresult="FAIL"; fi
	echo ./scanscan-tokudb --prelock --prelockflag >>$runfile 2>&1
	./scanscan-tokudb --prelock --prelockflag >>$runfile 2>&1
	exitcode=$?
	if [ $exitcode != 0 ] ; then testresult="FAIL"; fi
	echo ./scanscan-tokudb --prelock --prelockflag >>$runfile 2>&1
	./scanscan-tokudb --prelock --prelockflag >>$runfile 2>&1
	exitcode=$?
	if [ $exitcode != 0 ] ; then testresult="FAIL"; fi
    popd
fi

if [ $testresult = "PASS" ] ; then
    let i=2*$n
    pushd $testdir/db-benchmark-test
        echo ./db-benchmark-test-tokudb -x --norandom $i >>$runfile 2>&1
        ./db-benchmark-test-tokudb -x --norandom $i >>$runfile 2>&1
	exitcode=$?
	if [ $exitcode != 0 ] ; then testresult="FAIL"; fi
	echo ./scanscan-tokudb --prelock --prelockflag >>$runfile 2>&1
	./scanscan-tokudb --prelock --prelockflag >>$runfile 2>&1
	exitcode=$?
	if [ $exitcode != 0 ] ; then testresult="FAIL"; fi
	echo ./scanscan-tokudb --prelock --prelockflag >>$runfile 2>&1
	./scanscan-tokudb --prelock --prelockflag >>$runfile 2>&1
	exitcode=$?
	if [ $exitcode != 0 ] ; then testresult="FAIL"; fi
    popd
fi

if [ $testresult = "PASS" ] ; then
    let i=2*$n
    pushd $testdir/db-benchmark-test
        echo ./db-benchmark-test-tokudb -x --noserial $i >>$runfile 2>&1
        ./db-benchmark-test-tokudb -x --noserial $i >>$runfile 2>&1
	exitcode=$?
	if [ $exitcode != 0 ] ; then testresult="FAIL"; fi
	echo ./scanscan-tokudb --prelock --prelockflag >>$runfile 2>&1
	./scanscan-tokudb --prelock --prelockflag >>$runfile 2>&1
	exitcode=$?
	if [ $exitcode != 0 ] ; then testresult="FAIL"; fi
	echo ./scanscan-tokudb --prelock --prelockflag >>$runfile 2>&1
	./scanscan-tokudb --prelock --prelockflag >>$runfile 2>&1
	exitcode=$?
	if [ $exitcode != 0 ] ; then testresult="FAIL"; fi
    popd
fi

# commit results
if [ $commit != 0 ] ; then
    svn add $runfile
    retry svn commit -m \"$testresult db-benchmark-test $branchrevision $system $arch\" $runfile
fi

popd

exit 0
