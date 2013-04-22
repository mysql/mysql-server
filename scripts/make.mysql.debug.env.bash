#!/usr/bin/env bash

function usage() {
    echo "generate a script that builds a debug mysql from github repo's"
    echo "--git_tag=$git_tag"
    echo "--mysql=$mysql --mysql_branch=$mysql_branch"
    echo "--jemalloc=$jemalloc --jemalloc_branch=$jemalloc_branch"
    echo "--ftengine=$ftengine --ftengine_branch=$ftengine_branch"
    echo "--ftindex=$ftindex --ftindex_branch=$ftindex_branch"
    echo "--backup=$backup --backup_branch=$backup_branch"
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

while [ $# -ne 0 ] ; do
    arg=$1; shift
    if [[ $arg =~ --(.*)=(.*) ]] ; then
        eval ${BASH_REMATCH[1]}=${BASH_REMATCH[2]};
    else
        usage; exit 1;
    fi
done

echo '# setup environment variables'
echo builddir=\$PWD/mysql-build
echo installdir=\$PWD/mysql
echo mkdir \$builddir \$installdir
echo 'if [ $? != 0 ] ; then exit 1; fi'

echo export TOKUFRACTALTREE=\$builddir/$ftindex/install.debug
echo export TOKUFRACTALTREE_LIBNAME=tokudb
echo export TOKUPORTABILITY_LIBNAME=tokuportability
echo export TOKUDB_VERSION=0

echo '# checkout the fractal tree'
echo cd \$builddir
github_clone $jemalloc $jemalloc_branch
github_clone $ftindex $ftindex_branch

echo '# build the fractal tree'
echo cd \$builddir/ft-index
echo mkdir build.debug
echo cd build.debug
echo CC=gcc47 CXX=g++47 cmake -DCMAKE_INSTALL_PREFIX=\$TOKUFRACTALTREE -D BUILD_TESTING=OFF -D CMAKE_BUILD_TYPE=Debug -D JEMALLOC_SOURCE_DIR=\$builddir/$jemalloc ..
echo 'if [ $? != 0 ] ; then exit 1; fi'
echo make install
echo 'if [ $? != 0 ] ; then exit 1; fi'

echo '# checkout mysql'
echo cd \$builddir
github_clone $mysql $mysql_branch

echo '# checkout the community backup'
echo cd \$builddir
github_clone $backup $backup_branch

echo '# checkout the tokudb handlerton'
echo cd \$builddir
github_clone $ftengine $ftengine_branch

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
echo cd \$builddir/$mysql
echo export TOKUFRACTALTREE_LIBNAME=\${TOKUFRACTALTREE_LIBNAME}_static
echo export TOKUPORTABILITY_LIBNAME=\${TOKUPORTABILITY_LIBNAME}_static
echo mkdir build.debug
echo cd build.debug
echo CC=gcc47 CXX=g++47 cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=\$installdir ..
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
