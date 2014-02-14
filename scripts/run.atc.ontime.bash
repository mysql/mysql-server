#!/usr/bin/env bash

function usage() {
    echo "run the atc ontime load and run"
    echo "--mysqlbuild=$mysqlbuild"
    echo "[--commit=$commit]"
    echo "[--dbname=$dbname]"
    echo "[--load=$load] [--check=$check] [--run=$run]"
    echo "[--engine=$engine]"
    echo "[--tokudb_load_save_space=$tokudb_load_save_space] [--tokudb_row_format=$tokudb_row_format] [--tokudb_loader_memory_size=$tokudb_loader_memory_size]"
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

mysqlbuild=
commit=0
mysqlserver=`hostname`
mysqluser=`whoami`
mysqlsocket=/tmp/mysql.sock
svnserver=https://svn.tokutek.com/tokudb
basedir=$HOME/svn.build
builddir=$basedir/mysql.build
dbname=atc
tblname=ontime
load=1
check=1
run=1
engine=tokudb
tokudb_load_save_space=0
tokudb_row_format=
tokudb_loader_memory_size=
verbose=0
svn_server=https://svn.tokutek.com/tokudb
svn_branch=.
svn_revision=HEAD

# parse the command line
while [ $# -gt 0 ] ; do
    arg=$1; shift
    if [[ $arg =~ --(.*)=(.*) ]] ; then
        eval ${BASH_REMATCH[1]}=${BASH_REMATCH[2]}
    else
        usage; exit 1
    fi
done

if [[ $mysqlbuild =~ (.*)-(tokudb\-.*)-(linux)-(x86_64) ]] ; then
    mysql=${BASH_REMATCH[1]}
    tokudb=${BASH_REMATCH[2]}
    system=${BASH_REMATCH[3]}
    arch=${BASH_REMATCH[4]}
else
    exit 1
fi

if [ -d /usr/local/mysql/bin ] ; then
    export PATH=/usr/local/mysql/bin:$PATH
fi

if [ -d /usr/local/mysql/lib/mysql ] ; then
    export LD_LIBRARY_PATH=/usr/local/mysql/lib/mysql:$PATH
fi

# goto the base directory
if [ ! -d $basedir ] ; then mkdir $basedir; fi

pushd $basedir

# update the build directory
if [ ! -d $builddir ] ; then mkdir $builddir; fi

date=`date +%Y%m%d`
testresultsdir=$builddir/$date
pushd $builddir
    while [ ! -d $date ] ; do
        svn mkdir $svn_server/mysql.build/$date -m ""
        svn checkout $svn_server/mysql.build/$date
        if [ $? -ne 0 ] ; then rm -rf $date; fi
    done
popd

if [ $dbname = "atc" -a $engine != "tokudb" ] ; then dbname="atc_$engine"; fi

runfile=$testresultsdir/$dbname-$tblname-$mysqlbuild-$mysqlserver
if [ $tokudb_load_save_space != 0 ] ; then runfile=$runfile-compress; fi
if [ "$tokudb_row_format" != "" ] ; then runfile=$runfile-$tokudb_row_format; fi
if [ "$tokudb_loader_memory_size" != "" ] ; then runfile=$runfile-$tokudb_loader_memory_size; fi
rm -rf $runfile

testresult="PASS"

# maybe get the atc data from s3
if [ $testresult = "PASS" ] ; then
    f=atc_On_Time_Performance.mysql.csv
    if [ ! -f $f ] ; then
        f=$f.gz
        if [ ! -f $f ] ; then
            echo `date` s3get --bundle tokutek-mysql-data $f >>$runfile 2>&1
            s3get --verbose --bundle tokutek-mysql-data $f >>$runfile 2>&1
            exitcode=$?
            echo `date` s3get --bundle tokutek-mysql-data $f $exitcode >>$runfile 2>&1
            if [ $exitcode -ne 0 ] ; then testresult="FAIL"; fi
            if [ $testresult = "PASS" ] ; then
                echo `date` gunzip $f >>$runfile 2>&1
                gunzip $f
                exitcode=$?
                echo `date` gunzip $f $exitcode >>$runfile 2>&1
                if [ $exitcode -ne 0 ] ; then testresult="FAIL"; fi
            fi
        fi
    fi
fi

# checkout the atc test from svn
atc=atc-$mysqlbuild
if [ $testresult = "PASS" ] ; then
    if [ -d atc-$mysqlbuild ] ; then rm -rf atc-$mysqlbuild; fi

    retry svn export -r $svn_revision $svn_server/$svn_branch/mysql/tests/atc atc-$mysqlbuild
    exitcode=$?
    echo `date` svn export -r $svn_revision $svn_server/$svn_branch/mysql/tests/atc $exitcode >>$runfile 2>&1
    if [ $exitcode != 0 ] ; then 
        retry svn export -r $svn_revision $svn_server/mysql/tests/atc atc-$mysqlbuild
        exitcode=$?
        echo `date` svn export -r $svn_revision $svn_server/mysql/tests/atc $exitcode >>$runfile 2>&1
    fi
    if [ $exitcode != 0 ] ; then testresult="FAIL"; fi
fi

# create the database
if [ $load -ne 0 -a $testresult = "PASS" ] ; then
    echo `date` drop database if exists $dbname >>$runfile
    mysql -S $mysqlsocket -u $mysqluser -e "drop database if exists $dbname"  >>$runfile 2>&1
    exitcode=$?
    echo `date` drop database if exists $dbname $exitcode>>$runfile
    if [ $exitcode -ne 0 ] ; then testresult="FAIL"; fi
    echo `date` create database $dbname >>$runfile
    mysql -S $mysqlsocket -u $mysqluser -e "create database $dbname"  >>$runfile 2>&1
    exitcode=$?
    echo `date` create database $dbname $exitcode >>$runfile
    if [ $exitcode -ne 0 ] ; then testresult="FAIL"; fi
fi

# create the table
if [ $load -ne 0 -a $testresult = "PASS" ] ; then
    echo `date` create table $dbname.$tblname >>$runfile
    mysql -S $mysqlsocket -u $mysqluser -D $dbname -e "source $atc/atc_ontime_create_covered.sql"  >>$runfile 2>&1
    exitcode=$?
    echo `date` create table $exitcode >>$runfile
    if [ $exitcode -ne 0 ] ; then testresult="FAIL"; fi
fi

if [ $load -ne 0 -a $testresult = "PASS" -a "$tokudb_row_format" != "" ] ; then
    echo `date` create table $dbname.$tblname >>$runfile
    mysql -S $mysqlsocket -u $mysqluser -D $dbname -e "alter table $tblname row_format=$tokudb_row_format"  >>$runfile 2>&1
    exitcode=$?
    echo `date` create table $exitcode >>$runfile
    if [ $exitcode -ne 0 ] ; then testresult="FAIL"; fi
fi

if [ $load -ne 0 -a $testresult = "PASS" -a $engine != "tokudb" ] ; then
    echo `date` alter table $engine >>$runfile
    mysql -S $mysqlsocket -u $mysqluser -D $dbname -e "alter table $tblname engine=$engine"  >>$runfile 2>&1
    exitcode=$?
    echo `date` alter table $engine $exitcode >>$runfile
    if [ $exitcode -ne 0 ] ; then testresult="FAIL"; fi
fi  

if [ $testresult = "PASS" ] ; then
    mysql -S $mysqlsocket -u $mysqluser -D $dbname -e "show create table $tblname"  >>$runfile 2>&1
fi

if [ $testresult = "PASS" ] ; then
    let default_loader_memory_size="$(mysql -S $mysqlsocket -u $mysqluser -e'select @@tokudb_loader_memory_size' --silent --skip-column-names)"
    exitcode=$?
    echo `date` get tokudb_loader_memory_size $exitcode >>$runfile
    if [ $exitcode -ne 0 ] ; then testresult="FAIL"; fi
    if [ "$tokudb_loader_memory_size" = "" ] ; then tokudb_loader_memory_size=$default_loader_memory_size; fi
fi

# load the data
if [ $load -ne 0 -a $testresult = "PASS" ] ; then
    echo `date` load data >>$runfile
    start=$(date +%s)
    mysql -S $mysqlsocket -u $mysqluser -D $dbname -e "set tokudb_loader_memory_size=$tokudb_loader_memory_size;\
        set tokudb_load_save_space=$tokudb_load_save_space; load data infile '$basedir/atc_On_Time_Performance.mysql.csv' into table $tblname" >>$runfile 2>&1
    exitcode=$?
    let loadtime=$(date +%s)-$start
    echo `date` load data loadtime=$loadtime $exitcode >>$runfile
    if [ $exitcode -ne 0 ] ; then testresult="FAIL"; fi
fi

# check the tables
if [ $check -ne 0 -a $testresult = "PASS" ] ; then
    echo `date` check table $tblname >> $runfile
    mysql -S $mysqlsocket -u $mysqluser -D $dbname -e "check table $tblname"  >>$runfile 2>&1
    exitcode=$?
    echo `date` check table $tblname $exitcode >> $runfile
    if [ $exitcode -ne 0 ] ; then testresult="FAIL"; fi
fi

# run the queries
if [ $run -ne 0 -a $testresult = "PASS" ] ; then
    pushd $atc
    for qfile in q*.sql ; do
        if [[ $qfile =~ q(.*)\.sql ]] ; then
            qname=${BASH_REMATCH[1]}
            q=`cat $qfile`
            qrun=q${qname}.run

            echo `date` explain $qfile >>$runfile
            if [ $verbose -ne 0 ] ; then echo explain $q >>$runfile; fi
            mysql -S $mysqlsocket -u $mysqluser -D $dbname -e "explain $q"  >$qrun
            exitcode=$?
            echo `date` explain $qfile $exitcode >>$runfile
            if [ $verbose -ne 0 ] ; then cat $qrun >>$runfile; fi

            echo `date` $qfile >>$runfile
            start=$(date +%s)
            if [ $verbose -ne 0 ] ; then echo $q >>$runfile; fi
            mysql -S $mysqlsocket -u $mysqluser -D $dbname -e "$q"  >$qrun
            exitcode=$?
            let qtime=$(date +%s)-$start
            echo `date` $qfile qtime=$qtime $exitcode >>$runfile
            if [ $verbose -ne 0 ] ; then cat $qrun >>$runfile; fi
            if [ $exitcode -ne 0 ] ; then 
                testresult="FAIL"
            else
                if [ -f q${qname}.result ] ; then
                    diff $qrun q${qname}.result >>$runfile
                    exitcode=$?
                    if [ $exitcode -ne 0 ] ; then
                        testresult="FAIL"
                    fi
                fi
            fi
        fi
    done
    popd
fi

# commit results
if [ $commit != 0 ] ; then
    svn add $runfile
    retry svn commit -m \"$testresult $dbname $tblname $mysqlbuild $mysqlserver\" $runfile
fi

popd

if [ $testresult = "PASS" ] ; then exitcode=0; else exitcode=1; fi
exit $exitcode
