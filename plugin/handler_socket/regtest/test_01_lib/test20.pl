#!/usr/bin/perl

# vim:sw=2:ai

# test for a bug that table mdl is not released when open_index is failed

BEGIN {
	push @INC, "../common/";
};

use strict;
use warnings;
use hstest;

my $dbh = hstest::init_testdb();
my $dbname = $hstest::conf{dbname};
my $table = 'hstesttbl';

$dbh->do("drop table if exists $table");

my $hs = hstest::get_hs_connection();
my $r = $hs->open_index(1, $dbname, $table, '', 'k,v'); # fails
print "open_index 1st r=$r\n";
undef $hs;

$dbh->do(
  "create table $table (k varchar(30) primary key, v varchar(30) not null) " .
  "engine = innodb");

$hs = hstest::get_hs_connection();
$r = $hs->open_index(1, $dbname, $table, '', 'k,v'); # success
print "open_index 2nd r=$r\n";

