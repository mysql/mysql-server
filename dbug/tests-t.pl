#!/usr/bin/env perl

#
#  A driver program to test DBUG features - runs tests (shell commands)
#  from the end of file to invoke tests.c, which does the real dbug work.
#

use Test::More;

$exe=$0;

die unless $exe =~ s/(tests)-t(\.exe)?$/$1$2 /;

# load tests
@tests=();
while (<DATA>) {
  if (/^% \.\/tests /) {
    push @tests, [ $' ]
  } elsif (/^#/) {
    next;
  } else {
    push @{$tests[$#tests]}, $_
  }
}

plan skip_all => "because dbug is disabled" if system $exe;

plan tests => scalar(@tests);

for (@tests) {
  $t=$exe . shift @$_;
  chomp($t);
  open F, '-|',  $t or die "open($t|): $!";
  local $";
  $out=join($", <F>); close(F);
  # special cases are handled here:
  $out =~ s/Memory: 0x[0-9A-Fa-f]+/Memory: 0x####/g if $t =~ /dump/;
  # compare ("\n" at the beginning makes better output in case of errors)
  is("\n$out","\n@$_", $t);
}

__DATA__
% ./tests -#d
func2: info: s=ok
=> execute
=> evaluate: ON
=> evaluate_if: OFF
main: explain: dbug explained: d
func2: info: s=ok
% ./tests d,ret3
=> evaluate: OFF
=> evaluate_if: OFF
#
## Testing negative lists
#
% ./tests d:-d,ret3
func2: info: s=ko
=> execute
=> evaluate: ON
=> evaluate_if: OFF
main: explain: dbug explained: d:-d,ret3
func2: info: s=ko
% ./tests t:-d,ret3
>main
| >func1
| | >func2
| | | >func3
| | | <func3
| | <func2
| <func1
=> evaluate: OFF
=> evaluate_if: OFF
| >func2
| | >func3
| | <func3
| <func2
<main
% ./tests t:d,info:-d,ret3
>main
| >func1
| | >func2
| | | >func3
| | | <func3
| | | info: s=ko
| | <func2
| <func1
=> evaluate: OFF
=> evaluate_if: OFF
| >func2
| | >func3
| | <func3
| | info: s=ko
| <func2
<main
% ./tests t:d,info:-d,ret3:-f,func2
>main
| >func1
| | | >func3
| | | <func3
| <func1
=> evaluate: OFF
=> evaluate_if: OFF
| | >func3
| | <func3
<main
% ./tests t:d,info:-d,ret3:-f,func2 d,evaluate
=> evaluate: ON
=> evaluate_if: OFF
% ./tests t:d,info:-d,ret3:-f,func2 d,evaluate_if
=> evaluate: OFF
=> evaluate_if: ON
% ./tests t:d:-d,ret3:-f,func2 d,evaluate_if
=> evaluate: OFF
=> evaluate_if: ON
% ./tests t:d:-d,ret3:-f,func2
>main
| >func1
| | | >func3
| | | <func3
| <func1
=> execute
=> evaluate: ON
=> evaluate_if: OFF
| explain: dbug explained: d:-d,ret3:f:-f,func2:t
| | >func3
| | <func3
<main
#
## Adding incremental settings to the brew
#
% ./tests t:d:-d,ret3:-f,func2 +d,evaluate_if
>main
| >func1
| | | >func3
| | | <func3
| <func1
=> evaluate: OFF
=> evaluate_if: ON
| | >func3
| | <func3
<main
#
## DBUG_DUMP
#
% ./tests t:d:-d,ret3:f:-f,func2 +d,dump
>main
| >func1
| | | >func3
| | | <func3
| <func1
| dump: Memory: 0x####  Bytes: (27)
64 2C 64 75 6D 70 3A 2D 64 2C 72 65 74 33 3A 66 3A 2D 66 2C 66 75 6E 63 32 3A 
74 
=> evaluate: OFF
=> evaluate_if: OFF
| | >func3
| | <func3
<main
% ./tests t:d:-d,ret3:f:-f,func2 +d,dump
>main
| >func1
| | | >func3
| | | <func3
| <func1
| dump: Memory: 0x####  Bytes: (27)
64 2C 64 75 6D 70 3A 2D 64 2C 72 65 74 33 3A 66 3A 2D 66 2C 66 75 6E 63 32 3A 
74 
=> evaluate: OFF
=> evaluate_if: OFF
| | >func3
| | <func3
<main
% ./tests t:d:-d,ret3:f:-f,func2:+d,dump
>main
| >func1
| | | >func3
| | | <func3
| <func1
| dump: Memory: 0x####  Bytes: (27)
64 2C 64 75 6D 70 3A 2D 64 2C 72 65 74 33 3A 66 3A 2D 66 2C 66 75 6E 63 32 3A 
74 
=> evaluate: OFF
=> evaluate_if: OFF
| | >func3
| | <func3
<main
% ./tests t:d:-d,ret3:f:-f,func2 +d,dump,explain
>main
| >func1
| | | >func3
| | | <func3
| <func1
| dump: Memory: 0x####  Bytes: (35)
64 2C 64 75 6D 70 2C 65 78 70 6C 61 69 6E 3A 2D 64 2C 72 65 74 33 3A 66 3A 2D 
66 2C 66 75 6E 63 32 3A 74 
=> evaluate: OFF
=> evaluate_if: OFF
| explain: dbug explained: d,dump,explain:-d,ret3:f:-f,func2:t
| | >func3
| | <func3
<main
% ./tests t:d:-d,ret3:f:-f,func2 +d,dump,explain:P
dbug: >main
dbug-tests: | >func1
dbug-tests: | | | >func3
dbug-tests: | | | <func3
dbug-tests: | <func1
dbug-tests: | dump: Memory: 0x####  Bytes: (37)
64 2C 64 75 6D 70 2C 65 78 70 6C 61 69 6E 3A 2D 64 2C 72 65 74 33 3A 66 3A 2D 
66 2C 66 75 6E 63 32 3A 50 3A 74 
=> evaluate: OFF
=> evaluate_if: OFF
dbug-tests: | explain: dbug explained: d,dump,explain:-d,ret3:f:-f,func2:P:t
dbug-tests: | | >func3
dbug-tests: | | <func3
dbug-tests: <main
% ./tests t:d:-d,ret3:f:-f,func2 +d,dump,explain:P:F
dbug:        tests.c: >main
dbug-tests:        tests.c: | >func1
dbug-tests:        tests.c: | | | >func3
dbug-tests:        tests.c: | | | <func3
dbug-tests:        tests.c: | <func1
dbug-tests:        tests.c: | dump: Memory: 0x####  Bytes: (39)
64 2C 64 75 6D 70 2C 65 78 70 6C 61 69 6E 3A 2D 64 2C 72 65 74 33 3A 66 3A 2D 
66 2C 66 75 6E 63 32 3A 46 3A 50 3A 74 
=> evaluate: OFF
=> evaluate_if: OFF
dbug-tests:        tests.c: | explain: dbug explained: d,dump,explain:-d,ret3:f:-f,func2:F:P:t
dbug-tests:        tests.c: | | >func3
dbug-tests:        tests.c: | | <func3
dbug-tests:        tests.c: <main
#
## DBUG_EXPLAIN, DBUG_PUSH, DBUG_POP, DBUG_SET
#
% ./tests t:d:-d,ret3:f:-f,func2
>main
| >func1
| | | >func3
| | | <func3
| <func1
=> execute
=> evaluate: ON
=> evaluate_if: OFF
| explain: dbug explained: d:-d,ret3:f:-f,func2:t
| | >func3
| | <func3
<main
% ./tests t:d:-d,ret3
>main
| >func1
| | >func2
| | | >func3
| | | <func3
| | | info: s=ko
| | <func2
| <func1
=> execute
=> evaluate: ON
=> evaluate_if: OFF
| explain: dbug explained: d:-d,ret3:t
| >func2
| | >func3
| | <func3
| | info: s=ko
| <func2
<main
% ./tests d,info:-d,ret3:d,push
func2: info: s=ko
=> evaluate: OFF
=> evaluate_if: OFF
| >func2
| | >func3
| | <func3
| | info: s=ko
| <func2
<main
% ./tests d,info:-d,ret3:d,push,explain
func2: info: s=ko
=> evaluate: OFF
=> evaluate_if: OFF
| explain: dbug explained: d,info,push,explain:-d,ret3:t
| >func2
| | >func3
| | <func3
| | info: s=ko
| <func2
<main
% ./tests d,info:-d,ret3:d,explain
func2: info: s=ko
=> evaluate: OFF
=> evaluate_if: OFF
main: explain: dbug explained: d,info,explain:-d,ret3
func2: info: s=ko
% ./tests d,info:-d,ret3:d,explain,pop
func2: info: s=ko
=> evaluate: OFF
=> evaluate_if: OFF
% ./tests d,info:-d,ret3:d,explain t:d,pop
>main
| >func1
| | >func2
| | | >func3
| | | <func3
| | <func2
| <func1
=> evaluate: OFF
=> evaluate_if: OFF
main: explain: dbug explained: d,info,explain:-d,ret3
func2: info: s=ko
% ./tests d,info:-d,ret3:d,explain,pop +t
>main
| >func1
| | >func2
| | | >func3
| | | <func3
| | | info: s=ko
| | <func2
| <func1
=> evaluate: OFF
=> evaluate_if: OFF
main: explain: dbug explained: d,info,explain,pop:-d,ret3
func2: info: s=ko
% ./tests d,info:-d,ret3:d,explain,set
func2: info: s=ko
=> evaluate: OFF
=> evaluate_if: OFF
       tests.c: main: explain: dbug explained: d,info,explain,set:-d,ret3:F
       tests.c: func2: info: s=ko
% ./tests d,info:-d,ret3:d,explain,set:t
>main
| >func1
| | >func2
| | | >func3
| | | <func3
| | | info: s=ko
| | <func2
| <func1
=> evaluate: OFF
=> evaluate_if: OFF
       tests.c: | explain: dbug explained: d,info,explain,set:-d,ret3:F:t
       tests.c: | >func2
       tests.c: | | >func3
       tests.c: | | <func3
       tests.c: | | info: s=ko
       tests.c: | <func2
       tests.c: <main
% ./tests t d,info:-d,ret3:d,explain,set:t
>main
| >func1
| | >func2
| | | >func3
| | | <func3
| | | info: s=ko
| | <func2
| <func1
=> evaluate: OFF
=> evaluate_if: OFF
       tests.c: | explain: dbug explained: d,info,explain,set:-d,ret3:F:t
       tests.c: | >func2
       tests.c: | | >func3
       tests.c: | | <func3
       tests.c: | | info: s=ko
       tests.c: | <func2
       tests.c: <main
% ./tests t d,info:-d,ret3:d,explain,set,pop
func2: info: s=ko
=> evaluate: OFF
=> evaluate_if: OFF
| >func2
| | >func3
| | <func3
| <func2
<main
% ./tests t:f,func2
| | >func2
| | <func2
=> evaluate: OFF
=> evaluate_if: OFF
| >func2
| <func2
#
## Testing SUBDIR rules
#
% ./tests t:-f,func2/:d
>main
| >func1
| <func1
=> execute
=> evaluate: ON
=> evaluate_if: OFF
| explain: dbug explained: d:f:-f,func2/:t
<main
% ./tests t:f,func1/:d
| >func1
| | >func2
| | | >func3
| | | <func3
| | | info: s=ok
| | <func2
| <func1
=> evaluate: OFF
=> evaluate_if: OFF
% ./tests t:f,main/:d,pop
>main
| >func1
| | >func2
| | | >func3
| | | <func3
| | <func2
| <func1
=> evaluate: OFF
=> evaluate_if: OFF
% ./tests f,main/:d,push
=> evaluate: OFF
=> evaluate_if: OFF
| >func2
| | >func3
| | <func3
| <func2
<main
#
## Testing FixTraceFlags() - when we need to traverse the call stack
# (these tests fail with FixTraceFlags() disabled)
#
# delete the INCLUDE rule up the stack
% ./tests t:f,func1/ --push1=t:f,func3/
| >func1
| | >func2
| | | >func3
| | | <func3
| | <func2
=> push1
=> evaluate: OFF
=> evaluate_if: OFF
| | >func3
| | <func3
# delete the EXCLUDE rule up the stack
% ./tests t:-f,func1/ --push1=t
>main
=> push1
| <func1
=> evaluate: OFF
=> evaluate_if: OFF
| >func2
| | >func3
| | <func3
| <func2
<main
# add the INCLUDE rule up the stack
% ./tests t:f,func3 --push1=t:f,main/
| | | >func3
| | | <func3
=> push1
| <func1
=> evaluate: OFF
=> evaluate_if: OFF
| >func2
| | >func3
| | <func3
| <func2
<main
# add the EXCLUDE rule up the stack
% ./tests t --push1=t:-f,main/
>main
| >func1
| | >func2
| | | >func3
| | | <func3
| | <func2
=> push1
=> evaluate: OFF
=> evaluate_if: OFF
# change the defaults
% ./tests t:f,func3 --push1=t
| | | >func3
| | | <func3
=> push1
| <func1
=> evaluate: OFF
=> evaluate_if: OFF
| >func2
| | >func3
| | <func3
| <func2
<main
# repeated keyword
% ./tests d:-d,info,info
=> execute
=> evaluate: ON
=> evaluate_if: OFF
main: explain: dbug explained: d:-d,info
% ./tests d:-d,info/,info
=> execute
=> evaluate: ON
=> evaluate_if: OFF
main: explain: dbug explained: d:-d,info/
