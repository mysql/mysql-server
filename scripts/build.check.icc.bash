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
    echo "--valgrind=$dovalgrind --VALGRIND=$VALGRIND"
    echo "--commit=$docommit"
    echo "--j=$makejobs"
    echo "--deleteafter=$deleteafter"
    echo "--doclean=$doclean"
    echo "--cc=$cc --cxx=$cxx"
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

# delete some characters that cygwin and osx have trouble with
function sanitize() {
    tr -d '[/:\\\\()]'
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
    local latest=`retry svn info $svntarget`
    if [[ $latest =~ "Last Changed Rev: ([0-9]+)" ]] ; then
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
	    let npass=npass+1
        else
            result="FAIL `mydate` $dir $cmd"
            let nfail=nfail+1
        fi
    else
        if [ $exitcode -eq 0 ] ; then
            result="XPASS `mydate` $dir $cmd"
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
	mkdir $buildbase
    fi

    # make the build directory, possibly on multiple machines simultaneously, there can be only one
    builddir=$buildbase/$date
    pushd $buildbase
    while [ ! -d $date ] ; do
	svn mkdir $svnserver/tokudb.build/$date -m ""
	svn co -q $svnserver/tokudb.build/$date
	if [ $? -ne 0 ] ; then rm -rf $date; fi
    done
    popd

    tracefile=$builddir/build+$productname-$CC-$BDB+$nodename+$system+$release+$arch

    # get some config info
    uname -a >>$tracefile 2>&1
    ulimit -a >>$tracefile 2>&1
    $CC -v >>$tracefile 2>&1
    $CXX -v >>$tracefile 2>&1
    cat /proc/version >>$tracefile 2>&1
    cat /proc/cpuinfo >>$tracefile 2>&1
    env >>$tracefile 2>&1

    # checkout the source dir
    productbuilddir=$svnbase/$productname
    # cleanup
    rm -rf $productbuilddir
    mkdir -p $productbuilddir

    let nfail=0
    let npass=0

    # checkout into $productbuilddir
    runcmd 0 $productbuilddir retry svn checkout -q -r $revision $svnserver/$checkout . >>$tracefile 2>&1

    # portability
    runcmd 0 $productbuilddir/$oschoice       make local DEBUG=1 CC=icc HAVE_CILK=0 >>$tracefile 2>&1
    runcmd 0 $productbuilddir/$oschoice/tests make check -k -s SUMMARIZE=1 DEBUG=1 CC=icc HAVE_CILK=0 >>$tracefile 2>&1

    # newbrt
    runcmd 0 $productbuilddir/newbrt make local -j$makejobs -k -s SUMMARIZE=1 DEBUG=1 CC=icc HAVE_CILK=0 >>$tracefile 2>&1
    runcmd 0 $productbuilddir/newbrt make check -j$makejobs -k -s SUMMARIZE=1 DEBUG=1 CC=icc HAVE_CILK=0 >>$tracefile 2>&1

    # src
    runcmd 0 $productbuilddir/src make local -k -s -j$makejobs DEBUG=1 CC=icc HAVE_CILK=0  >>$tracefile 2>&1

    # utils
    runcmd 0 $productbuilddir/utils make -k -s -j$makejobs DEBUG=1 CC=icc HAVE_CILK=0 >>$tracefile 2>&1
    runcmd 0 $productbuilddir/utils make check -k -j$makejobs -s SUMMARIZE=1 DEBUG=1 CC=icc HAVE_CILK=0 >>$tracefile 2>&1

    # lock tree
    runcmd 0 $productbuilddir/src/range_tree/tests make check -k -j$makejobs -s SUMMARIZE=1 DEBUG=1 CC=icc HAVE_CILK=0 >>$tracefile 2>&1
    runcmd 0 $productbuilddir/src/lock_tree/tests  make check -k -j$makejobs -s SUMMARIZE=1 DEBUG=1 CC=icc HAVE_CILK=0 >>$tracefile 2>&1

    # src/tests
    runcmd 0 $productbuilddir/src/tests make tests.bdb -j$makejobs -k -s SUMMARIZE=1 DEBUG=1 CC=icc HAVE_CILK=0 >>$tracefile 2>&1
    runcmd 0 $productbuilddir/src/tests make tests.tdb -j$makejobs -k -s SUMMARIZE=1 DEBUG=1 CC=icc HAVE_CILK=0 >>$tracefile 2>&1
    runcmd 0 $productbuilddir/src/tests make check.tdb -j$makejobs -k -s SUMMARIZE=1 DEBUG=1 CC=icc HAVE_CILK=0 >>$tracefile 2>&1
    runcmd 0 $productbuilddir/src/tests make stress_tests.drdrun -j$makejobs -k -s SUMMARIZE=1 DEBUG=1 CC=icc HAVE_CILK=0 >>$tracefile 2>&1

    # upgrade tests
    runcmd 0 $productbuilddir/src/tests make upgrade-tests.tdbrun -k -s SUMMARIZE=1 DEBUG=1 CC=icc HAVE_CILK=0 >>$tracefile 2>&1

    # benchmark tests
    runcmd 0 $productbuilddir/db-benchmark-test make -k -j$makejobs DEBUG=1 CC=icc HAVE_CILK=0 >>$tracefile 2>&1
    runcmd 0 $productbuilddir/db-benchmark-test make check -k -j$makejobs -k -s SUMMARIZE=1 DEBUG=1 CC=icc HAVE_CILK=0 >>$tracefile 2>&1

    # run the brtloader tests with a debug build
    runcmd 0 $productbuilddir/newbrt/tests make check_brtloader -k -j$makejobs -s SUMMARIZE=1 DEBUG=1 CC=icc HAVE_CILK=0 >>$tracefile 2>&1
    runcmd 0 $productbuilddir/src/tests    make loader-tests    -k -j$makejobs -s SUMMARIZE=1 DEBUG=1 CC=icc HAVE_CILK=0 >>$tracefile 2>&1

    # release build 
    runcmd 0 $productbuilddir make clean >>$tracefile 2>&1
    runcmd 0 $productbuilddir make release CC=icc >>$tracefile 2>&1
    runcmd 0 $productbuilddir/utils make CC=icc >>$tracefile 2>&1

    # release tests
    runcmd 0 $productbuilddir/newbrt/tests make check -j$makejobs -k -s SUMMARIZE=1 CC=icc VGRIND= >>$tracefile 2>&1
    runcmd 0 $productbuilddir/newbrt/tests make check_brtloader -k -j$makejobs -s SUMMARIZE=1 CC=icc VGRIND= >>$tracefile 2>&1
    runcmd 0 $productbuilddir/src/tests make tests.tdb -j$makejobs -k -s SUMMARIZE=1 CC=icc >>$tracefile 2>&1
    runcmd 0 $productbuilddir/src/tests make check.tdb -j$makejobs -k -s SUMMARIZE=1 CC=icc VGRIND= >>$tracefile 2>&1

    # cilk tests
    runcmd 0 $productbuilddir make clean >>$tracefile 2>&1
    runcmd 0 $productbuilddir make release CC=icc DEBUG=1 >>$tracefile 2>&1
    runcmd 0 $productbuilddir/newbrt/tests make cilkscreen_brtloader -k -s SUMMARIZE=1 CC=icc DEBUG=1 >>$tracefile 2>&1

    # cxx
    if [ 0 = 1 ] ; then
    runcmd $dowindows $productbuilddir/cxx make -k -s >>$tracefile 2>&1
    runcmd $dowindows $productbuilddir/cxx make -k -s install >>$tracefile 2>&1
    runcmd $dowindows $productbuilddir/cxx/tests make -k -s check SUMMARIZE=1 >>$tracefile 2>&1
    runcmd $dowindows $productbuilddir/db-benchmark-test-cxx make -k -s >>$tracefile 2>&1
    runcmd $dowindows $productbuilddir/db-benchmark-test-cxx make -k -s check >>$tracefile 2>&1
    fi
    
    # Makefile for release/examples is NOT ported to windows.  Expect it to fail.
    if [ 0 = 1 ] ; then 
    runcmd $dowindows $productbuilddir/release make -k setup >>$tracefile 2>&1
    runcmd $dowindows $productbuilddir/release/examples make -k check >>$tracefile 2>&1
    fi

    # man
    if [ 0 = 1 ] ; then 
    runcmd 0 $productbuilddir/man/texi make -k -s -j$makejobs >>$tracefile 2>&1
    fi

    if [ 0 = 1 -a $docoverage -eq 1 ] ; then
        # run coverage
        runcmd 0 $productbuilddir make -s clean >>$tracefile 2>&1
        runcmd 0 $productbuilddir make build-coverage >>$tracefile 2>&1
        runcmd 0 $productbuilddir make check-coverage >>$tracefile 2>&1

        # summarize the coverage data
        coveragefile=$builddir/coverage+$productname-$BDB+$nodename+$system+$release+$arch
        rawcoverage=`mktemp`
        for d in newbrt src src/range_tree src/lock_tree; do
            (cd $productbuilddir/$d; python ~/bin/gcovsumdir.py -b *.gcno >>$rawcoverage)
            if [ -d $productbuilddir/$d/tests ] ; then
                (cd $productbuilddir/$d/tests; python ~/bin/gcovsumdir.py -b *.gcno >>$rawcoverage)
            fi
        done
        python ~/bin/gcovsumsum.py <$rawcoverage >$coveragefile
        rm $rawcoverage
    fi

    # put the trace into svn
    if [ $docommit -ne 0 ] ; then
	testresult="PASS=$npass"
	if [ $nfail != 0 ] ; then
	    testresult="FAIL=$nfail $testresult"
	fi

	local cf=`mktemp`
	echo "$testresult tokudb-build $productname $CC $GCCVERSION $system $release $arch $nodename" >$cf
	echo >>$cf; echo >>$cf
	cat $commit_msg >>$cf
	if [ $nfail != 0 ] ; then egrep " FAIL" $tracefile >>$cf; fi
	
	svn add $tracefile $coveragefile
	retry svn commit -F "$cf" $tracefile $coveragefile
	rm $cf
    else
	cat $commit_msg
    fi
    rm $commit_msg	

    if [ $deleteafter -eq 1 ] ; then
        rm -rfv $productbuilddir > /dev/null #windows rm sometimes hangs on giant dirs without -v
    fi

    return 0
}

# set defaults
exitcode=0
svnserver=https://svn.tokutek.com/tokudb
nodename=`uname -n`
system=`uname -s | sanitize`
release=`uname -r | sanitize`
arch=`uname -m | sanitize`
date=`date +%Y%m%d`
branch="."
tokudb="tokudb"
bdb="4.6"
makejobs=`get_ncpus`
revision=0
dovalgrind=1
VALGRIND=valgrind
docommit=1
docoverage=0
dowindows=0
oschoice=linux
deleteafter=0
j=-1
cc=icc
cxx=icpc

arg=$1;
shopt -s compat31 #Necessary in some flavors of linux and windows
if [ "$arg" = "--windows=yes" -o "$arg" = "--windows=1" ] ; then
    shift
    dowindows=1
    export CC=icc
    export CXX=icc
    export CYGWIN=CYGWIN
    oschoice=windows
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
    elif [[ $arg =~ ^--(.*)=(.*) ]] ; then
	eval ${BASH_REMATCH[1]}=${BASH_REMATCH[2]}
    else
	usage; exit 1
    fi
done

# setup default compiler
if [ "$CC" = "" ] ; then export CC=$cc; fi
if [ "$CXX" = "" ] ; then export CXX=$cxx; fi

if [ $CC = icc ] ; then
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

if [ $j -ne "-1" ] ; then makejobs=$j; fi
if [ $makejobs -eq 0 ] ; then usage; exit 1; fi

if [ $branch = "." ] ; then branch="toku"; fi
if [ $revision -eq 0 ] ; then revision=`get_latest_svn_revision`; fi

# setup GCCVERSION
export GCCVERSION=`$CC --version|head -1|cut -f3 -d" "`
export VALGRIND=$VALGRIND

# setup VGRIND
if [ $dovalgrind = 0 ] ; then export VGRIND=""; fi

# limit execution time to 2 hours
ulimit -t 7200

build $bdb
