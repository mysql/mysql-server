#! /usr/bin/perl

# Test START TRANSACTION WITH CONSISTENT SNAPSHOT.
# With MWL#116, this is implemented so it is actually consistent.

use strict;
use warnings;

use DBI;

my $UPDATERS= 10;
my $READERS= 5;

my $ROWS= 50;
my $DURATION= 20;

my $stop_time= time() + $DURATION;

sub my_connect {
  my $dbh= DBI->connect("dbi:mysql:mysql_socket=/tmp/mysql.sock;database=test",
                        "root", undef, { RaiseError=>1, PrintError=>0, AutoCommit=>0});
  $dbh->do("SET SESSION TRANSACTION ISOLATION LEVEL REPEATABLE READ");
  $dbh->do("SET SESSION autocommit = 0");
  return $dbh;
}

sub my_setup {
  my $dbh= my_connect();

  $dbh->do("DROP TABLE IF EXISTS test_consistent_snapshot1, test_consistent_snapshot2");
  $dbh->do(<<TABLE);
CREATE TABLE test_consistent_snapshot1 (
  a INT PRIMARY KEY,
  b INT NOT NULL
) ENGINE=InnoDB
TABLE
  $dbh->do(<<TABLE);
CREATE TABLE test_consistent_snapshot2(
  a INT PRIMARY KEY,
  b INT NOT NULL
) ENGINE=PBXT
TABLE

  for (my $i= 0; $i < $ROWS; $i++) {
    my $value= int(rand()*1000);
    $dbh->do("INSERT INTO test_consistent_snapshot1 VALUES (?, ?)", undef,
             $i, $value);
    $dbh->do("INSERT INTO test_consistent_snapshot2 VALUES (?, ?)", undef,
             $i, -$value);
  }
  $dbh->commit();
  $dbh->disconnect();
}

sub my_updater {
  my $dbh= my_connect();

  while (time() < $stop_time) {
    my $i1= int(rand()*$ROWS);
    my $i2= int(rand()*$ROWS);
    my $v= int(rand()*99)-49;
    $dbh->do("UPDATE test_consistent_snapshot1 SET b = b + ? WHERE a = ?",
             undef, $v, $i1);
    $dbh->do("UPDATE test_consistent_snapshot2 SET b = b - ? WHERE a = ?",
             undef, $v, $i2);
    $dbh->commit();
  }

  $dbh->disconnect();
  exit(0);
}

sub my_reader {
  my $dbh= my_connect();

  my $iteration= 0;
  while (time() < $stop_time) {
    $dbh->do("START TRANSACTION WITH CONSISTENT SNAPSHOT");
    my $s1= $dbh->selectrow_arrayref("SELECT SUM(b) FROM test_consistent_snapshot1");
    $s1= $s1->[0];
    my $s2= $dbh->selectrow_arrayref("SELECT SUM(b) FROM test_consistent_snapshot2");
    $s2= $s2->[0];
    $dbh->commit();
    if ($s1 + $s2 != 0) {
      print STDERR "Found inconsistency, s1=$s1 s2=$s2 iteration=$iteration\n";
      last;
    }
    ++$iteration;
  }

  $dbh->disconnect();
  exit(0);
}

my_setup();

for (1 .. $UPDATERS) {
  fork() || my_updater();
}

for (1 .. $READERS) {
  fork() || my_reader();
}

waitpid(-1, 0) for (1 .. ($UPDATERS + $READERS));

print "All checks done\n";
