#!/usr/bin/perl

# vim:sw=2:ai

# test for filters

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
my $datetime_types = [
        [ 'DATE', '0000-00-00', '2011-01-01', '9999-12-31' ],
        [ 'DATETIME', 0, '2011-01-01 18:30:25' ],
        [ 'TIME', 0, '18:30:25' ],
        [ 'YEAR(4)', 1901, 2011, 2155 ],
        # [ 'TIMESTAMP', 0, 999999999 ],   # DOES NOT WORK YET
];
my $string_types = [
        [ 'CHAR(10)', undef, 1, 2, 5, 10 ],
        [ 'VARCHAR(10)', undef, 1, 2, 5, 10 ],
        [ 'BINARY(10)', undef, 1, 2, 5, 10 ],
        [ 'VARBINARY(10)', undef, 1, 2, 5, 10 ],
        [ 'CHAR(255)', undef, 1, 2, 5, 10, 100, 200, 255 ],
        [ 'VARCHAR(255)', undef, 1, 2, 5, 10, 100, 200, 255 ],
        [ 'VARCHAR(511)', undef, 1, 2, 5, 10, 100, 200, 511 ],
        [ 'LONGTEXT', 500, 1, 2, 5, 10, 100, 200, 511 ], # NOT SUPPORTED YET
        [ 'LONGBLOB', 500, 1, 2, 5, 10, 100, 200, 511 ], # NOT SUPPORTED YET
#       [ 'VARCHAR(4096)', 500, 1, 2, 5, 10, 100, 200, 255, 256, 4095 ],
#       [ 'VARCHAR(16383)', 500, 1, 2, 5, 10, 100, 200, 255, 256, 4095, 4096, 16383 ],
#       [ 'VARBINARY(16383)', 500, 1, 2, 5, 10, 100, 200, 255, 256, 4095, 4096, 16383 ],
];

for my $rec (@$numeric_types) {
  my ($typ, $minval, $maxval) = @$rec;
  my @vals = ();
  push(@vals, 0);
  push(@vals, $maxval);
  if ($minval != 0) {
    push(@vals, $minval);
  }
  my $v1 = $minval;
  my $v2 = $maxval;
  for (my $i = 0; $i < 3; ++$i) {
    $v1 /= 3;
    $v2 /= 3;
    push(@vals, int($v1));
    push(@vals, int($v2));
  }
  my %vm = map { $_ => 1 } @vals;
  @vals = sort { $a <=> $b } keys %vm;
  push(@vals, undef);
  test_one($typ, undef, \@vals);
}

for my $rec (@$datetime_types) {
  my ($typ, @vals) = @$rec;
  push(@vals, undef);
  test_one($typ, undef, \@vals);
}

for my $rec (@$string_types) {
  my ($typ, $keylen, @vs) = @$rec;
  my @vals = ();
  srand(999);
  for my $len (@vs) {
    my $s = '';
    my @arr = ();
    # print "$len 1\n";
    for (my $i = 0; $i < $len; ++$i) {
      my $v = int(rand(10));
      $arr[$i] = chr(65 + $v);
    }
    # print "2\n";
    push(@vals, join('', @arr));
  }
  push(@vals, undef);
  test_one($typ, $keylen, \@vals);
}

my $hs;

sub test_one {
  my ($typ, $keylen, $values) = @_;
  print "\n$typ -------------------------------------------------\n\n";
  my $keylen_str = '';
  if (defined($keylen)) {
    $keylen_str = "($keylen)";
  }
  my $dbh = hstest::init_testdb();
  my $table = 'hstesttbl';
  my $tablesize = 3;
  $dbh->do(
    "create table $table " .
    "(k1 int not null, k2 int not null, " .
    "v1 int not null, v2 $typ default null, " .
    "primary key (k1, k2) ) engine = innodb");
  my $sth = $dbh->prepare("insert into $table values (?,?,?,?)");
  for (my $i = 0; $i < $tablesize; ++$i) {
    my $j = 0;
    for my $v (@$values) {
      $sth->execute($i, $j, $i, $v);
      ++$j;
    }
  }
  $hs = hstest::get_hs_connection(undef, 9999);
  my $dbname = $hstest::conf{dbname};
  $hs->open_index(1, $dbname, $table, '', 'k1,k2,v1,v2', 'v2');
  my $minval = $values->[0];
  # select * ... where (k1, k2) >= ('', $minval)
  exec_multi(
    4, "FILTER($typ) NO FILTER",
    [ 1, '>=', [ '', $minval ], 1000, 0 ]
  );
  for my $v (@$values) {
    my $vstr = defined($v) ? $v : 'NULL';
    # select * ... where (k1, k2) >= ('', $minval) and v2 = $v
    exec_multi(
      4, "FILTER($typ) v2 = $vstr",
      [ 1, '>=', [ '', $minval ], 1000, 0, undef, undef, [ [ 'F', '=', 0, $v ] ] ]
    );
    # select * ... where (k1, k2) >= ('', $minval) and v2 != $v
    exec_multi(
      4, "FILTER($typ) v2 != $vstr",
      [ 1, '>=', [ '', $minval ], 1000, 0, undef, undef, [ [ 'F', '!=', 0, $v ] ] ]
    );
    # select * ... where (k1, k2) >= ('', $minval) and v2 >= $v
    exec_multi(
      4, "FILTER($typ) v2 >= $vstr",
      [ 1, '>=', [ '', $minval ], 1000, 0, undef, undef, [ [ 'F', '>=', 0, $v ] ] ]
    );
    # select * ... where (k1, k2) >= ('', $minval) and v2 < $v
    exec_multi(
      4, "FILTER($typ) v2 < $vstr",
      [ 1, '>=', [ '', $minval ], 1000, 0, undef, undef, [ [ 'F', '<', 0, $v ] ] ]
    );
    # select * ... where (k1, k2) >= ('', $minval) and v2 > $v
    exec_multi(
      4, "FILTER($typ) v2 > $vstr",
      [ 1, '>=', [ '', $minval ], 1000, 0, undef, undef, [ [ 'F', '>', 0, $v ] ] ]
    );
    # select * ... where (k1, k2) >= ('', $minval) and v2 <= $v
    exec_multi(
      4, "FILTER($typ) v2 <= $vstr",
      [ 1, '>=', [ '', $minval ], 1000, 0, undef, undef, [ [ 'F', '<=', 0, $v ] ] ]
    );
  }
  undef $hs;
}

sub exec_multi {
  my $width = shift(@_);
  my $mess = shift(@_);
  print "$mess\n";
  my $mres = $hs->execute_multi(\@_);
  for my $res (@$mres) {
    my $code = shift(@$res);
    my $nrows = $code == 0 ? scalar(@$res) / $width : 0;
    print "code=$code rows=$nrows\n";
    my $i = 0;
    for my $fld (@$res) {
      $fld = 'NULL' if !defined($fld);
      print "[$fld]";
      if (++$i >= $width) {
	print "\n";
	$i = 0;
      }
    }
    print "\n";
  }
}

