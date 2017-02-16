# -*- cperl -*-
# Copyright (c) 2004-2008 MySQL AB, 2008 Sun Microsystems, Inc.
# Use is subject to license terms.
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

use strict;
use Carp;
use My::Platform;

sub mtr_fromfile ($);
sub mtr_tofile ($@);
sub mtr_tonewfile($@);
sub mtr_appendfile_to_file ($$);
sub mtr_grab_file($);
sub mtr_printfile($);
sub mtr_lastlinesfromfile ($$);

# Read a whole file, stripping leading and trailing whitespace.
sub mtr_fromfile ($) {
  my $file=  shift;

  open(FILE,"<",$file) or mtr_error("can't open file \"$file\": $!");
  my $text= join('', <FILE>);
  close FILE;
  $text =~ s/^\s+//;                    # Remove starting space, incl newlines
  $text =~ s/\s+$//;                    # Remove ending space, incl newlines
  return $text;
}


sub mtr_tofile ($@) {
  my $file=  shift;
  my $fh= open_for_append $file;
  mtr_error("can't open file \"$file\": $!") unless defined($fh);
  print $fh join("", @_);
  close $fh;
}


sub mtr_tonewfile ($@) {
  my $file=  shift;

  open(FILE,">",$file) or mtr_error("can't open file \"$file\": $!");
  print FILE join("", @_);
  close FILE;
}


sub mtr_appendfile_to_file ($$) {
  my $from_file=  shift;
  my $to_file=  shift;

  open(TOFILE,">>",$to_file) or mtr_error("can't open file \"$to_file\": $!");
  open(FROMFILE,"<",$from_file)
    or mtr_error("can't open file \"$from_file\": $!");
  print TOFILE while (<FROMFILE>);
  close FROMFILE;
  close TOFILE;
}


# Read a whole file verbatim.
sub mtr_grab_file($) {
  my $file= shift;
  open(FILE, '<', $file)
    or return undef;
  local $/= undef;
  my $data= scalar(<FILE>);
  close FILE;
  return $data;
}


# Print the file to STDOUT
sub mtr_printfile($) {
  my $file= shift;
  open(FILE, '<', $file)
    or warn $!;
  print while(<FILE>);
  close FILE;
  return;
}

sub mtr_lastlinesfromfile ($$) {
  croak "usage: mtr_lastlinesfromfile(file,numlines)" unless (@_ == 2);
  my ($file, $num_lines)= @_; 
  my $text;
  open(FILE,"<",$file) or mtr_error("can't open file \"$file\": $!");
  my @lines= reverse <FILE>;
  close FILE;
  my $size= scalar(@lines);
  $num_lines= $size unless ($size >= $num_lines);
  return join("", reverse(splice(@lines, 0, $num_lines)));
}

1;
