#!/usr/bin/perl
# -*- cperl -*-

use strict;
use Getopt::Long;

my $opt_exit_code= 0;

GetOptions
  (
   # Exit with the specified exit code
   'exit-code=i'   => \$opt_exit_code
  );


print "Hello stdout\n";
print STDERR "Hello stderr\n";

exit ($opt_exit_code);


