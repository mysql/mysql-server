#!/usr/bin/perl -w

# Copyright (c) 2006, 2013, Oracle and/or its affiliates. All rights reserved.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

use strict;
use Getopt::Long;

sub usage
{
    print STDERR "Usage:\n";
    print STDERR "\tndb_error_reporter config.ini [username] [--fs] [--connection-timeout=#] [--skip-nodegroup=#] [--dry-scp] [--help]\n\n";
    print STDERR "\tusername is a user that you can use to ssh into\n";
    print STDERR "\t  all of your nodes with.\n\n";
    print STDERR "\t--fs means include the filesystems in the report\n";
    print STDERR "\t WARNING: This may require a lot of disk space.\n";
    print STDERR "\t          Only use this option when asked to.\n\n";
    print STDERR "\t--connection-timeout is the timeout in seconds\n";
    print STDERR "\t  to connect to a node\n\n";
    print STDERR "\t--skip-nodegroup allows you to skip all the nodes\n";
    print STDERR "\t  belonging to a specific nodegroup\n\n";
    print STDERR "\t--dry-scp allows running the script for testing without\n";
    print STDERR "\t  scp from the remote hosts\n\n";
    print STDERR "\t--help prints this message and exits\n\n";
    exit(1);
}

my $config_get_fs= 0;
my $config_connect_timeout;
my @config_skip_nodegroups;
my $config_dry_scp= 0;
my $config_help= 0;

my %options= (
    "--fs"                      => \$config_get_fs,
    "--connection-timeout=i"    => \$config_connect_timeout,
    "--skip-nodegroup=i@"       => \@config_skip_nodegroups,
    "--dry-scp"                 => \$config_dry_scp,
    "--help|usage|?"            => \$config_help,
);

GetOptions(%options) or usage();

# If --help provided, print usage and exit
if($config_help)
{
  print STDERR "This program creates an archive from data node and management node files.\n\n";
  usage();
}

# At least one positional argument must be given
# but never more than 2
if(@ARGV < 1 || @ARGV > 2)
{
  usage();
}

# First positional argument is name of the config file
my $config_file= $ARGV[0];
if(!stat($config_file))
{
    print STDERR "Cannot open configuration file.\n\n";
    usage(); 
}

# Second positional argument may contain scp username
my $config_scp_user= '';
if(defined($ARGV[1]))
{
  $config_scp_user.= $ARGV[1].'@';
}

use File::Basename;
my $dirname= dirname(__FILE__);
my $ndb_config= "$dirname/ndb_config";
my @nodes= split ' ',`$ndb_config --config-file=$config_file --nodes --query=id --type=ndbd`;
push @nodes, split ' ',`$ndb_config --config-file=$config_file --nodes --query=id --type=ndb_mgmd`;

sub config {
    my $nodeid= shift;
    my $query= shift;
    my $res= `$ndb_config --config-file=$config_file --id=$nodeid --query=$query`;
    chomp $res;
    $res;
}

my @t= localtime();
my $reportdir= sprintf('ndb_error_report_%u%02u%02u%02u%02u%02u',
		       ($t[5]+1900),($t[4]+1),$t[3],$t[2],$t[1],$t[0]);

if(stat($reportdir) || stat($reportdir.'tar.bz2'))
{
    print STDERR "It looks like another ndb_error_report process is running.\n";
    print STDERR "If that is not the case, remove the ndb_error_report directory";
    print STDERR " and run ndb_error_report again.\n\n";
    exit(1);
}

mkdir($reportdir);

sub scp
{
  my ($host, $from_dir, $to_dir, $recurse) = @_;

  my $scp_options = '';
  $scp_options .= ' -p '; # Preserve times from original
  if (defined($config_connect_timeout))
  {
    $scp_options .=  " -o ConnectTimeout=$config_connect_timeout ";
  }
  if ($recurse)
  {
    $scp_options .= ' -r ';
  }
  
  my $cmd = "scp $scp_options $config_scp_user$host:$from_dir $to_dir";
  if ($config_dry_scp)
  {
    # --dry-scp just prints the scp command
    print $cmd, "\n";
    return;
  }
  system($cmd);
}

sub skip_nodegroup
{
  my $nodegroup= shift;
  foreach my $skip_nodegroup(@config_skip_nodegroups)
  {
    if($nodegroup eq $skip_nodegroup)
    {
      return 1;
    }
  }
  return 0;
}

foreach my $node (@nodes)
{
    my $nodegroup= config($node, 'nodegroup');
    if(skip_nodegroup($nodegroup))
    {
      print("\n\n Node $node belongs to nodegroup $nodegroup: skipping.");
      next;
    }
    print "\n\n Copying data from node $node".
	(($config_get_fs)?" with filesystem":"").
	"\n\n";
    
    my $from_path = "ndb_$node*"; # Copy everything starting with ndb_<nodeid>
    my $datadir = config($node,'datadir');
    if ($datadir)
    {
      # Prepend datadir
      $from_path = "$datadir/$from_path";
    }
    scp(config($node,'host'), $from_path,  
	"$reportdir/", $config_get_fs);

    # Extract cluster log name from LogDestination(if any)
    foreach my $file_handler (grep(s/^FILE://i, split(/;/, config($node, 'LogDestination'))))
    {
      foreach my $file_name (grep(s/^filename=//i, split(/,/, $file_handler)))
      {
        # Check whether the file has an absolute path - otherwise the file
        # will be located in $datadir
        if (substr($file_name, 0, 1) ne '/')
        {
          $file_name = $datadir.'/'.$file_name;
        }
        print "  Copying cluster log from '$file_name' on node $node...\n";
	scp(config($node,'host'),
            "$file_name*",
	    "$reportdir/", 0);
      }
    }
}

print "\n\n Copying configuration file...\n\n\t$config_file\n\n";
system "cp -p $config_file $reportdir/";

my $r = system 'bzip2 2>&1 > /dev/null  < /dev/null';
my $outfile;
if($r==0)
{
    $outfile= "$reportdir.tar.bz2";
    system "tar cf - $reportdir|bzip2 > $outfile";
}
else
{
    $outfile= "$reportdir.tar.gz";
    system "tar cf - $reportdir|gzip > $outfile";
}

system "rm -rf $reportdir";

print "\n\nPlease attach $outfile to your error report\n\n";
