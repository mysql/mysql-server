#!/usr/bin/perl

# vim:sw=2:ai

# test for nulls

BEGIN {
	push @INC, "../common/";
};

# use strict;
use warnings;
use hstest;

my $dbh = hstest::init_testdb();
my $table = 'hstesttbl';
my $tablesize = 100;
$dbh->do(
  "create table $table (" .
  "k int primary key, v1 varchar(30), v2 varchar(30), " .
  "key idxv1 (v1) " .
  ") engine = innodb");
srand(999);

my %valmap = ();

my $sth = $dbh->prepare("insert into $table values (?,?,?)");
for (my $i = 0; $i < $tablesize; ++$i) {
  my $k = "" . $i;
  my $v1 = "1v" . int(rand(1000)) . $i;
  my $v2 = "2v" . int(rand(1000)) . $i;
  if ($i % 10 == 3) {
    $v1 = undef;
  }
  $sth->execute($k, $v1, $v2);
  $valmap{$k} = $v1;
}

print "MY\n";
my $aref = $dbh->selectall_arrayref("select k,v1,v2 from $table order by k");
for my $row (@$aref) {
  my ($k, $v1, $v2) = @$row;
  $v1 = "[NULL]" if (!defined($v1));
  print "$k $v1 $v2\n";
}

print "HS\n";
my $hs = hstest::get_hs_connection();
my $dbname = $hstest::conf{dbname};
$hs->open_index(1, $dbname, $table, '', 'k,v1,v2');
my $r = $hs->execute_single(1, '>=', [ '' ], 10000, 0);
shift(@$r);
for (my $i = 0; $i < $tablesize; ++$i) {
  my $k = $r->[$i * 3];
  my $v1 = $r->[$i * 3 + 1];
  my $v2 = $r->[$i * 3 + 2];
  $v1 = "[NULL]" if (!defined($v1));
  print "$k $v1 $v2\n";
}

print "2ndIDX\n";
$hs->open_index(2, $dbname, $table, 'idxv1', 'k,v1,v2');
for (my $i = 0; $i < $tablesize; ++$i) {
  my $k = "" . $i;
  my $v1 = $valmap{$k};
  next if !defined($v1);
  my $r = $hs->execute_single(2, '=', [ $v1 ], 1, 0);
  shift(@$r);
  my $r_k = $r->[0];
  my $r_v1 = $r->[1];
  my $r_v2 = $r->[2];
  print "2ndidx $k $v1 => $r_k $r_v1 $r_v2\n";
}

print "2ndIDX NULL\n";
{
  my %rvals = ();
  my $v1 = undef;
  my @arr;
  push(@arr, undef);
  my $kv = \@arr;
  my $r = $hs->execute_single(2, "=", $kv, 10000, 0);
  shift(@$r);
  for (my $i = 0; $i < scalar(@$r); $i += 3) {
    my $k = $r->[$i];
    my $v1 = $r->[$i + 1];
    my $v2 = $r->[$i + 2];
    $rvals{$k} = [ $k, $v1, $v2 ];
  }
  for my $i (sort { $a <=> $b } keys %rvals) {
    my $rec = $rvals{$i};
    my $k = $rec->[0];
    my $v1 = $rec->[1];
    my $v2 = $rec->[2];
    print "2ndidxnull $k $v2\n";
  }
}

