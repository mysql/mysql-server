#!/usr/bin/env bash

shopt -s compat31 2> /dev/null

function usage() {
    echo "make.mysql.bash - build mysql with the fractal tree"
    echo "[--branch=$branch] [--revision=$revision] [--suffix=$suffix]"
    echo "[--mysql=$mysql]"
    echo "[--tokudb=$tokudb] [--tokudbengine=$tokudbengine]"
    echo "[--jemalloc=$jemalloc]"
    echo "[--cc=$cc --cxx=$cxx] [--ftcc=$ftcc --ftcxx=$ftcxx] [--debugbuild=$debugbuild] [--staticft=$staticft]"
    echo "[--rpm=$rpm] [--do_s3=$do_s3] [--do_make_check=$do_make_check]"
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

function get_ncpus() {
    if [ -f /proc/cpuinfo ]; then
        grep bogomips /proc/cpuinfo | wc -l
    elif [ $system = darwin ] ; then
        sysctl -n hw.ncpu
    else
        echo 1
    fi
}

function get_branchrevision() {
    local branch=$1; local revision=$2; local suffix=$3
    local branchrevision
    if [ $branch != "." ] ; then
        branchrevision=$(basename $branch)-$revision
    else
        branchrevision=$revision
    fi
    if [ "$suffix" != "." ] ; then
        branchrevision=$branchrevision-$suffix
    fi
    echo $branchrevision
}

function mysql_version() {
    local mysql=$1
    if [[ $mysql =~ (mysql|mysqlcom|mariadb)-(.*) ]] ; then 
        echo ${BASH_REMATCH[2]}
    fi
}

function build_jemalloc() {
    local jemalloc=$1; local branch=$2; local revision=$3; local branchrevision=$4

    if [ ! -d $jemalloc ] ; then
        retry svn export -q -r $revision $svnserver/$branch/$jemalloc $jemalloc
        if [ $? != 0 ] ; then exit 1; fi

        pushd $jemalloc
        if [ $? = 0 ] ; then
            CC=$cc ./configure --with-private-namespace=jemalloc_
            if [ $? != 0 ] ; then exit 1; fi
            make
            if [ $? != 0 ] ; then exit 1; fi
            popd
        fi
    fi
}

# check out the fractal tree source from subversion, build it, and make the fractal tree tarballs
function build_fractal_tree() {
    if [ ! -d $tokufractaltreedir ] ; then
        mkdir $tokufractaltreedir

        if [ $branch = "." ] ; then tokudb_branch=toku; else tokudb_branch=$branch; fi
        retry svn export -q -r $revision $svnserver/$tokudb_branch/$tokudb
        exitcode=$?; if [ $exitcode != 0 ] ; then exit 1; fi

        if [[ $ftcc =~ icc ]] ; then
            if [ -d /opt/intel/bin ] ; then
                export PATH=$PATH:/opt/intel/bin
                . /opt/intel/bin/compilervars.sh intel64
            fi
        fi

        retry svn export -q -r $revision $svnserver/$branch/$xz
        exitcode=$?; if [ $exitcode != 0 ] ; then exit 1; fi

        pushd $tokudb

            echo `date` make $tokudb $ftcc $($ftcc --version)
            local intel_cc=""
            if [[ $ftcc = "icc" ]]; then
                intel_cc="ON"
                cmake_env=""
            else
                intel_cc="OFF"
                cmake_env="CC=$ftcc CXX=$ftcxx"
            fi
            local use_valgrind=""
            local debug_paranoid=""
            if [[ $debugbuild = 1 ]]; then
                use_valgrind="ON"
                debug_paranoid="ON"
            else
                use_valgrind="OFF"
                debug_paranoid="OFF"
            fi

            mkdir -p build
            cd build

            # only use FT Release or Debug build types
            local ft_build_type=$build_type
            if [ $ft_build_type = RelWithDebInfo ] ; then ft_build_type=Release; fi

            eval $cmake_env cmake \
                -D LIBTOKUDB=$tokufractaltree \
                -D LIBTOKUPORTABILITY=$tokuportability \
                -D CMAKE_TOKUDB_REVISION=$revision \
                -D CMAKE_BUILD_TYPE=$ft_build_type \
                -D TOKU_SVNROOT=$basedir \
                -D JEMALLOC_SOURCE_DIR=../../$jemalloc \
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
            exitcode=$?; if [ $exitcode != 0 ] ; then exit 1; fi
        popd

        pushd $tokufractaltreedir/examples
            # test the examples
            sed -ie "s/LIBTOKUDB = tokudb/LIBTOKUDB = $tokufractaltree/" Makefile 
            sed -ie "s/LIBTOKUPORTABILITY = tokuportability/LIBTOKUPORTABILITY = $tokuportability/" Makefile
            if [ $system = darwin ] ; then
                DYLD_LIBRARY_PATH=$PWD/../lib:$DYLD_LIBRARY_PATH make check CC=$ftcc
                exitcode=$?
            else
                if [ $do_make_check != 0 ] ; then
                    make check CC=$ftcc
                    exitcode=$?
                else
                    exitcode=0
                fi
            fi
            echo `date` make check examples $tokufractaltree $exitcode
            if [ $exitcode != 0 ] ; then exit 1; fi
            make clean
        popd

        # add jemalloc.so
        if [ $system = darwin ] ; then
            cp $jemalloc/lib/libjemalloc.1.dylib $tokufractaltreedir/lib/lib$jemalloc-$branchrevision.dylib
        else
            cp $jemalloc/lib/libjemalloc.so.1 $tokufractaltreedir/lib/lib$jemalloc-$branchrevision.so
        fi
        cp $jemalloc/lib/libjemalloc.a $tokufractaltreedir/lib/libjemalloc.a

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

    echo -n cmake .. $extra_flags -DBUILD_CONFIG=mysql_release -DCMAKE_BUILD_TYPE=$build_type
    if [ $system = linux ] ; then
        echo -n " " \
            -DCMAKE_EXE_LINKER_FLAGS=\"-Wl,--whole-archive \$TOKUFRACTALTREE/lib/libjemalloc.a -Wl,-no-whole-archive\"
    elif [ $system = darwin ] ; then
        echo -n " " \
            -DWITH_SAFEMALLOC=OFF -DWITH_SSL=system \
            -DCMAKE_EXE_LINKER_FLAGS=\"-Wl,-force_load -Wl,\$TOKUFRACTALTREE/lib/libjemalloc.a\"
    else
        exit 1
    fi
    if [ $link_enterprise_hot_backup != 0 ] ; then
        echo -n " " -DCOMPILATION_COMMENT=\"TokuDB Enterprise Server \(GPL\)\"
    fi
    echo
}

# generate a script that builds from the mysql src tarball and the fractal tree tarball
function generate_build_from_src() {
    local tokumysqldir=$1; local mysqlsrc=$2; local tokufractaltreedir=$3
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
        echo export TOKUDB_VERSION=$branchrevision
        echo export TOKUFRACTALTREE='$PWD'/$tokufractaltreedir
        echo export TOKUDB_PATCHES=$tokudb_patches
        if [ $staticft != 0 ] ; then
            echo export TOKUFRACTALTREE_LIBNAME=${tokufractaltree}_static
            echo export TOKUPORTABILITY_LIBNAME=${tokuportability}_static
        else
            echo export TOKUFRACTALTREE_LIBNAME=${tokufractaltree}
            echo export TOKUPORTABILITY_LIBNAME=${tokuportability}
        fi
        echo 'export CC=$cc; export CXX=$cxx'

        echo "pushd $mysqlsrc"
        echo 'if [ $? != 0 ] ; then exit 1; fi'

        if [ ! -f CMakeLists.txt ] ; then exit 1; fi

        if [ $build_tarball != 0 ] ; then
            echo "mkdir build.$build_type; pushd build.$build_type"
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
            if [ $staticft = 0 ] ; then
                # add fractal tree libs to the mysql tarball only if we are linking to shared libs
                echo "if [ -d $mysqldir/lib/mysql ] ; then"
                echo "    cp ../../$tokufractaltreedir/lib/lib* $mysqldir/lib/mysql"
                echo else
                echo "    cp ../../$tokufractaltreedir/lib/lib* $mysqldir/lib"
                echo fi
                echo 'if [ $? != 0 ] ; then exit 1; fi'
            fi
            # add ftdump to mysql tarball
            echo "cp ../../$tokufractaltreedir/bin/ftdump $mysqldir/bin/tokuftdump"
            echo 'if [ $? != 0 ] ; then exit 1; fi'
            # add tokustat to the mysql tarball
            echo "cp ../../$mysqlsrc/scripts/tokustat.py $mysqldir/bin"
            echo 'if [ $? != 0 ] ; then exit 1; fi'
            # add tokufilecheck to the mysql tarball
            echo "cp ../../$mysqlsrc/scripts/tokufilecheck.py $mysqldir/bin"
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
        if [ $system = linux -a $rpm != 0 ]; then
            echo "mkdir build.${build_type}.rpms; pushd build.${build_type}.rpms"
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

                # add jemalloc to the linker flags
                echo 'sed -i -e"s/^\(.*-DMYSQL_SERVER_SUFFIX=.*\)$/& -DCMAKE_EXE_LINKER_FLAGS=\"-Wl,--whole-archive \$\{TOKUFRACTALTREE\}\/lib\/libjemalloc.a -Wl,-no-whole-archive\"/" SPECS/$specfile'

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

# checkout the mysql source from subversion, generate a build script, and make the mysql source tarball
function build_mysql_src() {
    if [ ! -d $mysqlsrc ] ; then

        # get the mysql source
        if [ $branch = "." ] ; then mysql_branch=mysql.com; else mysql_branch=$branch; fi

        retry svn export -q -r $revision $svnserver/$mysql_branch/$mysql $mysqlsrc
        if [ $? != 0 ] ; then exit 1; fi

        # get the tokudb handlerton source
        if [ $branch = "." ] ; then tokumysql_branch=mysql; else tokumysql_branch=$branch/mysql; fi
        pushd $mysqlsrc/storage
            retry svn export -q -r $revision $svnserver/$tokumysql_branch/tokudb-engine/$tokudbengine tokudb
            if [ $? != 0 ] ; then exit 1; fi
        popd
        pushd $mysqlsrc/scripts
            retry svn export -q -r $revision $svnserver/$tokumysql_branch/scripts/tokustat.py tokustat.py
            if [ $? != 0 ] ; then exit 1; fi
        popd
        pushd $mysqlsrc/scripts
            retry svn export -q -r $revision $svnserver/$tokumysql_branch/scripts/tokufilecheck.py tokufilecheck.py
            if [ $? != 0 ] ; then exit 1; fi
        popd

        # append the tokudb version to the MYSQL_VERSION_EXTRA variable in the VERSION file
        pushd $mysqlsrc
        if [ $? = 0 ] ; then
            sed -i "" -e"s/^MYSQL_VERSION_EXTRA=.*/&-tokudb-${branchrevision}/" VERSION
            popd
        fi

        # merge the common mysql tests into the source
        pushd $mysqlsrc/mysql-test
        if [ $? = 0 ] ; then
            mkdir ../mysql-test-save
            mv * ../mysql-test-save
            retry svn export -q -r $revision $svnserver/$tokumysql_branch/tests/mysql-test
            mv mysql-test/* .
            rmdir mysql-test
            cp -r ../mysql-test-save/* .
            rm -rf ../mysql-test-save
            popd
        fi

	# get the hot backup source
	pushd $mysqlsrc
	if [ $? = 0 ] ; then
	    if [ $link_enterprise_hot_backup != 0 ] ; then
		retry svn export -q -r $revision $svnserver/backup-restore/backup/ toku_backup
	    else
		retry svn export -q -r $revision $svnserver/backup-restore/backup-community/ toku_backup
	    fi
	fi
	popd

        generate_build_from_src $tokumysql_branch $mysqlsrc $tokufractaltreedir >$mysqlsrc/scripts/tokudb.build.bash

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
        pushd $mysqlsrc/build.${build_type}
        if [ $? = 0 ] ; then
            cp $mysqldir.tar.gz* $basedir; if [ $? != 0 ] ; then exit 1; fi
            popd
        fi
        if [ $rpm != 0 ] ; then
            pushd $mysqlsrc/build.${build_type}.rpms
            if [ $? = 0 ] ; then
                rpmfiles=$(ls *.rpm *.rpm.md5)
                for x in $rpmfiles; do cp $x $basedir; if [ $? != 0 ] ; then  exit 1; fi; done
                popd
            fi
        fi

        popd
    fi
}

PATH=$HOME/bin:$PATH

branch=.
revision=0
suffix=.
mysql=mysql-5.5.28
tokudb=tokudb
tokudbengine=tokudb-engine
link_enterprise_hot_backup=0
do_s3=1
do_make_check=1
cc=gcc47
cxx=g++47
ftcc=gcc47
ftcxx=g++47
system=`uname -s | tr '[:upper:]' '[:lower:]'`
arch=`uname -m | tr '[:upper:]' '[:lower:]'`
svnserver=https://svn.tokutek.com/tokudb
jemalloc=jemalloc-3.3.0
xz=xz-4.999.9beta
makejobs=$(get_ncpus)
debugbuild=0
staticft=1
tokudb_patches=1
build_type=RelWithDebInfo
build_tarball=1
rpm=0

while [ $# -gt 0 ] ; do
    arg=$1; shift
    if [ $arg = "--gcc44" ] ; then
        cc=gcc44; cxx=g++44
    elif [[ $arg =~ --(.*)=(.*) ]] ; then
        eval ${BASH_REMATCH[1]}=${BASH_REMATCH[2]}
    else
        usage; exit 1;
    fi
done

if [ $debugbuild != 0 ] ; then
    if [ $build_type = RelWithDebInfo ] ; then build_type=Debug; fi
    if [ $suffix = "." ] ; then suffix=debug; fi
fi

if [ $link_enterprise_hot_backup != 0 ] ; then 
    if [ $suffix = "." ] ; then suffix="e"; else suffix="$suffix-e"; fi
fi

branchrevision=$(get_branchrevision $branch $revision $suffix)
builddir=build-$tokudb-$branchrevision
if [ ! -d $builddir ] ; then mkdir $builddir; fi
pushd $builddir

basedir=$PWD

# build the jemalloc library
build_jemalloc $jemalloc $branch $revision $branchrevision

# build the fractal tree tarball
tokufractaltree=tokufractaltreeindex-$branchrevision
tokuportability=tokuportability-$branchrevision
tokufractaltreedir=$tokufractaltree-$system-$arch
build_fractal_tree

# build the mysql source tarball
mysqlversion=$(mysql_version $mysql)
mysqldir=$mysql-tokudb-$branchrevision-$system-$arch
mysqlsrc=$mysql-tokudb-$branchrevision
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
    if [ $branch != '.' ] ; then
        branch=$(basename $branch)
        s3mkbucket tokutek-mysql-$branch
        if [ $? = 0 ] ; then
            files=$(ls $tokufractaltreedir.tar.gz* $mysqldir.tar.gz* $mysqlsrc.tar.gz* *.rpm*)
            for f in $files; do
                echo `date` s3copykey $branch $f
                s3copykey tokutek-mysql-$branch $f tokutek-mysql-build $f
                exitcode=$?
                echo `date` s3copykey $branch $f $exitcode
            done
        fi
    fi
fi

popd

exit 0
