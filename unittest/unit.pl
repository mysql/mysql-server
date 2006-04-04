#!/usr/bin/perl

# Override _command_line in the standard Perl test harness to prevent
# it from using "perl" to run the test scripts.
package MySQL::Straps;
use base qw(Test::Harness::Straps);
sub _command_line { return $_[1] }

package main;

use strict;
use Test::Harness;
use File::Find;

sub run_cmd (@);

my %dispatch = (
    "run" => \&run_cmd,
);

=head1 NAME

unit - Run unit tests in directory

=head1 SYNOPSIS

  unit run

=cut

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


sub _find_test_files (@) {
    my @dirs = @_;
    my @files;
    find sub { 
        $File::Find::prune = 1 if /^SCCS$/;
        push(@files, $File::Find::name) if -x _ && /\.t\z/;
    }, @dirs;
    return @files;
}

sub run_cmd (@) {
    my @files;

    push(@_, '.') if @_ == 0;

    foreach my $name (@_) {
        push(@files, _find_test_files $name) if -d $name;
        push(@files, $name) if -f $name;
    }
    
    if (@files > 0) {
        # Removing the first './' from the file names
        foreach (@files) { s!^\./!! }
        
        # Install the strap above instead of the default strap
        $Test::Harness::Strap = MySQL::Straps->new;
        
        runtests @files;
    }
}

