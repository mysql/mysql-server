#!/usr/bin/perl
# Copyright (c) 2006, 2010, Oracle and/or its affiliates. All rights reserved.
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

use File::Find;
use Getopt::Long;

use strict;

sub run_cmd (@);

my %dispatch = (
    "run" => \&run_cmd,
);

=head1 NAME

unit - Run unit tests in directory

=head1 SYNOPSIS

  unit [--[no]big] [--[no]verbose] run [tests to run]

=cut

my $big= $ENV{'MYTAP_CONFIG'} eq 'big';

my $opt_verbose;
my $result = GetOptions (
  "big!"        => \$big,
  "verbose!"    => \$opt_verbose,
);

$ENV{'MYTAP_CONFIG'} = $big ? 'big' : '';

my $cmd = shift;

if (defined $cmd && exists $dispatch{$cmd}) {
    $dispatch{$cmd}->(@ARGV);
} else {
    print "Unknown command", (defined $cmd ? " $cmd" : ""), ".\n";
    print "Available commands are: ", join(", ", keys %dispatch), "\n";
}

=head2 run

Run all unit tests in the current directory and all subdirectories.

=cut

BEGIN {
    # Test::Harness have been extensively rewritten in newer perl
    # versions and is now just a backward compatibility wrapper
    # (with a bug causing the HARNESS_PERL_SWITCHES to be mangled)
    # Prefer to use TAP::Harness directly if available
    if (eval "use TAP::Harness; 1") {
        eval 'sub NEW_HARNESS { 1 }';
        warn "using TAP::Harness";
    } else {
        eval "use Test::Harness; 1" or  die "couldn't find Test::Harness!";
        eval 'sub NEW_HARNESS { 0 }';
    }
}

sub _find_test_files (@) {
    my @dirs = @_;
    my @files;
    find sub { 
        $File::Find::prune = 1 if /^SCCS$/;
        $File::Find::prune = 1 if /^.libs$/;
        push(@files, $File::Find::name) if -x _ && (/-t\z/ || /-t\.exe\z/);
    }, @dirs;
    return @files;
}

sub run_cmd (@) {
    my @files;

    # If no directories were supplied, we add all directories in the
    # current directory except 'mytap' since it is not part of the
    # test suite.
    if (@_ == 0) {
      # Ignore these directories
      my @ignore = qw(mytap SCCS);

      # Build an expression from the directories above that tests if a
      # directory should be included in the list or not.
      my $ignore = join(' && ', map { '$_ ne ' . "'$_'"} @ignore);

      # Open and read the directory. Filter out all files, hidden
      # directories, and directories named above.
      opendir(DIR, ".") or die "Cannot open '.': $!\n";
      @_ = grep { -d $_ && $_ !~ /^\..*/ && eval $ignore } readdir(DIR);
      closedir(DIR);
    }

    print "Running tests: @_\n";

    foreach my $name (@_) {
        push(@files, _find_test_files $name) if -d $name;
        push(@files, $name) if -f $name;
    }

    if (@files > 0) {
        # Removing the first './' from the file names
        foreach (@files) { s!^\./!! }

        if (NEW_HARNESS())
        {
          my %args = ( exec => [ ], verbosity => $opt_verbose );
          my $harness = TAP::Harness->new( \%args );
	  my $aggreg= $harness->runtests(@files);
	  # Signal failure to calling scripts
	  exit(1) if $aggreg->get_status() ne 'PASS';
        }
        else
        {
          $ENV{'HARNESS_VERBOSE'} =  $opt_verbose;
          $ENV{'HARNESS_PERL_SWITCHES'} .= ' -e "exec @ARGV"';
          runtests(@files);
        }
    }
}

