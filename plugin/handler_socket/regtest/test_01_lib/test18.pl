#!/usr/bin/perl

# vim:sw=2:ai

# tests that columns to be inserted are specified by open_index

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
  "create table $table (" .
  "k int primary key auto_increment, " .
  "v1 varchar(30), " .
  "v2 varchar(30)) " .
  "engine = myisam default charset = binary");
srand(999);

my %valmap = ();

print "HSINSERT\n";
my $hs = hstest::get_hs_connection(undef, 9999);
my $dbname = $hstest::conf{dbname};
$hs->open_index(1, $dbname, $table, '', 'v1');
# inserts with auto_increment
for (my $i = 0; $i < $tablesize; ++$i) {
  my $k = 0;
  my $v1 = "v1hs_" . $i;
  my $v2 = "v2hs_" . $i;
  my $r = $hs->execute_insert(1, [ $v1 ]);
  my $err = $r->[0];
  if ($err != 0) {
    my $err_str = $r->[1];
    print "$err $err_str\n";
  } else {
    my $id = $r->[1];
    print "$id $v1\n";
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

