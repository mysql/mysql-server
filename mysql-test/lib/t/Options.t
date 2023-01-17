# -*- cperl -*-

# Copyright (c) 2008, 2023, Oracle and/or its affiliates.
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

use Test::More qw(no_plan);
use strict;
use warnings 'FATAL';
use lib "lib";

BEGIN { use_ok("My::Options");}

my @tests=
(
  [
   ['--binlog-format=row', '--binlog-format=ms'],
   ['--binlog-format=row', '--binlog-format=statement'],
   ['--binlog-format=statement']
  ],

  [
   ['--binlog-format=row', '--binlog-format=statement'],
   ['--binlog-format=row', '--binlog-format=mixed'],
   ['--binlog-format=mixed']
  ],

  [
   ['--binlog-format=row', '--binlog-format=mixed'],
   ['--binlog-format=row', '--binlog-format=statement'],
   ['--binlog-format=statement']
  ],

  [
   ['--binlog-format=mixed', '--binlog-format=row'],
   ['--binlog-format=statement', '--binlog-format=row'],
   [ ]
  ],

  [
   ['--binlog-format=row'],
   [ ],
   ['--binlog-format=default']
  ],

  [
   [ ],
   ['--binlog-format=row'],
   ['--binlog-format=row']
  ],

  [
   [ ],
   ['max_binlog_size=1' ],
   ['--max_binlog_size=1' ]
  ],

  [
   ['max_binlog_size=1' ],
   ['max_binlog_size=1' ],
   [ ],
  ],

  [
   ['max_binlog_size=1' ],
   [ ],
   ['--max_binlog_size=default' ]
  ],

  [
   [ ],
   ['max_binlog_size=1', '--binlog-format=row' ],
   ['--max_binlog_size=1', '--binlog-format=row' ]
  ],
  [
   ['--binlog-format=statement' ],
   ['max_binlog_size=1', '--binlog-format=row' ],
   ['--max_binlog_size=1', '--binlog-format=row']
  ],

  [
   [ '--binlog-format=statement' ],
   ['max_binlog_size=1', '--binlog-format=statement' ],
   ['--max_binlog_size=1' ]
  ],

 [
   [ '--binlog-format=statement' ],
   ['max_binlog_size=1', '--binlog-format=statement' ],
   ['--max_binlog_size=1' ]
 ],

 [
  [ '--binlog-format=statement' ],
  ['--relay-log=/path/to/a/relay-log', '--binlog-format=row'],
  ['--relay-log=/path/to/a/relay-log', '--binlog-format=row' ]
 ],


 [
  [ '--binlog-format=statement' ],
  ['--relay-log=/path/to/a/relay-log', 'max_binlog_size=1'],
  ['--max_binlog_size=1', '--relay-log=/path/to/a/relay-log', '--binlog-format=default' ]
 ],

 [
  [ '--slow-query-log=0' ],
  [ '--slow-query-log' ],
  [ '--slow-query-log' ]
 ],


);


my $test_no= 0;
foreach my $test (@tests){
  print "test", $test_no++, "\n";
  foreach my $opts (@$test){
    print My::Options::toStr("", @$opts);
  }
  my $from= $test->[0];
  my $to= $test->[1];
  my @result= sort(My::Options::diff($from, $to));
  ok(My::Options::same(\@result, $test->[2]));
  if (!My::Options::same(\@result, $test->[2])){
    print "failed\n";
    print My::Options::toStr("result", @result);
    print My::Options::toStr("expect", @{$test->[2]});
  }
  print My::Options::toSQL(@result), "\n";
  print "\n";
}
