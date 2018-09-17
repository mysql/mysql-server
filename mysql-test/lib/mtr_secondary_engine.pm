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
  add_secondary_engine_client_options
  check_number_of_servers
  check_plugin_dir
  check_secondary_engine_options
  get_secondary_engine_server_log
  install_secondary_engine_plugin
  save_secondary_engine_logdir
  secondary_engine_environment_setup
  start_secondary_engine_server
  stop_secondary_engine_server
  $secondary_engine_plugin_dir
  $secondary_engine_port
);

use File::Basename;
use IO::File;

use My::File::Path;
use My::Find;

use mtr_match;
use mtr_report;

do "mtr_misc.pl";

our $secondary_engine_plugin_dir;
our $secondary_engine_port;

## Find path to secondary engine server executable.
##
## Arguments:
##   $bindir Location of bin directory
##
## Returns:
##   Location of secondary engine server executable
sub find_secondary_engine($) {
  my $bindir = shift;

  my $secondary_engine_bindir = my_find_dir($bindir, ["internal/rapid/"],
                                           [ "olrapid-debug", "olrapid-perf" ]);

  return my_find_bin($secondary_engine_bindir, ["bin"], ["rpdserver"]);
}

## Find the directory location containing the secondary engine plugin.
##
## Arguments:
##   $find_plugin Reference to find_plugin() subroutine
##   $bindir      Location of bin directory
##
## Returns:
##   Location of secondary engine plugin directory
sub secondary_engine_plugin_dir($$) {
  my $find_plugin = shift;
  my $bindir      = shift;

  my $secondary_engine_plugin =
    $find_plugin->('ha_rpd', 'plugin_output_directory');

  if ($secondary_engine_plugin) {
    return dirname($secondary_engine_plugin);
  }

  # Couldn't find secondary_engine plugin
  mtr_error("Can't find secondary_engine plugin 'ha_rpd' in " .
            "'$bindir/plugin_output_directory' location");
}

## Setup needed for secondary engine server
##
## Arguments:
##   $find_plugin Reference to find_plugin() subroutine
##   $bindir      Location of bin directory
sub secondary_engine_environment_setup($$) {
  my $find_plugin = shift;
  my $bindir      = shift;

  # Search for secondary engine server executable location.
  $ENV{'SECONDARY_ENGINE'} = find_secondary_engine($bindir);

  # Set 'RPDMASTER_FILEPREFIX' environment variable
  $ENV{'RPDMASTER_FILEPREFIX'} = "$::opt_vardir/log/secondary_engine/";

  my $secondary_engine_ld_library_path =
    my_find_dir($bindir, ["internal/rapid/"],
                [ "rpdmaster-debug", "rpdmaster-perf" ]);

  $ENV{'LD_LIBRARY_PATH'} = join(":",
            $secondary_engine_ld_library_path,
            $ENV{'LD_LIBRARY_PATH'} ? split(':', $ENV{'LD_LIBRARY_PATH'}) : ());

  $secondary_engine_plugin_dir =
    secondary_engine_plugin_dir($find_plugin, $bindir);
}

## Start the secondary engine server.
sub start_secondary_engine_server() {
  my $secondary_engine = $::config->group('rapid');

  # Create secondary engine specific logdir under '$opt_vardir/log'.
  $secondary_engine->{'logdir'} = "$::opt_vardir/log/secondary_engine/";
  mkpath($secondary_engine->{'logdir'});

  my $args;
  mtr_init_args(\$args);

  mtr_add_arg($args, "--heapSize=4096");
  mtr_add_arg($args, "--nwHeapSize=3584");
  mtr_add_arg($args, "--ifname=" . My::SysInfo->network_interface());
  mtr_add_arg($args, "-c 1");
  mtr_add_arg($args, "-d 0");
  mtr_add_arg($args, "-n 0");
  mtr_add_arg($args, "-p $secondary_engine_port");
  mtr_add_arg($args, "-i BOO");
  mtr_add_arg($args, "--network=FOO");
  mtr_add_arg($args, "-l 0");
  mtr_add_arg($args, "-r 1");
  mtr_add_arg($args, "-v");
  mtr_add_arg($args, "-j");
  mtr_add_arg($args, $secondary_engine->{'logdir'});

  mtr_verbose(My::Options::toStr("secondary_engine_start", @$args));

  my $errfile = "$secondary_engine->{'logdir'}/secondary_engine.err";

  $secondary_engine->{'proc'} =
    My::SafeProcess->new(append  => 1,
                         args    => \$args,
                         error   => $errfile,
                         name    => "secondary_engine",
                         output  => $errfile,
                         path    => $ENV{'SECONDARY_ENGINE'},
                         verbose => $::opt_verbose);

  mtr_verbose("Started $secondary_engine->{'proc'}");
}

## Stop the secondary engine server
sub stop_secondary_engine_server () {
  return if not defined $::config;
  my $secondary_engine = $::config->group('rapid');
  mtr_verbose("Stopping secondary engine server $secondary_engine->{'proc'}");
  My::SafeProcess::shutdown($::opt_shutdown_timeout,
                            $secondary_engine->{'proc'});
}

## Add secondary engine client options
##
## Arguments:
##   $args List containing the arguments to be passed
sub add_secondary_engine_client_options($) {
  my $args = shift;
  mtr_add_arg($args, "--secondary-engine=rapid");
  mtr_add_arg($args, "--change-propagation=%d", $::opt_change_propagation);
}

## Skip tests starting more than one server.
##
## Arguments:
##   $mysqlds Reference to mysqlds() subroutine
##   $tinfo   Test object
sub check_number_of_servers($$) {
  my $mysqlds = shift;
  my $tinfo   = shift;

  if (scalar($mysqlds->()) > 1) {
    $tinfo->{'skip'}    = 1;
    $tinfo->{'comment'} = "Can't run tests starting more than one server.";
  }
}

## Check if the 'plugin-dir' is set to a path other than secondary
## engine plugin directory location.
##
## Arguments:
##   $tinfo   Test object
sub check_plugin_dir($) {
  my $tinfo = shift;

  my $plugin_dir;
  foreach my $opt (@{ $tinfo->{master_opt} }) {
    $plugin_dir = mtr_match_prefix($opt, "--plugin-dir=") ||
      mtr_match_prefix($opt, "--plugin_dir=");
  }

  if (defined $plugin_dir) {
    if ($plugin_dir ne $secondary_engine_plugin_dir) {
      # Different plugin dir, skip the test.
      $tinfo->{'skip'} = 1;
      $tinfo->{'comment'} =
        "Test requires plugin-dir to set to plugin_output_directory.";
    }
  }
}

## Check secondary engine related options
sub check_secondary_engine_options() {
  if (defined $::opt_change_propagation) {
    if (not defined $::opt_secondary_engine) {
      mtr_error("Can't use '--change-propagation' option without enabling " .
                "'--secondary-engine' option.");
    } elsif ($::opt_change_propagation < 0 or $::opt_change_propagation > 1) {
      # 'change-propagation' option value should be either 0 or 1.
      mtr_error("Invalid value '$::opt_change_propagation' for option " .
                "'--change-propagation'.");
    }
  }
}

## Save secondary engine log directory contents
##
## Arguments:
##   $savedir Save directory location
sub save_secondary_engine_logdir($) {
  my $savedir          = shift;
  my $secondary_engine = $::config->group('rapid');
  my $log_dirname      = basename($secondary_engine->{'logdir'});
  rename($secondary_engine->{'logdir'}, "$savedir/$log_dirname");
}

## Wait for secondary engine server to start.
##
## Arguments:
##   $run_query Reference to run_query() subroutine
##   $mysqld    mysqld object
##   $tinfo     Test object
sub sleep_until_secondary_engine_cluster_bootstrapped($$$) {
  my $run_query = shift;
  my $mysqld    = shift;
  my $tinfo     = shift;

  my $sleeptime  = 100;                                        # In milliseconds
  my $total_time = 0;                                          # In milliseconds
  my $loops      = ($::opt_start_timeout * 1000) / $sleeptime;

  my $outfile = "$::opt_vardir/tmp/secondary_engine_cluster_status.out";
  my $query   = "SHOW STATUS LIKE 'rapid_cluster_status'";

  for (my $loop = 1 ; $loop <= $loops ; $loop++) {
    if (!$run_query->($mysqld, $query, $outfile, undef)) {
      # Query succeeded, fetch the status value.
      my $filehandle              = IO::File->new($outfile);
      my $secondary_engine_status = <$filehandle>;

      # No need of file handle now, close it.
      $filehandle->close();

      # Check the secondary engine cluster status
      if ($secondary_engine_status =~ /^rapid_cluster_status\s+ON/) {
        mtr_verbose("Waited $total_time milliseconds for secondary engine " .
                    "server to be started.");
        unlink($outfile);
        return;
      }

      # Sleep for 100 milliseconds
      mtr_milli_sleep(100);

      $total_time = $total_time + 100;
    } else {
      unlink($outfile);
      mtr_error(
           "Can't get secondary engine cluster status, query '$query' failed.");
    }
  }

  unlink($outfile);

  # Failed to start secondary engine server.
  # TODO: Throw an error instead of skipping the test.
  $tinfo->{'skip'} = 1;
  $tinfo->{'comment'} =
    "Timeout after MTR waited for secondary engine to get bootstrapped.";
}

## Install secondary engine plugin on all servers. Skipping the test
## if INSTALL PLUGIN statement fails.
##
## Arguments:
##   $mysqlds   Reference to mysqlds() subroutine
##   $run_query Reference to run_query() subroutine
##   $tinfo     Test object
sub install_secondary_engine_plugin($$$) {
  my $mysqlds   = shift;
  my $run_query = shift;
  my $tinfo     = shift;

  foreach my $mysqld ($mysqlds->()) {
    if ($mysqld->{install_secondary_engine_plugin}) {
      my $errfile = "$::opt_vardir/tmp/plugin_install.err";
      my $query   = "INSTALL PLUGIN RAPID SONAME 'ha_rpd.so'";

      # Run the query to install secondary engine plugin
      if ($run_query->($mysqld, $query, undef, $errfile)) {
        # Install plugin failed
        $tinfo->{'skip'} = 1;
        my $filehandle = IO::File->new($errfile);
        my $error_msg  = <$filehandle>;
        chomp($error_msg);
        $error_msg =~ s/ at line \d+//g;
        $tinfo->{'comment'} =
          "Can't install secondary engine plugin, $error_msg";
        $filehandle->close();
      } else {
        # Wait for secondary engine cluster to get bootstrapped.
        sleep_until_secondary_engine_cluster_bootstrapped($run_query, $mysqld,
                                                          $tinfo);
      }

      # Delete the query error output file if exists
      unlink($errfile) if -e $errfile;
    }
  }
}

## Get log from secondary engine server error log file and return the
## contents as a single string.
##
## Returns:
##   String value containing log output
sub get_secondary_engine_server_log() {
  my $secondary_engine = $::config->group('rapid');
  my $secondary_engine_err_log =
    "$secondary_engine->{'logdir'}/secondary_engine.err";

  my $fh = IO::File->new($secondary_engine_err_log) or
    mtr_error("Could not open file '$secondary_engine_err_log': $!");

  my @lines;
  while (<$fh>) {
    push(@lines, $_);
    if (scalar(@lines) > 1000000) {
      $fh->close();
      mtr_warning("Too much log from secondary engine server.");
      return;
    }
  }

  # Close error log file
  $fh->close();

  my $secondary_engine_server_log =
    "\nSecondary engine server log from this test\n" .
    "---------- SECONDARY ENGINE SERVER LOG START -----------\n" .
    join("", @lines) .
    "---------- SECONDARY ENGINE SERVER LOG END -------------\n";

  return $secondary_engine_server_log;
}

1;
