#!/usr/bin/perl
# -*- cperl -*-

# Copyright (c) 2007, 2008 MySQL AB
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
use FindBin;
use My::SafeProcess;

#
# Test longterm running of SafeProcess
#

my $perl_path= $^X;
my $verbose= 0;
my $loops= 100;

print "kill one and wait for one\n";
for (1...$loops){
  use File::Temp qw / tempdir /;
  my $dir = tempdir( CLEANUP => 1 );

  my @procs;
  for (1..10){

    my $args= [ "$FindBin::Bin/dummyd.pl", "--vardir=$dir" ];
    my $proc= My::SafeProcess->new
      (
       path          => $perl_path,
       args          => \$args,
       verbose       => $verbose,
      );
    push(@procs, $proc);
  }

  foreach my $proc (@procs) {
    $proc->kill();
    # dummyd will always be killed and thus
    # exit_status should have been set to 1
    die "oops, exit_status: ", $proc->exit_status()
      unless $proc->exit_status() == 1;
  }

  print "=" x 60, "\n";
}


print "With 1 second sleep in dummyd\n";
for (1...$loops){
  use File::Temp qw / tempdir /;
  my $dir = tempdir( CLEANUP => 1 );

  my @procs;
  for (1..10){

    my $args= [ "$FindBin::Bin/dummyd.pl",
		"--vardir=$dir",
		"--sleep=1" ];
    my $proc= My::SafeProcess->new
      (
       path          => $perl_path,
       args          => \$args,
       verbose       => $verbose,
      );
    push(@procs, $proc);
  }

  foreach my $proc (@procs) {
    $proc->kill();
  }

  print "=" x 60, "\n";
}

print "kill all and wait for one\n";
for (1...$loops){
  use File::Temp qw / tempdir /;
  my $dir = tempdir( CLEANUP => 1 );

  my @procs;
  for (1..10){

    my $args= [ "$FindBin::Bin/dummyd.pl", "--vardir=$dir" ];
    my $proc= My::SafeProcess->new
      (
       path          => $perl_path,
       args          => \$args,
       verbose       => $verbose,
      );
    push(@procs, $proc);
  }

  foreach my $proc (@procs) {
    $proc->start_kill();
  }

  foreach my $proc (@procs) {
    $proc->wait_one();
  }

  print "=" x 60, "\n";
}

print "kill all using shutdown without callback\n";
for (1...$loops){
  use File::Temp qw / tempdir /;
  my $dir = tempdir( CLEANUP => 1 );

  my @procs;
  for (1..10){

    my $args= [ "$FindBin::Bin/dummyd.pl", "--vardir=$dir" ];
    my $proc= My::SafeProcess->new
      (
       path          => $perl_path,
       args          => \$args,
       verbose       => $verbose,
       );
    push(@procs, $proc);
  }

  My::SafeProcess::shutdown(2, @procs);

  print "=" x 60, "\n";
}

print "kill all using shutdown\n";
for (1...$loops){
  use File::Temp qw / tempdir /;
  my $dir = tempdir( CLEANUP => 1 );

  my @procs;
  for (1..10){

    my $args= [ "$FindBin::Bin/dummyd.pl", "--vardir=$dir" ];
    my $proc= My::SafeProcess->new
      (
       path          => $perl_path,
       args          => \$args,
       verbose       => $verbose,
       shutdown      => sub {  }, # Does nothing
      );
    push(@procs, $proc);
  }

  My::SafeProcess::shutdown(2, @procs);

  print "=" x 60, "\n";
}

exit(0);
