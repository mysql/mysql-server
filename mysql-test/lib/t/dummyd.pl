#!/usr/bin/perl
# -*- cperl -*-

# Copyright (c) 2007 MySQL AB
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

use strict;
use Getopt::Long;
use IO::File;

my $vardir;
my $randie= 0;
my $sleep= 0;
GetOptions
  (
   # Directory where to write files
   'vardir=s'     => \$vardir,
   'die-randomly' => \$randie,
   'sleep=i'      => \$sleep,
  );

die("invalid vardir ") unless defined $vardir and -d $vardir;

my $pid= $$;
while(1){
  for my $i (1..64){
    # Write to file
    my $name= "$vardir/$pid.$i.tmp";
    my $F= IO::File->new($name, "w")
      or warn "$$, Could not open $name: $!" and next;
    print $F rand($.) for (1..1000);
    $F->close();
    sleep($sleep);
    die "ooops!" if $randie and rand() < 0.0001
  }
}


exit (0);


