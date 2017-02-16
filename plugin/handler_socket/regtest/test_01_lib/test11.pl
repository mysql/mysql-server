#!/usr/bin/perl

# vim:sw=2:ai

# test for increment/decrement

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
  my $v = $i;
  $sth->execute($k, $v);
  $valmap{$k} = $v;
}

my $hs = hstest::get_hs_connection(undef, 9999);
my $dbname = $hstest::conf{dbname};
$hs->open_index(1, $dbname, $table, '', 'k,v');
$hs->open_index(2, $dbname, $table, '', 'v');

exec_multi(
  "VAL",
  [ 1, '=', [ 'k5' ], 1, 0 ],
  [ 1, '=', [ 'k6' ], 1, 0 ],
  [ 1, '=', [ 'k7' ], 1, 0 ],
  [ 1, '=', [ 'k8' ], 1, 0 ],
);
# 5, 6, 7, 8

exec_multi(
  "INCREMENT",
  [ 2, '=', [ 'k5' ], 1, 0, '+', [ 3 ] ],
  [ 2, '=', [ 'k6' ], 1, 0, '+', [ 12 ] ],
  [ 2, '=', [ 'k7' ], 1, 0, '+', [ -11 ] ],
  [ 2, '=', [ 'k8' ], 1, 0, '+', [ -15 ] ],
);

exec_multi(
  "VAL",
  [ 1, '=', [ 'k5' ], 1, 0 ],
  [ 1, '=', [ 'k6' ], 1, 0 ],
  [ 1, '=', [ 'k7' ], 1, 0 ],
  [ 1, '=', [ 'k8' ], 1, 0 ],
);
# 8, 18, -4, -7

exec_multi(
  "DECREMENT",
  [ 2, '=', [ 'k5' ], 1, 0, '-', [ 2 ] ],
  [ 2, '=', [ 'k6' ], 1, 0, '-', [ 24 ] ],
  [ 2, '=', [ 'k7' ], 1, 0, '-', [ 80 ] ],
  [ 2, '=', [ 'k8' ], 1, 0, '-', [ -80 ] ],
);
# mod, no, mod, no

exec_multi(
  "VAL",
  [ 1, '=', [ 'k5' ], 1, 0 ],
  [ 1, '=', [ 'k6' ], 1, 0 ],
  [ 1, '=', [ 'k7' ], 1, 0 ],
  [ 1, '=', [ 'k8' ], 1, 0 ],
);
# 6, 18, -84, -7

exec_multi(
  "INCREMENT",
  [ 2, '=', [ 'k5' ], 1, 0, '+?', [ 1 ] ],
  [ 2, '=', [ 'k5' ], 1, 0, '+?', [ 1 ] ],
  [ 2, '=', [ 'k5' ], 1, 0, '+?', [ 1 ] ],
  [ 2, '=', [ 'k5' ], 1, 0, '+?', [ 1 ] ],
  [ 2, '=', [ 'k5' ], 1, 0, '+?', [ 1 ] ],
  [ 2, '=', [ 'k5' ], 1, 0, '+?', [ 1 ] ],
  [ 2, '=', [ 'k5' ], 1, 0, '+?', [ 1 ] ],
  [ 2, '=', [ 'k5' ], 1, 0, '+?', [ 1 ] ],
  [ 2, '=', [ 'k5' ], 1, 0, '+?', [ 1 ] ],
);

exec_multi(
  "VAL",
  [ 1, '=', [ 'k5' ], 1, 0 ],
);
# 15

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

