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
if ! [ -d scripts ] ; then
    svn --username $SVN_USER --password $SVN_PASS co -q --depth=empty https://svn.tokutek.com/toku/tokudb/scripts || exit 1
fi

cd scripts
svn --username $SVN_USER --password $SVN_PASS up || exit 1

cd bin

./build.check.bash --windows=1 "$@" || exit 1

