#!/usr/bin/env bash

set -e -u

function usage() {
    echo "build a debug mysql in the current directory"
    echo "with default parameters it builds a debug $mysql-$mysql_tree"
    echo "--git_tag=$git_tag"
    echo "--mysql=$mysql --mysql_tree=$mysql_tree"
    echo "--ftengine=$ftengine --ftengine_tree=$ftengine_tree"
    echo "--ftindex=$ftindex --ftindex_tree=$ftindex_tree"
    echo "--jemalloc=$jemalloc --jemalloc_tree=$jemalloc_tree"
    echo "--backup=$backup --backup_tree=$backup_tree"
    echo "--cc=$cc --cxx=$cxx"
    echo "--local_cache_dir=$local_cache_dir --local_cache_update=$local_cache_update"
    echo "--cmake_valgrind=$cmake_valgrind --cmake_debug_paranoid=$cmake_debug_paranoid"
}

function github_clone() {
    local repo=$1; local tree=$2
    if [[ -z "$local_cache_dir" ]] ; then
        git clone git@github.com:Tokutek/$repo
        if [ $? != 0 ] ; then exit 1; fi
    else
        if (( "$local_cache_update" )) ; then
            pushd $local_cache_dir/$repo.git
            git fetch --all -f -p -v
            git fetch --all -f -p -v -t
            popd
        fi
        git clone --reference $local_cache_dir/$repo.git git@github.com:Tokutek/$repo
        if [ $? != 0 ] ; then exit 1; fi
    fi

    pushd $repo
    if [ $? != 0 ] ; then exit 1; fi
    if [ -z $git_tag ] ; then
        if ! git branch | grep "\<$tree\>" > /dev/null && git branch -a | grep "remotes/origin/$tree\>" > /dev/null; then
            git checkout --track origin/$tree
        else
            git checkout $tree
        fi
    else
        git checkout $git_tag
    fi
    if [ $? != 0 ] ; then exit 1; fi
    popd
}

# shopt -s compat31 2>/dev/null

git_tag=
mysql=mysql
mysql_tree=5.5.30
jemalloc=jemalloc
jemalloc_tree=3.3.1
ftengine=ft-engine
ftengine_tree=master
ftindex=ft-index
ftindex_tree=master
backup=backup-community
backup_tree=master
cc=gcc47
cxx=g++47
local_cache_dir=
local_cache_update=1
cmake_valgrind=
cmake_debug_paranoid=

while [ $# -ne 0 ] ; do
    arg=$1; shift
    if [[ $arg =~ --(.*)=(.*) ]] ; then
        eval ${BASH_REMATCH[1]}=${BASH_REMATCH[2]};
    else
        usage; exit 1;
    fi
done

# setup environment variables
install_dir=$PWD/$mysql-install
mkdir $install_dir
if [ $? != 0 ] ; then exit 1; fi

# checkout the fractal tree
github_clone $ftindex $ftindex_tree
github_clone $jemalloc $jemalloc_tree
pushd $ftindex/third_party
if [ $? != 0 ] ; then exit 1; fi
ln -s ../../$jemalloc $jemalloc
if [ $? != 0 ] ; then exit 1; fi
popd

# checkout mysql'
github_clone $mysql $mysql_tree

# checkout the community backup
github_clone $backup $backup_tree

# checkout the tokudb handlerton
github_clone $ftengine $ftengine_tree

# setup links'
pushd $ftengine/storage/tokudb
if [ $? != 0 ] ; then exit 1; fi
ln -s ../../../$ftindex ft-index
if [ $? != 0 ] ; then exit 1; fi
popd
pushd $mysql/storage
if [ $? != 0 ] ; then exit 1; fi
ln -s ../../$ftengine/storage/tokudb tokudb
if [ $? != 0 ] ; then exit 1; fi
popd
pushd $mysql
if [ $? != 0 ] ; then exit 1; fi
ln -s ../$backup/backup toku_backup
if [ $? != 0 ] ; then exit 1; fi
popd
pushd $mysql/scripts
if [ $? != 0 ] ; then exit 1; fi
ln ../../$ftengine/scripts/tokustat.py
if [ $? != 0 ] ; then exit 1; fi
ln ../../$ftengine/scripts/tokufilecheck.py
if [ $? != 0 ] ; then exit 1; fi
popd

# build in the mysql directory
mkdir $mysql/build.debug
if [ $? != 0 ] ; then exit 1; fi
pushd $mysql/build.debug
if [ $? != 0 ] ; then exit 1; fi
extra_cmake_options="-DCMAKE_LINK_DEPENDS_NO_SHARED=ON"
if (( $cmake_valgrind )) ; then
    extra_cmake_options+=" -DUSE_VALGRIND=ON"
fi
if (( $cmake_debug_paranoid )) ; then
    extra_cmake_options+=" -DTOKU_DEBUG_PARANOID=ON"
fi
CC=$cc CXX=$cxx cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=$install_dir -DBUILD_TESTING=OFF $extra_cmake_options
if [ $? != 0 ] ; then exit 1; fi
make -j4 install
if [ $? != 0 ] ; then exit 1; fi
popd

pushd $install_dir
scripts/mysql_install_db --defaults-file=$HOME/$(whoami).cnf
popd
