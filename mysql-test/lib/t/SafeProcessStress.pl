#!/usr/bin/perl
# -*- cperl -*-

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
