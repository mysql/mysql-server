#!/usr/bin/perl

# vim:sw=2:ai

# test for insert/update/delete

BEGIN {
	push @INC, "../common/";
};

use strict;
use warnings;
use hstest;

my $dbh = hstest::init_testdb();
my $table = 'hstesttbl';
my $tablesize = 256;
$dbh->do(
  "create table $table (" .
  "k varchar(30) primary key, " .
  "v1 varchar(30), " .
  "v2 varchar(30)) " .
  "engine = innodb default charset = binary");
srand(999);

my %valmap = ();

print "HSINSERT";
my $hs = hstest::get_hs_connection(undef, 9999);
my $dbname = $hstest::conf{dbname};
$hs->open_index(1, $dbname, $table, '', 'k,v1,v2');
for (my $i = 0; $i < $tablesize; ++$i) {
  my $k = "" . $i;
  my $v1 = "v1_" . $i;
  my $v2 = "v2_" . $i;
  my $r = $hs->execute_insert(1, [ $k, $v1, $v2 ]);
  my $err = $r->[0];
  if ($err != 0) {
    my $err_str = $r->[1];
    print "$err $err_str\n";
  }
}
undef $hs;

dump_table();

print "HSUPDATE";
$hs = hstest::get_hs_connection(undef, 9999);
$dbname = $hstest::conf{dbname};
$hs->open_index(2, $dbname, $table, '', 'v1');
for (my $i = 0; $i < $tablesize; ++$i) {
  my $r = $hs->execute_single(2, '=', [ $i ], 1000, 0, 'U', [ "mod_$i" ]);
  my $err = $r->[0];
  if ($err != 0) {
    my $err_str = $r->[1];
    print "$err $err_str\n";
  }
}
undef $hs;

dump_table();

print "HSDELETE\n";
$hs = hstest::get_hs_connection(undef, 9999);
$dbname = $hstest::conf{dbname};
$hs->open_index(3, $dbname, $table, '', '');
for (my $i = 0; $i < $tablesize; $i = $i + 2) {
  my $r = $hs->execute_single(3, '=', [ $i ], 1000, 0, 'D');
  my $err = $r->[0];
  if ($err != 0) {
    my $err_str = $r->[1];
    print "$err $err_str\n";
  }
}
undef $hs;

dump_table();

sub dump_table {
  print "DUMP_TABLE\n";
  my $aref = $dbh->selectall_arrayref("select k,v1,v2 from $table order by k");
  for my $row (@$aref) {
    my ($k, $v1, $v2) = @$row;
    $v1 = "[null]" if !defined($v1);
    $v2 = "[null]" if !defined($v2);
    print "$k $v1 $v2\n";
    # print "MISMATCH\n" if ($valmap{$k} ne $v);
  }
}

