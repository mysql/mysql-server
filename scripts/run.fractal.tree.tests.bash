#!/bin/bash

# build.check.bash --revision=10500 --ftcc=icc
# build.check.bash --tokudb=tokudb.1489 --revision=10500 --ftcc=gcc44
# build.check.bash --branch=mysql.branches/1.1.3 --revision=10500

function usage() {
    echo "build tokudb and run regressions"
    echo "--branch=$branch --tokudb=$tokudb --revision=$revision --bdb=$bdb"
    echo "--ftcc=$ftcc --VALGRIND=$VALGRIND --makejobs=$makejobs --parallel=$parallel"
    echo "--debugtests=$debugtests --releasetests=$releasetests"
    echo "--commit=$commit"
}

function retry() {
    local cmd
    local retries
    local exitcode
    cmd=$*
    let retries=0
    while [ $retries -le 10 ] ; do
	echo $(date) $cmd
	bash -c "$cmd"
	exitcode=$?
	echo $(date) $cmd $exitcode $retries
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
    local latest=$(retry svn info $svntarget)
    if [[ $latest =~ Last\ Changed\ Rev:\ ([0-9]+) ]] ; then
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
	echo $(echo $tokudb_dir-$tokudb | tr / -)
    fi
}

# run a command and trace its result
function runcmd() {
    local fail=$1; shift
    local makedir=$1; shift
    local cmd=$*

    echo $(mydate) $makedir $cmd
    pushd $makedir
    if [ $? = 0 ] ; then
	$cmd
	exitcode=$?
	local dir=$makedir
	if [[ $dir =~ $HOME/svn.build/(.*) ]] ; then
	    dir=${BASH_REMATCH[1]}
	fi
	result="$(mydate) $dir $cmd"
	if [ $fail -eq 0 ] ; then
	    if [ $exitcode -eq 0 ] ; then
		result="PASS $result"
	    else
		result="FAIL $result"
	    fi
	else
	    if [ $exitcode -eq 0 ] ; then
		result="XPASS $result"
	    else
		result="XFAIL $result"
	    fi
	fi
    fi
    echo $result
    echo $result >>$commit_msg
    echo $result >>/tmp/$(whoami).build.check.trace
    popd
}

function my_mktemp() {
    mktemp /tmp/$(whoami).$1.XXXXXXXXXX
}

# build a version of tokudb with a specific BDB target
function build() {

    # setup build environment
    export BDBVERSION=$1
    if [[ $BDBVERSION =~ ([0-9]+)\.([0-9]+) ]] ;then
        export BDBMAJOR=${BASH_REMATCH[1]}
        export BDBMINOR=${BASH_REMATCH[2]}
    else
        return 1
    fi
    export BDBDIR=/usr/local/BerkeleyDB.$BDBVERSION
    if [ ! -d $BDBDIR ] ; then return 2; fi

    tokudb_name=$(make_tokudb_name $branch $tokudb)
    export TOKUDB_NAME=$tokudb_name
    export TOKUDB_REVISION=$revision

    productname=$tokudb_name-$revision
    checkout=$branch/$tokudb

    latestrev=$(get_latest_svn_revision $checkout)
    if [ $latestrev -eq 0 ] ; then return 3; fi

    commit_msg=$(my_mktemp ft)

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

    tracefile=$builddir/$productname+$ftcc-$GCCVERSION+bdb-$BDBVERSION+$nodename+$system+$release+$arch
    if [ $have_cilk != 0 ] ; then tracefile=$tracefile+cilk; fi
    if [ $debugtests != 0 ] ; then tracefile=$tracefile+debug; fi
    if [ $releasetests != 0 ] ; then tracefile=$tracefile+release; fi

    # get some config info
    uname -a >$tracefile 2>&1
    ulimit -a >>$tracefile 2>&1
    $ftcc -v >>$tracefile 2>&1
    valgrind --version >>$tracefile 2>&1
    cat /etc/issue >>$tracefile 2>&1
    cat /proc/version >>$tracefile 2>&1
    cat /proc/cpuinfo >>$tracefile 2>&1
    env >>$tracefile 2>&1

    # checkout the source dir
    productbuilddir=$svnbase/$productname

    # cleanup
    rm -rf $productbuilddir
    mkdir -p $productbuilddir

    # checkout into $productbuilddir
    runcmd 0 $productbuilddir retry svn checkout -q -r $revision $svnserver/$checkout . >>$tracefile 2>&1

    if [ $debugtests != 0 ] ; then

	# make debug
	eval runcmd 0 $productbuilddir make release DEBUG=1 CC=$ftcc HAVE_CILK=$have_cilk >>$tracefile 2>&1
	eval runcmd 0 $productbuilddir/utils make -j$makejobs DEBUG=1 CC=$ftcc HAVE_CILK=$have_cilk >>$tracefile 2>&1
	eval runcmd 0 $productbuilddir/newbrt/tests make -j$makejobs DEBUG=1 CC=$ftcc HAVE_CILK=$have_cilk >>$tracefile 2>&1
	eval runcmd 0 $productbuilddir/src/tests make tests -j$makejobs -k -s SUMMARIZE=1 DEBUG=1 CC=$ftcc HAVE_CILK=$have_cilk >>$tracefile 2>&1

	# debug tests
	eval runcmd 0 $productbuilddir/$system/tests make check -k -s SUMMARIZE=1 DEBUG=1 CC=$ftcc HAVE_CILK=$have_cilk >>$tracefile 2>&1
	eval runcmd 0 $productbuilddir/utils make check -k -j$makejobs -s SUMMARIZE=1 DEBUG=1 CC=$ftcc HAVE_CILK=$have_cilk >>$tracefile 2>&1

	let n=makejobs; if [ $parallel != 0 ] ; then let n=n/2; fi
	range_trace=$(my_mktemp range)
	eval runcmd 0 $productbuilddir/src/range_tree/tests make check -k -j$n -s SUMMARIZE=1 CC=$ftcc DEBUG=1 HAVE_CILK=$have_cilk >>$range_trace 2>&1 $BG
	lock_trace=$(my_mktemp lock)
	eval runcmd 0 $productbuilddir/src/lock_tree/tests  make check -k -j$n -s SUMMARIZE=1 CC=$ftcc DEBUG=1 HAVE_CILK=$have_cilk >>$lock_trace 2>&1 $BG
	wait
	cat $range_trace >>$tracefile; rm $range_trace
	cat $lock_trace >>$tracefile; rm $lock_trace

	let n=makejobs; if [ $parallel != 0 ] ; then let n=n/2; fi
	newbrt_trace=$(my_mktemp newbrt)
	eval runcmd 0 $productbuilddir/newbrt make check -j$n -k -s SUMMARIZE=1 DEBUG=1 CC=$ftcc HAVE_CILK=$have_cilk >>$newbrt_trace 2>&1 $BG
	ydb_trace=$(my_mktemp ydb)
	eval runcmd 0 $productbuilddir/src/tests make check.tdb -j$n -k -s SUMMARIZE=1 DEBUG=1 CC=$ftcc HAVE_CILK=$have_cilk >>$ydb_trace 2>&1 $BG
	wait
	cat $newbrt_trace >>$tracefile; rm $newbrt_trace
	cat $ydb_trace >>$tracefile; rm $ydb_trace

        # benchmark tests
	eval runcmd 0 $productbuilddir/db-benchmark-test make -k -j$makejobs DEBUG=1 CC=$ftcc HAVE_CILK=$have_cilk >>$tracefile 2>&1

	let n=makejobs; if [ $parallel != 0 ] ; then let n=n/3; fi
	bench_trace=$(my_mktemp bench)
	eval runcmd 0 $productbuilddir/db-benchmark-test make check -k -j$n -k -s SUMMARIZE=1 DEBUG=1 CC=$ftcc HAVE_CILK=$have_cilk >>$bench_trace 2>&1 $BG

	# stress tests
	stress_trace=$(my_mktemp stress)
	if [ $stresstests != 0 ] ; then
	eval runcmd 0 $productbuilddir/src/tests make stress_tests.tdbrun -j$n -k -s SUMMARIZE=1 DEBUG=1 CC=$ftcc HAVE_CILK=$have_cilk >>$stress_trace 2>&1 $BG
	fi
	drd_trace=$(my_mktemp drd)
	if [ $drdtests != 0 ] ; then
	eval runcmd 0 $productbuilddir/src/tests make tiny_stress_tests.drdrun mid_stress_tests.drdrun -j$n -k -s SUMMARIZE=1 DEBUG=1 CC=$ftcc HAVE_CILK=$have_cilk >>$drd_trace 2>&1 $BG
	fi
	wait
	cat $bench_trace >>$tracefile; rm $bench_trace
	cat $stress_trace >>$tracefile; rm $stress_trace
	cat $drd_trace >>$tracefile; rm $drd_trace

        # upgrade tests
	if [ $upgradetests != 0 ] ; then
	    runcmd 0 $productbuilddir/src/tests make upgrade-tests.tdbrun -k -s SUMMARIZE=1 DEBUG=1 CC=$ftcc HAVE_CILK=$have_cilk >>$tracefile 2>&1
	fi
    fi

    if [ $releasetests != 0 ] ; then

        # release build 
	eval runcmd 0 $productbuilddir make clean >>$tracefile 2>&1
	eval runcmd 0 $productbuilddir make release CC=$ftcc HAVE_CILK=$have_cilk >>$tracefile 2>&1
	eval runcmd 0 $productbuilddir/utils make -j$makejobs CC=$ftcc HAVE_CILK=$have_cilk >>$tracefile 2>&1
	eval runcmd 0 $productbuilddir/release/examples make check -j$makejobs CC=$ftcc HAVE_CILK=$have_cilk >>$tracefile 2>&1

        # release tests
	eval runcmd 0 $productbuilddir/$system/tests make check -k -s SUMMARIZE=1 CC=$ftcc HAVE_CILK=$have_cilk VGRIND= >>$tracefile 2>&1
	eval runcmd 0 $productbuilddir/utils make check -k -j$makejobs -s SUMMARIZE=1 CC=$ftcc HAVE_CILK=$have_cilk VGRIND= >>$tracefile 2>&1
	eval runcmd 0 $productbuilddir/src/range_tree/tests make check -k -j$makejobs -s SUMMARIZE=1 CC=$ftcc HAVE_CILK=$have_cilk VGRIND= >>$tracefile 2>&1
	eval runcmd 0 $productbuilddir/src/lock_tree/tests  make check -k -j$makejobs -s SUMMARIZE=1 CC=$ftcc HAVE_CILK=$have_cilk VGRIND= >>$tracefile 2>&1
	eval runcmd 0 $productbuilddir/newbrt/tests make check -j$makejobs -k -s SUMMARIZE=1 CC=$ftcc HAVE_CILK=$have_cilk VGRIND= >>$tracefile 2>&1
	eval runcmd 0 $productbuilddir/src/tests make check.tdb -j$makejobs -k -s SUMMARIZE=1 CC=$ftcc HAVE_CILK=$have_cilk VGRIND= >>$tracefile 2>&1
	eval runcmd 0 $productbuilddir/src/tests make stress_tests.tdbrun -j$makejobs -k -s SUMMARIZE=1 CC=$ftcc HAVE_CILK=$have_cilk VGRIND= >>$tracefile 2>&1
	if [ $bdbtests != 0 ] ; then
	    bdb_trace=$(my_mktemp bdb)
	    eval runcmd 0 $productbuilddir/src/tests make check.bdb -j$makejobs -k -s SUMMARIZE=1 CC=$ftcc HAVE_CILK=$have_cilk VGRIND= >>$bdb_trace 2>&1
	    cat $bdb_trace >>$tracefile; rm $bdb_trace
	fi
    fi

    # cilk tests
    if [ $cilktests != 0 ] ; then
    runcmd 0 $productbuilddir make clean >>$tracefile 2>&1
    runcmd 0 $productbuilddir make release CC=$ftcc DEBUG=1 >>$tracefile 2>&1
    runcmd 0 $productbuilddir/newbrt/tests make cilkscreen_brtloader -k -s SUMMARIZE=1 CC=$ftcc DEBUG=1 >>$tracefile 2>&1
    fi

    # cxx
    if [ $cxxtests != 0 ] ; then
    runcmd $dowindows $productbuilddir/cxx make -k -s >>$tracefile 2>&1
    runcmd $dowindows $productbuilddir/cxx make -k -s install >>$tracefile 2>&1
    runcmd $dowindows $productbuilddir/cxx/tests make -k -s check SUMMARIZE=1 >>$tracefile 2>&1
    runcmd $dowindows $productbuilddir/db-benchmark-test-cxx make -k -s >>$tracefile 2>&1
    runcmd $dowindows $productbuilddir/db-benchmark-test-cxx make -k -s check >>$tracefile 2>&1
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
        coveragefile=$builddir/coverage+$productname-$BDBVERSION+$nodename+$system+$release+$arch
        rawcoverage=$(my_mktemp ftcover)
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
    if [ $commit -ne 0 ] ; then
	npass=$(egrep "^PASS " $tracefile | wc -l)
	nfail=$(egrep "^FAIL " $tracefile | wc -l)
	testresult="PASS=$npass"
	if [ $nfail != 0 ] ; then testresult="FAIL=$nfail $testresult"; fi

	local cf=$(my_mktemp ftresult)
	echo "$testresult $productname $ftcc-$GCCVERSION bdb-$BDBVERSION $system $release $arch $nodename" >$cf
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

    return 0
}

# set defaults
exitcode=0
svnserver=https://svn.tokutek.com/tokudb
nodename=$(uname -n)
system=$(uname -s | tr '[:upper:]' '[:lower:]' | sanitize)
release=$(uname -r | sanitize)
arch=$(uname -m | sanitize)
date=$(date +%Y%m%d)
branch=.
tokudb=tokudb
bdb=5.3
ncpus=$(get_ncpus)
toku_ncpus=8
makejobs=$ncpus
revision=0
VALGRIND=tokugrind
commit=1
docoverage=0
ftcc=gcc
have_cilk=0
have_poly=0
debugtests=1
releasetests=1
upgradetests=0
stresstests=1
drdtests=1
cilktests=0
cxxtests=0
parallel=0
bdbtests=1

while [ $# -gt 0 ] ; do
    arg=$1; shift
    if [[ $arg =~ ^--(.*)=(.*) ]] ; then
	eval ${BASH_REMATCH[1]}=${BASH_REMATCH[2]}
    else
	usage; exit 1
    fi
done

# setup intel compiler env
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

if [ $branch = "." ] ; then branch="toku"; fi
if [ $revision -eq 0 ] ; then revision=$(get_latest_svn_revision); fi
if [ $parallel -ne 0 ] ; then BG="&"; fi

# setup GCCVERSION
export GCCVERSION=$($ftcc --version|head -1|cut -f3 -d" ")
export VALGRIND=$VALGRIND

# setup TOKU_NCPUS
if [ -z "$TOKU_NCPUS" -a $toku_ncpus -le $ncpus ] ; then export TOKU_NCPUS=$toku_ncpus; fi

# limit execution time to 3 hours
let t=3*3600
ulimit -t $t
ulimit -c unlimited

build $bdb
