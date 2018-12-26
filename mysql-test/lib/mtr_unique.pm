# -*- cperl -*-

# Copyright (c) 2004, 2018, Oracle and/or its affiliates. All rights reserved.
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

package mtr_unique;

use strict;

use base qw(Exporter);
our @EXPORT = qw(mtr_get_unique_id mtr_release_unique_id);

use Fcntl ':flock';

use My::Platform;

my @mtr_unique_fh;
my @mtr_unique_ids;

END {
  mtr_release_unique_id();
}

# Get a unique, numerical ID in a specified range.
#
# If no unique ID within the specified parameters can be
# obtained, return undef.
sub mtr_get_unique_id($$$) {
  my ($min, $max, $build_threads_per_thread) = @_;

  if (scalar @mtr_unique_fh == $build_threads_per_thread) {
    die "Can only get $build_threads_per_thread unique id(s) per process!";
  }

  my $build_thread = 0;
  while ($build_thread < $build_threads_per_thread) {
    for (my $id = $min ; $id <= $max ; $id++) {
      my $fh;
      open($fh, ">$::build_thread_id_dir/$id");
      chmod 0666, "$::build_thread_id_dir/$id";

      # Try to lock the file exclusively. If lock succeeds, we're done.
      if (flock($fh, LOCK_EX | LOCK_NB)) {
        # Store file handle - we would need it to release the
        # ID (i.e to unlock the file)
        $mtr_unique_fh[$build_thread]  = $fh;
        $mtr_unique_ids[$build_thread] = "$::build_thread_id_dir/$id";
        $build_thread                  = $build_thread + 1;
      } else {
        # Not able to get a lock on the file, start the search from
        # next id(i.e min+1).
        $min = $min + 1;

        for (; $build_thread > 0 ; $build_thread--) {
          if (defined $mtr_unique_fh[ $build_thread - 1 ]) {
            close $mtr_unique_fh[ $build_thread - 1 ];
            unlink $mtr_unique_ids[ $build_thread - 1 ] or
              warn "Could not unlink $mtr_unique_ids[$build_thread-1]: $!";
          }
        }

        # Close the file opened in the current iterartion.
        close $fh;
        last;
      }

      if ($build_thread == $build_threads_per_thread) {
        open(FH, ">>", $::build_thread_id_file) or
          die "Can't open file $::build_thread_id_file: $!";
        for (my $i = 0 ; $i <= $#mtr_unique_ids ; $i++) {
          # Write the build thread id file path to 'unique_ids.log' file
          print FH $mtr_unique_ids[$i] . "\n";
        }
        close(FH);
        return $id - $build_thread + 1;
      }
    }

    return undef if ($min > $max);
  }

  return undef;
}

# Release a unique ID.
sub mtr_release_unique_id() {
  for (my $i = 0 ; $i <= $#mtr_unique_fh ; $i++) {
    if (defined $mtr_unique_fh[$i]) {
      close $mtr_unique_fh[$i];
    }
  }

  @mtr_unique_fh = ();
}

1;
