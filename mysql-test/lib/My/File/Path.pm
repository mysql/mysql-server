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
use File::Copy;
use File::Spec;
use Carp;
use My::Handles;
use My::Platform;

sub rmtree {
  my ($dir)= @_;
  find( {
	 bydepth 		=> 1,
	 no_chdir 		=> 1,
	 wanted => sub {
	   my $name= $_;
	   if (!-l $name && -d _){
	     return if (rmdir($name) == 1);

	     chmod(0777, $name) or carp("couldn't chmod(0777, $name): $!");

	     return if (rmdir($name) == 1);

	     # Failed to remove the directory, analyze
	     carp("Couldn't remove directory '$name': $!");
	     My::Handles::show_handles($name);
	   } else {
	     return if (unlink($name) == 1);

	     chmod(0777, $name) or carp("couldn't chmod(0777, $name): $!");

	     return if (unlink($name) == 1);

	     carp("Couldn't delete file '$name': $!");
	     My::Handles::show_handles($name);
	   }
	 }
	}, $dir );
};


sub mkpath {
  my $path;
  foreach my $dir ( File::Spec->splitdir( @_ ) ) {
    #print "dir: $dir\n";
    if ($dir =~ /^[a-z]:/i){
      # Found volume ie. C:
      $path= $dir;
      next;
    }

    $path= File::Spec->catdir($path, $dir);
    #print "path: $path\n";

    next if -d $path; # Path already exist
    next if mkdir($path); # mkdir worked

    # mkdir failed, try one more time
    next if mkdir($path);

    # mkdir failed again, try two more time after sleep(s)
    sleep(1);
    next if mkdir($path);
    sleep(1);
    next if mkdir($path);

    # Report failure and die
    croak("Couldn't create directory '$path' ",
	  " after 4 attempts and 2 sleep(1): $!");
  }
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
