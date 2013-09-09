#!/bin/bash

function usage() {
    echo "run.fractal.tree.tests.bash - run the nightly fractal tree test suite"
    echo "[--ftcc=$ftcc] [--ftcxx=$ftcxx] [--BDBVERSION=$BDBVERSION] [--ctest_model=$ctest_model]"
    echo "[--commit=$commit] [--generator=$generator] [--toku_svnroot=$toku_svnroot]"
    return 1
}

[ -f /etc/profile.d/gcc47.sh ] && . /etc/profile.d/gcc47.sh
[ -f /etc/profile.d/binutils222.sh ] && . /etc/profile.d/binutils222.sh

set -e

pushd $(dirname $0) &>/dev/null
SCRIPTDIR=$PWD
popd &>/dev/null
FULLTOKUDBDIR=$(dirname $SCRIPTDIR)
TOKUDBDIR=$(basename $FULLTOKUDBDIR)
BRANCHDIR=$(basename $(dirname $FULLTOKUDBDIR))

function make_tokudb_name() {
    local tokudb_dir=$1
    local tokudb=$2
    if [ $tokudb_dir = "toku" ] ; then
        echo $tokudb
    else
        echo $(echo $tokudb_dir-$tokudb | tr / -)
    fi
}
tokudb_name=$(make_tokudb_name $BRANCHDIR $TOKUDBDIR)
export TOKUDB_NAME=$tokudb_name

productname=$tokudb_name

ftcc=gcc47
ftcxx=g++47
BDBVERSION=5.3
ctest_model=Nightly
generator="Unix Makefiles"
toku_svnroot=$FULLTOKUDBDIR/../..
commit=1
while [ $# -gt 0 ] ; do
    arg=$1; shift
    if [[ $arg =~ --(.*)=(.*) ]] ; then
        eval ${BASH_REMATCH[1]}=${BASH_REMATCH[2]}
    else
        usage; exit 1;
    fi
done

if [[ ! ( ( $ctest_model = Nightly ) || ( $ctest_model = Experimental ) || ( $ctest_model = Continuous ) ) ]]; then
    echo "--ctest_model must be Nightly, Experimental, or Continuous"
    usage
fi

BDBDIR=/usr/local/BerkeleyDB.$BDBVERSION
if [ -d $BDBDIR ] ; then
    CMAKE_PREFIX_PATH=$BDBDIR:$CMAKE_PREFIX_PATH
    export CMAKE_PREFIX_PATH
fi

# delete some characters that cygwin and osx have trouble with
function sanitize() {
    tr -d '[/:\\\\()]'
}

# gather some info
svnserver=https://svn.tokutek.com/tokudb
nodename=$(uname -n)
system=$(uname -s | tr '[:upper:]' '[:lower:]' | sanitize)
release=$(uname -r | sanitize)
arch=$(uname -m | sanitize)
date=$(date +%Y%m%d)
ncpus=$([ -f /proc/cpuinfo ] && (grep bogomips /proc/cpuinfo | wc -l) || sysctl -n hw.ncpu)
njobs=$(if [ $ncpus -gt 8 ] ; then echo "$ncpus / 3" | bc ; else echo "$ncpus" ; fi)

GCCVERSION=$($ftcc --version|head -1|cut -f3 -d" ")
export GCCVERSION
CC=$ftcc
export CC
CXX=$ftcxx
export CXX

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

if [[ $commit -eq 1 ]]; then
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
    set +e
    svn mkdir $svnserver/tokudb.build/$date -m "" || true
    retry svn co -q $svnserver/tokudb.build/$date
    if [ ! -d $date ] ; then
        exit 1
    fi
    set -e
    popd

    tracefilepfx=$builddir/$productname+$ftcc-$GCCVERSION+bdb-$BDBVERSION+$nodename+$system+$release+$arch
else
    tracefilepfx=$FULLTOKUDBDIR/test-trace
fi

function getsysinfo() {
    tracefile=$1; shift
    set +e
    uname -a >$tracefile 2>&1
    ulimit -a >>$tracefile 2>&1
    cmake --version >>$tracefile 2>&1
    $ftcc -v >>$tracefile 2>&1
    $ftcxx -v >>$tracefile 2>&1
    valgrind --version >>$tracefile 2>&1
    cat /etc/issue >>$tracefile 2>&1
    cat /proc/version >>$tracefile 2>&1
    cat /proc/cpuinfo >>$tracefile 2>&1
    env >>$tracefile 2>&1
    set -e
}

function get_latest_svn_revision() {
    svn info $1 | awk -v ORS="" '/Last Changed Rev:/ { print $4 }'
}

function my_mktemp() {
    mktemp /tmp/$(whoami).$1.XXXXXXXXXX
}

yesterday="$(date -u -d yesterday +%F) 03:59:00 +0000"

if [[ $commit -eq 1 ]]; then
    # hack to make long tests run nightly but not when run in experimental mode
    longtests=ON
else
    longtests=OFF
fi
################################################################################
## run normal and valgrind on optimized build
resultsdir=$tracefilepfx-Release
mkdir $resultsdir
tracefile=$tracefilepfx-Release/trace

getsysinfo $tracefile

mkdir -p $FULLTOKUDBDIR/opt >/dev/null 2>&1
cd $FULLTOKUDBDIR/opt
cmake \
    -D CMAKE_BUILD_TYPE=Release \
    -D USE_VALGRIND=ON \
    -D USE_BDB=ON \
    -D RUN_LONG_TESTS=$longtests \
    -D USE_CTAGS=OFF \
    -D USE_GTAGS=OFF \
    -D USE_ETAGS=OFF \
    -D USE_CSCOPE=OFF \
    -D TOKU_SVNROOT="$toku_svnroot" \
    -G "$generator" \
    .. 2>&1 | tee -a $tracefile
cmake --system-information $resultsdir/sysinfo
make clean
# update to yesterday exactly just before ctest does nightly update
svn up -q -r "{$yesterday}" ..
set +e
ctest -j$njobs \
    -D ${ctest_model}Start \
    -D ${ctest_model}Update \
    -D ${ctest_model}Configure \
    -D ${ctest_model}Build \
    -D ${ctest_model}Test \
    -E '/drd|/helgrind' \
    2>&1 | tee -a $tracefile
ctest -j$njobs \
    -D ${ctest_model}MemCheck \
    -E '^ydb/.*\.bdb$|test1426.tdb|/drd|/helgrind' \
    2>&1 | tee -a $tracefile
set -e

cp $tracefile notes.txt
set +e
ctest -D ${ctest_model}Submit -A notes.txt \
    2>&1 | tee -a $tracefile
set -e
rm notes.txt

tag=$(head -n1 Testing/TAG)
cp -r Testing/$tag $resultsdir
if [[ $commit -eq 1 ]]; then
    cf=$(my_mktemp ftresult)
    cat "$resultsdir/trace" | awk '
BEGIN {
    errs=0;
    look=0;
    ORS=" ";
}
/[0-9]+% tests passed, [0-9]+ tests failed out of [0-9]+/ {
    fail=$4;
    total=$9;
    pass=total-fail;
}
/^Memory checking results:/ {
    look=1;
    FS=" - ";
}
/Errors while running CTest/ {
    look=0;
    FS=" ";
}
{
    if (look) {
        errs+=$2;
    }
}
END {
    print "ERRORS=" errs;
    if (fail>0) {
        print "FAIL=" fail
    }
    print "PASS=" pass
}' >"$cf"
    get_latest_svn_revision $FULLTOKUDBDIR >>"$cf"
    echo -n " " >>"$cf"
    cat "$resultsdir/trace" | awk '
BEGIN {
    FS=": ";
}
/Build name/ {
    print $2;
    exit
}' >>"$cf"
    (echo; echo) >>"$cf"
    cat "$resultsdir/trace" | awk '
BEGIN {
    printit=0
}
/[0-9]*\% tests passed, [0-9]* tests failed out of [0-9]*/ { printit=1 }
/Memory check project/ { printit=0 }
/^   Site:/ { printit=0 }
{
    if (printit) {
        print $0
    }
}' >>"$cf"
    svn add $resultsdir
    svn commit -F "$cf" $resultsdir
    rm $cf
fi

################################################################################
## run drd tests on debug build
resultsdir=$tracefilepfx-Debug
mkdir $resultsdir
tracefile=$tracefilepfx-Debug/trace

getsysinfo $tracefile

mkdir -p $FULLTOKUDBDIR/dbg >/dev/null 2>&1
cd $FULLTOKUDBDIR/dbg
cmake \
    -D CMAKE_BUILD_TYPE=Debug \
    -D USE_VALGRIND=ON \
    -D USE_BDB=OFF \
    -D RUN_LONG_TESTS=$longtests \
    -D USE_CTAGS=OFF \
    -D USE_GTAGS=OFF \
    -D USE_ETAGS=OFF \
    -D USE_CSCOPE=OFF \
    -D CMAKE_C_FLAGS_DEBUG="-O1" \
    -D CMAKE_CXX_FLAGS_DEBUG="-O1" \
    -D TOKU_SVNROOT="$toku_svnroot" \
    -G "$generator" \
    .. 2>&1 | tee -a $tracefile
cmake --system-information $resultsdir/sysinfo
make clean
# update to yesterday exactly just before ctest does nightly update
svn up -q -r "{$yesterday}" ..
set +e
ctest -j$njobs \
    -D ${ctest_model}Start \
    -D ${ctest_model}Update \
    -D ${ctest_model}Configure \
    -D ${ctest_model}Build \
    -D ${ctest_model}Test \
    -R '/drd|/helgrind' \
    2>&1 | tee -a $tracefile
set -e

cp $tracefile notes.txt
set +e
ctest -D ${ctest_model}Submit -A notes.txt \
    2>&1 | tee -a $tracefile
set -e
rm notes.txt

tag=$(head -n1 Testing/TAG)
cp -r Testing/$tag $resultsdir
if [[ $commit -eq 1 ]]; then
    cf=$(my_mktemp ftresult)
    cat "$resultsdir/trace" | awk '
BEGIN {
    ORS=" ";
}
/[0-9]+% tests passed, [0-9]+ tests failed out of [0-9]+/ {
    fail=$4;
    total=$9;
    pass=total-fail;
}
END {
    if (fail>0) {
        print "FAIL=" fail
    }
    print "PASS=" pass
}' >"$cf"
    get_latest_svn_revision $FULLTOKUDBDIR >>"$cf"
    echo -n " " >>"$cf"
    cat "$resultsdir/trace" | awk '
BEGIN {
    FS=": ";
}
/Build name/ {
    print $2;
    exit
}' >>"$cf"
    (echo; echo) >>"$cf"
    cat "$resultsdir/trace" | awk '
BEGIN {
    printit=0
}
/[0-9]*\% tests passed, [0-9]* tests failed out of [0-9]*/ { printit=1 }
/^   Site:/ { printit=0 }
{
    if (printit) {
        print $0
    }
}' >>"$cf"
    svn add $resultsdir
    svn commit -F "$cf" $resultsdir
    rm $cf
fi

################################################################################
## run gcov on debug build
resultsdir=$tracefilepfx-Coverage
mkdir $resultsdir
tracefile=$tracefilepfx-Coverage/trace

getsysinfo $tracefile

mkdir -p $FULLTOKUDBDIR/cov >/dev/null 2>&1
cd $FULLTOKUDBDIR/cov
cmake \
    -D CMAKE_BUILD_TYPE=Debug \
    -D BUILD_TESTING=ON \
    -D USE_GCOV=ON \
    -D USE_BDB=OFF \
    -D RUN_LONG_TESTS=$longtests \
    -D USE_CTAGS=OFF \
    -D USE_GTAGS=OFF \
    -D USE_ETAGS=OFF \
    -D USE_CSCOPE=OFF \
    -D TOKU_SVNROOT="$toku_svnroot" \
    -G "$generator" \
    .. 2>&1 | tee -a $tracefile
cmake --system-information $resultsdir/sysinfo
make clean
# update to yesterday exactly just before ctest does nightly update
svn up -q -r "{$yesterday}" ..
set +e
ctest -j$njobs \
    -D ${ctest_model}Start \
    -D ${ctest_model}Update \
    -D ${ctest_model}Configure \
    -D ${ctest_model}Build \
    -D ${ctest_model}Test \
    -D ${ctest_model}Coverage \
    2>&1 | tee -a $tracefile
set -e

cp $tracefile notes.txt
set +e
ctest -D ${ctest_model}Submit -A notes.txt \
    2>&1 | tee -a $tracefile
set -e
rm notes.txt

tag=$(head -n1 Testing/TAG)
cp -r Testing/$tag $resultsdir
if [[ $commit -eq 1 ]]; then
    cf=$(my_mktemp ftresult)
    cat "$resultsdir/trace" | awk '
BEGIN {
    ORS=" ";
}
/Percentage Coverage:/ {
    covpct=$3;
}
/[0-9]+% tests passed, [0-9]+ tests failed out of [0-9]+/ {
    fail=$4;
    total=$9;
    pass=total-fail;
}
END {
    print "COVERAGE=" covpct
    if (fail>0) {
        print "FAIL=" fail
    }
    print "PASS=" pass
}' >"$cf"
    get_latest_svn_revision $FULLTOKUDBDIR >>"$cf"
    echo -n " " >>"$cf"
    cat "$resultsdir/trace" | awk '
BEGIN {
    FS=": ";
}
/Build name/ {
    print $2;
    exit
}' >>"$cf"
    (echo; echo) >>"$cf"
    cat "$resultsdir/trace" | awk '
BEGIN {
    printit=0
}
/[0-9]*\% tests passed, [0-9]* tests failed out of [0-9]*/ { printit=1 }
/^   Site:/ { printit=0 }
{
    if (printit) {
        print $0
    }
}' >>"$cf"
    svn add $resultsdir
    svn commit -F "$cf" $resultsdir
    rm $cf
fi

exit 0
