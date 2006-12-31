# -*- cperl -*-
# Copyright (C) 2005 MySQL AB
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

# This is a library file used by the Perl version of mysql-test-run,
# and is part of the translation of the Bourne shell script with the
# same name.

#use Data::Dumper;
use strict;

# $Data::Dumper::Indent= 1;

sub mtr_diff($$);

##############################################################################
#
#  This is a simplified unified diff, with some special handling
#  of unsorted result sets
#
##############################################################################

# FIXME replace die with mtr_error

#require "mtr_report.pl";
#mtr_diff("a.txt","b.txt");

sub mtr_diff ($$) {
  my $file1 = shift;
  my $file2 = shift;

  # ----------------------------------------------------------------------
  # We read in all of the files at once
  # ----------------------------------------------------------------------

  unless ( open(FILE1, $file1) )
  {
    mtr_warning("can't open \"$file1\": $!");
    return;
  }

  unless ( open(FILE2, $file2) )
  {
    mtr_warning("can't open \"$file2\": $!");
    return;
  }

  my $lines1= collect_lines(<FILE1>);
  my $lines2= collect_lines(<FILE2>);
  close FILE1;
  close FILE2;

#  print Dumper($lines1);
#  print Dumper($lines2);

  # ----------------------------------------------------------------------
  # We compare line by line, but don't shift off elements until we know
  # what to do. This way we use the "restart" method, do simple change
  # and restart by entering the diff loop from the beginning again.
  # ----------------------------------------------------------------------

  my @context;
  my @info;                     # Collect information, and output later
  my $lno1= 1;
  my $lno2= 1;

  while ( @$lines1 or @$lines2 )
  {
    unless ( @$lines1 )
    {
      push(@info, map {['+',$lno1,$lno2++,$_]} @$lines2);
      last;
    }
    unless ( @$lines2 )
    {
      push(@info, map {['-',$lno1++,$lno2,$_]} @$lines1);
      last;
    }

    # ----------------------------------------------------------------------
    # We know both have lines
    # ----------------------------------------------------------------------

    if ( $lines1->[0] eq $lines2->[0] )
    {
      # Simple case, first line match and all is well
      push(@info, ['',$lno1++,$lno2++,$lines1->[0]]);
      shift @$lines1;
      shift @$lines2;
      next;
    }

    # ----------------------------------------------------------------------
    # Now, we know they differ
    # ----------------------------------------------------------------------

    # How far in the other one, is there a match?

    my $idx2= find_next_match($lines1->[0], $lines2);
    my $idx1= find_next_match($lines2->[0], $lines1);

    # Here we could test "if ( !defined $idx2 or !defined $idx1 )" and
    # use a more complicated diff algorithm in the case both contains
    # each others lines, just dislocated. But for this application, there
    # should be no need.

    if ( !defined $idx2 )
    {
      push(@info, ['-',$lno1++,$lno2,$lines1->[0]]);
      shift @$lines1;
    }
    else
    {
      push(@info, ['+',$lno1,$lno2++,$lines2->[0]]);
      shift @$lines2;
    }
  }

  # ----------------------------------------------------------------------
  # Try to output nicely
  # ----------------------------------------------------------------------

#  print Dumper(\@info);

  # We divide into "chunks" to output
  # We want at least three lines of context

  my @chunks;
  my @chunk;
  my $state= 'pre';          # 'pre', 'in' and 'post' difference
  my $post_count= 0;

  foreach my $info ( @info )
  {
    if ( $info->[0] eq '' and $state eq 'pre' )
    {
      # Collect no more than three lines of context before diff
      push(@chunk, $info);
      shift(@chunk) if @chunk > 3;
      next;
    }

    if ( $info->[0] =~ /(\+|\-)/ and $state =~ /(pre|in)/ )
    {
      # Start/continue collecting diff
      $state= 'in';
      push(@chunk, $info);
      next;
    }

    if ( $info->[0] eq '' and $state eq 'in' )
    {
      # Stop collecting diff, and collect context after diff
      $state= 'post';
      $post_count= 1;
      push(@chunk, $info);
      next;
    }

    if ( $info->[0] eq '' and $state eq 'post' and $post_count < 6 )
    {
      # We might find a new diff sequence soon, continue to collect
      # non diffs but five up on 6.
      $post_count++;
      push(@chunk, $info);
      next;
    }

    if ( $info->[0] eq '' and $state eq 'post' )
    {
      # We put an end to this, giving three non diff lines to
      # the old chunk, and three to the new one.
      my @left= splice(@chunk, -3, 3);
      push(@chunks, [@chunk]);
      $state= 'pre';
      $post_count= 0;
      @chunk= @left;
      next;
    }

    if ( $info->[0] =~ /(\+|\-)/ and $state eq 'post' )
    {
      # We didn't split, continue collect diff
      $state= 'in';
      push(@chunk, $info);
      next;
    }

  }

  if ( $post_count > 3 )
  {
    $post_count -= 3;
    splice(@chunk, -$post_count, $post_count);
  }
  push(@chunks, [@chunk]) if @chunk and $state ne 'pre';

  foreach my $chunk ( @chunks )
  {
    my $from_file_start=  $chunk->[0]->[1];
    my $to_file_start=    $chunk->[0]->[2];
    my $from_file_offset= $chunk->[$#$chunk]->[1] - $from_file_start;
    my $to_file_offset=   $chunk->[$#$chunk]->[2] - $to_file_start;
    print "\@\@ -$from_file_start,$from_file_offset ",
          "+$to_file_start,$to_file_offset \@\@\n";

    foreach my $info ( @$chunk )
    {
      if ( $info->[0] eq '' )
      {
        print "  $info->[3]\n";
      }
      elsif ( $info->[0] eq '-' )
      {
        print "- $info->[3]\n";
      }
      elsif ( $info->[0] eq '+' )
      {
        print "+ $info->[3]\n";
      }
    }
  }

#  print Dumper(\@chunks);
  
}


##############################################################################
#  Find if the string is found in the array, return the index if found,
#  if not found, return "undef"
##############################################################################

sub find_next_match {
  my $line= shift;
  my $lines= shift;

  for ( my $idx= 0; $idx < @$lines; $idx++ )
  {
    return $idx if $lines->[$idx] eq $line;
  }

  return undef;                 # No match found
}


##############################################################################
#  Just read the lines, but handle "sets" of lines that are unordered
##############################################################################

sub collect_lines {

  my @recordset;
  my @lines;

  while (@_)
  {
    my $line= shift @_;
    chomp($line);

    if ( $line =~ /^\Q%unordered%\E\t/ )
    {
      push(@recordset, $line);
    }
    elsif ( @recordset )
    {
      push(@lines, sort @recordset);
      @recordset= ();         # Clear it
    }
    else
    {
      push(@lines, $line);
    }
  }

  if ( @recordset )
  {
    push(@lines, sort @recordset);
    @recordset= ();         # Clear it
  }

  return \@lines;
}

1;
