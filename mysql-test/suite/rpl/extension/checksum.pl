#!/usr/bin/perl

# Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.
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

use File::Basename;
use File::Copy qw(copy);
use File::Spec qw(catdir);
use File::Path;
use IO::File;
use strict;

# Constants and variables with default values
my $suites;
my $suffix = "_checksum";
my $percent_random_test = 10;
my $mtr_script;
my @mtr_argv;
my @mtr_suites;

# Check some arguments
foreach my $arg ( @ARGV )
{
    if ($arg =~ m/\-\-suite\=(.+)/i)
    {
        $suites = $1;
    }
    elsif ($arg =~ m/\-\-percent\=(\d{1,2})/i)
    {
	$percent_random_test= $1;
    }
    else
    {
	push(@mtr_argv, $arg);
    }
    
}
if (! defined( $suites ) )
{
    die("The script requires --suite argument");
}

print "#################################################################\n";
print "# Binlog checksum testing\n";
print "# Run randomly $percent_random_test\% of tests from following suites: $suites\n";
print "#################################################################\n";

# Set extension directory
my $ext_dir= dirname(File::Spec->rel2abs($0));
# Set mysql-test directory
my $mysql_test_dir= $ext_dir;
$mysql_test_dir =~ s/(\/|\\)suite(\/|\\)rpl(\/|\\)extension$//;

# Main loop
foreach my $src_suite (split(",", $suites))
{
    $src_suite=~ s/ //g;
    my $dest_suite= $src_suite . $suffix;
    push( @mtr_suites, $dest_suite);
    print "Creating suite $dest_suite\n";
    # *** Set platform-independent pathes ***
    # Set source directory of suite
    my $src_suite_dir = File::Spec->catdir($mysql_test_dir, "suite", $src_suite);
    # Set destination directory of suite
    my $dest_suite_dir = File::Spec->catdir($mysql_test_dir, "suite", $dest_suite);
    print "Copying files\n\tfrom '$src_suite_dir'\n\tto '$dest_suite_dir'\n";
    dircopy($src_suite_dir, $dest_suite_dir);
    my $test_case_dir= File::Spec->catdir($dest_suite_dir, "t");
    # Read disabled.def
    my %disabled = ();
    print "Read disabled.def\n";
    my $fh = new IO::File File::Spec->catdir($test_case_dir, "disabled.def"), "r";
    if ( defined $fh ) 
    {
	my @lines = <$fh>;
	undef $fh;
	foreach my $line ( @lines )
	{
	    if ($line =~ m/^([a-zA-Z0-9_]+).+\:.+/i)
	    {
		$disabled{$1}= 1;
	    }
	}
    }
    # Read test case list
    my %tests = ();
    print "Generate test case list\n";
    opendir my ($dh), $test_case_dir or die "Could not open dir '$test_case_dir': $!";
    for my $entry (readdir $dh) 
    {
	if ( $entry =~ m/^([a-zA-Z0-9_]+)\.test$/i )
	{
	    my $test= $1;
	    if ( ! defined( $disabled{$test}) )
	    {
		$tests{$test}= 1;
	    }
	}
    }
    closedir($dh);
    # 
    my @excluded = ();
    my $excluded_test= int((((100 - $percent_random_test)/100) * scalar( keys %tests )));
    while  ( $excluded_test > 0 )
    {
	my @cases = keys %tests;
	my $test = $cases[int(rand(scalar(@cases)))];
	push ( @excluded, $test . "\t\t: Excluded for $dest_suite\n" );
	delete $tests{$test};		
	$excluded_test--;
    }
    my $fh = new IO::File File::Spec->catdir($test_case_dir, "disabled.def"), O_WRONLY|O_APPEND;
    if (defined $fh) {
	print $fh join ("", sort @excluded);
	undef $fh;
    }
    print "\t" . join("\n\t", sort keys %tests) . "\n";
    
}

# Set path to mtr with arguments
my $mtr_script = "perl " . File::Spec->catdir($mysql_test_dir, "mysql-test-run.pl") . 
    " --suite=" . join(",", @mtr_suites) . " " . 
    " --mysqld=--binlog-checksum=CRC32 " . 
    join (" ", @mtr_argv);

print "Run $mtr_script\n";
system( $mtr_script );

sub dircopy
{
    my ($from_dir, $to_dir)= @_;
    mkdir $to_dir if (! -e $to_dir);
    opendir my($dh), $from_dir or die "Could not open dir '$from_dir': $!";
    for my $entry (readdir $dh) 
    {
	next if $entry =~ /^(\.|\.\.)$/;
        my $source = File::Spec->catdir($from_dir, $entry);
        my $destination = File::Spec->catdir($to_dir, $entry);
        if (-d $source) 
        {
    	    mkdir $destination or die "mkdir '$destination' failed: $!" if not -e $destination;
            dircopy($source, $destination);
        } 
        else 
        {
    	    copy($source, $destination) or die "copy '$source' to '$destination' failed: $!";
        }
    }
    closedir $dh;
    return;                                                                                                  
}
