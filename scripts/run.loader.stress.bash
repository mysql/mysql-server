#!/usr/bin/env bash

function usage() {
    echo "run the loader verify test"
    echo "[--rows=$rows]"
    echo "[--dictionaries=$dictionaries]"
    echo "[--ft_loader=$ft_loader]"
    echo "[--tokudb=$tokudb]"
    echo "[--branch=$branch]"
    echo "[--revision=$revision]"
    echo "[--suffix=$suffix]"
    echo "[--commit=$commit]"
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

rows=100000000
dictionaries=3
ft_loader=cilk
tokudb=tokudb
branch=.
revision=0
suffix=.
commit=0
svnserver=https://svn.tokutek.com/tokudb
basedir=~/svn.build
builddir=$basedir/mysql.build
system=`uname -s | tr [:upper:] [:lower:]`
arch=`uname -m | tr [:upper:] [:lower:]`
myhost=`hostname`
instancetype=""
ftcc=gcc
have_cilk=0

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
if [ $revision -eq 0 ] ; then 
    exit 1
fi

# build 
if [ $ftcc = icc ] ; then
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

# setup the branchrevision string
if [ $branch = "." ] ; then
    branchrevision=$revision
else
    branchrevision=`basename $branch`-$revision
fi
if [ $suffix != "." ] ; then
    branchrevision=$branchrevision-$suffix
fi

ftccversion=$($ftcc --version|head -1|cut -f3 -d" ")

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
	svn checkout $svnserver/mysql.build/$date
        if [ $? -ne 0 ] ; then rm -rf $date; fi
    done
popd

testresult="PASS"
runfile=$testresultsdir/loader-stress-$rows-$dictionaries-$tokudb-$branchrevision-$ftcc-$ftccversion-$system-$arch-$myhost
if [ "$instancetype" != "" ] ; then runfilefile=$runfile-$instancetype; fi
rm -f $runfile

# checkout the code
if [ -d loader-stress-$branchrevision ] ; then rm -rf loader-stress-$branchrevision; fi
mkdir loader-stress-$branchrevision

if [ $branch = "." ] ; then branch=toku; fi

retry svn export -r $revision -q $svnserver/$branch/$tokudb loader-stress-$branchrevision/$tokudb
exitcode=$?
if [ $exitcode != 0 ] ; then 
    testresult="FAIL"
fi

if [ $testresult = "PASS" ] ; then
    pushd loader-stress-$branchrevision/$tokudb
        echo `date` make release -s CC=$ftcc HAVE_CILK=$have_cilk FTLOADER=$ft_loader >>$runfile
        make -s release CC=$ftcc HAVE_CILK=$have_cilk FTLOADER=$ft_loader >>$runfile 2>&1
	exitcode=$?
	echo `date` complete $exitcode >>$runfile
	if [ $exitcode != 0 ] ; then testresult="FAIL"; fi
    popd
fi
if [ $testresult = "PASS" ] ; then
    pushd loader-stress-$branchrevision/$tokudb/src/tests
        echo `date` make loader-stress-test.tdb CC=$ftcc HAVE_CILK=$have_cilk >>$runfile
        make loader-stress-test.tdb -s CC=$ftcc HAVE_CILK=$have_cilk >>$runfile 2>&1
	exitcode=$?
	echo `date` complete $exitcode >>$runfile
	if [ $exitcode != 0 ] ; then testresult="FAIL"; fi
    popd
fi

# run
if [ $testresult = "PASS" ] ; then
    pushd loader-stress-$branchrevision/$tokudb/src/tests
        echo `date` ./loader-stress-test.tdb -v -r $rows -d $dictionaries -c >>$runfile
	./loader-stress-test.tdb -v -r $rows -d $dictionaries -c >>$runfile 2>&1
	exitcode=$?
	echo `date` complete $exitcode >>$runfile
	if [ $exitcode != 0 ] ; then testresult="FAIL"; fi
    popd
fi

if [ $commit != 0 ] ; then
    svn add $runfile
    retry svn commit -m \"$testresult loader stress $rows $dictionaries $tokudb $branchrevision $ftcc $ftccversion $system $arch $myhost\" $runfile
fi

popd

if [ $testresult = "PASS" ] ; then exitcode=0; else exitcode=1; fi
exit $exitcode
