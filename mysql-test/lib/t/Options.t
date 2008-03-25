
# -*- cperl -*-
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
   ['-O', 'max_binlog_size=1' ],
   ['--max_binlog_size=1' ]
  ],

  [
   ['-O', 'max_binlog_size=1' ],
   ['-O', 'max_binlog_size=1' ],
   [ ],
  ],

  [
   ['-O', 'max_binlog_size=1' ],
   [ ],
   ['--max_binlog_size=default' ]
  ],

  [
   [ ],
   ['-O', 'max_binlog_size=1', '--binlog-format=row' ],
   ['--max_binlog_size=1', '--binlog-format=row' ]
  ],
  [
   ['--binlog-format=statement' ],
   ['-O', 'max_binlog_size=1', '--binlog-format=row' ],
   ['--max_binlog_size=1', '--binlog-format=row']
  ],

  [
   [ '--binlog-format=statement' ],
   ['-O', 'max_binlog_size=1', '--binlog-format=statement' ],
   ['--max_binlog_size=1' ]
  ],

 [
   [ '--binlog-format=statement' ],
   ['-O', 'max_binlog_size=1', '--binlog-format=statement' ],
   ['--max_binlog_size=1' ]
 ],

 [
  [ '--binlog-format=statement' ],
  ['--relay-log=/path/to/a/relay-log', '--binlog-format=row'],
  ['--relay-log=/path/to/a/relay-log', '--binlog-format=row' ]
 ],


 [
  [ '--binlog-format=statement' ],
  ['--relay-log=/path/to/a/relay-log', '-O', 'max_binlog_size=1'],
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
