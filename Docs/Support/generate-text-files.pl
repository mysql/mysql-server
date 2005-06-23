#!/usr/bin/perl -w -*- perl -*-
# Generate text files from top directory from the manual.

$from = shift(@ARGV);
$fnode = shift(@ARGV);
$tnode = shift(@ARGV);

open(IN, "$from") || die "Cannot open $from: $!";

$in = 0;

while (<IN>)
{
  if ($in)
  {
    if (/Node: $tnode,/ || /\[index/)
    {
      $in = 0;
    }
    elsif (/^File: mysql.info/ || (/^/))
    {
      # Just Skip node beginnings
    }
    else
    {
      print;
    }
  }
  else
  {
    if (/Node: $fnode,/)
    {
      $in = 1;
      # Skip first empty line
      <IN>;
    }
  }
}

close(IN);

die "Could not find node \"$tnode\"" if ($in == 1);
exit 0;
