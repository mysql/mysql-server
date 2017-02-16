#!/usr/bin/perl

# vim:sw=2:ai

# test for auto_increment

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

my $sth = $dbh->prepare("insert into $table values (?,?,?)");
for (my $i = 0; $i < $tablesize; ++$i) {
  my $k = 0;
  my $v1 = "v1sql_" . $i;
  my $v2 = "v2sql_" . $i;
  $sth->execute($k, $v1, $v2);
}

my %valmap = ();

print "HSINSERT\n";
my $hs = hstest::get_hs_connection(undef, 9999);
my $dbname = $hstest::conf{dbname};
$hs->open_index(1, $dbname, $table, '', 'k,v1,v2');
# inserts with auto_increment
for (my $i = 0; $i < $tablesize; ++$i) {
  my $k = 0;
  my $v1 = "v1hs_" . $i;
  my $v2 = "v2hs_" . $i;
  my $r = $hs->execute_insert(1, [ $k, $v1, $v2 ]);
  my $err = $r->[0];
  if ($err != 0) {
    my $err_str = $r->[1];
    print "$err $err_str\n";
  } else {
    my $id = $r->[1];
    print "$id $v1\n";
  }
}
# make sure that it works even when inserts are pipelined. these requests
# are possibly executed in a single transaction.
for (my $i = 0; $i < $tablesize; ++$i) {
  my $k = 0;
  my $v1 = "v1hs3_" . $i;
  my $v2 = "v2hs3_" . $i;
  my $r = $hs->execute_multi([
  	[ 1, '+', [$k, $v1, $v2] ],
  	[ 1, '+', [$k, $v1, $v2] ],
  	[ 1, '+', [$k, $v1, $v2] ],
	]);
  for (my $i = 0; $i < 3; ++$i) {
    my $err = $r->[$i]->[0];
    if ($err != 0) {
      my $err_str = $r->[$i]->[1];
      print "$err $err_str\n";
    } else {
      my $id = $r->[$i]->[1];
      print "$id $v1\n";
    }
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

