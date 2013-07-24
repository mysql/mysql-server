#!/usr/bin/env bash

# for all source tarballs and their coresponding md5 files, build a binary release tarball

system=$(uname -s|tr [:upper:] [:lower:])
arch=$(uname -m)

function expand() {
    echo $* | tr ,: " "
}

for f in *.md5; do
    if [[ $f =~ (.*).tar.gz.md5 ]] ; then
        mysqlsrc=${BASH_REMATCH[1]}
    else
        exit 1
    fi
    if [ -d $mysqlsrc ] ; then continue; fi
    md5sum --check $mysqlsrc.tar.gz.md5
    if [ $? != 0 ] ; then exit 1; fi
    tar xzf $mysqlsrc.tar.gz
    if [ $? != 0 ] ; then exit 1; fi
    mkdir $mysqlsrc/build.RelWithDebInfo
    pushd $mysqlsrc/build.RelWithDebInfo
    if [ $? != 0 ] ; then exit 1; fi
    cmake -D BUILD_CONFIG=mysql_release -D CMAKE_BUILD_TYPE=RelWithDebInfo -D BUILD_TESTING=OFF ..
    if [ $? != 0 ] ; then exit 1; fi
    make -j4 package
    if [ $? != 0 ] ; then exit 1; fi
    if [ ! -f $mysqlsrc-$system-$arch.tar.gz ] ; then exit 1; fi
    popd
done	
