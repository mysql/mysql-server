# -*- cperl -*-
package My::File::Path;
use strict;


#
# File::Path::rmtree has a problem with deleting files
# and directories where it hasn't got read permission
#
# Patch this by installing a 'rmtree' function in local
# scope that first chmod all files to 0777 before calling
# the original rmtree function.
#
# This is almost gone in version 1.08 of File::Path -
# but unfortunately some hosts still suffers
# from this also in 1.08
#

use Exporter;
use base "Exporter";
our @EXPORT= qw / rmtree mkpath copytree /;


use File::Find;
use File::Path;
use File::Copy;
use Carp;

no warnings 'redefine';

sub rmtree {
  my ($dir)= @_;

  #
  # chmod all files to 0777 before calling rmtree
  #
  find( {
	 no_chdir => 1,
	 wanted => sub {
	   chmod(0777, $_)
	     or warn("couldn't chmod(0777, $_): $!");
	 }
	},
	$dir
      );


  # Call rmtree from File::Path
  goto &File::Path::rmtree;
};


sub mkpath {
  goto &File::Path::mkpath;
};


sub copytree {
  my ($from_dir, $to_dir) = @_;

  die "Usage: copytree(<fromdir>, <todir>" unless @_ == 2;

  mkpath("$to_dir");
  opendir(DIR, "$from_dir")
    or croak("Can't find $from_dir$!");
  for(readdir(DIR)) {

    next if "$_" eq "." or "$_" eq "..";

    # Skip SCCS/ directories
    next if "$_" eq "SCCS";

    if ( -d "$from_dir/$_" )
    {
      copytree("$from_dir/$_", "$to_dir/$_");
      next;
    }
    copy("$from_dir/$_", "$to_dir/$_");
  }
  closedir(DIR);
}

1;
