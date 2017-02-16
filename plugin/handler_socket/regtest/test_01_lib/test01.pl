#!/usr/bin/perl

# vim:sw=2:ai

# test for libmysql

BEGIN {
	push @INC, "../common/";
};

use strict;
use warnings;
use hstest;

my $dbh = hstest::init_testdb();
my $table = 'hstesttbl';
my $tablesize = 100;
$dbh->do(
  "create table $table (k varchar(30) primary key, v varchar(30) not null) " .
  "engine = innodb");
srand(999);

my %valmap = ();

my $sth = $dbh->prepare("insert into $table values (?,?)");
for (my $i = 0; $i < $tablesize; ++$i) {
  my $k = "k" . $i;
  my $v = "v" . int(rand(1000)) . $i;
  $sth->execute($k, $v);
  $valmap{$k} = $v;
}

my $aref = $dbh->selectall_arrayref("select k,v from $table order by k");
for my $row (@$aref) {
  my ($k, $v) = @$row;
  print "$k $v\n";
}

