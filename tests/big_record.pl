#!/usr/bin/perl

# This is a test with stores big records in a blob
# Note that for the default test the mysql server should have been
# started with at least 'mysqld -O max_allowed_packet=200k'

$host= shift || "";
$test_db="test";
$opt_user=$opt_password="";

use DBI;
$|= 1;				# Autoflush

$table="test_big_record";
$rows=20;			# Test of blobs up to ($rows-1)*10000+1 bytes

print "Connection to database $test_db\n";

$dbh = DBI->connect("DBI:mysql:$test_db:$host",$opt_user,$opt_password) || die "Can't connect: $DBI::errstr\n";

$dbh->do("drop table if exists $table");

print "Creating table $table\n";

($dbh->do("\
CREATE TABLE $table (
  auto int(5) unsigned NOT NULL DEFAULT '0' auto_increment,
  test mediumblob,
  PRIMARY KEY (auto))"))  or die $DBI::errstr;

print "Inserting $rows records\n";

for ($i=0 ; $i < $rows ; $i++)
{
  $tmp= chr(65+$i) x ($i*10000+1);
  $tmp= $dbh->quote($tmp);
  $dbh->do("insert into $table (test) values ($tmp)") or die $DBI::errstr;
}

print "Testing records\n";

$sth=$dbh->prepare("select * from $table") or die $dbh->errstr;
$sth->execute() or die $sth->errstr;

$i=0;
while (($row = $sth->fetchrow_arrayref))
{
  print $row->[0]," ",length($row->[1]),"\n";
  die "Record $i had wrong data in blob" if ($row->[1] ne (chr(65+$i)) x ($i*10000+1));
  $i++;
}

die "Didn't get all rows from server" if ($i != $rows);

$dbh->do("drop table $table") or die $DBI::errstr;

print "Test ok\n";
exit 0;
