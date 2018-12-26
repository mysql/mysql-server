# -*- cperl -*-
# Copyright (c) 2005, 2017, Oracle and/or its affiliates. All rights reserved.
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
#
# This is a library file used by the Perl version of mysql-test-run,
# and is part of the translation of the Bourne shell script with the
# same name.

package mtr_cases_from_list;
use strict;
use mtr_report;
use base qw(Exporter);
our @EXPORT= qw(collect_test_cases_from_list);

##############################################################################
#
##  Collect information about test cases to be run
##
###############################################################################

sub collect_test_cases_from_list ($$$$) {
  my $suites= shift; # Semicolon separated list of test suites
  my $opt_cases= shift;
  my $opt_do_test_list= shift;
  my $opt_ctest= shift;
  my $ret= 0;
  if ($opt_do_test_list ne "") {
      $opt_do_test_list=~ s/^\~\//$ENV{HOME}\//;
  }
  open(FILE, "<", $opt_do_test_list) or $ret= 1;
  if ($ret) {
    mtr_report("Cannot open \"$opt_do_test_list\".");
    exit 1;
  }
  else {
    my @test_list= <FILE>;
    chomp(@test_list);
    close FILE;
    foreach my $elem (@test_list) {
      # Skip comments.
      next if ($elem =~ /^[\s ]*#/);
      if (length($elem) > 0) {
        my @tmp= split(/[ .]/, $elem);
        my $suite= @tmp[0];
        my $test=  @tmp[1];
        my $found = 1;
        # Check if suite is already in list of suites
        foreach my $elem (split(",", $$suites)) {
          if ($suite eq $elem) {
            $found= 0;
          }
        }
        # If not yet in list of suites add the suite to it.
        $$suites= $$suites.",".$suite if ($found);
        # Passing the qualified test name so that if --no-reorder
        # is passed, the test will not be looked for in all the suites.
        push (@$opt_cases, $suite.".".$test);
        }
    }
    if (@$opt_cases == 0) {
      mtr_report("Error: Test list doesn't contain test cases. Script interrupted!");
      exit 1;
    }
    # To avoid execution of unit tests.
    $$opt_ctest= 0;
  }
}
