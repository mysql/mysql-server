# -*- cperl -*-
# Copyright (c) 2006, 2008 MySQL AB, 2009 Sun Microsystems, Inc.
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

package mtr_unique;

use strict;
use Fcntl ':flock';

use base qw(Exporter);
our @EXPORT= qw(mtr_get_unique_id mtr_release_unique_id);

use My::Platform;

sub msg {
 # print "### unique($$) - ", join(" ", @_), "\n";
}

my $dir;

if(!IS_WINDOWS)
{
  $dir= "/tmp/mysql-unique-ids";
}
else
{
  # Try to use machine-wide directory location for unique IDs,
  # $ALLUSERSPROFILE . IF it is not available, fallback to $TEMP
  # which is typically a per-user temporary directory
  if (exists $ENV{'ALLUSERSPROFILE'} && -w $ENV{'ALLUSERSPROFILE'})
  {
    $dir= $ENV{'ALLUSERSPROFILE'}."/mysql-unique-ids";
  }
  else
  {
    $dir= $ENV{'TEMP'}."/mysql-unique-ids";
  }
}

my $mtr_unique_fh = undef;

END
{
  mtr_release_unique_id();
}

#
# Get a unique, numerical ID in a specified range.
#
# If no unique ID within the specified parameters can be
# obtained, return undef.
#
sub mtr_get_unique_id($$) {
  my ($min, $max)= @_;;

  msg("get $min-$max, $$");

  die "Can only get one unique id per process!" if defined $mtr_unique_fh;


  # Make sure our ID directory exists
  if (! -d $dir)
  {
    # If there is a file with the reserved
    # directory name, just delete the file.
    if (-e $dir)
    {
      unlink($dir);
    }

    mkdir $dir;
    chmod 0777, $dir;

    if(! -d $dir)
    {
      die "can't make directory $dir";
    }
  }


  my $fh;
  for(my $id = $min; $id <= $max; $id++)
  {
    open( $fh, ">$dir/$id");
    chmod 0666, "$dir/$id";
    # Try to lock the file exclusively. If lock succeeds, we're done.
    if (flock($fh, LOCK_EX|LOCK_NB))
    {
      # Store file handle - we would need it to release the ID (==unlock the file)
      $mtr_unique_fh = $fh;
      return $id;
    }
    else
    {
      close $fh;
    }
  }
  return undef;
}


#
# Release a unique ID.
#
sub mtr_release_unique_id()
{
  msg("release $$");
  if (defined $mtr_unique_fh)
  {
    close $mtr_unique_fh;
    $mtr_unique_fh = undef;
  }
}


1;

