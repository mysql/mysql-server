#!/bin/bash

set -e
set -u

pushd $(dirname $0)
    source ./common.sh
popd

PATH=$HOME/bin:$PATH

system=$(uname -s | tr '[:upper:]' '[:lower:]')
arch=$(uname -m | tr '[:upper:]' '[:lower:]')
makejobs=$(get_ncpus)

github_use_ssh=0
git_tag=HEAD
mysqlbuild=
mysql=mysql-5.5.30
do_s3=0
s3_build_bucket=tokutek-mysql-build
s3_release_bucket=tokutek-mysql
do_make_check=0
cc=gcc47
cxx=g++47
build_debug=0
build_type=community
build_tgz=1
build_rpm=0
tokudb_version=
tokudb_patches=1
cmake_build_type=RelWithDebInfo
mysql_tree=
ftengine_tree=
ftindex_tree=
jemalloc_version=3.3.0
jemalloc_tree=
backup_tree=

# parse arguments
while [ $# -gt 0 ] ; do
    arg=$1; shift
    if [[ $arg =~ --(.*)=(.*) ]] ; then
        k=${BASH_REMATCH[1]}; v=${BASH_REMATCH[2]}
        eval $k=$v
        if [ $k = mysqlbuild ] ; then
            parse_mysqlbuild $mysqlbuild
            if [ $? != 0 ] ; then exit 1; fi
        elif [ $k = mysql ] ; then
            parse_mysql $mysql
            if [ $? != 0 ] ; then exit 1; fi
        fi
    else
        usage; exit 1;
    fi
done

# compute more version names etc.
if [ -z $mysqlbuild ] ; then
    if [ -z $tokudb_version ] ; then
        if [ $git_tag = HEAD ] ; then
            tokudb_version=$(date +%s)
        elif [[ $git_tag =~ tokudb-(.*) ]] ; then
            tokudb_version=${BASH_REMATCH[1]}
        else
            tokudb_version=$git_tag
            git_tag=HEAD
        fi
    fi
    if [ $build_debug != 0 ] ; then
        if [ $cmake_build_type = RelWithDebInfo ] ; then cmake_build_type=Debug; fi
        tokudb_version=$tokudb_version-debug
    fi
    if [ $build_type = enterprise ] ; then
        tokudb_version=$tokudb_version-e
    fi
fi

# download all the source
if [ ! -d $mysql_distro ] ; then
    github_download Tokutek/$mysql_distro $(git_tree $git_tag $mysql_tree) $mysql_distro
fi

cd $mysql_distro

if [ ! -d toku_backup ] ; then
    github_download Tokutek/backup-$build_type $(git_tree $git_tag $backup_tree) backup-$build_type
    cp -r backup-$build_type/backup toku_backup
fi

if [ ! -d ft-engine ] ; then
    github_download Tokutek/ft-engine $(git_tree $git_tag $ftengine_tree) ft-engine

    cp -r ft-engine/storage/tokudb storage/

    mv mysql-test mysql-test-save
    cp -r ft-engine/mysql-test .
    cp -r mysql-test-save/* mysql-test
    rm -rf mysql-test-save

    cp -r ft-engine/scripts/* scripts/
fi

if [ ! -d storage/tokudb/ft-index ] ; then
    github_download Tokutek/ft-index $(git_tree $git_tag $ftindex_tree) storage/tokudb/ft-index
fi

if [ ! -d storage/tokudb/ft-index/third_party/jemalloc ] ; then
    github_download Tokutek/jemalloc $(git_tree $git_tag $jemalloc_tree) storage/tokudb/ft-index/third_party/jemalloc
fi

# append tokudb-specific version
if grep -qv tokudb VERSION ; then
    # append the tokudb version to the MYSQL_VERSION_EXTRA variable in the VERSION file
    sed -i "" -e"s/^MYSQL_VERSION_EXTRA=.*/&-tokudb-${tokudb_version}/" VERSION
fi

# prints a cmake command to eval
function generate_cmake_cmd () {
    local ft_revision=0x$(git ls-remote https://github.com/Tokutek/ft-index.git $git_tag | cut -c-7)

    echo -n CC=$cc CXX=$cxx cmake \
        -D BUILD_CONFIG=mysql_release \
        -D CMAKE_BUILD_TYPE=$cmake_build_type \
        -D CMAKE_TOKUDB_REVISION=$ft_revision \
        -D BUILD_TESTING=OFF \
        -D USE_GTAGS=OFF \
        -D USE_CTAGS=OFF \
        -D USE_ETAGS=OFF \
        -D USE_CSCOPE=OFF

    if [ $build_debug = 1 ] ; then
        echo -n " " \
            -D USE_VALGRIND=ON \
            -D TOKU_DEBUG_PARANOID=ON
    else
        echo -n " " \
            -D USE_VALGRIND=OFF \
            -D TOKU_DEBUG_PARANOID=OFF
    fi

    if [[ $mysql_distro =~ ^Percona ]] ; then
        echo -n " " \
            -D WITH_EMBEDDED_SERVER=OFF
    fi

    if [ $system = darwin ] ; then
        echo -n " " \
            -D WITH_SAFEMALLOC=OFF \
            -D WITH_SSL=system
    fi

    if [ $build_type = enterprise ] ; then
        echo -n " " \
            -D COMPILATION_COMMENT=\"TokuDB Enterprise Server \(GPL\)\"
    fi

    if [ $system = linux -a $build_rpm != 0 -a $mysql_distro = mariadb ] ; then
        linux_distro=linux
        if [ -f /etc/issue ] ; then
            if [[ "$(head -1 /etc/issue)" =~ "Red Hat Enterprise Linux Server release ([56])" ]] ; then
                linux_distro=rhel${BASH_REMATCH[1])
            fi
            if [[ "$(head -1 /etc/issue)" =~ "CentOS release ([56])" ]] ; then
                linux_distro=centos${BASH_REMATCH[1])
            fi
        fi
        echo -n " " -D RPM=$linux_distro
    fi
}

mkdir -p build.$cmake_build_type
cd build.$cmake_build_type

# actually build
eval $(generate_cmake_cmd) ..
make package -j$makejobs

if [ $system = linux -a $build_rpm != 0 -a $mysql_distro != mariadb ] ; then
    echo 1>&2 "I don't know how to build rpms for mysql yet."
    exit 1
fi
