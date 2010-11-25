#!/usr/bin/perl
# -*- cperl -*-

# This script processes a .gcov coverage report to honor purecov 
# annotations: lines marked as inspected or as deadcode are changed
# from looking like lines with code that was never executed to look
# like lines that have no executable code.

use strict;
use warnings;

foreach my $in_file_name ( @ARGV )
{
  my $out_file_name=$in_file_name . ".tmp";
  my $skipping=0;

  open(IN, "<", $in_file_name) || next;
  open(OUT, ">", $out_file_name);
  while(<IN>)
  {
    my $line= $_;
    my $check= $line;
    
    # process purecov: start/end multi-blocks
    my $started=0;
    my $ended= 0;
    while (($started=($check =~ s/purecov: *begin *(deadcode|inspected)//)) ||
           ($ended=($check =~ s/purecov: *end//)))
    {
      $skipping= $skipping + $started - $ended;
    }
    if ($skipping < 0)
    {
       print OUT "WARNING: #####: incorrect order of purecov begin/end annotations\n";
       $skipping= 0;
    }
    
    # Besides purecov annotations, also remove uncovered code mark from cases
    # like the following:
    # 
    #     -:  211:*/
    #     -:  212:class Field_value : public Value_dep
    # #####:  213:{
    #     -:  214:public:
    #
    # I have no idea why would gcov think there is uncovered code there
    #
    my @arr= split(/:/, $line);
    if ($skipping || $line =~ /purecov: *(inspected|deadcode)/ || 
        $arr[2] =~ m/^{ *$/)
    {
      # Change '####' to '-'.
      $arr[0] =~ s/#####/    -/g;
      $line= join(":", @arr);
    }
    print OUT $line;
  }
  close(IN);
  close(OUT);
  system("mv", "-f", $out_file_name, $in_file_name);
}


