#!/usr/bin/perl
# -*- cperl -*-

use strict;

use My::File::Path;

use Test::Simple tests => 8;
use File::Temp qw / tempdir /;
my $dir = tempdir( CLEANUP => 1 );
my $testdir="$dir/test";

my $subdir= "$testdir/test1/test2/test3";

#
# 1. Create and remove a directory structure
#
mkpath($subdir);
ok( -d $subdir, "Check '$subdir' is created");

rmtree($testdir);
ok( ! -d $testdir, "Check '$testdir' is gone");

#
# 2. Create and remove a directory structure
# where one directory is chmod to 0000
#
mkpath($subdir);
ok( -d $subdir, "Check '$subdir' is created");

ok( chmod(0000, $subdir) == 1 , "Check one dir was chmoded");

rmtree($testdir);
ok( ! -d $testdir, "Check '$testdir' is gone");

#
# 3. Create and remove a directory structure
# where one file is chmod to 0000
#
mkpath($subdir);
ok( -d $subdir, "Check '$subdir' is created");

my $testfile= "$subdir/test.file";
open(F, ">", $testfile) or die;
print F "hello\n";
close(F);

ok( chmod(0000, $testfile) == 1 , "Check one file was chmoded");

rmtree($testdir);
ok( ! -d $testdir, "Check '$testdir' is gone");

