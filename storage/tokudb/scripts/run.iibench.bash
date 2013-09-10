#!/usr/bin/env bash

function usage() {
    echo "run iibench"
    echo "--mysqlbuild=$mysqlbuild"
    echo "[--max_row=$max_rows] [--rows_per_report=$rows_per_report] [--insert_only=$insert_only] [ --check=$check]"
    echo "[--commit=$commit]"
}

function retry() {
    local cmd=$*
    local retries
    local exitcode
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

mysqlbuild=
commit=0
check=1
mysqlserver=`hostname`
mysqluser=`whoami`
mysqlsocket=/tmp/mysql.sock
svn_server=https://svn.tokutek.com/tokudb
svn_branch=.
svn_revision=HEAD
basedir=$HOME/svn.build
builddir=$basedir/mysql.build
system=`uname -s | tr [:upper:] [:lower:]`
instancetype=
testinstance=
arch=`uname -m | tr [:upper:] [:lower:]`
tracefile=/tmp/run.iibench.trace
cmd=iibench
dbname=$cmd
engine=tokudb
tblname=testit
max_rows=50000000
rows_per_report=1000000
insert_only=1

# parse the command line
while [ $# -gt 0 ] ; do
    arg=$1; shift
    if [ $arg = "--replace_into" ] ; then 
        cmd=replace_into
    elif [ $arg = "--insert_ignore" ] ; then
        cmd=insert_ignore
    elif [[ $arg =~ --(.*)=(.*) ]] ; then
        eval ${BASH_REMATCH[1]}=${BASH_REMATCH[2]}
    else
        usage; exit 1
    fi
done

if [[ $mysqlbuild =~ (.*)-(tokudb-.*)-(linux)-(x86_64) ]] ; then
    mysql=${BASH_REMATCH[1]}
    tokudb=${BASH_REMATCH[2]}
    system=${BASH_REMATCH[3]}
    arch=${BASH_REMATCH[4]}
else
    exit 1
fi

# setup the dbname
if [ $dbname = "iibench" ] ; then dbname=${cmd}_${engine}; fi
if [ "$testinstance" != "" ] ; then dbname=${dbname}_${testinstance}; fi

if [ -d /usr/local/mysql ] ; then
    export PATH=/usr/local/mysql/bin:$PATH
fi

if [ -d /usr/local/mysql/lib/mysql ] ; then
    export LD_LIBRARY_PATH=/usr/local/mysql/lib/mysql:$PATH
fi

# goto the base directory
if [ ! -d $basedir ] ; then mkdir $basedir; fi
pushd $basedir

# update the build directory
if [ $commit != 0 ] ; then
    if [ ! -d $builddir ] ; then mkdir $builddir; fi

    date=`date +%Y%m%d`
    testresultsdir=$builddir/$date
    pushd $builddir
        while [ ! -d $date ] ; do
            svn mkdir $svn_server/mysql.build/$date -m ""
            svn checkout -q $svn_server/mysql.build/$date
            if [ $? -ne 0 ] ; then rm -rf $date; fi
        done
    popd
else
    testresultsdir=$PWD
fi

# checkout the code
testdir=iibench-$mysqlbuild-$mysqlserver
if [ "$testinstance" != "" ] ; then testdir=$testdir-$testinstance; fi
rm -rf $testdir
retry svn export -q -r $svn_revision $svn_server/$svn_branch/iibench $testdir
exitcode=$?
if [ $exitcode != 0 ] ; then 
    retry svn export -q -r $svn_revision $svn_server/iibench $testdir
    exitcode=$?
fi
if [ $exitcode != 0 ] ; then  exit 1; fi

# create the iibench database
mysql -S $mysqlsocket -u root -e "grant all on *.* to '$mysqluser'@'$mysqlserver'"
exitcode=$?
if [ $exitcode != 0 ] ; then exit 1; fi

mysql -S $mysqlsocket -u $mysqluser -e "drop database if exists $dbname"
exitcode=$?
if [ $exitcode != 0 ] ; then exit 1; fi

mysql -S $mysqlsocket -u $mysqluser -e "create database $dbname"
exitcode=$?
if [ $exitcode != 0 ] ; then exit 1; fi

# run
if [ $cmd = "iibench" -a $insert_only != 0 ] ; then
    runfile=$testresultsdir/$dbname-insert_only-$max_rows-$mysqlbuild-$mysqlserver
else
    runfile=$testresultsdir/$dbname-$max_rows-$mysqlbuild-$mysqlserver
fi
if [ "$instancetype" != "" ] ; then runfile=$runfile-$instancetype; fi
testresult="PASS"

pushd $testdir/py
    echo `date` $cmd start $mysql $svn_branch $svn_revision $max_rows $rows_per_report >>$runfile
    runcmd=$cmd.py
    args="--db_user=$mysqluser --db_name=$dbname --db_socket=$mysqlsocket --engine=$engine --setup --max_rows=$max_rows --rows_per_report=$rows_per_report --table_name=$tblname"
    if [ $cmd = "iibench" -a $insert_only != 0 ] ; then runcmd="$runcmd --insert_only"; fi
    if [ $cmd = "replace_into" ] ; then runcmd="replace_into.py --use_replace_into"; fi
    if [ $cmd = "insert_ignore" ] ; then runcmd="replace_into.py"; fi
    ./$runcmd $args >>$runfile 2>&1
    exitcode=$?
    echo `date` $cmd complete $exitcode >>$runfile
    if [ $exitcode != 0 ] ; then testresult="FAIL"; fi
popd

if [ $check != 0 -a $testresult = "PASS" ] ; then
    echo `date` check table $tblname >>$runfile
    mysql -S $mysqlsocket -u $mysqluser -D $dbname -e "check table $tblname" >>$runfile 2>&1
    exitcode=$?
    echo `date` check table $tblname $exitcode >>$runfile
    if [ $exitcode != 0 ] ; then testresult="FAIL"; fi
fi

# commit results
if [ $commit != 0 ] ; then
    if [ $cmd = "iibench" -a $insert_only != 0 ] ; then cmd="$cmd insert_only"; fi
    svn add $runfile
    retry svn commit -m \"$testresult $cmd $max_rows $dbname $mysqlbuild $mysqlserver `hostname`\" $runfile
fi

popd

if [ $testresult = "PASS" ] ; then exitcode=0; else exitcode=1; fi
exit $exitcode
