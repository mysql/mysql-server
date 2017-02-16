# -*- cperl -*-
# Copyright (c) 2007 MySQL AB, 2008, 2009 Sun Microsystems, Inc.
# Use is subject to license terms.
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
our @EXPORT= qw /rmtree mkpath copytree/;

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


use File::Basename;
sub _mkpath_debug {
  my ($message, $path, $dir, $err)= @_;

  print "=" x 40, "\n";
  print $message, "\n";
  print "err: '$err'\n";
  print "path: '$path'\n";
  print "dir: '$dir'\n";

  print "-" x 40, "\n";
  my $dirname= dirname($path);
  print "ls -l $dirname\n";
  print `ls -l $dirname`, "\n";
  print "-" x 40, "\n";
  print "dir $dirname\n";
  print `dir $dirname`, "\n";
  print "-" x 40, "\n";
  my $dirname2= dirname($dirname);
  print "ls -l $dirname2\n";
  print `ls -l $dirname2`, "\n";
  print "-" x 40, "\n";
  print "dir $dirname2\n";
  print `dir $dirname2`, "\n";
  print "-" x 40, "\n";
  print "file exists\n" if (-e $path);
  print "file is a plain file\n" if (-f $path);
  print "file is a directory\n" if (-d $path);
  print "-" x 40, "\n";
  print "showing handles for $path\n";
  My::Handles::show_handles($path);

  print "=" x 40, "\n";

}


sub mkpath {
  my $path;

  die "Usage: mkpath(<path>)" unless @_ == 1;

  foreach my $dir ( File::Spec->splitdir( @_ ) ) {
    #print "dir: $dir\n";
    if ($dir =~ /^[a-z]:/i){
      # Found volume ie. C:
      $path= $dir;
      next;
    }

    $path= File::Spec->catdir($path, $dir);
    #print "path: $path\n";

    next if -d $path; # Path already exists and is a directory
    croak("File already exists but is not a directory: '$path'") if -e $path;
    next if mkdir($path);
    _mkpath_debug("mkdir failed", $path, $dir, $!);

    # mkdir failed, try one more time
    next if mkdir($path);
    _mkpath_debug("mkdir failed, second time", $path, $dir, $!);

    # mkdir failed again, try two more time after sleep(s)
    sleep(1);
    next if mkdir($path);
    _mkpath_debug("mkdir failed, third time", $path, $dir, $!);

    sleep(1);
    next if mkdir($path);
    _mkpath_debug("mkdir failed, fourth time", $path, $dir, $!);

    # Report failure and die
    croak("Couldn't create directory '$path' ",
	  " after 4 attempts and 2 sleep(1): $!");
  }
};


sub copytree {
  my ($from_dir, $to_dir, $use_umask) = @_;

  die "Usage: copytree(<fromdir>, <todir>, [<umask>])"
    unless @_ == 2 or @_ == 3;

  my $orig_umask;
  if ($use_umask){
    # Set new umask and remember the original
    $orig_umask= umask(oct($use_umask));
  }

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

    # Only copy plain files
    next unless -f "$from_dir/$_";
    copy("$from_dir/$_", "$to_dir/$_");
  }
  closedir(DIR);

  if ($orig_umask){
    # Set the original umask
    umask($orig_umask);
  }
}

1;
