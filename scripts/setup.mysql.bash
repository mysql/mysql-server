#!/usr/bin/env bash

function usage() {
    echo "setup.mysql.bash"
    echo "--mysqlbuild=$mysqlbuild --shutdown=$shutdown --install=$install --startup=$startup"
}

mysqlbuild=
shutdown=1
install=1
startup=1
s3bucket=tokutek-mysql-build
sleeptime=60
builtins="mysqlbuild shutdown install startup s3bucket sleeptime"
mysqld_args="--user=mysql --core-file --core-file-size=unlimited"
sudo=/usr/bin/sudo
defaultsfile=""
if [ -f /etc/$(whoami).my.cnf ] ; then
    defaultsfile=/etc/$(whoami).my.cnf
fi

function is_builtin() {
    local v=$1; shift
    local x
    for x in $* ; do 
        if [ $v = $x ] ; then echo 1; return; fi
    done
    echo 0
}

while [ $# -gt 0 ] ; do
    arg=$1; shift
    if [ $arg = "--help" -o $arg = "-h" -o $arg = "-?" ] ; then
        usage; exit 1
    elif [[ $arg =~ --(.*)=(.*) ]] ; then
        r=$(is_builtin ${BASH_REMATCH[1]} $builtins)
        if [ $r = 1 ] ; then
            eval ${BASH_REMATCH[1]}=${BASH_REMATCH[2]}
        else
            mysqld_args="$mysqld_args $arg"
        fi
    else
        mysqld_args="$mysqld_args $arg"
    fi
done

if [ -d /data/mysql/tmp ] ; then mysqld_args="$mysqld_args --tmpdir=/data/mysql/tmp"; fi

if [[ $mysqlbuild =~ (.*)-(tokudb\-.*)-(linux)-(x86_64) ]] ; then
    mysql=${BASH_REMATCH[1]}
    tokudb=${BASH_REMATCH[2]}
    system=${BASH_REMATCH[3]}
    arch=${BASH_REMATCH[4]}
else
    echo $mysqlbuild is not a tokudb build
fi

if [ ! -d downloads ] ; then mkdir downloads; fi

pushd downloads
if [ $? != 0 ] ; then exit 1; fi

basedir=$PWD

mysqltarball=$mysqlbuild.tar.gz

if [ -f $mysqlbuild.tar.gz ] ; then
    compression=-z
    mysqltarball=$mysqlbuild.tar.gz
elif [ -f $mysqlbuild.tar.bz2 ] ; then
    compression=-j
    mysqltarball=$mysqlbuild.tar.bz2
fi

# get the release
if [ ! -f $mysqltarball ] ; then
    s3get $s3bucket $mysqltarball $mysqltarball 
    if [ $? -ne 0 ] ; then exit 1; fi
fi
if [ ! -f $mysqltarball.md5 ] ; then
    s3get $s3bucket $mysqltarball.md5 $mysqltarball.md5
    if [ $? -ne 0 ] ; then exit 1; fi
fi

# check the md5 sum
md5sum --check $mysqltarball.md5
if [ $? -ne 0 ] ; then
    # support jacksum md5 output which is almost the same as md5sum
    diff -b <(cat $mysqltarball.md5) <(md5sum $mysqltarball)
    if [ $? -ne 0 ] ; then exit 1; fi
fi

# set ldpath
ldpath=""
if [ -d /usr/local/gcc-4.7/lib64 ] ; then
    echo skip ldpath="export LD_LIBRARY_PATH=/usr/local/gcc-4.7/lib64:\$LD_LIBRARY_PATH;"
fi

# shutdown mysql
if [ $shutdown -ne 0 ] ; then
    if [ -x /etc/init.d/mysql ] ; then
        $sudo setsid /etc/init.d/mysql stop
    else
        /usr/local/mysql/bin/mysqladmin shutdown
    fi
    sleep $sleeptime
fi

pushd /usr/local
if [ $? = 0 ] ; then 
    rm mysql
    popd
fi

# install the release
pushd /usr/local/mysqls 2>/dev/null
if [ $? = 0 ] ; then
    mysqldir=mysqls/$mysqlbuild
else
    pushd /usr/local
    if [ $? -ne 0 ] ; then exit 1; fi
    mysqldir=$mysqlbuild
fi

if [ ! -d $mysqlbuild ] || [ $install -ne 0 ] ; then
    rm mysql
    if [ -d $mysqlbuild ] ; then $sudo rm -rf $mysqlbuild; fi

    tar -x $compression -f $basedir/$mysqltarball
    if [ $? -ne 0 ] ; then exit 1; fi
    ln -s $mysqldir /usr/local/mysql
    if [ $? -ne 0 ] ; then exit 1; fi
    ln -s $mysqldir /usr/local/$mysqlbuild
    if [ $? -ne 0 ] ; then exit 1; fi

    installdb=$mysqlbuild/bin/mysql_install_db
    if [ ! -f $installdb ] ; then
        installdb=$mysqlbuild/scripts/mysql_install_db
    fi

    $sudo chown -R mysql $mysqlbuild/data
    $sudo chgrp -R mysql $mysqlbuild/data

    # 5.6 debug build needs this 
    if [ ! -f $mysqlbuild/bin/mysqld ] && [ -f $mysqlbuild/bin/mysqld-debug ] ; then
	ln $mysqlbuild/bin/mysqld-debug $mysqlbuild/bin/mysqld
    fi

    if [ -z "$defaultsfile" ] ; then 
        default_arg=""
    else
        default_arg="--defaults-file=$defaultsfile"
    fi
    $sudo bash -c "$ldpath $installdb $default_arg --user=mysql --basedir=$PWD/$mysqlbuild --datadir=$PWD/$mysqlbuild/data"
    if [ $? -ne 0 ] ; then exit 1; fi
else
    # create link
    rm /usr/local/mysql
    ln -s $mysqldir /usr/local/mysql
    if [ $? -ne 0 ] ; then exit 1; fi
    rm /usr/local/$mysqlbuild
    ln -s $mysqldir /usr/local/$mysqlbuild
    if [ $? -ne 0 ] ; then exit 1; fi
fi
popd

# start mysql
if [ $startup -ne 0 ] ; then 
    ulimit -a
    # increase the open file limit
    ulimit -n 10240
    exitcode=$?
    echo ulimit -n 10240 exitcode $exitcode

    if [ -x /etc/init.d/mysql ] ; then
        $sudo setsid /etc/init.d/mysql start
    else
        if [ -z "$defaultsfile" ] ; then
            default_arg=""
        else
            default_arg="--defaults-file=$defaultsfile"
        fi
        $sudo -b bash -c "$ldpath /usr/local/mysql/bin/mysqld_safe $default_arg $mysqld_args" >/dev/null 2>&1 &
    fi
    sleep $sleeptime

    # add mysql grants
    /usr/local/mysql/bin/mysql -u root -e "grant all on *.* to tokubuild@localhost"
    /usr/local/mysql/bin/mysql -u root -e "grant all on *.* to 'ec2-user'@localhost"
fi

popd

exit 0
