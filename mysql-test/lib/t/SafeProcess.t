# -*- cperl -*-

# Copyright (c) 2007, 2022, Oracle and/or its affiliates.
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

use strict;
use warnings 'FATAL';
use lib "lib";

use FindBin;
use IO::File;

use Test::More qw(no_plan);
BEGIN { use_ok ("My::SafeProcess");}


my $perl_path= $^X;

my $bindir= $ENV{MYSQL_BIN_PATH} || ".";
my $client_bindir = $ENV{MYSQL_CLIENT_BIN_PATH};

My::SafeProcess::find_bin($bindir, $client_bindir);

{
  # Test exit codes
  my $count= 32;
  my $ok_count= 0;
  for my $code (0..$count-1) {

    my $args= [ "$FindBin::Bin/test_child.pl", "--exit-code=$code" ];
    my $proc= My::SafeProcess->new
      (
       path          => $perl_path,
       args          => \$args,
       output        => "/dev/null",
       error         => "/dev/null",
      );
    # Wait max 10 seconds for the process to finish
    $ok_count++ if ($proc->wait_one(10) == 0 and
		    $proc->exit_status() == $code);
  }
  ok($count == $ok_count, "check exit_status, $ok_count");
}

{
  # spawn a number of concurrent processes
  my $count= 16;
  my $ok_count= 0;
  my %procs;
  for my $code (0..$count-1) {

    my $args= [ "$FindBin::Bin/test_child.pl", "--exit-code=$code" ];
    $procs{$code}= My::SafeProcess->new
      (
       path          => $perl_path,
       args          => \$args,
       output        => "/dev/null",
       error         => "/dev/null",
      );
  }

  for my $code (0..$count-1) {
    $ok_count++ if ($procs{$code}->wait_one(10) == 0 and
		    $procs{$code}->exit_status() == $code);
  }
  ok($count == $ok_count, "concurrent, $ok_count");
}


#
# Test stdout, stderr
#
{
  use File::Temp qw / tempdir /;
  my $dir = tempdir( CLEANUP => 1 );

  my $args= [ "$FindBin::Bin/test_child.pl" ];
  my $proc= My::SafeProcess->new
    (
     path          => $perl_path,
     args          => \$args,
     output        => "$dir/output.txt",
     error         => "$dir/error.txt",
    );

  $proc->wait_one(2); # Wait max 2 seconds for the process to finish

  my $fh= IO::File->new("$dir/output.txt");
  my @text= <$fh>;
  $fh->close();
  ok(scalar grep(/Hello stdout/, @text) == 1, "check stdout");
  $fh= IO::File->new("$dir/error.txt");
  @text= <$fh>;
  $fh->close();
  ok(scalar grep(/Hello stderr/, @text) == 1, "check stderr");

  # To same file
  $proc= My::SafeProcess->new
    (
     path          => $perl_path,
     args          => \$args,
     output        => "$dir/output.txt",
     error         => "$dir/output.txt",
    );

  $proc->wait_one(2); # Wait max 2 seconds for the process to finish

  $fh= IO::File->new("$dir/output.txt");
  @text= <$fh>;
  $fh->close();
  ok(scalar grep(/Hello stdout/, @text) == 1, "check stdout");
  ok(scalar grep(/Hello stderr/, @text) == 1, "check stderr");

  # To same file, but append data
  $proc= My::SafeProcess->new
    (
     path          => $perl_path,
     args          => \$args,
     output        => "$dir/output.txt",
     error         => "$dir/output.txt",
     append        => 1,
    );

  $proc->wait_one(2); # Wait max 2 seconds for the process to finish

  $fh= IO::File->new("$dir/output.txt");
  @text= <$fh>;
  $fh->close();
  ok(scalar grep(/Hello stdout/g, @text) == 2, "check stdout");
  ok(scalar grep(/Hello stderr/g, @text) == 2, "check stderr");

  # To same file, but overwrite data
  $proc= My::SafeProcess->new
    (
     path          => $perl_path,
     args          => \$args,
     output        => "$dir/output.txt",
     error         => "$dir/output.txt",
    );

  $proc->wait_one(2); # Wait max 2 seconds for the process to finish

  $fh= IO::File->new("$dir/output.txt");
  @text= <$fh>;
  $fh->close();
  ok(scalar grep(/Hello stdout/, @text) == 1, "check stdout");
  ok(scalar grep(/Hello stderr/, @text) == 1, "check stderr");

  # Don't redirect stdout, but only stderr to append
  $proc= My::SafeProcess->new
    (
     path          => $perl_path,
     args          => \$args,
     error         => "$dir/output.txt",
     append        => 1,
    );

  $proc->wait_one(2); # Wait max 2 seconds for the process to finish

  $fh= IO::File->new("$dir/output.txt");
  @text= <$fh>;
  $fh->close();
  ok(scalar grep(/Hello stdout/g, @text) == 1, "check stdout");
  ok(scalar grep(/Hello stderr/g, @text) == 2, "check stderr");

  # Don't redirect stderr, but only stdout to append
  $proc= My::SafeProcess->new
    (
     path          => $perl_path,
     args          => \$args,
     output        => "$dir/output.txt",
     append        => 1,
    );

  $proc->wait_one(2); # Wait max 2 seconds for the process to finish

  $fh= IO::File->new("$dir/output.txt");
  @text= <$fh>;
  $fh->close();
  ok(scalar grep(/Hello stdout/g, @text) == 2, "check stdout");
  ok(scalar grep(/Hello stderr/g, @text) == 2, "check stderr");

  # Redirect stderr to /dev/null, but stdout to appended file
  $proc= My::SafeProcess->new
    (
     path          => $perl_path,
     args          => \$args,
     output        => "/dev/null",
     error         => "$dir/output.txt",
     append        => 1,
    );

  $proc->wait_one(2); # Wait max 2 seconds for the process to finish

  $fh= IO::File->new("$dir/output.txt");
  @text= <$fh>;
  $fh->close();
  ok(scalar grep(/Hello stdout/g, @text) == 2, "check stdout");
  ok(scalar grep(/Hello stderr/g, @text) == 3, "check stderr");

  # Redirect stdout to /dev/null, but stderr to appended file
  $proc= My::SafeProcess->new
    (
     path          => $perl_path,
     args          => \$args,
     output        => "$dir/output.txt",
     error         => "/dev/null",
     append        => 1,
    );

  $proc->wait_one(2); # Wait max 2 seconds for the process to finish

  $fh= IO::File->new("$dir/output.txt");
  @text= <$fh>;
  $fh->close();
  ok(scalar grep(/Hello stdout/g, @text) == 3, "check stdout");
  ok(scalar grep(/Hello stderr/g, @text) == 3, "check stderr");

}
