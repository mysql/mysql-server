#!/usr/bin/perl

# vim:sw=2:ai

# test for '>='

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

my $hs = hstest::get_hs_connection();
my $dbname = $hstest::conf{dbname};
$hs->open_index(1, $dbname, $table, '', 'k,v');
my $r = $hs->execute_single(1, '>=', [ '' ], 10000, 0);
shift(@$r);
for (my $i = 0; $i < $tablesize; ++$i) {
  my $k = $r->[$i * 2];
  my $v = $r->[$i * 2 + 1];
  print "$k $v\n";
}

my $aref = $dbh->selectall_arrayref("select k,v from $table order by k");
for my $row (@$aref) {
  my ($k, $v) = @$row;
  #print "$k $v\n";
}

