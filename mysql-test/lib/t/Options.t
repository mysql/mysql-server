# -*- cperl -*-

# Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

use Test::More qw(no_plan);
use strict;

use_ok("My::Options");

my @tests=
(
  [
   ['--binlog-format=row', '--loose-skip-innodb', '--binlog-format=ms'],
   ['--binlog-format=row', '--loose-skip-innodb', '--binlog-format=statement'],
   ['--binlog-format=statement']
  ],

  [
   ['--binlog-format=row', '--loose-skip-innodb', '--binlog-format=statement'],
   ['--binlog-format=row', '--loose-skip-innodb', '--binlog-format=mixed'],
   ['--binlog-format=mixed']
  ],

  [
   ['--binlog-format=row', '--loose-skip-innodb', '--binlog-format=mixed'],
   ['--binlog-format=row', '--loose-skip-innodb', '--binlog-format=statement'],
   ['--binlog-format=statement']
  ],

  [
   ['--binlog-format=mixed', '--loose-skip-innodb', '--binlog-format=row'],
   ['--binlog-format=statement', '--loose-skip-innodb', '--binlog-format=row'],
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
   ['--max_binlog_size=1' ]
  ],

  [
   [ ],
   ['--max_binlog_size=default' ]
  ],

  [
   [ ],
   ['--max_binlog_size=1', '--binlog-format=row' ]
  ],
  [
   ['--binlog-format=statement' ],
   ['--max_binlog_size=1', '--binlog-format=row']
  ],

  [
   [ '--binlog-format=statement' ],
   ['--max_binlog_size=1' ]
  ],

 [
   [ '--binlog-format=statement' ],
   ['--max_binlog_size=1' ]
 ],

 [
  [ '--binlog-format=statement' ],
  ['--relay-log=/path/to/a/relay-log', '--binlog-format=row'],
  ['--relay-log=/path/to/a/relay-log', '--binlog-format=row' ]
 ],


 [
  [ '--binlog-format=statement' ],
  ['--relay-log=/path/to/a/relay-log', '--max_binlog_size=1'],
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
  my @result= My::Options::diff($from, $to);
  ok(My::Options::same(\@result, $test->[2]));
  if (!My::Options::same(\@result, $test->[2])){
    print "failed\n";
    print My::Options::toStr("result", @result);
    print My::Options::toStr("expect", @{$test->[2]});
  }
  print My::Options::toSQL(@result), "\n";
  print "\n";
}
