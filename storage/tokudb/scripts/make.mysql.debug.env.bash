#!/usr/bin/env bash

set -e -u

function usage() {
    echo "build a debug mysql in the current directory"
    echo "with default parameters it builds a debug $mysql-$mysql_tree"
    echo "--git_tag=$git_tag"
    echo "--mysql=$mysql --mysql_tree=$mysql_tree"
    echo "--tokudbengine=$tokudbengine --tokudbengine_tree=$tokudbengine_tree"
    echo "--ftindex=$ftindex --ftindex_tree=$ftindex_tree"
    echo "--jemalloc=$jemalloc --jemalloc_tree=$jemalloc_tree"
    echo "--backup=$backup --backup_tree=$backup_tree"
    echo "--cc=$cc --cxx=$cxx"
    echo "--local_cache_dir=$local_cache_dir --local_cache_update=$local_cache_update"
    echo "--cmake_valgrind=$cmake_valgrind --cmake_debug_paranoid=$cmake_debug_paranoid"
}

function github_clone() {
    local repo=
    if [ $# -gt 0 ] ; then repo=$1; shift; else test 0 = 1; return; fi
    local tree=
    if [ $# -gt 0 ] ; then tree=$1; shift; else test 0 = 1; return; fi
    local destdir=
    if [ $# -gt 0 ] ; then destdir=$1; shift; fi
    if [[ -z "$local_cache_dir" ]] ; then
        git clone git@github.com:Tokutek/$repo $destdir
        if [ $? != 0 ] ; then test 0 = 1; return; fi
    else
        if (( "$local_cache_update" )) ; then
            pushd $local_cache_dir/$repo.git
            git fetch --all -f -p -v
            git fetch --all -f -p -v -t
            popd
        fi
        git clone --reference $local_cache_dir/$repo.git git@github.com:Tokutek/$repo $destdir
        if [ $? != 0 ] ; then test 0 = 1; return; fi
    fi

    if [ -z "$destdir" ] ; then pushd $repo; else pushd $destdir; fi
    if [ -z "$git_tag" ] ; then
        if ! git branch | grep "\<$tree\>" > /dev/null && git branch -a | grep "remotes/origin/$tree\>" > /dev/null; then
            git checkout --track origin/$tree
        else
            git checkout $tree
        fi
    else
        git checkout $git_tag
    fi
    if [ $? != 0 ] ; then test 0 = 1; return; fi
    popd
}

# shopt -s compat31 2>/dev/null

git_tag=
mysql=mysql-5.5
mysql_tree=mysql-5.5.41
jemalloc=jemalloc
jemalloc_tree=3.6.0
tokudbengine=tokudb-engine
tokudbengine_tree=master
ftindex=ft-index
ftindex_tree=master
backup=tokudb-backup-plugin
backup_tree=master
cc=gcc
cxx=g++
local_cache_dir=
local_cache_update=1
cmake_valgrind=
cmake_debug_paranoid=

while [ $# -ne 0 ] ; do
    arg=$1; shift
    if [[ $arg =~ ^--(.*)=(.*) ]] ; then
        eval ${BASH_REMATCH[1]}=${BASH_REMATCH[2]};
    else
        usage; exit 1;
    fi
done

if [[ $mysql =~ ^(.*)-(([0-9]+\.[0-9]+)\.[0-9]+.*)$ ]] ; then
    mysql=${BASH_REMATCH[1]}-${BASH_REMATCH[3]}
    mysql_tree=${BASH_REMATCH[1]}-${BASH_REMATCH[2]}
fi

# setup environment variables
build_dir=$PWD/build
mkdir $build_dir
if [ $? != 0 ] ; then exit 1; fi
install_dir=$PWD/install
mkdir $install_dir
if [ $? != 0 ] ; then exit 1; fi

# checkout the fractal tree
github_clone $ftindex $ftindex_tree

# checkout jemalloc
github_clone $jemalloc $jemalloc_tree

# checkout mysql
github_clone $mysql $mysql_tree $mysql_tree

# checkout the community backup
github_clone $backup $backup_tree

# checkout the tokudb handlerton
github_clone $tokudbengine $tokudbengine_tree

# setup links'
pushd $tokudbengine/storage/tokudb
if [ $? != 0 ] ; then exit 1; fi
ln -s ../../../$ftindex ft-index
if [ $? != 0 ] ; then exit 1; fi
popd
pushd $mysql_tree/storage
if [ $? != 0 ] ; then exit 1; fi
ln -s ../../$tokudbengine/storage/tokudb tokudb
if [ $? != 0 ] ; then exit 1; fi
popd
pushd $mysql_tree/plugin
if [ $? != 0 ] ; then exit 1; fi
ln -s ../../$backup $backup
if [ $? != 0 ] ; then exit 1; fi
popd
pushd $mysql_tree/scripts
if [ $? != 0 ] ; then exit 1; fi
ln ../../$tokudbengine/scripts/tokustat.py
if [ $? != 0 ] ; then exit 1; fi
ln ../../$tokudbengine/scripts/tokufilecheck.py
if [ $? != 0 ] ; then exit 1; fi
popd
if [[ $mysql =~ mariadb ]] || [[ $mysql_tree =~ mariadb ]] ; then
    pushd $mysql_tree/extra
    if [ $? != 0 ] ; then exit 1; fi
    ln -s ../../$jemalloc $jemalloc
    if [ $? != 0 ] ; then exit 1; fi
    popd
else
    pushd $ftindex/third_party
    if [ $? != 0 ] ; then exit 1; fi
    ln -s ../../$jemalloc $jemalloc
    if [ $? != 0 ] ; then exit 1; fi
    popd
fi

pushd $build_dir
if [ $? != 0 ] ; then exit 1; fi
extra_cmake_options="-DCMAKE_LINK_DEPENDS_NO_SHARED=ON"
extra_cmake_options+=" -DBUILD_TESTING=OFF"
extra_cmake_options+=" -DMYSQL_MAINTAINER_MODE=OFF"
if (( $cmake_valgrind )) ; then
    extra_cmake_options+=" -DUSE_VALGRIND=ON"
fi
if (( $cmake_debug_paranoid )) ; then
    extra_cmake_options+=" -DTOKU_DEBUG_PARANOID=ON"
fi
CC=$cc CXX=$cxx cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=$install_dir $extra_cmake_options ../$mysql_tree
if [ $? != 0 ] ; then exit 1; fi
make -j4 install
if [ $? != 0 ] ; then exit 1; fi
popd

pushd $install_dir
scripts/mysql_install_db --defaults-file=$HOME/$(whoami).cnf
popd
