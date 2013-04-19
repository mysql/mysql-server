#!/usr/bin/env bash

function usage() {
    echo "generate a script that builds a debug mysql from subversion"
}

shopt -s compat31 2>/dev/null

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

echo export TOKUFRACTALTREE=\$builddir/ft-index/install.debug
echo export TOKUFRACTALTREE_LIBNAME=tokudb
echo export TOKUPORTABILITY_LIBNAME=tokuportability
echo export TOKUDB_VERSION=0

echo '# checkout the fractal tree'
echo cd \$builddir
echo git clone git@github.com:Tokutek/jemalloc
echo 'if [ $? != 0 ] ; then exit 1; fi'
echo git clone git@github.com:Tokutek/ft-index
echo 'if [ $? != 0 ] ; then exit 1; fi'

echo '# build the fractal tree'
echo cd \$builddir/ft-index
echo mkdir build.debug
echo cd build.debug
echo CC=gcc47 CXX=g++47 cmake -DCMAKE_INSTALL_PREFIX=\$TOKUFRACTALTREE -D BUILD_TESTING=OFF -D CMAKE_BUILD_TYPE=Debug -D JEMALLOC_SOURCE_DIR=\$builddir/jemalloc ..
echo 'if [ $? != 0 ] ; then exit 1; fi'
echo make install
echo 'if [ $? != 0 ] ; then exit 1; fi'

echo '# checkout mysql'
echo cd \$builddir
echo git clone git@github.com:Tokutek/mysql
echo 'if [ $? != 0 ] ; then exit 1; fi'

echo '# checkout the community backup'
echo cd \$builddir
echo git clone git@github.com:Tokutek/backup-community
echo 'if [ $? != 0 ] ; then exit 1; fi'

echo '# checkout the tokudb handlerton'
echo cd \$builddir
echo git clone git@github.com:Tokutek/ft-engine
echo 'if [ $? != 0 ] ; then exit 1; fi'

echo 'pushd mysql/storage; ln -s ../../ft-engine/storage/tokudb tokudb; popd'
echo 'pushd mysql; ln -s ../backup-community/backup toku_backup; popd'

echo '# build in the mysql directory'
echo cd \$builddir/mysql
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
