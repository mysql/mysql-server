#!/bin/bash

set -e

function usage() {
    echo "make mysql and copy the tarballs to an amazon s3 bucket"
    return 1
}

# copy build files to amazon s3
function copy_to_s3() {
    local s3_build_bucket=$1; local s3_release_bucket=$2
    for f in $(ls *.tar.gz*); do
        echo `date` s3put $s3_build_bucket $f
        s3put $s3_build_bucket $f $f
        exitcode=$?
        # index the file by date
        echo `date` s3put $s3_build_bucket $f $exitcode
        d=$(date +%Y%m%d)
        s3put $s3_build_bucket-date $d/$f /dev/null
        exitcode=$?
        echo `date` s3put $s3_build_bucket-date $d/$f $exitcode
    done
    if [[ $git_tag =~ tokudb-.* ]] ; then
        s3mkbucket $s3_release_bucket-$git_tag
        if [ $? = 0 ] ; then
            for f in $(ls *.tar.gz*); do
                echo `date` s3copykey $s3_release_bucket-$git_tag $f
                s3copykey $s3_release_bucket-$git_tag $f tokutek-mysql-build $f
                exitcode=$?
                echo `date` s3copykey $s3_release_bucket-$git_tag $f $exitcode
            done
        fi
    fi
}

mysqlbuild=
s3=1
s3_build_bucket=tokutek-mysql-build
s3_release_bucket=tokutek-mysql
system=$(uname -s | tr '[:upper:]' '[:lower:]')
arch=$(uname -m | tr '[:upper:]' '[:lower:]')

pushd $(dirname $0)
source ./common.sh
popd

make_args=
while [ $# -gt 0 ] ; do
    arg=$1; shift
    if [[ $arg =~ --(s3.*)=(.*) ]] ; then
        eval ${BASH_REMATCH[1]}=${BASH_REMATCH[2]}
    elif [[ $arg =~ --(.*)=(.*) ]] ; then
        eval ${BASH_REMATCH[1]}=${BASH_REMATCH[2]}
        make_args="$make_args $arg"
    else
        usage; exit 1
    fi   
done

if [ -z $mysqlbuild ] ; then exit 1; fi

# parse the mysqlbuild
parse_mysqlbuild $mysqlbuild
if [ $? != 0 ] ; then exit 1; fi

# make the build dir
mkdir build-tokudb-$tokudb_version
if [ $? != 0 ] ; then exit 1; fi
pushd build-tokudb-$tokudb_version

# make mysql
bash -x $HOME/github/ft-engine/scripts/make.mysql.new.bash $make_args
if [ $? != 0 ] ; then exit 1; fi

# generate md5 sums
files=$(ls $mysql_distro/build.*/*.tar.gz)
for f in $(ls $mysql_distro/build.*/*.tar.gz) ; do
    newf=$(basename $f)
    ln $f $newf
    md5sum $newf >$newf.md5
done

# copy to s3
if [ $s3 != 0 ] ; then
    copy_to_s3 $s3_build_bucket $s3_release_bucket
    exitcode=$?
fi

popd

exit $exitcode
