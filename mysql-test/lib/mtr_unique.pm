# -*- cperl -*-
# Copyright (C) 2006 MySQL AB
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

my $file;

if(!IS_WINDOWS)
{
  $file= "/tmp/mysql-test-ports";
}
else
{
  $file= $ENV{'TEMP'}."/mysql-test-ports";
}
  

my %mtr_unique_ids;

END {
  my $allocated_id= $mtr_unique_ids{$$};
  if (defined $allocated_id)
  {
    mtr_release_unique_id($allocated_id);
  }
  delete $mtr_unique_ids{$$};
}

#
# Get a unique, numerical ID, given a file name (where all
# requested IDs are stored), a minimum and a maximum value.
#
# If no unique ID within the specified parameters can be
# obtained, return undef.
#
sub mtr_get_unique_id($$) {
  my ($min, $max)= @_;;

  msg("get, '$file', $min-$max");

  die "Can only get one unique id per process!" if $mtr_unique_ids{$$};

  my $ret = undef;
  my $changed = 0;

  if(eval("readlink '$file'") || eval("readlink '$file.sem'")) {
    die 'lock file is a symbolic link';
  }

  chmod 0777, "$file.sem";
  open SEM, ">", "$file.sem" or die "can't write to $file.sem";
  flock SEM, LOCK_EX or die "can't lock $file.sem";
  if(! -e $file) {
    open FILE, ">", $file or die "can't create $file";
    close FILE;
  }

  msg("HAVE THE LOCK");

  if(eval("readlink '$file'") || eval("readlink '$file.sem'")) {
    die 'lock file is a symbolic link';
  }

  chmod 0777, $file;
  open FILE, "+<", $file or die "can't open $file";
  #select undef,undef,undef,0.2;
  seek FILE, 0, 0;
  my %taken = ();
  while(<FILE>) {
    chomp;
    my ($id, $pid) = split / /;
    $taken{$id} = $pid;
    msg("taken: $id, $pid");
    # Check if process with given pid is alive
    if(!process_alive($pid)) {
      print "Removing slot $id used by missing process $pid\n";
      msg("Removing slot $id used by missing process $pid");
      delete $taken{$id};
      $changed++;
    }
  }
  for(my $i=$min; $i<=$max; ++$i) {
    if(! exists $taken{$i}) {
      $ret = $i;
      $taken{$i} = $$;
      $changed++;
      # Remember the id this process got
      $mtr_unique_ids{$$}= $i;
      msg(" got $i"); 
      last;
    }
  }
  if($changed) {
    seek FILE, 0, 0;
    truncate FILE, 0 or die "can't truncate $file";
    for my $k (keys %taken) {
      print FILE $k . ' ' . $taken{$k} . "\n";
    }
  }
  close FILE;

  msg("RELEASING THE LOCK");
  flock SEM, LOCK_UN or warn "can't unlock $file.sem";
  close SEM;

  return $ret;
}


#
# Release a unique ID.
#
sub mtr_release_unique_id($) {
  my ($myid)= @_;

  msg("release, $myid");


  if(eval("readlink '$file'") || eval("readlink '$file.sem'")) {
    die 'lock file is a symbolic link';
  }

  open SEM, ">", "$file.sem" or die "can't write to $file.sem";
  flock SEM, LOCK_EX or die "can't lock $file.sem";

  msg("HAVE THE LOCK");

  if(eval("readlink '$file'") || eval("readlink '$file.sem'")) {
    die 'lock file is a symbolic link';
  }

  if(! -e $file) {
    open FILE, ">", $file or die "can't create $file";
    close FILE;
  }
  open FILE, "+<", $file or die "can't open $file";
  #select undef,undef,undef,0.2;
  seek FILE, 0, 0;
  my %taken = ();
  while(<FILE>) {
    chomp;
    my ($id, $pid) = split / /;
    msg(" taken, $id $pid");
    $taken{$id} = $pid;
  }

  if ($taken{$myid} != $$)
  {
    msg(" The unique id for this process does not match pid");
  }


  msg(" removing $myid");
  delete $taken{$myid};
  seek FILE, 0, 0;
  truncate FILE, 0 or die "can't truncate $file";
  for my $k (keys %taken) {
    print FILE $k . ' ' . $taken{$k} . "\n";
  }
  close FILE;

  msg("RELEASE THE LOCK");

  flock SEM, LOCK_UN or warn "can't unlock $file.sem";
  close SEM;

  delete $mtr_unique_ids{$$};
}


1;

