#!/usr/bin/perl
# -*- cperl -*-

use strict;
use Getopt::Long;
use IO::File;

my $vardir;
my $randie= 0;
my $sleep= 0;
GetOptions
  (
   # Directory where to write files
   'vardir=s'     => \$vardir,
   'die-randomly' => \$randie,
   'sleep=i'      => \$sleep,
  );

die("invalid vardir ") unless defined $vardir and -d $vardir;

my $pid= $$;
while(1){
  for my $i (1..64){
    # Write to file
    my $name= "$vardir/$pid.$i.tmp";
    my $F= IO::File->new($name, "w")
      or warn "$$, Could not open $name: $!" and next;
    print $F rand($.) for (1..1000);
    $F->close();
    sleep($sleep);
    die "ooops!" if $randie and rand() < 0.0001
  }
}


exit (0);


