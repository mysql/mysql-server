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
ft_loader=cilk
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

if [ $cc = icc ] ; then
    d=/opt/intel/bin
    if [ -d $d ] ; then
	export PATH=$d:$PATH
	. $d/compilervars.sh intel64
    fi
    d=/opt/intel/cilkutil/bin
    if [ -d $d ] ; then
	export PATH=$d:$PATH
    fi
fi

# require a revision
if [ $revision -eq 0 ] ; then exit 1; fi
if [ $branch = "." ] ; then branch="toku"; fi

function append() {
    local s=""; local x
    for x in $*; do
	if [ "$s" != "" ] ; then s=$s-$x; else s=$x; fi
    done
    echo $s
}

# setup the branchrevision string
branchrevision=""
if [ $branch != "toku" ] ; then branchrevision=$(append $branchrevision $(basename $branch)); fi
if [ $tokudb != "tokudb" ] ; then branchrevision=$(append $branchrevision $tokudb); fi
branchrevision=$(append $branchrevision $revision)
if [ $suffix != "." ] ; then branchrevision=$(append $branchrevision $suffix); fi

# goto the base directory
if [ ! -d $basedir ] ; then mkdir $basedir; fi

pushd $basedir

# update the build directory
if [ ! -d $builddir ] ; then mkdir $builddir; fi

date=`date +%Y%m%d`
pushd $builddir
    while [ ! -d $date ] ; do
        svn mkdir $svnserver/mysql.build/$date -m ""
        svn co -q $svnserver/mysql.build/$date
        if [ $? -ne 0 ] ; then rm -rf $date; fi
    done
popd
testresultsdir=$builddir/$date

gccversion=`$cc --version|head -1|cut -f3 -d" "`

runfile=$testresultsdir/db-benchmark-test-$branchrevision-$cc-$gccversion-$system-$arch-$hostname
if [ "$instancetype" != "" ] ; then runfile=$runfile-$instancetype; fi
rm -rf $runfile

testresult="PASS"
testdir=db-benchmark-test-$branchrevision
rm -rf $testdir

# checkout the tokudb branch
if [ $testresult = "PASS" ] ; then
    retry svn export -q https://svn.tokutek.com/tokudb/$branch/$tokudb $testdir
    exitcode=$?
    if [ $exitcode != 0 ] ; then testresult="FAIL"; fi
fi

# build it
if [ $testresult = "PASS" ] ; then
    pushd $testdir
        make release -s CC=$cc GCCVERSION=$gccversion FTLOADER=$ft_loader >>$runfile 2>&1
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
	echo ./scanscan-tokudb --lwc --prelock --prelockflag >>$runfile 2>&1
	./scanscan-tokudb --lwc --prelock --prelockflag >>$runfile 2>&1
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
	echo ./scanscan-tokudb --lwc --prelock --prelockflag >>$runfile 2>&1
	./scanscan-tokudb --lwc --prelock --prelockflag >>$runfile 2>&1
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
	echo ./scanscan-tokudb --lwc --prelock --prelockflag >>$runfile 2>&1
	./scanscan-tokudb --lwc --prelock --prelockflag >>$runfile 2>&1
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
