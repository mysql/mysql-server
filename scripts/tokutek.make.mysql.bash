#!/bin/bash

function usage() {
    echo "make mysql and copy the tarballs to an amazon s3 bucket"
    return 1
}

# copy build files to amazon s3
function copy_to_s3() {
    local s3_build_bucket=$1; shift
    local mysql_distro=$1; shift
    local ts=$(date +%s)
    local ymd=$(date +%Y%m%d -d @$ts)
    local exitcode=0; local r=0
    for f in $(find . -maxdepth 1 \( -name $mysql_distro-$mysql_version'*.tar.gz*' -o -name $mysql_distro-$mysql_version'*.rpm*' \) ) ; do
        f=$(basename $f)
        echo `date` s3put $s3_build_bucket $f
        s3put $s3_build_bucket $f $f
        r=$?
        # index the file by date
        echo `date` s3put $s3_build_bucket $f $r
        if [ $r != 0 ] ; then exitcode=1; fi
        s3put $s3_build_bucket-date $ymd/$f /dev/null
        r=$?
        echo `date` s3put $s3_build_bucket-date $ymd/$f $r
        if [ $r != 0 ] ; then exitcode=1; fi
    done
    if [[ $git_tag =~ tokudb-.* ]] ; then
        s3mkbucket $git_tag
        if [ $r != 0 ] ; then 
            exitcode=1
        else
            for f in $(find . -maxdepth 1 \( -name $mysql_distro-$mysql_version'*.tar.gz*' -o -name $mysql_distro-$mysql_version'*.rpm*' \) ) ; do
                f=$(basename $f)
                echo `date` s3copykey $git_tag $f
                s3copykey $git_tag $f $s3_build_bucket $f
                r=$?
                echo `date` s3copykey $git_tag $f $r
                if [ $r != 0 ] ; then exitcode=1; fi
            done
        fi
    fi
    test $exitcode = 0
}

mysqlbuild=
s3=1
s3_build_bucket=tokutek-mysql-build
system=$(uname -s | tr '[:upper:]' '[:lower:]')
arch=$(uname -m | tr '[:upper:]' '[:lower:]')

pushd $(dirname $0)
source ./common.sh
popd

exitcode=0
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

# parse the mysqlbuild string
parse_mysqlbuild $mysqlbuild
if [ $? != 0 ] ; then exit 1; fi

# make the build dir
build_dir=build-tokudb-$tokudb_version
if [ -d builds ] ; then build_dir=builds/$build_dir; fi
if [ ! -d $build_dir ] ; then mkdir $build_dir; fi
pushd $build_dir
if [ $? != 0 ] ; then exit 1; fi

# make mysql
bash -x $HOME/github/tokudb-engine/scripts/make.mysql.bash $make_args
if [ $? != 0 ] ; then exitcode=1; fi

# generate md5 sums
for f in $(find $mysql_distro-$mysql_version/build.* -maxdepth 1 \( -name '*.tar.gz' -o -name '*.rpm' \) ) ; do
    newf=$(basename $f)
    ln $f $newf
    if [ $? != 0 ] ; then exitcode=1; fi
    md5sum $newf >$newf.md5
    if [ $? != 0 ] ; then exitcode=1; fi
done

# copy to s3
if [ $s3 != 0 ] ; then
    copy_to_s3 $s3_build_bucket $mysql_distro
    if [ $? != 0 ] ; then exitcode=1; fi
fi

popd

exit $exitcode
