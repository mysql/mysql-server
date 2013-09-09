#!/bin/bash

set -e

pushd $(dirname $0) &>/dev/null
scriptdir=$PWD
popd &>/dev/null
tokudbdir=$(dirname $scriptdir)

cd $tokudbdir

if [ ! -d build ] ; then
    mkdir build
    pushd build
    CC=gcc47 CXX=g++47 cmake \
        -D CMAKE_BUILD_TYPE=Release \
        -D USE_VALGRIND=ON \
        -D TOKU_DEBUG_PARANOID=OFF \
        -D USE_CTAGS=OFF \
        -D USE_GTAGS=OFF \
        -D USE_CSCOPE=OFF \
        -D USE_ETAGS=OFF \
        -D USE_BDB=ON \
        -D CMAKE_LINK_DEPENDS_NO_SHARED=ON \
        -G Ninja \
        -D RUN_LONG_TESTS=ON \
        -D TOKUDB_DATA=$tokudbdir/../tokudb.data \
        ..
    ninja build_jemalloc build_lzma
    popd
fi

cd build
set +e
ctest -j16 \
    -D NightlyStart \
    -D NightlyUpdate \
    -D NightlyConfigure \
    -D NightlyBuild \
    -D NightlyTest \
    -E '/drd|/helgrind'
ctest -j16 \
    -D NightlyMemCheck \
    -E '^ydb/.*\.bdb|test1426\.tdb|/drd|/helgrind'
set -e
ctest -D NightlySubmit
