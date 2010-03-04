#!/bin/bash

SVN_USER="tokubuild"
SVN_PASS="Tb091638"


if ! [ -d $HOME/svn.build ] ; then
    mkdir -p $HOME/svn.build || exit 1
fi

cd $HOME/svn.build
if ! [ -d tokudb.build ] ; then
    svn --username $SVN_USER --password $SVN_PASS co -q --depth=empty https://svn.tokutek.com/tokudb/tokudb.build || exit 1
fi

cd tokudb.build
svn --username $SVN_USER --password $SVN_PASS up -q bin || exit 1

cd bin

./build.check.bash --windows=1 "$@" || exit 1

