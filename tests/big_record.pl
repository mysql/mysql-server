#!/usr/bin/perl

# Copyright (c) 2000, 2010, Oracle and/or its affiliates. All rights reserved.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

# This is a test with stores big records in a blob.
# Note that for the default test the mysql server should have been
# started with at least 'mysqld --max_allowed_packet=30M' and you should have
# at least 256M memory in your computer.

use DBI;
use Getopt::Long;

$opt_host="";
$opt_user=$opt_password="";
$opt_db="test";
$opt_rows=20;			# Test of blobs up to ($rows-1)*100000+1 bytes
$opt_compress=0;
$opt_table="test_big_record";
$opt_loop_count=100000; # Change this to make test harder/easier

GetOptions("host=s","db=s","user=s", "password=s", "table=s", "rows=i",
	   "compress", "loop-count=i") || die "Aborted";

print "Connection to database $test_db\n";

$extra_options="";
$extra_options.=":mysql_compression=1" if ($opt_compress);

$dbh = DBI->connect("DBI:mysql:$opt_db:$host$extra_options",$opt_user,$opt_password) || die "Can't connect: $DBI::errstr\n";

$dbh->do("drop table if exists $opt_table");

print "Creating table $opt_table\n";

($dbh->do("\
CREATE TABLE $opt_table (
  auto int(5) unsigned NOT NULL DEFAULT '0' auto_increment,
  test longblob,
  PRIMARY KEY (auto))"))  or die $DBI::errstr;

print "Inserting $opt_rows records\n";

$|=1;	# Flush output to stdout to be able to monitor process

for ($i=0 ; $i < $opt_rows ; $i++)
{
  $tmp= chr(65+($i % 16)) x ($i*100000+1);
  $tmp= $dbh->quote($tmp);
  $dbh->do("insert into $opt_table (test) values ($tmp)") or die $DBI::errstr;
  print ".";
}

print "\nReading records\n";

$sth=$dbh->prepare("select * from $opt_table", { "mysql_use_result" => 1}) or die $dbh->errstr;

$sth->execute() or die $sth->errstr;

$i=0;
while (($row = $sth->fetchrow_arrayref))
{
  die "Record $i had wrong data in blob" if ($row->[1] ne (chr(65+($i % 16)) x ($i*100000+1)));
  $i++;
}

die "Didn't get all rows from server" if ($i != $opt_rows);

#
# Test by insert/updating/deleting random rows for a while
#

print "Testing insert/update/delete\n";

$max_row_id= $rows;
for ($i= 0 ; $i < $opt_loop_count ; $i++)
{
  $length= int(rand 65535);
  $tmp= chr(65+($i % 16)) x $length;
  $tmp= $dbh->quote($tmp);
  $dbh->do("insert into $opt_table (test) values ($tmp)") or die $DBI::errstr;
  $max_row_id++;
  $length=int(rand 65535);
  $tmp= chr(65+($i % 16)) x $length;
  $tmp= $dbh->quote($tmp);
  $id= int(rand $max_row_id);
  $dbh->do("update $opt_table set test= $tmp where auto= $id") or die $DBI::errstr;
  if (($i % 2) == 1)
  {
    $id= int(rand $max_row_id);
    $dbh->do("delete from $opt_table where auto= $id") or die $DBI::errstr;
  }
  print "." if ($i % ($opt_loop_count/100) == 1);
}

# $dbh->do("drop table $opt_table") or die $DBI::errstr;

print "\nTest ok\n";
exit 0;
