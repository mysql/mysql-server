#!/bin/bash

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

if [[ -z $ftcc ]]; then
    export ftcc=icc
fi
if [[ -z $BDBVERSION ]]
then
    export BDBVERSION=5.3
fi
export BDBDIR=/usr/local/BerkeleyDB.$BDBVERSION

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
ncpus=$(grep bogomips /proc/cpuinfo | wc -l)

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
    export ftar=`which xiar`
    export ftld=`which xild`
else
    export ftar=`which ar`
    export ftld=`which ld`
fi

export GCCVERSION=$($ftcc --version|head -1|cut -f3 -d" ")

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
    (svn mkdir $svnserver/tokudb.build/$date -m "" ;
     svn co -q $svnserver/tokudb.build/$date) || rm -rf $date
done
popd

tracefilepfx=$builddir/$productname+$ftcc-$GCCVERSION+bdb-$BDBVERSION+$nodename+$system+$release+$arch

function getsysinfo() {
    tracefile=$1; shift
    uname -a >$tracefile 2>&1
    ulimit -a >>$tracefile 2>&1
    cmake --version >>$tracefile 2>&1
    $ftcc -v >>$tracefile 2>&1
    valgrind --version >>$tracefile 2>&1
    cat /etc/issue >>$tracefile 2>&1
    cat /proc/version >>$tracefile 2>&1
    cat /proc/cpuinfo >>$tracefile 2>&1
    env >>$tracefile 2>&1
}

function get_latest_svn_revision() {
    svn info | awk -v ORS="" '/Last Changed Rev:/ { print $4 }'
}

function my_mktemp() {
    mktemp /tmp/$(whoami).$1.XXXXXXXXXX
}

################################################################################
## run tests on icc release build
resultsdir=$tracefilepfx-Release
mkdir $resultsdir
tracefile=$tracefilepfx-Release/trace

getsysinfo $tracefile

mkdir -p $FULLTOKUDBDIR/Release >/dev/null 2>&1
cd $FULLTOKUDBDIR/Release
cmake \
    -D CMAKE_BUILD_TYPE=Release \
    -D INTEL_CC=ON \
    -D BUILD_TESTING=ON \
    -D USE_BDB=ON \
    -D BDBDIR=$BDBDIR \
    -D RUN_LONG_TESTS=ON \
    -D USE_CILK=OFF \
    .. 2>&1 | tee -a $tracefile
cmake --system-information $resultsdir/sysinfo
set +e
ctest -j$ncpus \
    -D NightlyStart \
    -D NightlyUpdate \
    -D NightlyConfigure \
    -D NightlyBuild \
    -D NightlyTest \
    2>&1 | tee -a $tracefile
set -e

cp $tracefile notes.txt
set +e
ctest -D NightlySubmit -A notes.txt \
    2>&1 | tee -a $tracefile
set -e
rm notes.txt

tag=$(head -n1 Testing/TAG)
cp -r Testing/$tag $resultsdir
cf=$(my_mktemp ftresult)
cat "$resultsdir/trace" | awk '
/[0-9]+% tests passed, [0-9]+ tests failed out of [0-9]+/ {
    fail=$4;
    total=$9;
    pass=total-fail;
    ORS=" ";
    if (fail>0) {
        print "FAIL=" fail
    }
    print "PASS=" pass
}' >"$cf"
get_latest_svn_revision >>"$cf"
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

################################################################################
## run valgrind on icc debug build
resultsdir=$tracefilepfx-Debug
mkdir $resultsdir
tracefile=$tracefilepfx-Debug/trace

getsysinfo $tracefile

mkdir -p $FULLTOKUDBDIR/Debug >/dev/null 2>&1
cd $FULLTOKUDBDIR/Debug
cmake \
    -D CMAKE_BUILD_TYPE=Debug \
    -D INTEL_CC=ON \
    -D BUILD_TESTING=ON \
    -D USE_BDB=OFF \
    -D USE_CILK=OFF \
    .. 2>&1 | tee -a $tracefile
cmake --system-information $resultsdir/sysinfo
set +e
ctest -j$ncpus \
    -D NightlyStart \
    -D NightlyUpdate \
    -D NightlyConfigure \
    -D NightlyBuild \
    -D NightlyMemCheck \
    2>&1 | tee -a $tracefile
set -e

cp $tracefile notes.txt
set +e
ctest -D NightlySubmit -A notes.txt \
    2>&1 | tee -a $tracefile
set -e
rm notes.txt

tag=$(head -n1 Testing/TAG)
cp -r Testing/$tag $resultsdir
cf=$(my_mktemp ftresult)
cat "$resultsdir/trace" | awk '
BEGIN {
    leaks=0;
    ORS=" ";
}
/[0-9]+% tests passed, [0-9]+ tests failed out of [0-9]+/ {
    fail=$4;
    total=$9;
    pass=total-fail;
}
/^Memory Leak - [0-9]+/ {
    leaks=$4;
}
END {
    print "LEAKS=" leaks;
    if (fail>0) {
        print "FAIL=" fail
    }
    print "PASS=" pass
}' >"$cf"
get_latest_svn_revision >>"$cf"
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

################################################################################
## run gcov on gcc debug build
resultsdir=$tracefilepfx-Coverage
mkdir $resultsdir
tracefile=$tracefilepfx-Coverage/trace

getsysinfo $tracefile

mkdir -p $FULLTOKUDBDIR/Coverage >/dev/null 2>&1
cd $FULLTOKUDBDIR/Coverage
cmake \
    -D CMAKE_BUILD_TYPE=Debug \
    -D BUILD_TESTING=ON \
    -D USE_GCOV=ON \
    -D USE_BDB=OFF \
    -D USE_CILK=OFF \
    .. 2>&1 | tee -a $tracefile
cmake --system-information $resultsdir/sysinfo
set +e
ctest -j$ncpus \
    -D NightlyStart \
    -D NightlyUpdate \
    -D NightlyConfigure \
    -D NightlyBuild \
    -D NightlyTest \
    -D NightlyCoverage \
    2>&1 | tee -a $tracefile
set -e

cp $tracefile notes.txt
set +e
ctest -D NightlySubmit -A notes.txt \
    2>&1 | tee -a $tracefile
set -e
rm notes.txt

tag=$(head -n1 Testing/TAG)
cp -r Testing/$tag $resultsdir
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
get_latest_svn_revision >>"$cf"
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

exit 0
