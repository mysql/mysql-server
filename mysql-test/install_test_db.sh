#!/bin/sh
# Copyright (C) 1997, 1998, 1999 TCX DataKonsult AB & Monty Program KB & Detron HB
# For a more info consult the file COPYRIGHT distributed with this file

# This scripts creates the privilege tables db, host, user, tables_priv,
# columns_priv in the mysql database, as well as the func table.

if [ x$1 = x"-bin" ]; then
 shift 1
 execdir=../bin
 bindir=../bin
 BINARY_DIST=1
 fix_bin=mysql-test
else
 execdir=../sql
 bindir=../client
 fix_bin=.
fi

vardir=var
logdir=$vardir/log
if [ x$1 = x"-slave" ] 
then
 shift 1
 data=var/slave-data
 ldata=$fix_bin/var/slave-data
else
 data=var/master-data
 ldata=$fix_bin/var/master-data
fi

mdata=$data/mysql
EXTRA_ARG=""

if test ! -x $execdir/mysqld
then
  echo "mysqld is missing - looked in $execdir"
  exit 1
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
EXTRA_ARG="--language=../sql/share/english/"
fi

# Initialize variables
c_d="" i_d=""
c_h="" i_h=""
c_u="" i_u=""
c_f="" i_f=""
c_t="" c_c=""

# Check for old tables
if test ! -f $mdata/db.frm
then
  # mysqld --bootstrap wants one command/line
  c_d="$c_d CREATE TABLE db ("
  c_d="$c_d   Host char(60) DEFAULT '' NOT NULL,"
  c_d="$c_d   Db char(64) DEFAULT '' NOT NULL,"
  c_d="$c_d   User char(16) DEFAULT '' NOT NULL,"
  c_d="$c_d   Select_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_d="$c_d   Insert_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_d="$c_d   Update_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_d="$c_d   Delete_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_d="$c_d   Create_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_d="$c_d   Drop_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_d="$c_d   Grant_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_d="$c_d   References_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_d="$c_d   Index_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_d="$c_d   Alter_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_d="$c_d PRIMARY KEY Host (Host,Db,User),"
  c_d="$c_d KEY User (User)"
  c_d="$c_d )"
  c_d="$c_d comment='Database privileges';"
  
  i_d="INSERT INTO db VALUES ('%','test','','Y','Y','Y','Y','Y','Y','N','Y','Y','Y');
  INSERT INTO db VALUES ('%','test\_%','','Y','Y','Y','Y','Y','Y','N','Y','Y','Y');"
fi

if test ! -f $mdata/host.frm
then
  c_h="$c_h CREATE TABLE host ("
  c_h="$c_h  Host char(60) DEFAULT '' NOT NULL,"
  c_h="$c_h  Db char(64) DEFAULT '' NOT NULL,"
  c_h="$c_h  Select_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_h="$c_h  Insert_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_h="$c_h  Update_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_h="$c_h  Delete_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_h="$c_h  Create_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_h="$c_h  Drop_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_h="$c_h  Grant_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_h="$c_h  References_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_h="$c_h  Index_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_h="$c_h  Alter_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_h="$c_h  PRIMARY KEY Host (Host,Db)"
  c_h="$c_h )"
  c_h="$c_h comment='Host privileges;  Merged with database privileges';"
fi

if test ! -f $mdata/user.frm
then
  c_u="$c_u CREATE TABLE user ("
  c_u="$c_u   Host char(60) DEFAULT '' NOT NULL,"
  c_u="$c_u   User char(16) DEFAULT '' NOT NULL,"
  c_u="$c_u   Password char(16) DEFAULT '' NOT NULL,"
  c_u="$c_u   Select_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Insert_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Update_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Delete_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Create_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Drop_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Reload_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Shutdown_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Process_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_u="$c_u   File_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Grant_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_u="$c_u   References_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Index_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Alter_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_u="$c_u   PRIMARY KEY Host (Host,User)"
  c_u="$c_u )"
  c_u="$c_u comment='Users and global privileges';"

  i_u="INSERT INTO user VALUES ('localhost','root','','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y');
  INSERT INTO user VALUES ('$hostname','root','','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y');
  REPLACE INTO user VALUES ('127.0.0.1','root','','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y');
  
  INSERT INTO user VALUES ('localhost','','','N','N','N','N','N','N','N','N','N','N','N','N','N','N');
  INSERT INTO user VALUES ('$hostname','','','N','N','N','N','N','N','N','N','N','N','N','N','N','N');"
fi

if test ! -f $mdata/func.frm
then
  c_f="$c_f CREATE TABLE func ("
  c_f="$c_f   name char(64) DEFAULT '' NOT NULL,"
  c_f="$c_f   ret tinyint(1) DEFAULT '0' NOT NULL,"
  c_f="$c_f   dl char(128) DEFAULT '' NOT NULL,"
  c_f="$c_f   type enum ('function','aggregate') NOT NULL,"
  c_f="$c_f   PRIMARY KEY (name)"
  c_f="$c_f )"
  c_f="$c_f   comment='User defined functions';"
fi

if test ! -f $mdata/tables_priv.frm
then
  c_t="$c_t CREATE TABLE tables_priv ("
  c_t="$c_t   Host char(60) DEFAULT '' NOT NULL,"
  c_t="$c_t   Db char(64) DEFAULT '' NOT NULL,"
  c_t="$c_t   User char(16) DEFAULT '' NOT NULL,"
  c_t="$c_t   Table_name char(60) DEFAULT '' NOT NULL,"
  c_t="$c_t   Grantor char(77) DEFAULT '' NOT NULL,"
  c_t="$c_t   Timestamp timestamp(14),"
  c_t="$c_t   Table_priv set('Select','Insert','Update','Delete','Create','Drop','Grant','References','Index','Alter') DEFAULT '' NOT NULL,"
  c_t="$c_t   Column_priv set('Select','Insert','Update','References') DEFAULT '' NOT NULL,"
  c_t="$c_t   PRIMARY KEY (Host,Db,User,Table_name),"
  c_t="$c_t   KEY Grantor (Grantor)"
  c_t="$c_t )"
  c_t="$c_t   comment='Table privileges';"
fi

if test ! -f $mdata/columns_priv.frm
then
  c_c="$c_c CREATE TABLE columns_priv ("
  c_c="$c_c   Host char(60) DEFAULT '' NOT NULL,"
  c_c="$c_c   Db char(64) DEFAULT '' NOT NULL,"
  c_c="$c_c   User char(16) DEFAULT '' NOT NULL,"
  c_c="$c_c   Table_name char(64) DEFAULT '' NOT NULL,"
  c_c="$c_c   Column_name char(64) DEFAULT '' NOT NULL,"
  c_c="$c_c   Timestamp timestamp(14),"
  c_c="$c_c   Column_priv set('Select','Insert','Update','References') DEFAULT '' NOT NULL,"
  c_c="$c_c   PRIMARY KEY (Host,Db,User,Table_name,Column_name)"
  c_c="$c_c )"
  c_c="$c_c   comment='Column privileges';"
fi

if $execdir/mysqld --no-defaults --bootstrap --skip-grant-tables \
    --basedir=$basedir --datadir=$ldata --skip-innodb --skip-bdb --skip-gemini $EXTRA_ARG << END_OF_DATA
use mysql;
$c_d
$i_d

$c_h
$i_h

$c_u
$i_u

$c_f
$i_f

$c_t
$c_c
END_OF_DATA
then
    exit 0
else
    exit 1
fi
