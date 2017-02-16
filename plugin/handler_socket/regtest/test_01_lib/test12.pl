#!/usr/bin/perl

# vim:sw=2:ai

# test for filters

BEGIN {
	push @INC, "../common/";
};

use strict;
use warnings;
use hstest;

my $dbh = hstest::init_testdb();
my $table = 'hstesttbl';
my $tablesize = 10;
$dbh->do(
  "create table $table " .
  "(k1 varchar(30) not null, k2 varchar(30) not null, " .
  "v1 int not null, v2 int not null, " .
  "primary key (k1, k2) ) engine = innodb");
srand(999);

my $sth = $dbh->prepare("insert into $table values (?,?,?,?)");
for (my $i = 0; $i < $tablesize; ++$i) {
  for (my $j = 0; $j < $tablesize; ++$j) {
    my $k1 = "k1_" . $i;
    my $k2 = "k2_" . $j;
    my $v1 = $i + $j;
    my $v2 = $i * $j;
    $sth->execute($k1, $k2, $v1, $v2);
  }
}

my $hs = hstest::get_hs_connection(undef, 9999);
my $dbname = $hstest::conf{dbname};
$hs->open_index(1, $dbname, $table, '', 'k1,k2,v1,v2', 'k2');
$hs->open_index(2, $dbname, $table, '', 'k1,k2,v1,v2', 'k1,k2,v1,v2');
$hs->open_index(3, $dbname, $table, '', 'v1', 'k1,v2');

exec_multi(
  4, "VAL",
  [ 1, '>=', [ '', '' ], 1000, 0 ],
);
# all

# select k1, k2, v1, v2 ... where (k1, k2) >= ('', '') and k2 = 'k2_5'
exec_single(
  4, "FILTER",
  [ 1, '>=', [ '', '' ], 1000, 0, undef, undef, [ [ 'F', '=', 0, 'k2_5' ] ] ]
);

# same as above
exec_multi(
  4, "FILTER",
  [ 1, '>=', [ '', '' ], 1000, 0, undef, undef, [ [ 'F', '=', 0, 'k2_5' ] ] ],
);

# select k1, k2, v1, v2 ... where (k1, k2) >= ('', '') and v1 = 3
exec_multi(
  4, "FILTER",
  [ 2, '>=', [ '', '' ], 1000, 0, undef, undef, [ [ 'F', '=', 2, 3 ] ] ],
);

# select k1, k2, v1, v2 ... where (k1, k2) >= ('k1_1', '') and k1 <= 'k1_2'
exec_multi(
  4, "FILTER",
  [ 2, '>=', [ 'k1_1', '' ], 1000, 0, undef, undef,
    [ [ 'W', '<=', 0, 'k1_2' ] ] ],
);

# select k1, k2, v1, v2 ... where (k1, k2) >= ('k1_1', '') and k1 <= 'k1_2'
#   and v2 >= 10
exec_multi(
  4, "FILTER",
  [ 2, '>=', [ 'k1_1', '' ], 1000, 0, undef, undef,
    [ [ 'W', '<=', 0, 'k1_2' ], [ 'F', '>=', 3, 10 ] ] ],
);

# update ... set v2 = -1 where (k1, k2) >= ('k1_3', '') and v2 = 10
exec_multi(
  4, "FILTER",
  [ 3, '>=', [ 'k1_1', '' ], 1000, 0, 'U', [ -1 ],
    [ [ 'F', '=', 1, 10 ] ] ],
);

exec_multi(
  4, "VAL",
  [ 1, '>=', [ '', '' ], 1000, 0 ],
);
# all

exit 0;

sub exec_single {
  my ($width, $mess, $req) = @_;
  print "$mess\n";
  my $res = $hs->execute_single(@$req);
  {
    my $code = shift(@$res);
    print "code=$code\n";
    my $i = 0;
    for my $fld (@$res) {
      print "[$fld]";
      if (++$i >= $width) {
	print "\n";
	$i = 0;
      }
    }
    print "\n";
  }
}

sub exec_multi {
  my $width = shift(@_);
  my $mess = shift(@_);
  print "$mess\n";
  my $mres = $hs->execute_multi(\@_);
  for my $res (@$mres) {
    my $code = shift(@$res);
    print "code=$code\n";
    my $i = 0;
    for my $fld (@$res) {
      print "[$fld]";
      if (++$i >= $width) {
	print "\n";
	$i = 0;
      }
    }
    print "\n";
  }
}

