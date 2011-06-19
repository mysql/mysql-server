#!/usr/bin/perl

# vim:sw=2:ai

# test for 'IN' and filters

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

my $hs = hstest::get_hs_connection();
my $dbname = $hstest::conf{dbname};
$hs->open_index(1, $dbname, $table, '', 'k,v,v2', 'v2');
my $vs = [ 'k10', 'k20x', 'k30', 'k40', 'k50' ];
# select k,v,v2 from $table where k in $vs
my $r = $hs->execute_single(1, '=', [ '' ], 10000, 0, undef, undef,
  [['F', '=', 0, '1']], 0, $vs);
shift(@$r);
print "HS\n";
my $len = scalar(@$r) / 3;
for (my $i = 0; $i < $len; ++$i) {
  my $k = $r->[$i * 3];
  my $v = $r->[$i * 3 + 1];
  my $v2 = $r->[$i * 3 + 2];
  print "$k $v $v2\n";
}

print "SQL\n";
my $aref = $dbh->selectall_arrayref(
  "select k,v,v2 from $table where k in ('k10', 'k20x', 'k30', 'k40', 'k50') "
  . "and v2 = '1' order by k");
for my $row (@$aref) {
  my ($k, $v, $v2) = @$row;
  print "$k $v $v2\n";
}
print "END\n";

