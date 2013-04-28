#!/bin/bash

set -e
set -u

repos=https://github.com/Tokutek
#repos=$HOME/repos

if [[ ! -d mysql ]]; then
    git clone $repos/mysql
    cd mysql
    git checkout simplify-build

    git clone $repos/backup-community
    ln -s backup-community/backup toku_backup

    git clone $repos/ft-engine
    pushd ft-engine
        git checkout simplify-build
    popd
    cp -r ft-engine/* .
    pushd storage/tokudb
        git clone $repos/ft-index
        pushd ft-index
            git checkout simplify-build
            pushd third_party
                git clone $repos/jemalloc
            popd
        popd
    popd
else
    cd mysql
fi

if [[ ! -d build ]]; then
    mkdir build
    cd build

    CC=gcc47 CXX=g++47 cmake \
        -D BUILD_CONFIG=mysql_release \
        -D CMAKE_BUILD_TYPE=Release \
        -D TOKU_DEBUG_PARANOID=OFF \
        -D USE_VALGRIND=OFF \
        -D BUILD_TESTING=OFF \
        -D USE_CSCOPE=OFF \
        -D USE_GTAGS=OFF \
        -D USE_CTAGS=OFF \
        -D USE_ETAGS=OFF \
        ..
else
    cd build
fi

make -j5 package
