#!/usr/bin/perl

#
#  A driver program to test DBUG features - runs tests (shell commands)
#  from the end of file to invoke tests.c, which does the real dbug work.
#

$exe=$0;

die unless $exe =~ s/(tests)-t(\.exe)?$/$1$2 /;

# load tests
@tests=();
while (<DATA>) {
  if (/^% tests /) {
    push @tests, [ $' ]
  } else {
    push @{$tests[$#tests]}, $_
  }
}

# require/import instead of use - we know the plan only when tests are loaded
require Test::More;
import Test::More tests => scalar(@tests);

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
% tests -#d
func2: info: s=ok
func2: info: s=ok
=> execute
=> evaluate: ON
=> evaluate_if: OFF
main: explain: dbug explained: d
% tests -#d,ret3
=> evaluate: OFF
=> evaluate_if: OFF
% tests -#d:-d,ret3
func2: info: s=ko
func2: info: s=ko
=> execute
=> evaluate: ON
=> evaluate_if: OFF
main: explain: dbug explained: d:-d,ret3
% tests -#t:-d,ret3
>main
| >func1
| | >func2
| | | >func3
| | | <func3
| | <func2
| <func1
| >func2
| | >func3
| | <func3
| <func2
=> evaluate: OFF
=> evaluate_if: OFF
<main
% tests -#t:d,info:-d,ret3
>main
| >func1
| | >func2
| | | >func3
| | | <func3
| | | info: s=ko
| | <func2
| <func1
| >func2
| | >func3
| | <func3
| | info: s=ko
| <func2
=> evaluate: OFF
=> evaluate_if: OFF
<main
% tests -#t:d,info:-d,ret3:-f,func2
>main
| >func1
| | | >func3
| | | <func3
| <func1
| | >func3
| | <func3
=> evaluate: OFF
=> evaluate_if: OFF
<main
% tests -#t:d,info:-d,ret3:-f,func2 d,evaluate
=> evaluate: ON
=> evaluate_if: OFF
% tests -#t:d,info:-d,ret3:-f,func2 d,evaluate_if
=> evaluate: OFF
=> evaluate_if: ON
% tests -#t:d:-d,ret3:-f,func2 d,evaluate_if
=> evaluate: OFF
=> evaluate_if: ON
% tests -#t:d:-d,ret3:-f,func2 +d,evaluate_if
>main
| >func1
| | | >func3
| | | <func3
| <func1
| | >func3
| | <func3
=> evaluate: OFF
=> evaluate_if: ON
<main
% tests -#t:d:-d,ret3:-f,func2
>main
| >func1
| | | >func3
| | | <func3
| <func1
| | >func3
| | <func3
=> execute
=> evaluate: ON
=> evaluate_if: OFF
| explain: dbug explained: d:-d,ret3:f:-f,func2:t
<main
% tests -#t:d:-d,ret3:f:-f,func2 -#+d,dump
>main
| >func1
| | | >func3
| | | <func3
| <func1
| | >func3
| | <func3
| dump: Memory: 0x####  Bytes: (27)
64 2C 64 75 6D 70 3A 2D 64 2C 72 65 74 33 3A 66 3A 2D 66 2C 66 75 6E 63 32 3A 
74 
=> evaluate: OFF
=> evaluate_if: OFF
<main
% tests -#t:d:-d,ret3:f:-f,func2 +d,dump
>main
| >func1
| | | >func3
| | | <func3
| <func1
| | >func3
| | <func3
| dump: Memory: 0x####  Bytes: (27)
64 2C 64 75 6D 70 3A 2D 64 2C 72 65 74 33 3A 66 3A 2D 66 2C 66 75 6E 63 32 3A 
74 
=> evaluate: OFF
=> evaluate_if: OFF
<main
% tests -#t:d:-d,ret3:f:-f,func2:+d,dump
>main
| >func1
| | | >func3
| | | <func3
| <func1
| | >func3
| | <func3
| dump: Memory: 0x####  Bytes: (27)
64 2C 64 75 6D 70 3A 2D 64 2C 72 65 74 33 3A 66 3A 2D 66 2C 66 75 6E 63 32 3A 
74 
=> evaluate: OFF
=> evaluate_if: OFF
<main
% tests -#t:d:-d,ret3:f:-f,func2 +d,dump,explain
>main
| >func1
| | | >func3
| | | <func3
| <func1
| | >func3
| | <func3
| dump: Memory: 0x####  Bytes: (35)
64 2C 64 75 6D 70 2C 65 78 70 6C 61 69 6E 3A 2D 64 2C 72 65 74 33 3A 66 3A 2D 
66 2C 66 75 6E 63 32 3A 74 
=> evaluate: OFF
=> evaluate_if: OFF
| explain: dbug explained: d,dump,explain:-d,ret3:f:-f,func2:t
<main
% tests -#t:d:-d,ret3:f:-f,func2 +d,dump,explain:P
dbug: >main
dbug-tests: | >func1
dbug-tests: | | | >func3
dbug-tests: | | | <func3
dbug-tests: | <func1
dbug-tests: | | >func3
dbug-tests: | | <func3
dbug-tests: | dump: Memory: 0x####  Bytes: (37)
64 2C 64 75 6D 70 2C 65 78 70 6C 61 69 6E 3A 2D 64 2C 72 65 74 33 3A 66 3A 2D 
66 2C 66 75 6E 63 32 3A 50 3A 74 
=> evaluate: OFF
=> evaluate_if: OFF
dbug-tests: | explain: dbug explained: d,dump,explain:-d,ret3:f:-f,func2:P:t
dbug-tests: <main
% tests -#t:d:-d,ret3:f:-f,func2 +d,dump,explain:P:F
dbug:        tests.c: >main
dbug-tests:        tests.c: | >func1
dbug-tests:        tests.c: | | | >func3
dbug-tests:        tests.c: | | | <func3
dbug-tests:        tests.c: | <func1
dbug-tests:        tests.c: | | >func3
dbug-tests:        tests.c: | | <func3
dbug-tests:        tests.c: | dump: Memory: 0x####  Bytes: (39)
64 2C 64 75 6D 70 2C 65 78 70 6C 61 69 6E 3A 2D 64 2C 72 65 74 33 3A 66 3A 2D 
66 2C 66 75 6E 63 32 3A 46 3A 50 3A 74 
=> evaluate: OFF
=> evaluate_if: OFF
dbug-tests:        tests.c: | explain: dbug explained: d,dump,explain:-d,ret3:f:-f,func2:F:P:t
dbug-tests:        tests.c: <main
% tests -#t:d:-d,ret3:f:-f,func2
>main
| >func1
| | | >func3
| | | <func3
| <func1
| | >func3
| | <func3
=> execute
=> evaluate: ON
=> evaluate_if: OFF
| explain: dbug explained: d:-d,ret3:f:-f,func2:t
<main
% tests -#t:d:-d,ret3
>main
| >func1
| | >func2
| | | >func3
| | | <func3
| | | info: s=ko
| | <func2
| <func1
| >func2
| | >func3
| | <func3
| | info: s=ko
| <func2
=> execute
=> evaluate: ON
=> evaluate_if: OFF
| explain: dbug explained: d:-d,ret3:t
<main
% tests -#t:d,info:-d,ret3:d,push
>main
| >func1
| | >func2
| | | >func3
| | | <func3
| | | info: s=ko
| | <func2
| <func1
| >func2
| | >func3
| | <func3
| | info: s=ko
| <func2
=> evaluate: OFF
=> evaluate_if: OFF
<main
% tests -#d,info:-d,ret3:d,push
func2: info: s=ko
func2: info: s=ko
=> evaluate: OFF
=> evaluate_if: OFF
<main
% tests -#d,info:-d,ret3:d,push,explain
func2: info: s=ko
func2: info: s=ko
=> evaluate: OFF
=> evaluate_if: OFF
| explain: dbug explained: d,info,push,explain:-d,ret3:t
<main
% tests -#d,info:-d,ret3:d,explain
func2: info: s=ko
func2: info: s=ko
=> evaluate: OFF
=> evaluate_if: OFF
main: explain: dbug explained: d,info,explain:-d,ret3
% tests -#d,info:-d,ret3:d,explain,pop
func2: info: s=ko
func2: info: s=ko
=> evaluate: OFF
=> evaluate_if: OFF
% tests -#d,info:-d,ret3:d,explain,pop t
>main
| >func1
| | >func2
| | | >func3
| | | <func3
| | <func2
| <func1
| >func2
| | >func3
| | <func3
| <func2
=> evaluate: OFF
=> evaluate_if: OFF
<main
% tests -#d,info:-d,ret3:d,explain,pop +t
>main
| >func1
| | >func2
| | | >func3
| | | <func3
| | | info: s=ko
| | <func2
| <func1
| >func2
| | >func3
| | <func3
| | info: s=ko
| <func2
=> evaluate: OFF
=> evaluate_if: OFF
main: explain: dbug explained: d,info,explain,pop:-d,ret3
% tests -#d,info:-d,ret3:d,explain,set
func2: info: s=ko
func2: info: s=ko
=> evaluate: OFF
=> evaluate_if: OFF
       tests.c: main: explain: dbug explained: d,info,explain,set:-d,ret3:F
% tests -#d,info:-d,ret3:d,explain,set:t
>main
| >func1
| | >func2
| | | >func3
| | | <func3
| | | info: s=ko
| | <func2
| <func1
| >func2
| | >func3
| | <func3
| | info: s=ko
| <func2
=> evaluate: OFF
=> evaluate_if: OFF
       tests.c: | explain: dbug explained: d,info,explain,set:-d,ret3:F:t
       tests.c: <main
% tests t -#d,info:-d,ret3:d,explain,set:t
>main
| >func1
| | >func2
| | | >func3
| | | <func3
| | | info: s=ko
| | <func2
| <func1
| >func2
| | >func3
| | <func3
| | info: s=ko
| <func2
=> evaluate: OFF
=> evaluate_if: OFF
       tests.c: | explain: dbug explained: d,info,explain,set:-d,ret3:F:t
       tests.c: <main
% tests t -#d,info:-d,ret3:d,explain,set,pop
func2: info: s=ko
func2: info: s=ko
=> evaluate: OFF
=> evaluate_if: OFF
<main
