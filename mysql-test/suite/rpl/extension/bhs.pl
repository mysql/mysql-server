#!/usr/bin/perl

# Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.
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

use File::Basename;
use File::Copy qw(copy);
use File::Spec qw(catdir);
use File::Path;
use IO::File;
use strict;

# Constants
my $case_header= "###############################################################################\n" 
 . "# Note! The test case updated for running under blackhole slave configuration #\n"
 . "###############################################################################\n\n";
my $before_replace= "# *** BHS ***\n";
my $after_replace= "# *** /BHS ***\n";
my %copy_dirs= (
    "include"	=> "include",
    "extra"	=> "extra"
);

# Variables
my %test_dirs;
my @update_test_cases;
my %rules;
my $opt_trans_test_list;

print "Creating suite rpl_bhs\n";

# *** Set platform-independent pathes ***

# Set extension directory
my $ext_dir= dirname(File::Spec->rel2abs($0));
# Set bhs directory
my $bhs_dir= File::Spec->catdir($ext_dir, "bhs");
# Set mysql-test directory
my $mysql_test_dir= $ext_dir;
$mysql_test_dir =~ s/(\/|\\)suite(\/|\\)rpl(\/|\\)extension$//;
# Set path to mtr
my $mtr_script = File::Spec->catdir($mysql_test_dir, "mysql-test-run.pl");
# Set directory of rpl suite
my $suite_rpl_dir = File::Spec->catdir($mysql_test_dir, "suite", "rpl");
# Set directory of rpl_bhs suite
my $suite_rpl_bhs_dir = File::Spec->catdir($mysql_test_dir, "suite", "rpl_bhs");
# Set test cases mask with path
my $suite_rpl_bhs_cases_dir = File::Spec->catdir($suite_rpl_bhs_dir, "t");

# Check first argument
if ($ARGV[0] =~ m/\-\-trans\-test\-list=(.+)/i)
{
    $opt_trans_test_list= File::Spec->catdir($suite_rpl_bhs_dir, $1);
    shift @ARGV;    
    $mtr_script= "perl " . $mtr_script . " " . join(" ", @ARGV);
}
else
{
    die("First argument of bhs.pl must be --trans-test-list with path to test case list");
}

# *** Copy files ***

# Copy rpl suite into rpl_bhs
print "copying:\n  $suite_rpl_dir\n  --> $suite_rpl_bhs_dir\n";
dircopy($suite_rpl_dir, $suite_rpl_bhs_dir);

# Copy additional dirs outside of rpl suite
foreach my $cur_dir (keys %copy_dirs)
{
    my $from_dir= File::Spec->catdir($mysql_test_dir, $cur_dir);
    my $to_dir= File::Spec->catdir($suite_rpl_bhs_dir, $copy_dirs{$cur_dir});
    print "  $from_dir\n  --> $to_dir\n";
    dircopy($from_dir, $to_dir);
}

# Copy server config files
print "  configuration files\n";
copy(File::Spec->catdir($ext_dir, "bhs", "my.cnf"), $suite_rpl_bhs_dir);
copy(File::Spec->catdir($ext_dir, "bhs", "rpl_1slave_base.cnf"), $suite_rpl_bhs_dir);

# Add BHS disabled.def
print "updating disabled.def\n";
my $fh = new IO::File File::Spec->catdir($bhs_dir, "disabled.def"), "r";
if (defined $fh) {
    my @disabled = <$fh>;
    undef $fh;
    my $fh = new IO::File File::Spec->catdir($suite_rpl_bhs_dir, "t", "disabled.def"), O_WRONLY|O_APPEND;
    if (defined $fh) {
	print $fh join ("", @disabled);
	undef $fh;
    }
}


# *** Update test cases

# Read update_rules
my $fh = new IO::File File::Spec->catdir($bhs_dir, "update_test_cases"), "r";
if (defined $fh) {
    @update_test_cases = <$fh>;
    undef $fh;
}

foreach my $update (@update_test_cases)
{
    $update =~ s/\s//g;
    my ($tmpl, $file)= split(/\:/, $update);
    $file= File::Spec->catdir($bhs_dir, $file);
    $fh = new IO::File $file, "r";
    if (defined $fh) 
    {
	my @lines= <$fh>;
	undef $fh;
	my $found= "";
	my $replace= "";
	my $line_num= 0;
	foreach my $line (@lines)
	{
	    if ($line =~ m/^\s*\[(.+)\]\s*$/ && $found eq "")
	    {
		$found= $1;
	    }
	    elsif ($line =~ m/^\s*\[(.+)\]\s*$/ && $found ne "")
	    {
		$rules{$tmpl}{$found} = $replace;
		chomp $rules{$tmpl}{$found};
		$found= $1;
		$replace= "";
		$line_num= 0;
	    }
	    elsif ($line !~ m/^\s*$/)
	    {
		$replace .= $line;
		$line_num++;
	    }
	}
	if ($found ne "")
	{
	    $rules{$tmpl}{$found}= $replace;
	}
    }
}

for (my $i = 0; $i < scalar(@update_test_cases); $i++)
{
    if ($update_test_cases[$i] =~ m/(.+)\:.+/)
    {
	$update_test_cases[$i]= $1;
	my @cur_path= split(/\//, $update_test_cases[$i]);
	$update_test_cases[$i]= File::Spec->catdir(@cur_path);
	# Collect directories with test cases
	pop(@cur_path);	
	$test_dirs{File::Spec->catdir(@cur_path)}= 1;
    }
}

# Updating test cases
my $case_num= 0;
foreach my $test_dir (keys %test_dirs)
{
    # Read list of test cases
    my $cur_path= File::Spec->catdir($suite_rpl_bhs_dir, $test_dir);
    opendir(my $dh, $cur_path) or exit(1);
    my @cases = grep(/\.(test|inc)$/,readdir($dh));
    closedir($dh);    
    foreach my $case (sort @cases)
    {	
	my $case2= File::Spec->catdir($test_dir, $case);
	foreach my $update_case (@update_test_cases)
	{
	    my @paths= split(/\//, $update_case);
	    my $update_case2= File::Spec->catdir(@paths);
	    if (compare_names($case2, $update_case2) == 1)
	    {
		$fh = new IO::File File::Spec->catdir($cur_path, $case), "r";
		my @lines;
		if (defined $fh) 
		{
		    @lines = <$fh>;
		    undef $fh;
		}
		my $content= "";
		foreach my $line (@lines)
		{
		    foreach my $cmd (keys %{$rules{$update_case}})
		    {
			if ($line =~ m/$cmd/i)
			{
			    my $orig_line= "# Replaced command: " . $line;
			    $line =~ s/$cmd/$rules{$update_case}{$cmd}/;
			    $line =~ s/\n\n$/\n/;
			    $line = $before_replace . $orig_line . $line . $after_replace;
			    last;
			}
		    }
		    $content .= $line;			
		}
		$fh = new IO::File File::Spec->catdir($cur_path, $case), "w";
		if (defined $fh) 
		{
		    print $fh $case_header . $content;
		    undef $fh;
		}
		$case_num++;
		last;	    
	    }	    
	}	
    }
}

print "updated $case_num files\n";

print "Run $mtr_script\n";

system( $mtr_script );

sub compare_names
{
    my ($test, $rule)= @_;
    my $res= 0;
    $res= 1 if ($test eq $rule);
    if ($rule =~ m/\*/)
    {
	$rule =~ s/(\\|\/)+/\ /g;
	$rule =~ s/\*/\.\*/g;
	$test =~ s/(\\|\/)+/\ /g;
	$res= 1 if ($test =~ m/^$rule$/i)
    }
    return $res;
}

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
