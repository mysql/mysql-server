#!/bin/sh
# Copyright (C) 1997-2002 MySQL AB
# For a more info consult the file COPYRIGHT distributed with this file

# This scripts creates the privilege tables db, host, user, tables_priv,
# columns_priv in the mysql database, as well as the func table.

if [ x$1 = x"--bin" ]; then
  shift 1

  # Check if it's a binary distribution or a 'make install'
  if test -x ../libexec/mysqld
  then
    execdir=../libexec
  else
    execdir=../bin
  fi
  bindir=../bin
  BINARY_DIST=1
  fix_bin=mysql-test
  scriptdir=../bin
  libexecdir=../libexec
else
  execdir=../sql
  bindir=../client
  fix_bin=.
  scriptdir=../scripts
  libexecdir=../libexec
fi

vardir=var
logdir=$vardir/log
if [ x$1 = x"-slave" ] 
then
 shift 1
 data=var/slave-data
else
 if [ x$1 = x"-1" ] 
 then
   data=var/master-data1
 else
   data=var/master-data
 fi
fi
ldata=$fix_bin/$data

mdata=$data/mysql
EXTRA_ARG=""

if test ! -x $execdir/mysqld
then
  if test ! -x $libexecdir/mysqld
  then
    echo "mysqld is missing - looked in $execdir and in $libexecdir"
    exit 1
  else
    execdir=$libexecdir
  fi
fi

# On IRIX hostname is in /usr/bsd so add this to the path
PATH=$PATH:/usr/bsd
hostname=`hostname`		# Install this too in the user table
hostname="$hostname%"		# Fix if not fully qualified hostname


#create the directories
[ -d $vardir ] || mkdir $vardir
[ -d $logdir ] || mkdir $logdir

# Create database directories mysql & test
if [ -d $data ] ; then rm -rf $data ; fi
mkdir $data $data/mysql $data/test 

#for error messages
if [ x$BINARY_DIST = x1 ] ; then
basedir=..
else
basedir=.
EXTRA_ARG="--language=../sql/share/english/ --character-sets-dir=../sql/share/charsets/"
fi

mysqld_boot=" $execdir/mysqld --no-defaults --bootstrap --skip-grant-tables --basedir=$basedir --datadir=$ldata --skip-innodb --skip-ndbcluster --tmpdir=. $EXTRA_ARG"
echo "running $mysqld_boot"

if $scriptdir/mysql_create_system_tables test $mdata $hostname | $mysqld_boot
then
    exit 0
else
    echo "Error executing mysqld --bootstrap"
    exit 1
fi
