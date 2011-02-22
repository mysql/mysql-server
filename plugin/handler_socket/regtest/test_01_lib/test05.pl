#!/usr/bin/perl

# vim:sw=2:ai

# test for binary cleanness (#3)

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
  "create table $table (k varchar(30) primary key, v varchar(30)) " .
  "engine = innodb default charset = binary");
srand(999);

my %valmap = ();

print "WR\n";
my $sth = $dbh->prepare("insert into $table values (?,?)");
for (my $i = 0; $i < $tablesize; ++$i) {
  my $k = "" . $i;
  my $v = ($i % 2 == 1) ? $i : undef;
  $sth->execute($k, $v);
  $v = "[null]" if !defined($v);
  print "$k $v\n";
  $valmap{$k} = $v;
}

my $hs = hstest::get_hs_connection();
my $dbname = $hstest::conf{dbname};
$hs->open_index(1, $dbname, $table, '', 'k,v');
my $r = $hs->execute_single(1, '>=', [ '' ], 10000, 0);
shift(@$r);
print "HS\n";
for (my $i = 0; $i < $tablesize; ++$i) {
  my $k = $r->[$i * 2];
  my $v = $r->[$i * 2 + 1];
  $v = "[null]" if !defined($v);
  print "$k $v\n";
  print "MISMATCH\n" if ($valmap{$k} ne $v);
}
undef $hs;

print "MY\n";
my $aref = $dbh->selectall_arrayref("select k,v from $table order by k");
for my $row (@$aref) {
  my ($k, $v) = @$row;
  $v = "[null]" if !defined($v);
  print "$k $v\n";
  print "MISMATCH\n" if ($valmap{$k} ne $v);
}

