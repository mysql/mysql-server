#!/usr/bin/perl

# vim:sw=2:ai

# test for 'IN', filters, and modifications

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
  "create table $table (k varchar(30) primary key, " .
  "v varchar(30) not null, v2 int not null) " .
  "engine = innodb");
srand(999);

my %valmap = ();

my $sth = $dbh->prepare("insert into $table values (?,?,?)");
for (my $i = 0; $i < $tablesize; ++$i) {
  my $k = "k" . $i;
  my $v = "v" . int(rand(1000)) . "-" . $i;
  my $v2 = ($i / 10) % 2;
  $sth->execute($k, $v, $v2);
  $valmap{$k} = $v;
}

my $hs = hstest::get_hs_connection(undef, 9999);
my $dbname = $hstest::conf{dbname};
$hs->open_index(1, $dbname, $table, '', 'k,v,v2', 'v2');
$hs->open_index(2, $dbname, $table, '', 'v', 'v2');
my $vs = [ 'k10', 'k20x', 'k30', 'k40', 'k50' ];
# update $table set v = 'MOD' where k in $vs and v2 = '1'
my $r = $hs->execute_single(2, '=', [ '' ], 10000, 0, 'U', [ 'MOD' ],
  [['F', '=', 0, '1']], 0, $vs);
$r = $hs->execute_single(1, '>=', [ '' ], 10000, 0);
shift(@$r);
print "HS\n";
my $len = scalar(@$r) / 3;
for (my $i = 0; $i < $len; ++$i) {
  my $k = $r->[$i * 3];
  my $v = $r->[$i * 3 + 1];
  my $v2 = $r->[$i * 3 + 2];
  print "$k $v $v2\n";
}

