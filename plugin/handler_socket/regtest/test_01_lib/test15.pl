#!/usr/bin/perl

# vim:sw=2:ai

# test for various numeric types

BEGIN {
	push @INC, "../common/";
};

use strict;
use warnings;
use bigint;
use hstest;

my $numeric_types = [
	[ 'TINYINT', -128, 127 ],
	[ 'TINYINT UNSIGNED', 0, 255 ],
	[ 'SMALLINT', -32768, 32768 ],
	[ 'SMALLINT UNSIGNED', 0, 65535 ],
	[ 'MEDIUMINT', -8388608, 8388607 ],
	[ 'MEDIUMINT UNSIGNED', 0, 16777215 ],
	[ 'INT', -2147483648, 2147483647 ],
	[ 'INT UNSIGNED', 0, 4294967295 ],
	[ 'BIGINT', -9223372036854775808, 9223372036854775807 ],
	[ 'BIGINT UNSIGNED', 0, 18446744073709551615 ],
	[ 'FLOAT', -32768, 32768 ],
	[ 'DOUBLE', -2147483648, 2147483647 ],
];

my $table = 'hstesttbl';
my $dbh;
for my $rec (@$numeric_types) {
  my ($typ, $minval, $maxval) = @$rec;
  my @vals = ();
  push(@vals, 0);
  push(@vals, 1);
  push(@vals, $maxval);
  if ($minval != 0) {
    push(@vals, -1);
    push(@vals, $minval);
  }
  my $v1 = $minval;
  my $v2 = $maxval;
  for (my $i = 0; $i < 5; ++$i) {
    $v1 /= 3;
    $v2 /= 3;
    if ($v1 != 0) {
      push(@vals, int($v1));
    }
    push(@vals, int($v2));
  }
  @vals = sort { $a <=> $b } @vals;
  print("TYPE $typ\n");
  test_one($typ, \@vals);
  print("\n");
}

sub test_one {
  my ($typ, $values) = @_;
  $dbh = hstest::init_testdb();
  $dbh->do(
    "create table $table (" .
    "k $typ primary key, " .
    "v1 varchar(512), " .
    "v2 $typ, " .
    "index i1(v1), index i2(v2, v1)) " .
    "engine = myisam default charset = binary");
  my $hs = hstest::get_hs_connection(undef, 9999);
  my $dbname = $hstest::conf{dbname};
  $hs->open_index(1, $dbname, $table, '', 'k,v1,v2');
  $hs->open_index(2, $dbname, $table, 'i1', 'k,v1,v2');
  $hs->open_index(3, $dbname, $table, 'i2', 'k,v1,v2');
  for my $k (@$values) {
    my $kstr = 's' . $k;
    $hs->execute_single(1, '+', [ $k, $kstr, $k ], 0, 0);
  }
  dump_table();
  for my $k (@$values) {
    my $kstr = 's' . $k;
    my ($rk, $rv1, $rv2);
    my $r;
    $r = $hs->execute_single(1, '=', [ $k ], 1, 0);
    shift(@$r);
    ($rk, $rv1, $rv2) = @$r;
    print "PK[$k] $rk $rv1 $rv2\n";
    $r = $hs->execute_single(2, '=', [ $kstr ], 1, 0);
    shift(@$r);
    ($rk, $rv1, $rv2) = @$r;
    print "I1[$kstr] $rk $rv1 $rv2\n";
    $r = $hs->execute_single(3, '=', [ $k, $kstr ], 1, 0);
    shift(@$r);
    ($rk, $rv1, $rv2) = @$r;
    print "I2[$k, $kstr] $rk $rv1 $rv2\n";
    $r = $hs->execute_single(3, '=', [ $k ], 1, 0);
    shift(@$r);
    ($rk, $rv1, $rv2) = @$r;
    print "I2p[$k] $rk $rv1 $rv2\n";
  }
}

sub dump_table {
  print "DUMP_TABLE_BEGIN\n";
  my $aref = $dbh->selectall_arrayref("select k,v1,v2 from $table order by k");
  for my $row (@$aref) {
    my ($k, $v1, $v2) = @$row;
    $v1 = "[null]" if !defined($v1);
    $v2 = "[null]" if !defined($v2);
    print "$k $v1 $v2\n";
    # print "MISMATCH\n" if ($valmap{$k} ne $v);
  }
  print "DUMP_TABLE_END\n";
}

