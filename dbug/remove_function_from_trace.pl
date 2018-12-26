#!/usr/bin/perl


die <<EEE unless @ARGV;
Usage: $0 func1 [func2 [ ...] ]

This filter (stdin->stdout) removes lines from dbug trace that were generated
by specified functions and all functions down the call stack. Produces the
same effect as if the original source had DBUG_PUSH(""); right after
DBUG_ENTER() and DBUG_POP(); right before DBUG_RETURN in every such a function.
EEE

$re=join('|', @ARGV);
$skip='';

while(<STDIN>) {
  print unless $skip;
  next unless /^(?:.*: )*((?:\| )*)([<>])($re)\n/o;
  if ($2 eq '>') {
    $skip=$1.$3 unless $skip;
    next;
  }
  next if $skip ne $1.$3;
  $skip='';
  print;
}
