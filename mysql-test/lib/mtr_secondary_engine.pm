# -*- cperl -*-
# Copyright (c) 2005, 2018, Oracle and/or its affiliates. All rights reserved.
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

package mtr_secondary_engine;

use strict;
use warnings;

use base qw(Exporter);
our @EXPORT = qw(
  check_number_of_servers
  check_plugin_dir
  get_rapid_server_log
  install_rapid_plugin
  rapid_environment_setup
  start_rapid_server
  stop_rapid_server
  $rapid_plugin_dir
  $rapid_port
);

use File::Basename;
use IO::File;

use My::File::Path;
use My::Find;

use mtr_match;
use mtr_report;

do "mtr_misc.pl";

our $rapid_plugin_dir;
our $rapid_port;

# Find path to rapid server executable.
sub find_rapid($) {
  my $bindir = shift;

  my $rapid_bindir = my_find_dir($bindir, ["internal/rapid/"],
                                 [ "olrapid-debug", "olrapid-perf" ]);
  return my_find_bin($rapid_bindir, ["bin"], ["rpdserver"]);
}

# Find the directory location containing the rapid plugin.
sub rapid_plugin_dir($$) {
  my $find_plugin = shift;
  my $bindir      = shift;

  my $rapid_plugin = $find_plugin->('ha_rpd', 'plugin_output_directory');

  if ($rapid_plugin) {
    # return the directory containing the rapid plugin.
    return dirname($rapid_plugin);
  }

  # Couldn't find rapid plugin 'ha_rpd'
  mtr_error("Can't find rapid plugin 'ha_rpd' in " .
            "'$bindir/plugin_output_directory' location");
}

# Setup needed for rapid server
sub rapid_environment_setup($$) {
  my $find_plugin = shift;
  my $bindir      = shift;

  # Search for rapid executable location.
  $ENV{'RAPID'} = find_rapid($bindir);

  # Set 'RPDMASTER_FILEPREFIX' environment variable
  $ENV{'RPDMASTER_FILEPREFIX'} = "$::opt_vardir/log/rapid/";

  my $rapid_ld_library_path =
    my_find_dir($bindir, ["internal/rapid/"],
                [ "rpdmaster-debug", "rpdmaster-perf" ]);

  $ENV{'LD_LIBRARY_PATH'} = join(":",
            $rapid_ld_library_path,
            $ENV{'LD_LIBRARY_PATH'} ? split(':', $ENV{'LD_LIBRARY_PATH'}) : ());

  $rapid_plugin_dir = rapid_plugin_dir($find_plugin, $bindir);
}

# Start the rapid server.
sub start_rapid_server() {
  my $rapid = $::config->group('rapid');

  # Create rapid specific logdir under '$opt_vardir/log'.
  $rapid->{'logdir'} = "$::opt_vardir/log/rapid/";
  mkpath($rapid->{'logdir'});

  my $args;
  mtr_init_args(\$args);

  mtr_add_arg($args, "--heapSize=4096");
  mtr_add_arg($args, "--nwHeapSize=3584");
  mtr_add_arg($args, "--ifname=" . My::SysInfo->network_interface());
  mtr_add_arg($args, "-c 1");
  mtr_add_arg($args, "-d 0");
  mtr_add_arg($args, "-n 0");
  mtr_add_arg($args, "-p $rapid_port");
  mtr_add_arg($args, "-i BOO");
  mtr_add_arg($args, "--network=FOO");
  mtr_add_arg($args, "-l 0");
  mtr_add_arg($args, "-r 1");
  mtr_add_arg($args, "-v");
  mtr_add_arg($args, "-j");
  mtr_add_arg($args, $rapid->{'logdir'});

  mtr_verbose(My::Options::toStr("rapid_start", @$args));

  my $errfile = "$rapid->{'logdir'}/rapid.err";

  $rapid->{'proc'} =
    My::SafeProcess->new(append  => 1,
                         args    => \$args,
                         error   => $errfile,
                         name    => "rapid",
                         output  => $errfile,
                         path    => $ENV{'RAPID'},
                         verbose => $::opt_verbose);

  mtr_verbose("Started $rapid->{'proc'}");
}

sub stop_rapid_server () {
  return if not defined $::config;
  my $rapid = $::config->group('rapid');
  mtr_verbose("Stopping rapid server $rapid->{'proc'}");
  My::SafeProcess::shutdown($::opt_shutdown_timeout, $rapid->{proc});
}

# Skip tests starting more than one server.
sub check_number_of_servers($$) {
  my $mysqlds = shift;
  my $tinfo   = shift;

  if (scalar($mysqlds->()) > 1) {
    $tinfo->{'skip'} = 1;
    $tinfo->{'comment'} =
      "Can't run tests starting more than one server with RAPID.";
  }
}

# Check if the 'plugin-dir' is set to a path other than rapid
# plugin directory location.
sub check_plugin_dir($) {
  my $tinfo = shift;

  my $plugin_dir;
  foreach my $opt (@{ $tinfo->{master_opt} }) {
    $plugin_dir = mtr_match_prefix($opt, "--plugin-dir=") ||
      mtr_match_prefix($opt, "--plugin_dir=");
  }

  if (defined $plugin_dir) {
    if ($plugin_dir ne $rapid_plugin_dir) {
      # Different plugin dir, skip the test with rapid.
      $tinfo->{'skip'} = 1;
      $tinfo->{'comment'} =
        "Test requires plugin-dir to set to plugin_output_directory.";
    }
  }
}

# Wait for RAPID server to start.
sub sleep_until_rapid_cluster_bootstrapped($$$) {
  my $run_query = shift;
  my $mysqld    = shift;
  my $tinfo     = shift;

  my $sleeptime  = 100;                                        # In milliseconds
  my $total_time = 0;                                          # In milliseconds
  my $loops      = ($::opt_start_timeout * 1000) / $sleeptime;

  my $outfile = "$::opt_vardir/tmp/show_rapid_cluster_status.out";
  my $query   = "SHOW STATUS LIKE 'rapid_cluster_status'";

  for (my $loop = 1 ; $loop <= $loops ; $loop++) {
    if (!$run_query->($mysqld, $query, $outfile, undef)) {
      # Query succeeded, fetch the status value.
      my $filehandle   = IO::File->new($outfile);
      my $rapid_status = <$filehandle>;

      # No need of file handle now, close it.
      $filehandle->close();

      # Check the RAPID cluster status
      if ($rapid_status =~ /^rapid_cluster_status\s+ON/) {
        mtr_verbose("Waited $total_time milliseconds for RAPID server to " .
                    "be started.");
        unlink($outfile);
        return;
      }

      # Sleep for 100 milliseconds
      mtr_milli_sleep(100);

      $total_time = $total_time + 100;
    } else {
      unlink($outfile);
      mtr_error("Can't get RAPID cluster status, query '$query' failed.");
    }
  }

  unlink($outfile);

  # Failed to start rapid server.
  # TODO: Throw an error instead of skipping the test.
  $tinfo->{'skip'} = 1;
  $tinfo->{'comment'} =
    "Timeout after MTR waited for RAPID cluster to get bootstrapped.";
}

# Install rapid plugin on all servers. Skipping the test if INSTALL
# PLUGIN statement fails.
sub install_rapid_plugin($$$) {
  my $mysqlds   = shift;
  my $run_query = shift;
  my $tinfo     = shift;

  foreach my $mysqld ($mysqlds->()) {
    if ($mysqld->{install_rapid_plugin}) {
      my $errfile = "$::opt_vardir/tmp/rapid_plugin_install.err";
      my $query   = "INSTALL PLUGIN RAPID SONAME 'ha_rpd.so'";

      # Run the query to install rapid plugin
      if ($run_query->($mysqld, $query, undef, $errfile)) {
        # Install plugin failed
        $tinfo->{'skip'} = 1;
        my $filehandle = IO::File->new($errfile);
        my $error_msg  = <$filehandle>;
        chomp($error_msg);
        $error_msg =~ s/ at line \d+//g;
        $tinfo->{'comment'} = "Can't install RAPID plugin, $error_msg";
        $filehandle->close();
      } else {
        # Wait for RAPID cluster to get bootstrapped.
        sleep_until_rapid_cluster_bootstrapped($run_query, $mysqld, $tinfo);
      }

      # Delete the query error output file if exists
      unlink($errfile) if -e $errfile;
    }
  }
}

# Get log from rapid server error log file and return the content
# as a single string.
sub get_rapid_server_log() {
  my $rapid         = $::config->group('rapid');
  my $rapid_err_log = "$rapid->{'logdir'}/rapid.err";

  my $rapid_fh = IO::File->new($rapid_err_log) or
    mtr_error("Could not open file '$rapid_err_log' for reading: $!");

  my @lines;

  while (<$rapid_fh>) {
    push(@lines, $_);
    if (scalar(@lines) > 1000000) {
      $rapid_fh = undef;
      mtr_warning(
                "Too much log from rapid server, bailing out from extracting.");
      return;
    }
  }

  # Close error log file
  $rapid_fh->close();

  my $rapid_server_log =
    "\nRapid server log from this test\n" .
    "---------- RAPID SERVER LOG START -----------\n" .
    join("", @lines) . "---------- RAPID SERVER LOG END -------------\n";

  return $rapid_server_log;
}

1;
