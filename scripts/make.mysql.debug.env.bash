#!/usr/bin/env bash

function usage() {
    echo "generate a script that builds a debug mysql from github repo's"
    echo "--git_tag=$git_tag"
    echo "--mysql=$mysql --mysql_branch=$mysql_branch"
    echo "--ftengine=$ftengine --ftengine_branch=$ftengine_branch"
    echo "--ftindex=$ftindex --ftindex_branch=$ftindex_branch"
    echo "--jemalloc=$jemalloc --jemalloc_branch=$jemalloc_branch"
    echo "--backup=$backup --backup_branch=$backup_branch"
    echo "--install_dir=$install_dir"
}

function github_clone() {
    local repo=$1; local branch=$2
    echo git clone git@github.com:Tokutek/$repo
    echo 'if [ $? != 0 ] ; then exit 1; fi'
    echo pushd $repo
    echo 'if [ $? != 0 ] ; then exit 1; fi'
    if [ -z $git_tag ] ; then
        echo git checkout $branch
    else
        echo git checkout $git_tag
    fi
    echo 'if [ $? != 0 ] ; then exit 1; fi'
    echo popd
}

shopt -s compat31 2>/dev/null

git_tag=
mysql=mysql
mysql_branch=5.5.30
jemalloc=jemalloc
jemalloc_branch=3.3.1
ftengine=ft-engine
ftengine_branch=master
ftindex=ft-index
ftindex_branch=master
backup=backup-community
backup_branch=master
install_dir=mysql

while [ $# -ne 0 ] ; do
    arg=$1; shift
    if [[ $arg =~ --(.*)=(.*) ]] ; then
        eval ${BASH_REMATCH[1]}=${BASH_REMATCH[2]};
    else
        usage; exit 1;
    fi
done

echo '# setup environment variables'
echo install_dir=\$PWD/$install_dir
echo mkdir \$install_dir-build \$install_dir
echo 'if [ $? != 0 ] ; then exit 1; fi'
echo cd \$install_dir-build

echo '# checkout the fractal tree'
github_clone $ftindex $ftindex_branch
github_clone $jemalloc $jemalloc_branch
echo pushd $ftindex/third_party
echo 'if [ $? != 0 ] ; then exit 1; fi'
echo ln -s ../../$jemalloc $jemalloc
echo 'if [ $? != 0 ] ; then exit 1; fi'
echo popd

echo '# checkout mysql'
github_clone $mysql $mysql_branch

echo '# checkout the community backup'
github_clone $backup $backup_branch

echo '# checkout the tokudb handlerton'
github_clone $ftengine $ftengine_branch

echo '# setup links'
echo pushd $ftengine/storage/tokudb
echo 'if [ $? != 0 ] ; then exit 1; fi'
echo ln -s ../../../$ftindex ft-index
echo 'if [ $? != 0 ] ; then exit 1; fi'
echo popd
echo pushd $mysql/storage
echo 'if [ $? != 0 ] ; then exit 1; fi'
echo ln -s ../../$ftengine/storage/tokudb tokudb
echo 'if [ $? != 0 ] ; then exit 1; fi'
echo popd
echo pushd $mysql
echo 'if [ $? != 0 ] ; then exit 1; fi'
echo ln -s ../$backup/backup toku_backup
echo 'if [ $? != 0 ] ; then exit 1; fi'
echo popd
echo pushd $mysql/scripts
echo 'if [ $? != 0 ] ; then exit 1; fi'
echo ln ../../$ftengine/scripts/tokustat.py
echo 'if [ $? != 0 ] ; then exit 1; fi'
echo ln ../../$ftengine/scripts/tokufilecheck.py
echo 'if [ $? != 0 ] ; then exit 1; fi'
echo popd

echo '# build in the mysql directory'
echo cd $mysql
echo mkdir build.debug
echo 'if [ $? != 0 ] ; then exit 1; fi'
echo cd build.debug
echo CC=gcc47 CXX=g++47 cmake .. -DBUILD_CONFIG=mysql_release -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=\$install_dir -DBUILD_TESTING=OFF
echo 'if [ $? != 0 ] ; then exit 1; fi'

echo '# install'
echo make -j4 install
echo 'if [ $? != 0 ] ; then exit 1; fi'

echo '# create a var directory so mysql does not complain'
echo 'cd $installdir'
echo 'if [ $? != 0 ] ; then exit 1; fi'
echo mkdir var
echo 'if [ $? != 0 ] ; then exit 1; fi'

echo '# install the databases in msyql'
echo scripts/mysql_install_db --defaults-file=\$HOME/my.cnf
echo 'if [ $? != 0 ] ; then exit 1; fi'
