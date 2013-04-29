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
    elif [ $github_use_ssh != 0 ] ; then
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
    else
        retry curl \
            --location https://api.github.com/repos/$repo/tarball/$rev \
            --output $dest.tar.gz
        tar --extract \
            --gzip \
            --directory=$dest \
            --strip-components=1 \
            --file $dest.tar.gz
        if [ $? != 0 ] ; then return; fi
        rm -f $dest.tar.gz
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
