# -*- cperl -*-

# Copyright (c) 2004, 2016, Oracle and/or its affiliates. All rights reserved.
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

my @mtr_unique_fh;

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
sub mtr_get_unique_id($$$) {
  my ($min, $max, $build_threads_per_thread)= @_;

  msg("get $min-$max, $$");

  if (scalar @mtr_unique_fh == $build_threads_per_thread)
  {
    die "Can only get $build_threads_per_thread unique id(s) per process!";
  }

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

  my $build_thread= 0;
  while ( $build_thread < $build_threads_per_thread )
  {
    for (my $id= $min; $id <= $max; $id++)
    {
      my $fh;
      open( $fh, ">$dir/$id");
      chmod 0666, "$dir/$id";

      # Try to lock the file exclusively. If lock succeeds, we're done.
      if (flock($fh, LOCK_EX|LOCK_NB))
      {
        # Store file handle - we would need it to release the
        # ID (i.e to unlock the file)
        $mtr_unique_fh[$build_thread] = $fh;
        $build_thread= $build_thread + 1;
      }
      else
      {
        # Not able to get a lock on the file, start the search from
        # next id(i.e min+1).
        $min= $min + 1;

        for (;$build_thread > 0; $build_thread--)
        {
          if (defined $mtr_unique_fh[$build_thread-1])
          {
            close $mtr_unique_fh[$build_thread-1];
          }
        }

        # Close the file opened in the current iterartion.
        close $fh;
        last;
      }

      if ($build_thread == $build_threads_per_thread)
      {
        return $id - $build_thread + 1;
      }
    }

    return undef if ($min > $max);
  }

  return undef;
}


#
# Release a unique ID.
#
sub mtr_release_unique_id()
{
  msg("release $$");

  for (my $i= 0; $i <= $#mtr_unique_fh; $i++)
  {
    if (defined $mtr_unique_fh[$i])
    {
      close $mtr_unique_fh[$i];
    }
  }
  @mtr_unique_fh= ();
}

1;
