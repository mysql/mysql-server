# -*- cperl -*-
# Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.
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

package mtr_results;
use strict;
use IO::Handle qw[ flush ];

use base qw(Exporter);
our @EXPORT= qw(resfile_init resfile_global resfile_new_test resfile_test_info
                resfile_output resfile_output_file resfile_print
                resfile_print_test resfile_to_test resfile_from_test );

my %curr_result;		# Result for current test
my $curr_output;		# Output for current test
my $do_resfile;

END {
  close RESF if $do_resfile;
}

sub resfile_init($)
{
  my $fname= shift;
  open (RESF, " > $fname") or die ("Could not open result file $fname");
  %curr_result= ();
  $curr_output= "";
  $do_resfile= 1;
}

# Strings need to be quoted if they start with white space or ",
# or if they contain newlines. Pass a reference to the string.
# If the string is quoted, " must be escaped, thus \ also must be escaped

sub quote_value($)
{
  my $stref= shift;

  for ($$stref) {
    return unless /^[\s"]/ or /\n/;
    s/\\/\\\\/g;
    s/"/\\"/g;
    $_= '"' . $_ . '"';
  }
}

# Output global variable setting to result file.

sub resfile_global($$)
{
  return unless $do_resfile;
  my ($tag, $val) = @_;
  $val= join (' ', @$val) if ref($val) eq 'ARRAY';
  quote_value(\$val);
  print RESF "$tag : $val\n";
}

# Prepare to add results for new test

sub resfile_new_test()
{
  %curr_result= ();
  $curr_output= "";
}

# Add (or change) one variable setting for current test

sub resfile_test_info($$)
{
  my ($tag, $val) = @_;
  return unless $do_resfile;
  quote_value(\$val);
  $curr_result{$tag} = $val;
}

# Add to output value for current test.
# Will be quoted if necessary, truncated if length over 5000.

sub resfile_output($)
{
  return unless $do_resfile;

  for (shift) {
    my $len= length;
    if ($len > 5000) {
      my $trlen= $len - 5000;
      $_= substr($_, 0, 5000) . "\n[TRUNCATED $trlen chars removed]\n";
    }
    s/\\/\\\\/g;
    s/"/\\"/g;
    $curr_output .= $_;
  }
}

# Add to output, read from named file

sub resfile_output_file($)
{
  resfile_output(::mtr_grab_file(shift)) if $do_resfile;
}

# Print text, and also append to current output if we're collecting results

sub resfile_print($)
{
  my $txt= shift;
  print($txt);
  resfile_output($txt) if $do_resfile;
}

# Print results for current test, then reset
# (So calling a second time without having generated new results
#  will have no effect)

sub resfile_print_test()
{
  return unless %curr_result;

  print RESF "{\n";
  while (my ($t, $v) = each %curr_result) {
    print RESF "$t : $v\n";
  }
  if ($curr_output) {
    chomp($curr_output);
    print RESF "  output : " . $curr_output . "\"\n";
  }
  print RESF "}\n";
  IO::Handle::flush(\*RESF);
  resfile_new_test();
}

# Add current test results to test object (to send from worker)

sub resfile_to_test($)
{
  return unless $do_resfile;
  my $tinfo= shift;
  my @res_array= %curr_result;
  $tinfo->{'resfile'}= \@res_array;
  $tinfo->{'output'}= $curr_output if $curr_output;
}

# Get test results (from worker) from test object

sub resfile_from_test($)
{
  return unless $do_resfile;
  my $tinfo= shift;
  my $res_array= $tinfo->{'resfile'};
  return unless $res_array;
  %curr_result= @$res_array;
  $curr_output= $tinfo->{'output'} if defined $tinfo->{'output'};
}

1;
