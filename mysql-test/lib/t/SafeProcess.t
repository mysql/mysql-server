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
use FindBin;
use IO::File;

use Test::More qw(no_plan);
use_ok ("My::SafeProcess");


my $perl_path= $^X;

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
  ok(grep(/Hello stdout/, @text), "check stdout");
  $fh= IO::File->new("$dir/error.txt");
  my @text= <$fh>;
  ok(grep(/Hello stderr/, @text), "check stderr");

  # To same file
  $proc= My::SafeProcess->new
    (
     path          => $perl_path,
     args          => \$args,
     output        => "$dir/output.txt",
     error         => "$dir/output.txt",
     debug         => 1,
    );

  $proc->wait_one(2); # Wait max 2 seconds for the process to finish

  my $fh= IO::File->new("$dir/output.txt");
  my @text= <$fh>;
  ok((grep(/Hello stdout/, @text) and grep(/Hello stderr/, @text)),
     "check stdout and stderr");

}
