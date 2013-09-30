#!/usr/bin/env bash

function usage() {
    echo "run the TPCH load and compare test"
    echo "[--SCALE=$SCALE] [--ENGINE=$ENGINE]"
    echo "[--dbgen=$dbgen] [--load=$load] [--check=$check] [--compare=$compare] [--query=$query]"
    echo "[--mysqlbuild=$mysqlbuild] [--commit=$commit]"
    echo "[--testinstance=$testinstance] [--tokudb_load_save_space=$tokudb_load_save_space]"
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
        sleep 1
    done
    test $exitcode = 0
}

SCALE=1
ENGINE=tokudb
TABLES="part partsupp customer lineitem nation orders region supplier"
dbgen=1
load=1
compare=1
query=0
check=1
datadir=/usr/local/mysql/data
mysqlbuild=
commit=0
mysqlserver=`hostname`
mysqluser=`whoami`
mysqlsocket=/tmp/mysql.sock
basedir=$HOME/svn.build
builddir=$basedir/mysql.build
system=`uname -s | tr [:upper:] [:lower:]`
arch=`uname -m | tr [:upper:] [:lower:]`
testinstance=
tokudb_load_save_space=0
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

dbname=tpch${SCALE}G_${ENGINE}
if [ "$testinstance" != "" ] ; then dbname=${dbname}_${testinstance}; fi
tpchdir=$basedir/tpch${SCALE}G

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

runfile=$testresultsdir/$dbname
if [ $tokudb_load_save_space != 0 ] ; then runfile=$runfile-compress; fi
runfile=$runfile-$mysqlbuild-$mysqlserver
rm -rf $runfile

testresult="PASS"

# maybe get the tpch data from AWS S3
if [ $compare != 0 ] && [ ! -d $tpchdir ] ; then
    tpchtarball=tpch${SCALE}G_data_dump.tar
    if [ ! -f $tpchtarball ] ; then
        echo `date` s3get --bundle tokutek-mysql-data $tpchtarball >>$runfile 2>&1
        s3get --verbose --bundle tokutek-mysql-data $tpchtarball >>$runfile 2>&1
        exitcode=$?
        echo `date` s3get --bundle tokutek-mysql-data $tpchtarball $exitcode >>$runfile 2>&1
        if [ $exitcode -ne 0 ] ; then testresult="FAIL"; fi
    fi
    if [ $testresult = "PASS" ] ; then
        tar xf $tpchtarball
        exitcode=$?
        echo `date` tar xf $tpchtarball $exitcode >>$runfile 2>&1
        if [ $exitcode -ne 0 ] ; then 
            testresult="FAIL"
        else
            # gunzip the data files
            pushd tpch${SCALE}G/data/tpch${SCALE}G
            for f in *.gz ; do
                echo `date` gunzip $f >>$runfile 2>&1
                gunzip $f
            done
            ls -l >>$runfile 2>&1
            popd
        fi
    fi
fi

# checkout the tpch scripts
tpchtestdir=tpch-$mysqlbuild
if [ "$testinstance" != "" ] ; then tpchtestdir=${tpchtestdir}_${testinstance}; fi
if [ $testresult = "PASS" ] ; then
    rm -rf $tpchtestdir
    retry svn export -q -r $svn_revision $svn_server/$svn_branch/tpch $tpchtestdir
    exitcode=$?
    echo `date` export $svn_server/$svn_branch/tpch $exitcode >>$runfile 2>&1
    if [ $exitcode != 0 ] ; then
        retry svn export -q -r $svn_revision $svn_server/tpch $tpchtestdir
        exitcode=$?
        echo `date` export $svn_server/tpch $exitcode >>$runfile 2>&1
    fi
    if [ $exitcode != 0 ] ; then testresult="FAIL"; fi
fi

# generate the tpch data
if [ $dbgen != 0 -a $testresult = "PASS" ] ; then
    pushd $tpchtestdir/dbgen
        make
        exitcode=$?
        echo `date` make dbgen $exitcode >>$runfile 2>&1
        if [ $exitcode != 0 ] ; then testresult="FAIL"; fi
    popd
    if [ $testresult = "PASS" ] ; then
        dbgen=0
        mkdir -p tpch${SCALE}G/data/tpch${SCALE}G
        pushd tpch${SCALE}G/data/tpch${SCALE}G
            if [ ! -f lineitem.tbl ] ; then dbgen=1; fi
        popd
        if [ $dbgen != 0 ] ; then
            pushd $tpchtestdir/dbgen
                ./dbgen -fF -s $SCALE
                exitcode=$?
                echo `date` dbgen -fF -s $SCALE $exitcode >>$runfile 2>&1
                if [ $exitcode != 0 ] ; then
                    testresult="FAIL"
                else
                    ls -l *.tbl >>$runfile
                    chmod 0644 *.tbl 
                    ls -l *.tbl >>$runfile
                    mv *.tbl $basedir/tpch${SCALE}G/data/tpch${SCALE}G
                fi
            popd
        fi
    fi
fi

# create the tpch database
if [ $load != 0 -a $testresult = "PASS" ] ; then
    echo `date` drop database if exists $dbname >>$runfile
    mysql -S $mysqlsocket -u $mysqluser -e "drop database if exists $dbname" >>$runfile 2>&1
    exitcode=$?
    echo `date` drop database if exists $dbname $exitcode>>$runfile
    if [ $exitcode -ne 0 ] ; then testresult="FAIL"; fi
    echo `date` create database $dbname >>$runfile
    mysql -S $mysqlsocket -u $mysqluser -e "create database $dbname" >>$runfile 2>&1
    exitcode=$?
    echo `date` create database $dbname $exitcode >>$runfile
    if [ $exitcode -ne 0 ] ; then testresult="FAIL"; fi
fi

# create the tpch tables
if [ $load != 0 -a $testresult = "PASS" ] ; then
    echo `date` create table >>$runfile
    mysql -S $mysqlsocket -u $mysqluser -D $dbname -e "source $basedir/tpch-$mysqlbuild/scripts/${ENGINE}_tpch_create_table.sql" >>$runfile 2>&1
    exitcode=$?
    echo `date` create table $exitcode >>$runfile
    if [ $exitcode -ne 0 ] ; then testresult="FAIL"; fi
fi

# load the data
if [ $load != 0 -a $testresult = "PASS" ] ; then
    for tblname in $TABLES ; do
        echo `date` load table $tblname >>$runfile
        ls -l $tpchdir/data/tpch${SCALE}G/$tblname.tbl >>$runfile
        start=$(date +%s)
        mysql -S $mysqlsocket -u $mysqluser -D $dbname -e "set session tokudb_load_save_space=$tokudb_load_save_space; load data infile '$tpchdir/data/tpch${SCALE}G/$tblname.tbl' into table $tblname fields terminated by '|'" >>$runfile 2>&1
        exitcode=$?
        let loadtime=$(date +%s)-$start
        echo `date` load table $tblname $exitcode loadtime=$loadtime>>$runfile
        if [ $exitcode -ne 0 ] ; then testresult="FAIL"; fi
    done
fi

if [ $check != 0 -a $testresult = "PASS" ] ; then
    for tblname in lineitem ; do
        echo `date` add clustering index $tblname >>$runfile
        start=$(date +%s)
        mysql -S $mysqlsocket -u $mysqluser -D $dbname -e "set session tokudb_create_index_online=0;create clustering index i_shipdate on lineitem (l_shipdate)" >>$runfile 2>&1
        exitcode=$?
        let loadtime=$(date +%s)-$start
        echo `date` add clustering index $tblname $exitcode loadtime=$loadtime >>$runfile
        if [ $exitcode -ne 0 ] ; then testresult="FAIL"; fi
    done
fi

# check the tables
if [ $check != 0 -a $testresult = "PASS" ] ; then
    for tblname in $TABLES ; do
        echo `date` check table $tblname >>$runfile
        start=$(date +%s)
        mysql -S $mysqlsocket -u $mysqluser -D $dbname -e "check table $tblname" >>$runfile 2>&1
        exitcode=$?
        let checktime=$(date +%s)-$start
        echo `date` check table $tblname $exitcode checktime=$checktime >>$runfile
        if [ $exitcode -ne 0 ] ; then testresult="FAIL"; fi
    done
fi

if [ $check != 0 -a $testresult = "PASS" ] ; then
    for tblname in lineitem ; do
        echo `date` drop index $tblname >>$runfile
        mysql -S $mysqlsocket -u $mysqluser -D $dbname -e "drop index i_shipdate on lineitem" >>$runfile 2>&1
        exitcode=$?
        echo `date` drop index $tblname $exitcode >>$runfile
        if [ $exitcode -ne 0 ] ; then testresult="FAIL"; fi
    done
fi

# compare the data
if [ $compare != 0 -a $testresult = "PASS" ] ; then
    if [ -d $tpchdir/dump/tpch${SCALE}G ] ; then
        mysql -S $mysqlsocket -u $mysqluser -D $dbname -e "source $basedir/tpch-$mysqlbuild/scripts/dumptpch.sql" >>$runfile 2>&1
        exitcode=$?
        echo `date` dump data $exitcode >>$runfile
        if [ $exitcode -ne 0 ] ; then 
            testresult="FAIL"
        else
            # force the permissions on the dumpdir open
            pushd $datadir/$dbname
            exitcode=$?
            if [ $exitcode != 0 ] ; then
                sudo chmod g+rwx $datadir
                sudo chmod g+rwx $datadir/$dbname
                pushd $datadir/$dbname
                exitcode=$?
            fi
            if [ $exitcode = 0 ] ; then
                popd
            fi

            # compare the dump files
            dumpdir=$datadir/$dbname
            comparedir=$tpchdir/dump/tpch${SCALE}G
            for f in $dumpdir/dump* ; do
                d=`basename $f`
                if [ ! -f $comparedir/$d ] && [ -f $comparedir/$d.gz ] ; then
                    pushd $comparedir; gunzip $d.gz; popd
                fi
                if [ -f $comparedir/$d ] ; then
                    diff -q $dumpdir/$d $comparedir/$d
                    if [ $? = 0 ] ; then
                        result="PASS"
                    else 
                        result="FAIL"
                        testresult="FAIL"
                    fi
                else
                    result="MISSING"
                    testresult="FAIL"
                fi
                echo `date` $d $result >>$runfile
            done
            if [ $testresult = "PASS" ] ; then
                     # remove the dump files
                rm -f $datadir/$dbname/dump*
            fi
        fi
    fi
fi

# commit results
if [ $commit != 0 ] ; then
    svn add $runfile
    retry svn commit -m \"$testresult $dbname $mysqlbuild $mysqlserver\" $runfile
fi

popd

if [ $testresult = "PASS" ] ; then exitcode=0; else exitcode=1; fi
exit $exitcode
