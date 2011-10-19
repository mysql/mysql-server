#!/usr/bin/perl

# vim:sw=2:ai

# test for date/datetime types

BEGIN {
	push @INC, "../common/";
};

use strict;
use warnings;
use bigint;
use hstest;

my $datetime_types = [
	[ 'DATE', '0000-00-00', '2011-01-01', '9999-12-31' ],
	[ 'DATETIME', 0, '2011-01-01 18:30:25' ],
	[ 'TIME', 0, '18:30:25' ],
	[ 'YEAR(4)', 1901, 2011, 2155 ],
	# [ 'TIMESTAMP', 0, 999999999 ],   # DOES NOT WORK YET
];

my $table = 'hstesttbl';
my $dbh;
for my $rec (@$datetime_types) {
  my ($typ, @vals) = @$rec;
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

