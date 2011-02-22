#!/usr/bin/perl

# vim:sw=2:ai

# test for not-found

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

dump_rec($hs, 1, 'k5');      # found
dump_rec($hs, 1, 'k000000'); # notfound

sub dump_rec {
  my ($hs, $idxid, $key) = @_;
  my $r = $hs->execute_single($idxid, '=', [ $key ], 1, 0);
  for my $fld (@$r) {
    print "[$fld]";
  }
  print "\n";
}

