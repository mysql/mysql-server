#!/usr/bin/perl
# -*- cperl -*-

use strict;

use My::File::Path;

use Test::Simple tests => 7;
use File::Temp qw / tempdir /;
my $dir = tempdir( CLEANUP => 1 );
my $testdir="$dir/test";
my $test_todir="$dir/to";

my $subdir= "$testdir/test1/test2/test3";

#
# 1. Create, copy and remove a directory structure
#
mkpath($subdir);
ok( -d $subdir, "Check '$subdir' is created");

copytree($testdir, $test_todir);
ok( -d $test_todir, "Check '$test_todir' is created");
ok( -d "$test_todir/test1", "Check 'test1' is created");
ok( -d "$test_todir/test1/test2", "Check 'test2' is created");
ok( -d "$test_todir/test1/test2/test3", "Check 'test3' is created");


rmtree($testdir);
ok( ! -d $testdir, "Check '$testdir' is gone");

rmtree($test_todir);
ok( ! -d $test_todir, "Check '$test_todir' is gone");

