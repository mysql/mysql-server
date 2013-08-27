#!/usr/bin/perl

# Copyright (C) 2000, 2001 MySQL AB
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA

# This is a test with uses two processes to a database.
# The other inserts records in two tables, the other does a lot of joins
# on these.
#
# Warning, the output from this test will differ in 'found' from time to time,
# but there should never be any errors
#

$host= shift || "";
$test_db="test";

use Mysql;
$|= 1;				# Autoflush

$org_file="/tmp/export-org.$$";
$tmp_file="/tmp/export-old.$$";
$tmp_file2="/tmp/export-new.$$";

print "Connection to database $test_db\n";

$dbh = Mysql->Connect($host) || die "Can't connect: $Mysql::db_errstr\n";
$dbh->SelectDB($test_db) || die "Can't use database $test_db: $Mysql::db_errstr\n";

$dbh->Query("drop table if exists export"); # Ignore this error

print "Creating table\n";

($dbh->Query("\
CREATE TABLE export (
  auto int(5) unsigned NOT NULL DEFAULT '0' auto_increment,
  string char(11) NOT NULL,
  tiny tinyint(4) NOT NULL DEFAULT '0',
  short smallint(6) NOT NULL DEFAULT '0',
  medium mediumint(8) NOT NULL DEFAULT '0',
  longint int(11) NOT NULL DEFAULT '0',
  longlong bigint(20) NOT NULL DEFAULT '0',
  real_float float(13,1) NOT NULL DEFAULT '0.0',
  real_double double(13,1) NOT NULL,
  utiny tinyint(3) unsigned NOT NULL DEFAULT '0',
  ushort smallint(5) unsigned zerofill NOT NULL DEFAULT '00000',
  umedium mediumint(8) unsigned NOT NULL DEFAULT '0',
  ulong int(11) unsigned NOT NULL DEFAULT '0',
  ulonglong bigint(20) unsigned NOT NULL DEFAULT '0',
  time_stamp timestamp,
  blob_col blob,
  tinyblob_col tinyblob,
  mediumblob_col tinyblob not null,
  longblob_col longblob not null,
  PRIMARY KEY (auto),
  KEY (string(5)),
  KEY unsigned_tinykey (utiny),
  KEY (tiny),
  KEY (short),
  FOREIGN KEY (medium) references export,
  KEY (longlong),
  KEY (real_float),
  KEY (real_double),
  KEY (ushort),
  KEY (umedium),
  KEY (ulong),
  KEY (ulonglong),
  KEY (ulonglong,ulong))"))  or die $Mysql::db_errstr;

print "Inserting data\n";

@A=("insert into export values (10, 1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1)",
    "insert into export values (NULL,2,2,2,2,2,2,2,2,2,2,2,2,2,NULL,NULL,NULL,2,2)",
    "insert into export values (0,1/3,3,3,3,3,3,3,3,3,3,3,3,3,3,'','','','3')",
    "insert into export values (0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,'-1')",
    "insert into export values (0,-4294967295,-4294967295,-4294967295,-4294967295,-4294967295,-4294967295,-4294967295,-4294967295,-4294967295,-4294967295,-4294967295,-4294967295,-4294967295,-4294967295,-4294967295,-4294967295,-4294967295,'-4294967295')",
    "insert into export values (0,4294967295,4294967295,4294967295,4294967295,4294967295,4294967295,4294967295,4294967295,4294967295,4294967295,4294967295,4294967295,4294967295,4294967295,4294967295,4294967295,4294967295,'4294967295')",
    "insert into export (string,tinyblob_col) values ('special','''\\0\\t\t\n''')",
    "insert into export (string) values (',,!!\\\\##')",
    "insert into export (tinyblob_col) values (',,!!!\\\\\\##')"
    );

foreach $A (@A)
{
  $dbh->Query($A) or die "query: $A returned: " . $Mysql::db_errstr;
}


print "Doing dump, load, check on different formats\n";

@A=(# Ordinary format
    "",
    # Field terminated by something
    "fields optionally enclosed by '+' escaped by '' terminated by ',,,' lines terminated by ',,,,'",
    "fields enclosed by '' terminated by ',' lines terminated by ''",
    "fields enclosed by '' terminated by ',' lines terminated by '!!'",
    #Fields enclosed by
    #"fields enclosed by '+' terminated by ''",
    #"fields enclosed by '+' terminated by '' lines terminated by ''",
    "fields enclosed by '+' terminated by ',,' lines terminated by '!!!'",
    "fields enclosed by '+' terminated by ',,' lines terminated by '##'",
    "fields enclosed by '+' escaped by '' terminated by ',,' lines terminated by '###'",
    "fields enclosed by '+' escaped by '' terminated by '!' lines terminated by ''",
    "fields enclosed by '+' terminated by ',' lines terminated by ''",
    #Fields optionally enclosed by
    "fields optionally enclosed by '+' terminated by ','",
    "fields optionally enclosed by '+' terminated by ',' lines terminated by ''",
    "fields optionally enclosed by '''' terminated by ',' lines starting by 'INSERT INTO a VALUES(' terminated by ');\n'",
    );

$dbh->Query("select * into outfile '$org_file' from export") or die $Mysql::db_errstr;


foreach $A (@A)
{
  unlink($tmp_file);
  unlink($tmp_file2);
  $dbh->Query("select * into outfile '$tmp_file' $A from export") or die $Mysql::db_errstr;
  $dbh->Query("delete from export") or die $Mysql::db_errstr;
  $dbh->Query("load data infile '$tmp_file' into table export $A") or die $Mysql::db_errstr . " with format: $A\n";
  $dbh->Query("select *  into outfile '$tmp_file2' from export") or die $Mysql::db_errstr;
  if (`cmp $tmp_file2 $org_file`)
  {
    print "Using format $A\n";
    print "$tmp_file2 and $org_file differ. Plese check files\n";
    exit 1;
  }
}


@A=(#Fixed size fields
    "fields enclosed by '' escaped by '' terminated by ''",
    "fields enclosed by '' escaped by '' terminated by '' lines terminated by '\\r\\n'",
    "fields enclosed by '' terminated by '' lines terminated by ''"
    );

unlink($org_file);

$field_list="auto,ifnull(string,''),tiny,short,medium,longint,longlong,real_float,ifnull(real_double,''),utiny,ushort,umedium,ulong,ulonglong,time_stamp";

$dbh->Query("select $field_list into outfile '$org_file' from export") or die $Mysql::db_errstr;

$field_list="auto,string,tiny,short,medium,longint,longlong,real_float,real_double,utiny,ushort,umedium,ulong,ulonglong,time_stamp";

foreach $A (@A)
{
  unlink($tmp_file);
  unlink($tmp_file2);
  $dbh->Query("select $field_list into outfile '$tmp_file' $A from export") or die $Mysql::db_errstr;
  $dbh->Query("delete from export") or die $Mysql::db_errstr;
  $dbh->Query("load data infile '$tmp_file' into table export $A ($field_list)") or die $Mysql::db_errstr;
  $dbh->Query("select $field_list into outfile '$tmp_file2' from export") or die $Mysql::db_errstr;
  if (`cmp $tmp_file2 $org_file`)
  {
    print "Using format $A\n";
    print "$tmp_file2 and $org_file differ. Plese check files\n";
    exit 1;
  }
}

unlink($tmp_file);
unlink($tmp_file2);
unlink($org_file);

$dbh->Query("drop table export") or die $Mysql::db_errstr;

print "Test ok\n";
exit 0;
