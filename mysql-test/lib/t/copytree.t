#!/usr/bin/perl
# -*- cperl -*-

# Copyright (c) 2007, 2022, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

use strict;
use warnings 'FATAL';
use lib "lib";

use Test::More qw(no_plan);

BEGIN { use_ok("My::File::Path");}

use File::Temp qw / tempdir /;

my $dir = tempdir( CLEANUP => 1 );
my $testdir="$dir/test";
my $test_todir="$dir/to";

my $subdir= "$testdir/test1/test2/test3";

#
# 1. Create, copy and remove a directory structure
#
mkpath($subdir);
ok( -d $subdir, "Check subdir is created");

copytree($testdir, $test_todir);
ok( -d $test_todir, "Check test_todir is created");
ok( -d "$test_todir/test1", "Check 'test1' is created");
ok( -d "$test_todir/test1/test2", "Check 'test2' is created");
ok( -d "$test_todir/test1/test2/test3", "Check 'test3' is created");


rmtree($testdir);
ok( ! -d $testdir, "Check testdir is gone");

rmtree($test_todir);
ok( ! -d $test_todir, "Check test_todir is gone");

