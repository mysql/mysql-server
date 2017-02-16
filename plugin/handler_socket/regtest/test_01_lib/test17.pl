#!/usr/bin/perl

# vim:sw=2:ai

# test for string types

BEGIN {
	push @INC, "../common/";
};

use strict;
use warnings;
use bigint;
use hstest;

my $string_types = [
	[ 'CHAR(10)', undef, 1, 2, 5, 10 ],
	[ 'VARCHAR(10)', undef, 1, 2, 5, 10 ],
	[ 'BINARY(10)', undef, 1, 2, 5, 10 ],
	[ 'VARBINARY(10)', undef, 1, 2, 5, 10 ],
	[ 'CHAR(255)', undef, 1, 2, 5, 10, 100, 200, 255 ],
	[ 'VARCHAR(255)', undef, 1, 2, 5, 10, 100, 200, 255 ],
	[ 'VARCHAR(511)', undef, 1, 2, 5, 10, 100, 200, 511 ],
	[ 'LONGTEXT', 500, 1, 2, 5, 10, 100, 200, 511 ],
	[ 'LONGBLOB', 500, 1, 2, 5, 10, 100, 200, 511 ],
#	[ 'VARCHAR(4096)', 500, 1, 2, 5, 10, 100, 200, 255, 256, 4095 ],
#	[ 'VARCHAR(16383)', 500, 1, 2, 5, 10, 100, 200, 255, 256, 4095, 4096, 16383 ],
#	[ 'VARBINARY(16383)', 500, 1, 2, 5, 10, 100, 200, 255, 256, 4095, 4096, 16383 ],
];

my $table = 'hstesttbl';
my $dbh;
for my $rec (@$string_types) {
  my ($typ, $keylen, @vs) = @$rec;
  my @vals = ();
  for my $len (@vs) {
    my $s = '';
    my @arr = ();
    srand(999);
    # print "$len 1\n";
    for (my $i = 0; $i < $len; ++$i) {
      my $v = int(rand(10));
      $arr[$i] = chr(65 + $v);
    }
    # print "2\n";
    push(@vals, join('', @arr));
  }
  print("TYPE $typ\n");
  test_one($typ, $keylen, \@vals);
  print("\n");
}

sub test_one {
  my ($typ, $keylen, $values) = @_;
  my $keylen_str = '';
  if (defined($keylen)) {
    $keylen_str = "($keylen)";
  }
  $dbh = hstest::init_testdb();
  $dbh->do(
    "create table $table (" .
    "k $typ, " .
    "v1 varchar(2047), " .
    "v2 $typ, " .
    "primary key(k$keylen_str), " .
    "index i1(v1), index i2(v2$keylen_str, v1(300))) " .
    "engine = myisam default charset = latin1");
  my $hs = hstest::get_hs_connection(undef, 9999);
  my $dbname = $hstest::conf{dbname};
  $hs->open_index(1, $dbname, $table, '', 'k,v1,v2');
  $hs->open_index(2, $dbname, $table, 'i1', 'k,v1,v2');
  $hs->open_index(3, $dbname, $table, 'i2', 'k,v1,v2');
  for my $k (@$values) {
    my $kstr = 's' . $k;
    $hs->execute_single(1, '+', [ $k, $kstr, $k ], 0, 0);
  }
  # dump_table();
  for my $k (@$values) {
    my $kstr = 's' . $k;
    my ($rk, $rv1, $rv2);
    my $r;
    $r = $hs->execute_single(1, '=', [ $k ], 1, 0);
    shift(@$r);
    check_value("$typ:PK", @$r);
    $r = $hs->execute_single(2, '=', [ $kstr ], 1, 0);
    shift(@$r);
    check_value("$typ:I1", @$r);
    $r = $hs->execute_single(3, '=', [ $k, $kstr ], 1, 0);
    shift(@$r);
    check_value("$typ:I2", @$r);
    $r = $hs->execute_single(3, '=', [ $k ], 1, 0);
    shift(@$r);
    check_value("$typ:I2p", @$r);
  }
}

sub check_value {
  my ($mess, $rk, $rv1, $rv2) = @_;
  $rk ||= '';
  $rv1 ||= '';
  $rv2 ||= '';
  if ($rv2 ne $rk) {
    print "$mess: V2 NE\n$rk\n$rv2\n";
    return;
  }
  if ($rv1 ne 's' . $rk) {
    print "$mess: V1 NE\n$rk\n$rv1\n";
    return;
  }
  print "$mess: EQ\n";
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

