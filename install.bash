#!/bin/sh

prefix=/usr/local

while [ $# -gt 0 ] ;do
    if [[ "$1" =~ "--prefix=(.*)" ]] ; then
	prefix=${BASH_REMATCH[1]}
    fi
    shift
done

if [ ! -d include ] ; then exit 1; fi

if [[ `grep DB_VERSION_MAJOR include/db.h` =~ "DB_VERSION_MAJOR (.*)" ]] ;then
    bdbmajor=${BASH_REMATCH[1]}
else
    exit 1
fi

if [[ `grep DB_VERSION_MINOR include/db.h` =~ "DB_VERSION_MINOR (.*)" ]] ;then
    bdbminor=${BASH_REMATCH[1]}
else
    exit 1
fi

d=`basename $PWD`
if [[ $d =~ "tokudb" ]] ; then
    tokudb="tokudb"
else
    tokudb="tokudb-$d"
fi

targetdir=$prefix/$tokudb-$bdbmajor.$bdbminor
if [ -d $targetdir ] ; then rm -rf $targetdir ; fi

mkdir $targetdir
mkdir $targetdir/include
cp include/db.h $targetdir/include
cp include/db_cxx.h $targetdir/include

mkdir $targetdir/lib
cp lib/libtokudb.so $targetdir/lib
cp lib/libtokudb_cxx.a $targetdir/lib

mkdir $targetdir/bin
cp utils/tokudb_load_static $targetdir/bin/tokudb_load
cp utils/tokudb_dump_static $targetdir/bin/tokudb_dump
cp utils/tokudb_gen_static $targetdir/bin/tokudb_gen

mkdir $targetdir/man
for f in man/*.[0-9]* man/texi/*.[0-9]* ;do
    if [[ $f =~ "(.*)\.(.*)" ]] ; then
	manpage=${BASH_REMATCH[1]}
	section=${BASH_REMATCH[2]}
	mkdir -p $targetdir/man/man$section
	cp $f $targetdir/man/man$section
    fi
done

exit 0