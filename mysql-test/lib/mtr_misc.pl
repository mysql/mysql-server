# -*- cperl -*-
# Copyright (c) 2004, 2022, Oracle and/or its affiliates.
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

# This is a library file used by the Perl version of mysql-test-run,
# and is part of the translation of the Bourne shell script with the
# same name.

use strict;

use My::Platform;

our $opt_report_times;

# Initialize an empty array or list
sub mtr_init_args ($) {
  my $args = shift;
  $$args = [];
}

sub mtr_add_arg ($$@) {
  my $args   = shift;
  my $format = shift;
  my @fargs  = @_;

  # Quote args if args contain space
  $format = "\"$format\""
    if (IS_WINDOWS and grep(/\s/, @fargs));

  push(@$args, sprintf($format, @fargs));
}

sub mtr_args2str($@) {
  my $exe = shift or die;
  return join(" ", native_path($exe), @_);
}

# NOTE! More specific paths should be given before less specific.
# For example /client/debug should be listed before /client
sub mtr_path_exists (@) {
  foreach my $path (@_) {
    return $path if -e $path;
  }

  if (@_ == 1) {
    mtr_error("Could not find $_[0]");
  } else {
    mtr_error("Could not find any of " . join(" ", @_));
  }
}

# NOTE! More specific paths should be given before less specific.
# For example /client/debug should be listed before /client
sub mtr_script_exists (@) {
  foreach my $path (@_) {
    if (IS_WINDOWS) {
      return $path if -f $path;
    } else {
      return $path if -x $path;
    }
  }

  if (@_ == 1) {
    mtr_error("Could not find $_[0]");
  } else {
    mtr_error("Could not find any of " . join(" ", @_));
  }
}

# NOTE! More specific paths should be given before less specific.
# For example /client/debug should be listed before /client
sub mtr_file_exists (@) {
  foreach my $path (@_) {
    return $path if -e $path;
  }
  return "";
}

# NOTE! More specific paths should be given before less specific.
# For example /client/debug should be listed before /client
sub mtr_exe_maybe_exists (@) {
  my @path = @_;

  map { $_ .= ".exe" } @path if IS_WINDOWS;
  foreach my $path (@path) {
    if (IS_WINDOWS) {
      return $path if -f $path;
    } else {
      return $path if -x $path;
    }
  }
  return "";
}

# NOTE! More specific paths should be given before less specific.
sub mtr_pl_maybe_exists (@) {
  my @path = @_;

  map { $_ .= ".pl" } @path if IS_WINDOWS;
  foreach my $path (@path) {
    if (IS_WINDOWS) {
      return $path if -f $path;
    } else {
      return $path if -x $path;
    }
  }
  return "";
}

# NOTE! More specific paths should be given before less specific.
# For example /client/debug should be listed before /client
sub mtr_exe_exists (@) {
  my @path = @_;
  if (my $path = mtr_exe_maybe_exists(@path)) {
    return $path;
  }

  # Could not find exe, show error
  if (@path == 1) {
    mtr_error("Could not find $path[0]");
  } else {
    mtr_error("Could not find any of " . join(" ", @path));
  }
}

# Try to compress file using tools that might be available.
# If zip/gzip is not available, just silently ignore.
sub mtr_compress_file ($) {
  my ($filename) = @_;

  mtr_error("File to compress not found: $filename") unless -f $filename;

  my $did_compress = 0;
  if (IS_WINDOWS) {
    # Capture stderr
    my $ziperr = `zip $filename.zip $filename 2>&1`;
    if ($?) {
      print "$ziperr\n" if $ziperr !~ /recognized as an internal or external/;
    } else {
      unlink($filename);
      $did_compress = 1;
    }
  } else {
    my $gzres = system("gzip $filename");
    $did_compress = !$gzres;
    if ($gzres && $gzres != -1) {
      mtr_error("Error: have gzip but it fails to compress core file");
    }
  }
  mtr_print("Compressed file $filename") if $did_compress;
}

sub mtr_milli_sleep ($) {
  die "usage: mtr_milli_sleep(milliseconds)" unless @_ == 1;

  my ($millis) = @_;
  select(undef, undef, undef, ($millis / 1000));
}

# Simple functions to start and check timers (have to be actively
# polled). Timer can be "killed" by setting it to 0.

sub start_timer ($) { return time + $_[0]; }

sub has_expired ($) { return $_[0] && time gt $_[0]; }

# Below code is for time usage reporting

use Time::HiRes qw(gettimeofday);

my %time_used;
my %time_text = ('admin'   => "Test administration",
                 'ch-warn' => "Check for warnings",
                 'check'   => "Check-testcase",
                 'collect' => "Collecting test cases",
                 'init'    => "Initialization/cleanup",
                 'restart' => "Server stop/start",
                 'test'    => "Test execution",);

# Counts number of reports from workers

my $last_timer_set;
my $time_totals = 0;

sub init_timers() {
  $last_timer_set = gettimeofday();

  # Initialize the 'time_used' hash for each worker
  %time_used = ('admin'   => 0,
                'ch-warn' => 0,
                'check'   => 0,
                'collect' => 0,
                'init'    => 0,
                'restart' => 0,
                'test'    => 0,);
}

sub mark_time_used($) {
  my ($name) = @_;
  return unless $opt_report_times;
  die "Unknown timer $name" unless exists $time_used{$name};

  my $curr_time = gettimeofday();
  $time_used{$name} += int(($curr_time - $last_timer_set) * 1000 + .5);
  $last_timer_set = $curr_time;
}

sub mark_time_idle() {
  $last_timer_set = gettimeofday() if $opt_report_times;
}

sub add_total_times($) {
  my ($dummy, $num, @line) = split(" ", $_[0]);

  $time_totals++;
  foreach my $elem (@line) {
    my ($name, $spent) = split(":", $elem);
    $time_used{$name} += $spent;
  }
}

sub print_times_used($$) {
  my ($server, $num) = @_;
  return unless $opt_report_times;

  my $output = "SPENT $num";
  foreach my $name (keys %time_used) {
    my $spent = $time_used{$name};
    $output .= " $name:$spent";
  }
  print $server $output . "\n";
}

sub print_total_times($) {
  # Don't print if we haven't received all worker data
  return if $time_totals != $_[0];

  foreach my $name (keys %time_used) {
    my $spent = $time_used{$name} / 1000;
    my $text  = $time_text{$name};
    print("Spent $spent seconds on $text\n");
  }
}

1;
