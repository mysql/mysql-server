#!/bin/bash

# build.check.bash --revision=10500
# build.check.bash --tokudb=tokudb.1489 --revision=10500
# build.check.bash --branch=mysql.branches/1.1.3 --revision=10500
# build.check.bash --windows=1 --revision=10500

function usage() {
    echo "build tokudb and run regressions"
    echo "--windows=$dowindows (if yes/1 must be first option)"
    echo "--branch=$branch"
    echo "--tokudb=$tokudb"
    echo "--revision=$revision"
    echo "--bdb=$bdb"
    echo "--valgrind=$dovalgrind"
    echo "--commit=$docommit"
    echo "--j=$makejobs"
    echo "--deleteafter=$deleteafter"
    echo "--doclean=$doclean"
}

# delete some characters that cygwin and osx have trouble with
function sanitize() {
    tr -d [/:\\\\]
}

function mydate() {
    date +"%Y%m%d %H:%M:%S"
}

function get_ncpus() {
    grep bogomips /proc/cpuinfo | wc -l
}

function get_latest_svn_revision() {
    local revision=0
    local svntarget=$svnserver/$*
    local latest=`svn info $svntarget | grep "Last Changed Rev:"`
    if [[ $latest =~ "Last Changed Rev: (.*)" ]] ; then
        revision=${BASH_REMATCH[1]}
    fi
    echo $revision
}

function make_tokudb_name() {
    local tokudb_dir=$1
    local tokudb=$2
    if [ $tokudb_dir = "toku" ] ; then
	echo $tokudb
    else
	echo `echo $tokudb_dir-$tokudb | tr / -`
    fi
}

# run a command and trace its result
function runcmd() {
    local fail=$1; shift
    local makedir=$1; shift
    local cmd=$*

    echo `mydate` $makedir $cmd
    pushd $makedir
    $cmd
    exitcode=$?
    local dir=$makedir
    if [[ $dir =~ "$HOME/svn.build/(.*)" ]] ; then
	dir=${BASH_REMATCH[1]}
    fi
    if [ $fail -eq 0 ] ; then
        if [ $exitcode -eq 0 ] ; then
            result="PASS `mydate` $dir $cmd"
        else
            result="FAIL `mydate` $dir $cmd"
            let nfail=nfail+1
        fi
    else
        if [ $exitcode -eq 0 ] ; then
            result="XPASS `mydate` $dir $cmd"
            let nfail=nfail+1
        else
            result="XFAIL `mydate` $dir $cmd"
        fi
    fi
    echo $result
    echo $result >>$commit_msg
    echo $result >>/tmp/tokubuild.trace
    popd
}

# build a version of tokudb with a specific BDB target
function build() {

    # setup build environment
    export BDB=$1
    if [[ $BDB =~ "(.*)\.(.*)" ]] ;then
        export BDBMAJOR=${BASH_REMATCH[1]}
        export BDBMINOR=${BASH_REMATCH[2]}
    else
        return 1
    fi
    if [ $dowindows -eq 0 ] ; then
        export BDBDIR=/usr/local/BerkeleyDB.$BDB
    else
        export BDBDIR=c:/cygwin/usr/local/BerkeleyDB.$BDB
    fi
    if [ ! -d $BDBDIR ] ; then return 2; fi

    tokudb_name=`make_tokudb_name $branch $tokudb`
    export TOKUDB_NAME=$tokudb_name
    export TOKUDB_REVISION=$revision

    productname=$tokudb_name-$revision
    checkout=$branch/$tokudb

    latestrev=`get_latest_svn_revision $checkout`
    if [ $latestrev -eq 0 ] ; then return 3; fi

    commit_msg=`mktemp`

    svnbase=~/svn.build
    if [ ! -d $svnbase ] ; then mkdir $svnbase ; fi

    # checkout the build dir
    buildbase=$svnbase/tokudb.build
    if [ ! -d $buildbase ] ; then
        cd $svnbase
        svn checkout -q $svnserver/tokudb.build
        cd $buildbase
    else
        cd $buildbase
        svn update -q
	svn update -q $date # Could be a sparse directory
    fi

    # make the build directory, possibly on multiple machines simultaneously, there can be only one
    builddir=$buildbase/$date
    while [ ! -d $builddir ] ; do
        mkdir -p $builddir
        svn add $builddir
        svn commit $builddir -m ""
	if [ $? -eq 0 ] ; then break ; fi
	rmdir $builddir
	svn update -q
	svn update -q $date # Could be a sparse directory
    done

    tracefile=$builddir/benchmark+$productname-$BDB+$nodename+$system+$release+$arch

    # get some config info
    uname -a >>"$tracefile" 2>&1
    cc -v >>"$tracefile" 2>&1
    if [ -f /proc/version ] ; then
	cat /proc/version >>"$tracefile"
    fi

    # checkout the source dir
    productbuilddir=$svnbase/$productname/$BDB
    # cleanup
    rm -rf $productbuilddir
    mkdir -p $productbuilddir

    let nfail=0

    # checkout into $productbuilddir
    runcmd 0 $productbuilddir svn checkout -q -r $revision $svnserver/$checkout . >>"$tracefile" 2>&1

    # update the db.h file
    runcmd 0 $productbuilddir/buildheader cp db.h_"$BDBMAJOR"_"$BDBMINOR" ../include/db.h >>"$tracefile" 2>&1

    # build
    runcmd 0 $productbuilddir/src make -k install >>"$tracefile" 2>&1
    runcmd 0 $productbuilddir/db-benchmark-test make build.tdb -k >>"$tracefile" 2>&1

    for i in no-txn txn abort child child_abort child_abortfirst txn1 abort1 child1 child-abort1 child_abortfirst1; do
        # Run benchmarks
        runcmd 0 $productbuilddir/db-benchmark-test make $i.benchmark.dir -k >>"$tracefile" 2>&1
        runcmd 0 $productbuilddir/db-benchmark-test make $i.manybenchmark -k >>"$tracefile" 2>&1

        # Flattening scan
        runcmd 0 $productbuilddir/db-benchmark-test make $i.flatteningscan -k >>"$tracefile" 2>&1
        runcmd 0 $productbuilddir/db-benchmark-test make $i.manyflatteningscan -k >>"$tracefile" 2>&1

        runcmd 0 $productbuilddir/db-benchmark-test make $i.flattenedscan -k >>"$tracefile" 2>&1
        runcmd 0 $productbuilddir/db-benchmark-test make $i.manyflattenedscan -k >>"$tracefile" 2>&1
    done

    # clean
    if [ $doclean -eq 1 ] ; then
	# remove the huge 4g test file
	runcmd 0 $productbuilddir/db-benchmark-test make -k clean >>"$tracefile" 2>&1    
    fi

    # put the trace into svn
    if [ $docommit -ne 0 ] ; then
	testresult="PASS"
	if [ $nfail -ne 0 ] ; then testresult="FAIL"; fi

	local cf=`mktemp`
	echo "$testresult tokudb benchmark-test $productname-$BDB $system $release $arch $nodename" >$cf
	echo >>$cf; echo >>$cf
	cat $commit_msg >>$cf
	
	svn add "$tracefile" "$coveragefile"
	svn commit -F "$cf" "$tracefile" "$coveragefile"
	rm $cf
    else
	cat "$commit_msg"
    fi
    rm "$commit_msg"	
    if [ $deleteafter -eq 1 ] ; then
        rm -rf $productbuilddir
    fi
}

# set defaults
exitcode=0
svnserver=https://svn.tokutek.com/tokudb
nodename=`uname -n`
system=`uname -s | sanitize`
release=`uname -r | sanitize`
arch=`uname -m | sanitize`
date=`date +%Y%m%d`
branch="toku"
tokudb="tokudb"
bdb="4.6"
makejobs=`get_ncpus`
revision=0
dovalgrind=1
docommit=1
docoverage=0
dowindows=0
oschoice=linux
deleteafter=0
doclean=1
j=-1
export BINSUF=

arg=$1;
shopt -s compat31 #Necessary in some flavors of linux and windows
if [ "$arg" = "--windows=yes" -o "$arg" = "--windows=1" ] ; then
    shift
    dowindows=1
    export CC=icc
    export CXX=icc
    export CYGWIN=CYGWIN
    oschoice=windows
    export BINSUF=.exe
fi

# import the environment
while [ $# -gt 0 ] ; do
    arg=$1; shift
    if [ "$arg" = "--valgrind=no" -o "$arg" = "--valgrind=0" ] ; then
        dovalgrind=0
    elif [ "$arg" = "--commit=no" -o "$arg" = "--commit=0" ] ; then
	docommit=0
    elif [ "$arg" = "--windows=no" -o "$arg" = "--windows=0" ] ; then
        dowindows=0
    elif [ "$arg" = "--windows=yes" -o "$arg" = "--windows=1" ] ; then
        usage; exit 1
    elif [[ "$arg" =~ "^--(.*)=(.*)" ]] ; then
	eval ${BASH_REMATCH[1]}=${BASH_REMATCH[2]}
    else
	usage; exit 1
    fi
done

if [ $j -ne "-1" ] ; then
    makejobs=$j
fi
if [ $makejobs -eq 0 ] ; then
    usage
    exit 1
fi

if [ $revision -eq 0 ] ; then
    revision=`get_latest_svn_revision`
fi

build $bdb

exit $exitcode
