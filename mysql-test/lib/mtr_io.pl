# -*- cperl -*-
# Copyright (c) 2004, 2023, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

# This is a library file used by the Perl version of mysql-test-run,
# and is part of the translation of the Bourne shell script with the
# same name.

use strict;
use Carp;

# Read a whole file, stripping leading and trailing whitespace.
sub mtr_fromfile ($) {
  my $file = shift;

  open(FILE, "<", $file) or mtr_error("can't open file \"$file\": $!");
  my $text = join('', <FILE>);
  close FILE;

  # Remove starting space, incl newlines
  $text =~ s/^\s+//;

  # Remove ending space, incl newlines
  $text =~ s/\s+$//;

  return $text;
}

sub mtr_tofile ($@) {
  my $file = shift;

  open(FILE, ">>", $file) or mtr_error("can't open file \"$file\": $!");
  print FILE join("", @_);
  close FILE;
}

sub mtr_tonewfile ($@) {
  my $file = shift;

  open(FILE, ">", $file) or mtr_error("can't open file \"$file\": $!");
  print FILE join("", @_);
  close FILE;
}

sub mtr_appendfile_to_file ($$) {
  my $from_file = shift;
  my $to_file   = shift;

  open(TOFILE, ">>", $to_file) or mtr_error("can't open file \"$to_file\": $!");
  open(FROMFILE, "<", $from_file) or
    mtr_error("can't open file \"$from_file\": $!");

  print TOFILE while (<FROMFILE>);
  close FROMFILE;
  close TOFILE;
}

# Read a whole file verbatim.
sub mtr_grab_file($) {
  my $file = shift;

  open(FILE, '<', $file) or return undef;
  local $/ = undef;
  my $data = scalar(<FILE>);
  close FILE;
  return $data;
}

# Print the file to STDOUT
sub mtr_printfile($) {
  my $file = shift;

  open(FILE, '<', $file) or warn $!;
  print while (<FILE>);
  close FILE;
  return;
}

sub mtr_lastlinesfromfile ($$) {
  croak "usage: mtr_lastlinesfromfile(file,numlines)" unless (@_ == 2);

  my ($file, $num_lines) = @_;
  my $text;
  open(FILE, "<", $file) or mtr_error("can't open file \"$file\": $!");
  my @lines = reverse <FILE>;
  close FILE;
  my $size = scalar(@lines);
  $num_lines = $size unless ($size >= $num_lines);
  return join("", reverse(splice(@lines, 0, $num_lines)));
}

# Return a string containing callstack information
sub mtr_callstack_info ($) {
  my $path_current_testlog = shift;
  open(TESTLOG, "<", $path_current_testlog) or
    die "Can't open file $path_current_testlog";

  my $append_string  = 0;
  my $callstack_info = "";

OUTER:
  while (<TESTLOG>) {
    if ($_ =~ /mysqltest: At line/) {
      $callstack_info = $callstack_info . $_;
      while (<TESTLOG>) {
        last OUTER if ($_ =~ /conn->name/ or $_ =~ /Attempting backtrace/);
        $callstack_info = $callstack_info . $_;
      }
    }
  }

  close(TESTLOG);
  return $callstack_info;
}

1;
