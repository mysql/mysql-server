#!/bin/sh
# Copyright (C) 1997-2003 MySQL AB
# For a more info consult the file COPYRIGHT distributed with this file

# This script writes on stdout SQL commands to generate all not
# existing MySQL system tables. It also replaces the help tables with
# new context from the manual (from fill_help_tables.sql).

# $1 - "test" or "real" or "verbose" variant of database
# $2 - path to mysql-database directory
# $3 - hostname  
# $4 - windows option

if test "$1" = ""
then
  echo "
This script writes on stdout SQL commands to generate all not
existing MySQL system tables. It also replaces the help tables with
new context from the manual (from fill_help_tables.sql).

Usage:
  mysql_create_system_tables [test|verbose|real] <path to mysql-database directory> <hostname> <windows option>
"
  exit
fi

mdata=$2
hostname=$3
windows=$4

# Initialize variables
c_d="" i_d=""
c_h="" i_h=""
c_u="" i_u=""
c_f="" i_f=""
c_t="" c_c=""
c_ht=""
c_hc=""
c_hr="" 
c_hk="" 
i_ht=""

# Check for old tables
if test ! -f $mdata/db.frm
then
  if test "$1" = "verbose" ; then
    echo "Preparing db table" 1>&2; 
  fi

  # mysqld --bootstrap wants one command/line
  c_d="$c_d CREATE TABLE db ("
  c_d="$c_d   Host char(60) binary DEFAULT '' NOT NULL,"
  c_d="$c_d   Db char(64) binary DEFAULT '' NOT NULL,"
  c_d="$c_d   User char(16) binary DEFAULT '' NOT NULL,"
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
  c_d="$c_d   Create_tmp_table_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_d="$c_d   Lock_tables_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_d="$c_d PRIMARY KEY Host (Host,Db,User),"
  c_d="$c_d KEY User (User)"
  c_d="$c_d )"
  c_d="$c_d comment='Database privileges';"
  
  i_d="INSERT INTO db VALUES ('%','test','','Y','Y','Y','Y','Y','Y','N','Y','Y','Y','Y','Y');
  INSERT INTO db VALUES ('%','test\_%','','Y','Y','Y','Y','Y','Y','N','Y','Y','Y','Y','Y');"
fi

if test ! -f $mdata/host.frm
then
  if test "$1" = "verbose" ; then
    echo "Preparing host table" 1>&2;
  fi

  c_h="$c_h CREATE TABLE host ("
  c_h="$c_h  Host char(60) binary DEFAULT '' NOT NULL,"
  c_h="$c_h  Db char(64) binary DEFAULT '' NOT NULL,"
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
  c_h="$c_h  Create_tmp_table_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_h="$c_h  Lock_tables_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_h="$c_h  PRIMARY KEY Host (Host,Db)"
  c_h="$c_h )"
  c_h="$c_h comment='Host privileges;  Merged with database privileges';"
fi

if test ! -f $mdata/user.frm
then
  if test "$1" = "verbose" ; then
    echo "Preparing user table" 1>&2;
  fi

  c_u="$c_u CREATE TABLE user ("
  c_u="$c_u   Host char(60) binary DEFAULT '' NOT NULL,"
  c_u="$c_u   User char(16) binary DEFAULT '' NOT NULL,"
  c_u="$c_u   Password char(41) binary DEFAULT '' NOT NULL,"
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
  c_u="$c_u   Show_db_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Super_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Create_tmp_table_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Lock_tables_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Execute_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Repl_slave_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_u="$c_u   Repl_client_priv enum('N','Y') DEFAULT 'N' NOT NULL,"
  c_u="$c_u   ssl_type enum('','ANY','X509', 'SPECIFIED') DEFAULT '' NOT NULL,"
  c_u="$c_u   ssl_cipher BLOB NOT NULL,"
  c_u="$c_u   x509_issuer BLOB NOT NULL,"
  c_u="$c_u   x509_subject BLOB NOT NULL,"
  c_u="$c_u   max_questions int(11) unsigned DEFAULT 0  NOT NULL,"
  c_u="$c_u   max_updates int(11) unsigned DEFAULT 0  NOT NULL,"
  c_u="$c_u   max_connections int(11) unsigned DEFAULT 0  NOT NULL,"
  c_u="$c_u   PRIMARY KEY Host (Host,User)"
  c_u="$c_u )"
  c_u="$c_u comment='Users and global privileges';"

  if test "$1" = "test" 
  then
    i_u="INSERT INTO user VALUES ('localhost','root','','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','','','','',0,0,0);
    INSERT INTO user VALUES ('$hostname','root','','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','','','','',0,0,0);
    REPLACE INTO user VALUES ('127.0.0.1','root','','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','','','','',0,0,0);
    INSERT INTO user (host,user) values ('localhost','');
    INSERT INTO user (host,user) values ('$hostname','');"
  else
    i_u="INSERT INTO user VALUES ('localhost','root','','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','','','','',0,0,0);"
    if test "$windows" = "0"
    then
      i_u="$i_u 
           INSERT INTO user VALUES ('$hostname','root','','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','','','','',0,0,0);
           INSERT INTO user (host,user) values ('$hostname','');
           INSERT INTO user (host,user) values ('localhost','');"
    else
      i_u="INSERT INTO user VALUES ('localhost','','','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','','','','',0,0,0);"
    fi
  fi 
fi

if test ! -f $mdata/func.frm
then
  if test "$1" = "verbose" ; then
    echo "Preparing func table" 1>&2;
  fi

  c_f="$c_f CREATE TABLE func ("
  c_f="$c_f   name char(64) binary DEFAULT '' NOT NULL,"
  c_f="$c_f   ret tinyint(1) DEFAULT '0' NOT NULL,"
  c_f="$c_f   dl char(128) DEFAULT '' NOT NULL,"
  c_f="$c_f   type enum ('function','aggregate') NOT NULL,"
  c_f="$c_f   PRIMARY KEY (name)"
  c_f="$c_f )"
  c_f="$c_f   comment='User defined functions';"
fi

if test ! -f $mdata/tables_priv.frm
then
  if test "$1" = "verbose" ; then
    echo "Preparing tables_priv table" 1>&2;
  fi

  c_t="$c_t CREATE TABLE tables_priv ("
  c_t="$c_t   Host char(60) binary DEFAULT '' NOT NULL,"
  c_t="$c_t   Db char(64) binary DEFAULT '' NOT NULL,"
  c_t="$c_t   User char(16) binary DEFAULT '' NOT NULL,"
  c_t="$c_t   Table_name char(60) binary DEFAULT '' NOT NULL,"
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
  if test "$1" = "verbose" ; then
    echo "Preparing columns_priv table" 1>&2;
  fi

  c_c="$c_c CREATE TABLE columns_priv ("
  c_c="$c_c   Host char(60) binary DEFAULT '' NOT NULL,"
  c_c="$c_c   Db char(64) binary DEFAULT '' NOT NULL,"
  c_c="$c_c   User char(16) binary DEFAULT '' NOT NULL,"
  c_c="$c_c   Table_name char(64) binary DEFAULT '' NOT NULL,"
  c_c="$c_c   Column_name char(64) binary DEFAULT '' NOT NULL,"
  c_c="$c_c   Timestamp timestamp(14),"
  c_c="$c_c   Column_priv set('Select','Insert','Update','References') DEFAULT '' NOT NULL,"
  c_c="$c_c   PRIMARY KEY (Host,Db,User,Table_name,Column_name)"
  c_c="$c_c )"
  c_c="$c_c   comment='Column privileges';"
fi

if test ! -f $mdata/help_topic.frm
then
  if test "$1" = "verbose" ; then
    echo "Preparing help_topic table" 1>&2;
  fi

  c_ht="$c_ht CREATE TABLE help_topic ("
  c_ht="$c_ht   help_topic_id    int unsigned not null,"
  c_ht="$c_ht   name             varchar(64) not null,"
  c_ht="$c_ht   help_category_id smallint unsigned not null,"
  c_ht="$c_ht   description      text not null,"
  c_ht="$c_ht   example          text not null,"
  c_ht="$c_ht   url              varchar(128) not null,"
  c_ht="$c_ht   primary key      (help_topic_id),"
  c_ht="$c_ht   unique index     (name)"
  c_ht="$c_ht )"
  c_ht="$c_ht   comment='help topics';"
fi

old_categories="yes"
		    
if test ! -f $mdata/help_category.frm
then
  if test "$1" = "verbose" ; then
    echo "Preparing help_category table" 1>&2;
  fi
  
  c_hc="$c_hc CREATE TABLE help_category ("
  c_hc="$c_hc   help_category_id   smallint unsigned not null,"
  c_hc="$c_hc   name               varchar(64) not null,"
  c_hc="$c_hc   parent_category_id smallint unsigned null,"
  c_hc="$c_hc   url                varchar(128) not null,"
  c_hc="$c_hc   primary key        (help_category_id),"
  c_hc="$c_hc   unique index       (name)"
  c_hc="$c_hc )"
  c_hc="$c_hc   comment='help categories';"
fi

if test ! -f $mdata/help_keyword.frm
then
  if test "$1" = "verbose" ; then
    echo "Preparing help_keyword table" 1>&2;
  fi

  c_hk="$c_hk CREATE TABLE help_keyword ("
  c_hk="$c_hk   help_keyword_id  int unsigned not null,"
  c_hk="$c_hk   name             varchar(64) not null,"
  c_hk="$c_hk   primary key      (help_keyword_id),"
  c_hk="$c_hk   unique index     (name)"
  c_hk="$c_hk )"
  c_hk="$c_hk   comment='help keywords';"
fi
				    
if test ! -f $mdata/help_relation.frm
then
  if test "$1" = "verbose" ; then
   echo "Preparing help_relation table" 1>&2;
  fi

  c_hr="$c_hr CREATE TABLE help_relation ("
  c_hr="$c_hr   help_topic_id    int unsigned not null references help_topic,"
  c_hr="$c_hr   help_keyword_id  int unsigned not null references help_keyword,"
  c_hr="$c_hr   primary key      (help_keyword_id, help_topic_id)"
  c_hr="$c_hr )"
  c_hr="$c_hr   comment='keyword-topic relation';"
fi

cat << END_OF_DATA
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

$c_ht
$c_hc
$c_hr
$c_hk
END_OF_DATA

