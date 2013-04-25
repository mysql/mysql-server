#!/usr/bin/env bash

# build mysql with the tokudb storage engine and the fractal tree
#
# parse arguments
# checkout jemalloc
# build jemalloc
# checkout the fractal tree
# build the fractal tree
# checkout the mysql source
# checkout the tokudb storage engine
# checkout the tokudb backup source
# generate a mysql build script
# generate a mysql source tarball
# execute the build script

shopt -s compat31 2> /dev/null

function usage() {
    echo "make.mysql.bash - build mysql with the tokudb storage engine and the fractal tree"
    echo "--git_tag=$git_tag"
    echo "--mysqlbuild=$mysqlbuild"
    echo "--tokudb_version=$tokudb_version"
    echo "--mysql=$mysql --mysql_distro=$mysql_distro --mysql_version=$mysql_version"
    echo "--build_type=$build_type"
    echo "--build_debug=$build_debug"
    echo "--build_tgz=$build_tgz"
    echo "--build_rpm=$build_rpm"
    echo "--cc=$cc --cxx=$cxx"
    echo "--do_s3=$do_s3"
    echo "--do_make_check=$do_make_check"
    echo 
    echo "community release builds using the tokudb-7.0.1 git tag"
    echo "    make.mysql.bash --mysqlbuild=mysql-5.5.30-tokudb-7.0.1-linux-x86_64"
    echo "    make.mysql.bash --mysqlbuild=mariadb-5.5.30-tokudb-7.0.1-linux-x86_64"
    echo "    make.mysql.bash --git_tag=tokudb-7.0.1 --mysql=mysql-5.5.30"
    echo
    echo "community debug builds using the tokudb-7.0.1 git tag"
    echo "    make.mysql.bash --mysqlbuild=mysql-5.5.30-tokudb-7.0.1-debug-linux-x86_64"
    echo
    echo "enterprise release builds at the HEAD of the repos"
    echo "    make.mysql.bash --mysqlbuild=mysql-5.5.30-tokudb-test-e-linux-x86_64"
    echo 
    echo "community release builds of a branch"
    echo "    make.mysql.bash --mysql=mysql-5.5.30 --mysql_tree=<your mysql tree name> --ftengine_tree=<your ft-engine tree name> --tokudb_version=<your test string>>"
    return 1
}

function retry() {
    local cmd
    local retries
    local exitcode
    cmd=$*
    let retries=0
    while [ $retries -le 10 ] ; do
        echo `date` $cmd
        bash -c "$cmd"
        exitcode=$?
        echo `date` $cmd $exitcode $retries
        let retries=retries+1
        if [ $exitcode -eq 0 ] ; then break; fi
        sleep 10
    done
    test $exitcode = 0
}

function github_download() {
    repo=$1; shift
    rev=$1; shift
    dest=$1; shift
    mkdir $dest

    if [ ! -z $github_token ] ; then
        retry curl \
            --header "Authorization:\\ token\\ $github_token" \
            --location https://api.github.com/repos/$repo/tarball/$rev \
            --output $dest.tar.gz
        if [ $? != 0 ] ; then return; fi
        tar --extract \
            --gzip \
            --directory=$dest \
            --strip-components=1 \
            --file $dest.tar.gz
        if [ $? != 0 ] ; then return; fi
        rm -f $dest.tar.gz
    elif [ ! -z $github_user ] ; then
        retry curl \
            --user $github_user \
            --location https://api.github.com/repos/$repo/tarball/$rev \
            --output $dest.tar.gz
        if [ $? != 0 ] ; then return; fi
        tar --extract \
            --gzip \
            --directory=$dest \
            --strip-components=1 \
            --file $dest.tar.gz
        if [ $? != 0 ] ; then return; fi
        rm -f $dest.tar.gz
    else
        tempdir=$(mktemp -d -p $PWD)
        retry git clone git@github.com:${repo}.git $tempdir
        if [ $? != 0 ] ; then return; fi
        pushd $tempdir
        if [ $? != 0 ] ; then return; fi
        git checkout $rev
        if [ $? != 0 ] ; then return; fi
        popd

        # export the right branch or tag
        (cd $tempdir ;
            git archive \
                --format=tar \
                $rev) | \
            tar --extract \
                --directory $dest
        if [ $? != 0 ] ; then return; fi
        rm -rf $tempdir
    fi
}

# returns b if b is defined else returns a
function git_tree() {
    local a=$1; shift
    local b=$1; shift
    if [ ! -z $b ] ; then
        echo $b
    else
        echo $a;
    fi
}

# compute the number of cpus in this system.  used to parallelize the build.
function get_ncpus() {
    if [ -f /proc/cpuinfo ]; then
        grep bogomips /proc/cpuinfo | wc -l
    elif [ $system = darwin ] ; then
        sysctl -n hw.ncpu
    else
        echo 1
    fi
}

function build_jemalloc() {
    if [ ! -d jemalloc ] ; then
        # get the jemalloc repo jemalloc
        github_download Tokutek/jemalloc $(git_tree $git_tag $jemalloc_tree) jemalloc
        if [ $? != 0 ] ; then exit 1; fi
        pushd jemalloc
        if [ $? != 0 ] ; then exit 1; fi
        CC=$cc ./configure --with-private-namespace=jemalloc_
        if [ $? != 0 ] ; then exit 1; fi
        make
        if [ $? != 0 ] ; then exit 1; fi
        popd
    fi
}

# check out the fractal tree source from subversion, build it, and make the fractal tree tarballs
function build_fractal_tree() {
    if [ ! -d ft-index ] ; then

        # get the ft-index repo
        github_download Tokutek/ft-index $(git_tree $git_tag $ftindex_tree) ft-index
        if [ $? != 0 ] ; then exit 1; fi

        # get the commit id of the ft-index repo
        local ft_revision=0
        if [ ! -z $github_token ] ; then
            local ft_index_ls
            ft_index_ls=$(git ls-remote https://$github_token:x-oauth-basic@github.com/Tokutek/ft-index.git $git_tag)
            if [ $? != 0 ] ; then exit 1; fi
            local ft_revision_hex
            ft_revision_hex=0x$(echo $ft_index_ls | cut -c-7)
            let ft_revision=$ft_revision_hex
        fi

        pushd ft-index
        if [ $? != 0 ] ; then exit 1; fi

        local ft_build_type=""
        local use_valgrind=""
        local debug_paranoid=""
        if [[ $build_debug = 1 ]]; then
            ft_build_type=Debug
            use_valgrind="ON"
            debug_paranoid="ON"
        else
            ft_build_type=Release
            use_valgrind="OFF"
            debug_paranoid="OFF"
        fi

        mkdir -p build
        pushd build
        if [ $? != 0 ] ; then exit 1; fi
        
        CC=$cc CXX=$cxx cmake \
            -D LIBTOKUDB=$tokufractaltree \
            -D LIBTOKUPORTABILITY=$tokuportability \
            -D CMAKE_TOKUDB_REVISION=$ft_revision \
            -D CMAKE_BUILD_TYPE=$ft_build_type \
            -D JEMALLOC_SOURCE_DIR=../../jemalloc \
            -D CMAKE_INSTALL_PREFIX=../../$tokufractaltreedir \
            -D BUILD_TESTING=OFF \
            -D USE_GTAGS=OFF \
            -D USE_CTAGS=OFF \
            -D USE_ETAGS=OFF \
            -D USE_CSCOPE=OFF \
            -D USE_VALGRIND=$use_valgrind \
            -D TOKU_DEBUG_PARANOID=$debug_paranoid \
            ..
        make install -j$makejobs
        if [ $? != 0 ] ; then exit 1; fi

        popd # build

        popd # ft-index

        pushd $tokufractaltreedir/examples
        if [ $? != 0 ] ; then exit 1; fi

        # test the examples
        sed -ie "s/LIBTOKUDB = tokudb/LIBTOKUDB = $tokufractaltree/" Makefile 
        sed -ie "s/LIBTOKUPORTABILITY = tokuportability/LIBTOKUPORTABILITY = $tokuportability/" Makefile
        if [ $system = darwin ] ; then
            DYLD_LIBRARY_PATH=$PWD/../lib:$DYLD_LIBRARY_PATH make check CC=$cc
            exitcode=$?
        else
            if [ $do_make_check != 0 ] ; then
                make check CC=$cc
                exitcode=$?
            else
                exitcode=0
            fi
        fi
        echo `date` make check examples $tokufractaltree $exitcode
        if [ $exitcode != 0 ] ; then exit 1; fi
        make clean
        popd
        
        # make tarballs
        tar czf $tokufractaltreedir.tar.gz $tokufractaltreedir
        md5sum $tokufractaltreedir.tar.gz >$tokufractaltreedir.tar.gz.md5
        md5sum --check $tokufractaltreedir.tar.gz.md5
    fi
}

function generate_cmake_cmd() {
    local mysqlsrc=$1; shift
    local extra_flags=$*
    if [[ $mysqlsrc =~ ^Percona ]] ; then extra_flags=-DWITH_EMBEDDED_SERVER=OFF; fi

    echo -n cmake .. $extra_flags -DBUILD_CONFIG=mysql_release -DCMAKE_BUILD_TYPE=$cmake_build_type
    if [ $system = linux ] ; then
        echo -n " " \
            -DTOKU_JEMALLOC_LIBRARY=\"-Wl,--whole-archive \$TOKUFRACTALTREE/lib/libjemalloc.a -Wl,-no-whole-archive\"
    elif [ $system = darwin ] ; then
        echo -n " " \
            -DWITH_SAFEMALLOC=OFF -DWITH_SSL=system \
            -DTOKU_JEMALLOC_LIBRARY=\"-Wl,-force_load -Wl,\$TOKUFRACTALTREE/lib/libjemalloc.a\"
    else
        exit 1
    fi
    if [ $build_type = enterprise ] ; then
        echo -n " " -DCOMPILATION_COMMENT=\"TokuDB Enterprise Server \(GPL\)\"
    fi
    echo
}

# generate a script that builds from the mysql src tarball and the fractal tree tarball
function generate_build_from_src() {
    local mysqlsrc=$1; local tokufractaltreedir=$2
    pushd $mysqlsrc >/dev/null 2>&1
    if [ $? = 0 ] ; then
        echo '#/usr/bin/env bash'

        # defaults
        echo 'makejobs=1'
        echo 'cc=gcc47'
        echo 'cxx=g++47'

        # parse arguments
        echo 'while [ $# -gt 0 ] ; do'
        echo '    arg=$1; shift'
        echo '    if [[ $arg =~ --(.*)=(.*) ]] ; then'
        echo '        eval ${BASH_REMATCH[1]}=${BASH_REMATCH[2]}'
        echo '    else'
        echo '        break'
        echo '    fi'
        echo 'done'
        
        # check tarballs
        echo md5sum --check $mysqlsrc.tar.gz.md5
        echo 'if [ $? != 0 ] ; then exit 1; fi'
        echo md5sum --check $tokufractaltreedir.tar.gz.md5
        echo 'if [ $? != 0 ] ; then exit 1; fi'
        
        echo "if [ ! -d $mysqlsrc ] ; then"
        echo "    tar xzf $mysqlsrc.tar.gz"
        echo '    if [ $? != 0 ] ; then exit 1; fi'
        echo fi
        echo "if [ ! -d $tokufractaltreedir ] ;then"
        echo "    tar xzf $tokufractaltreedir.tar.gz"
        echo '    if [ $? != 0 ] ; then exit 1; fi'
        echo fi

        # setup environment variables
        echo export TOKUDB_VERSION=$tokudb_version
        echo export TOKUFRACTALTREE='$PWD'/$tokufractaltreedir
        echo export TOKUDB_PATCHES=$tokudb_patches
        echo export TOKUFRACTALTREE_LIBNAME=${tokufractaltree}_static
        echo export TOKUPORTABILITY_LIBNAME=${tokuportability}_static
        echo 'export CC=$cc; export CXX=$cxx'

        echo "pushd $mysqlsrc"
        echo 'if [ $? != 0 ] ; then exit 1; fi'

        if [ ! -f CMakeLists.txt ] ; then exit 1; fi

        if [ $build_tgz != 0 ] ; then
            echo "mkdir build.$cmake_build_type; pushd build.$cmake_build_type"
            echo 'if [ $? != 0 ] ; then exit 1; fi'

            cmd=$(generate_cmake_cmd $mysqlsrc)
            echo $cmd
            echo 'if [ $? != 0 ] ; then exit 1; fi'

            # make binary distribution package
            echo 'make package -j$makejobs'
            echo 'if [ $? != 0 ] ; then exit 1; fi'

            # fixup tarball name mismatch (if any)
            echo "if [ ! -f $mysqldir.tar.gz ] ; then"
            echo '    oldtb=$(ls *.gz)'
            echo '    if [[ $oldtb =~ (.*).tar.gz ]] ; then oldmysqldir=${BASH_REMATCH[1]}; fi'
            echo '    tar xzf $oldtb'
            echo '    if [ $? != 0 ] ; then exit 1; fi'
            echo '    mv $oldmysqldir' $mysqldir
            echo "    tar czf $mysqldir.tar.gz $mysqldir"
            echo '    if [ $? != 0 ] ; then exit 1; fi'
            echo 'fi'

            # add ft files to the mysql tarball
            echo tar xzf $mysqldir.tar.gz
            echo 'if [ $? != 0 ] ; then exit 1; fi'
            # add ftdump to mysql tarball
            echo "cp ../../$tokufractaltreedir/bin/ftdump $mysqldir/bin/tokuftdump"
            echo 'if [ $? != 0 ] ; then exit 1; fi'

            # cleanup
            echo rm $mysqldir.tar.gz
            echo 'if [ $? != 0 ] ; then exit 1; fi'
            echo tar czf $mysqldir.tar.gz $mysqldir
            echo 'if [ $? != 0 ] ; then exit 1; fi'

            # generate the md5 file
            echo "if [ -f $mysqldir.tar.gz ] ; then"
            echo "    md5sum $mysqldir.tar.gz >$mysqldir.tar.gz.md5"
            echo '    if [ $? != 0 ] ; then exit 1; fi'
            echo 'fi'

            echo popd
        fi

        # make RPMs
        if [ $system = linux -a $build_rpm != 0 ]; then
            echo "mkdir build.${cmake_build_type}.rpms; pushd build.${cmake_build_type}.rpms"
            echo 'if [ $? != 0 ] ; then exit 1; fi'

            if [[ $mysqlsrc =~ ^mariadb ]] ; then
                echo linux_distro=linux
                echo 'if [ -f /etc/issue ] ; then'
                echo '    if [[ "$(head -1 /etc/issue)" =~ Red\ Hat\ Enterprise\ Linux\ Server\ release\ ([56]) ]] ; then linux_distro=rhel${BASH_REMATCH[1]}; fi'
                echo '    if [[ "$(head -1 /etc/issue)" =~ CentOS\ release\ ([56]) ]] ; then linux_distro=centos${BASH_REMATCH[1]}; fi'
                echo fi

                cmd=$(generate_cmake_cmd $mysqlsrc -DRPM=\$linux_distro)
                echo $cmd
                echo 'if [ $? != 0 ] ; then exit 1; fi'

                # make binary distribution package
                echo 'make package -j$makejobs'
                echo 'if [ $? != 0 ] ; then exit 1; fi'
            else
                echo 'mkdir -p {tmp,BUILD,RPMS,SOURCES,SPECS,SRPMS}'

                # split myslqsrc into components
                echo "if [[ $mysqlsrc =~ (.*)-([0-9]+\.[0-9]+\.[0-9]+.*)-tokudb-(.*) ]] ; then"
                echo '    mysql_distro=${BASH_REMATCH[1]}'
                echo '    mysql_version=${BASH_REMATCH[2]}'
                echo '    mysql_distro_version=${mysql_distro}-${mysql_version}'
                echo '    tokudb_distro=tokudb'
                echo '    tokudb_version=${BASH_REMATCH[3]}'
                echo '    tokudb_distro_version=${tokudb_distro}-${tokudb_version}'
                echo else
                echo '    exit 1'
                echo fi

                # copy the source tarball
                echo "cp ../../$mysqlsrc.tar.gz SOURCES/$mysqlsrc.tar.gz"
                echo 'if [ $? != 0 ] ; then exit 1; fi'

                # hack on the spec file
                echo 'specfile=${mysql_distro}.${mysql_version}.spec'
                echo 'cp ../support-files/$specfile SPECS/$specfile'
                echo 'if [ $? != 0 ] ; then exit 1; fi'
                echo 'sed -i -e"s/^%define mysql_version.*/&-${tokudb_distro_version}/" SPECS/$specfile'
                echo 'sed -i -e"s/^%define release.*/%define release $(echo ${tokudb_distro_version}|tr - .)/" SPECS/$specfile'
                # echo 'sed -i -e"s/^\(.*-DMYSQL_SERVER_SUFFIX=.*\)$/& -DEXTRA_VERSION=-${tokudb_distro_version}/" SPECS/$specfile'
                
                # include README-TOKUDB in the RPM
                echo 'sed -i -e"s/README/& %{src_dir}\/README-TOKUDB/" SPECS/$specfile'

                # add jemalloc to the linker flags
                echo 'sed -i -e"s/^\(.*-DMYSQL_SERVER_SUFFIX=.*\)$/& -DTOKU_JEMALLOC_LIBRARY=\"-Wl,--whole-archive \$\{TOKUFRACTALTREE\}\/lib\/libjemalloc.a -Wl,-no-whole-archive\"/" SPECS/$specfile'

                # add the hot backup lib to the server package file list
                echo 'sed -i -e"s/%{_datadir}\/mysql\/$/&\n%attr(755, root, root) %{_libdir}\/libHotBackup*.so/" SPECS/$specfile'

                # build generic linux RPMs
                echo 'export MYSQL_BUILD_MAKE_JFLAG=-j$makejobs'
                echo 'rpmbuild -v --define="_topdir $PWD" --define="_tmppath $PWD/tmp" --define="src_base $mysql_distro" -ba SPECS/$specfile'
                echo 'if [ $? != 0 ] ; then exit 1; fi'

                # move the rpms
                echo 'pushd RPMS/$(uname -m)'
                echo 'if [ $? != 0 ] ; then exit 1; fi'
                echo 'mv *.rpm ../..'
                echo 'if [ $? != 0 ] ; then exit 1; fi'
                echo 'popd'
                echo 'pushd SRPMS'
                echo 'if [ $? != 0 ] ; then exit 1; fi'
                echo 'mv *.rpm ..'
                echo 'if [ $? != 0 ] ; then exit 1; fi'
                echo 'popd'
            fi

            # generate the md5 file
            echo 'rpmfiles=$(ls *.rpm)'
            echo 'for x in $rpmfiles; do'
            echo '    md5sum $x >$x.md5'
            echo '    if [ $? != 0 ] ; then exit 1; fi'
            echo 'done'

            echo popd
        fi
        
        echo popd

        popd >/dev/null 2>&1
    fi
}

# checkout the mysql source, generate a build script, and make the mysql source tarball
function build_mysql_src() {
    if [ ! -d $mysqlsrc ] ; then

        # get the mysql repo
        if [ ! -d $mysql_distro ] ; then
            github_download Tokutek/$mysql_distro $(git_tree $git_tag $mysql_tree) $mysqlsrc
            if [ $? != 0 ] ; then exit 1; fi
        fi

	# get the hot backup source
        if [ ! -d backup-$build_type ] ; then
            github_download Tokutek/backup-$build_type $(git_tree $git_tag $backup_tree) backup-$build_type
            if [ $? != 0 ] ; then exit 1; fi
        fi

        # get the ft-engine repo
        if [ ! -d ft-engine ] ; then
            github_download Tokutek/ft-engine $(git_tree $git_tag $ftengine_tree) ft-engine
            if [ $? != 0 ] ; then exit 1; fi
        fi

        # append the tokudb version to the MYSQL_VERSION_EXTRA variable in the VERSION file
        sed -i "" -e"s/^MYSQL_VERSION_EXTRA=.*/&-tokudb-${tokudb_version}/" $mysqlsrc/VERSION

        # add the backup source
        mkdir $mysqlsrc/toku_backup
        pushd backup-$build_type/backup
        if [ $? != 0 ] ; then exit 1; fi
        copy_files=$(ls CMakeLists.txt *.h *.cc *.cmake export.map)
        cp $copy_files ../../$mysqlsrc/toku_backup
        if [ $? != 0 ] ; then exit 1; fi
        popd

        # add the tokudb storage engine source
        mkdir -p $mysqlsrc/storage/tokudb
        pushd ft-engine/storage/tokudb
        if [ $? != 0 ] ; then exit 1; fi
        copy_files=$(ls CMakeLists.txt *.h *.cc)
        cp $copy_files ../../../$mysqlsrc/storage/tokudb
        if [ $? != 0 ] ; then exit 1; fi
        popd

        # merge the common mysql tests into the source
        mv $mysqlsrc/mysql-test $mysqlsrc/mysql-test-save
        cp -r ft-engine/mysql-test $mysqlsrc
        cp -r $mysqlsrc/mysql-test-save/* $mysqlsrc/mysql-test
        rm -rf $mysqlsrc/mysql-test-save

        # add the common scripts into the source
        cp -r ft-engine/scripts $mysqlsrc

        # generate the tokudb.build.bash script from the mysql src and the fractal tree tarballs
        generate_build_from_src $mysqlsrc $tokufractaltreedir >$mysqlsrc/scripts/tokudb.build.bash

        # add README-TOKUDB
        if [ ! -f $mysqlsrc/README-TOKUDB ] ; then
            cp ft-index/README-TOKUDB $mysqlsrc
        fi
        sed -i -e's/FILES README DESTINATION/FILES README README-TOKUDB DESTINATION/' $mysqlsrc/CMakeLists.txt

        # make the mysql src tarball
        tar czf $mysqlsrc.tar.gz $mysqlsrc
        if [ $? != 0 ] ; then exit 1; fi
        md5sum $mysqlsrc.tar.gz >$mysqlsrc.tar.gz.md5
        if [ $? != 0 ] ; then exit 1; fi
    fi
}

# make the mysql release tarball 
# by executing the build script in the mysql source tarball and compiling with the fractal tree tarball
function build_mysql_release() {
    if [ ! -d $mysqlbuild ] ; then

        mkdir $mysqlbuild
        pushd $mysqlbuild

        # create links to the mysql source and fractal tree tarballs
        ln $basedir/$mysqlsrc.tar.gz
        ln $basedir/$mysqlsrc.tar.gz.md5
        ln $basedir/$tokufractaltreedir.tar.gz
        ln $basedir/$tokufractaltreedir.tar.gz.md5

        # extract the build script
        tar xzf $mysqlsrc.tar.gz $mysqlsrc/scripts/tokudb.build.bash
        if [ $? != 0 ] ; then exit 1; fi
        cp $mysqlsrc/scripts/tokudb.build.bash .
        if [ $? != 0 ] ; then exit 1; fi
        rm -rf $mysqlsrc

        # execute the build script
        bash -x tokudb.build.bash --makejobs=$makejobs --cc=$cc --cxx=$cxx
        if [ $? != 0 ] ; then exit 1; fi

        # move the build files
        pushd $mysqlsrc/build.${cmake_build_type}
        if [ $? = 0 ] ; then
            cp $mysqldir.tar.gz* $basedir; if [ $? != 0 ] ; then exit 1; fi
            popd
        fi
        if [ $build_rpm != 0 ] ; then
            pushd $mysqlsrc/build.${cmake_build_type}.rpms
            if [ $? = 0 ] ; then
                rpmfiles=$(ls *.rpm *.rpm.md5)
                for x in $rpmfiles; do cp $x $basedir; if [ $? != 0 ] ; then  exit 1; fi; done
                popd
            fi
        fi

        popd
    fi
}

# split mysql into mysql_distro and mysql_version
function parse_mysql() {
    local mysql=$1
    if [[ $mysql =~ ^(mysql|mariadb)-(.*)$ ]] ; then
        mysql_distro=${BASH_REMATCH[1]}
        mysql_version=${BASH_REMATCH[2]}
        if [[ $mysql_distro = mysql && $mysql_version =~ ^5.6 ]] ; then mysql_distro=mysql56; fi
        exitcode=0
    else
        exitcode=1
    fi
    test $exitcode = 0
}

# parse a mysqlbuild string and extract the mysql_distro, mysql_version, tokudb_distro, tokudb_version, target_system, and target_arch
# compute build_type, build_debug, and git_tag
function parse_mysqlbuild() {
    local mysqlbuild=$1
    local exitcode=0
    if [[ $mysqlbuild =~ ((mysql|mariadb|percona\-server)-(.*))-((tokudb)-(.*))-(linux|darwin)-(x86_64|i386) ]] ; then
        # extract distros and versions from the components
        mysql=${BASH_REMATCH[1]}
        mysql_distro=${BASH_REMATCH[2]}
        mysql_version=${BASH_REMATCH[3]}
        tokudb=${BASH_REMATCH[4]}
        tokudb_distro=${BASH_REMATCH[5]}
        tokudb_version=${BASH_REMATCH[6]}
        target_system=${BASH_REMATCH[7]}
        target_arch=${BASH_REMATCH[8]}
        # verify targets
        if [ $target_system != $system ] ; then exitcode=1; fi
        if [ $target_arch != $arch ] ; then exitcode=1; fi

        local temp_tokudb_version=$tokudb_version
        # decode enterprise
        if [[ $temp_tokudb_version =~ (.*)-e$ ]] ; then
            build_type=enterprise
            temp_tokudb_version=${BASH_REMATCH[1]}
        else
            build_type=community
        fi
        # decode debug
        if [[ $temp_tokudb_version =~ (.*)-debug$ ]] ; then
            build_debug=1
            temp_tokudb_version=${BASH_REMATCH[1]}
            cmake_build_type=Debug
        else
            build_debug=0
        fi
        # set tag or HEAD
        if [[ $temp_tokudb_version =~ ^([0-9]+)\\.([0-9]+)\\.([0-9]+)$ ]] ; then
            git_tag=tokudb-$temp_tokudb_version
        else
            git_tag=HEAD
            # setup _tree defaults
            if [ -z $mysql_tree ] ; then mysql_tree=$mysql_version; fi
            if [ -z $jemalloc_tree ] ; then jemalloc_tree=$jemalloc_version; fi
        fi
        # 5.6 is temp in  another repo
        if [[ $mysql_distro = mysql && $mysql_version =~ ^5.6 ]] ; then mysql_distro=mysql56; fi
    else
        exitcode=1
    fi
    test $exitcode = 0
}

PATH=$HOME/bin:$PATH

system=$(uname -s | tr '[:upper:]' '[:lower:]')
arch=$(uname -m | tr '[:upper:]' '[:lower:]')
makejobs=$(get_ncpus)

git_tag=HEAD
mysqlbuild=
mysql=mysql-5.5.30
do_s3=1
do_make_check=1
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

while [ $# -gt 0 ] ; do
    arg=$1; shift
    if [[ $arg =~ --(.*)=(.*) ]] ; then
        k=${BASH_REMATCH[1]}; v=${BASH_REMATCH[2]}
        eval $k=$v
        case $k in
        mysqlbuild)
            parse_mysqlbuild $mysqlbuild
            if [ $? != 0 ] ; then exit 1; fi
            ;;
        mysql)
            parse_mysql $mysql
            if [ $? != 0 ] ; then exit 1; fi
        esac
    else
        usage; exit 1;
    fi
done

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

builddir=build-tokudb-$tokudb_version
if [ -d builds ] ; then builddir=builds/$builddir; fi
if [ ! -d $builddir ] ; then mkdir $builddir; fi
pushd $builddir

basedir=$PWD

# build the jemalloc library
build_jemalloc

# build the fractal tree tarball
tokufractaltree=tokufractaltreeindex-$tokudb_version
tokuportability=tokuportability-$tokudb_version
tokufractaltreedir=$tokufractaltree-$system-$arch
build_fractal_tree

# build the mysql source tarball
mysqldir=$mysql-tokudb-$tokudb_version-$system-$arch
mysqlsrc=$mysql-tokudb-$tokudb_version
build_mysql_src

# build the mysql release tarball
mysqlbuild=$mysqldir-build
build_mysql_release

# copy to s3
if [ $do_s3 != 0 ] ; then
    files=$(ls $tokufractaltreedir.tar.gz* $mysqldir.tar.gz* $mysqlsrc.tar.gz* *.rpm*)
    for f in $files; do
        # add the file to the tokutek-mysql-build bucket
        echo `date` s3put $f
        s3put tokutek-mysql-build $f $f
        exitcode=$?
        # index the file by date
        echo `date` s3put tokutek-mysql-build $f $exitcode
        d=$(date +%Y%m%d)
        s3put tokutek-mysql-build-date $d/$f /dev/null
        exitcode=$?
        echo `date` s3put tokutek-mysql-build-date $d/$f $exitcode
    done
    if [[ $git_tag =~ tokudb-.* ]] ; then
        s3mkbucket tokutek-mysql-$git_tag
        if [ $? = 0 ] ; then
            files=$(ls $tokufractaltreedir.tar.gz* $mysqldir.tar.gz* $mysqlsrc.tar.gz* *.rpm*)
            for f in $files; do
                echo `date` s3copykey $git_tag $f
                s3copykey tokutek-mysql-$git_tag $f tokutek-mysql-build $f
                exitcode=$?
                echo `date` s3copykey $git_tag $f $exitcode
            done
        fi
    fi
fi

popd

exit 0
