#!/usr/bin/perl

# vim:sw=2:ai

# test for multiple modify requests

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

my $hs = hstest::get_hs_connection(undef, 9999);
my $dbname = $hstest::conf{dbname};
$hs->open_index(1, $dbname, $table, '', 'k,v');

exec_multi(
  "DEL",
  [ 1, '=', [ 'k5' ], 1, 0, 'D' ],
  [ 1, '>=', [ 'k5' ], 2, 0 ],
);
exec_multi(
  "DELINS",
  [ 1, '>=', [ 'k6' ], 3, 0 ],
  [ 1, '=', [ 'k60' ], 1, 0, 'D' ],
  [ 1, '+', [ 'k60', 'INS' ] ],
  [ 1, '>=', [ 'k6' ], 3, 0 ],
);
exec_multi(
  "DELUPUP",
  [ 1, '>=', [ 'k7' ], 3, 0 ],
  [ 1, '=', [ 'k70' ], 1, 0, 'U', [ 'k70', 'UP' ] ],
  [ 1, '>=', [ 'k7' ], 3, 0 ],
);

sub exec_multi {
  my $mess = shift(@_);
  print "$mess\n";
  my $mres = $hs->execute_multi(\@_);
  for my $res (@$mres) {
    for my $fld (@$res) {
      print "[$fld]";
    }
    print "\n";
  }
}

