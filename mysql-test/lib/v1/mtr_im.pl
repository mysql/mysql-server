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

# This is a library file used by the Perl version of mysql-test-run,
# and is part of the translation of the Bourne shell script with the
# same name.

use strict;

# Private IM-related operations.

sub mtr_im_kill_process ($$$$);
sub mtr_im_load_pids ($);
sub mtr_im_terminate ($);
sub mtr_im_check_alive ($);
sub mtr_im_check_main_alive ($);
sub mtr_im_check_angel_alive ($);
sub mtr_im_check_mysqlds_alive ($);
sub mtr_im_check_mysqld_alive ($);
sub mtr_im_cleanup ($);
sub mtr_im_rm_file ($);
sub mtr_im_errlog ($);
sub mtr_im_kill ($);
sub mtr_im_wait_for_connection ($$$);
sub mtr_im_wait_for_mysqld($$$);

# Public IM-related operations.

sub mtr_im_start ($$);
sub mtr_im_stop ($);

##############################################################################
#
#  Private operations.
#
##############################################################################

sub mtr_im_kill_process ($$$$) {
  my $pid_lst= shift;
  my $signal= shift;
  my $total_retries= shift;
  my $timeout= shift;

  my %pids;

  foreach my $pid ( @{$pid_lst} )
  {
    $pids{$pid}= 1;
  }

  for ( my $cur_attempt= 1; $cur_attempt <= $total_retries; ++$cur_attempt )
  {
    foreach my $pid ( keys %pids )
    {
      mtr_debug("Sending $signal to $pid...");

      kill($signal, $pid);

      unless ( kill (0, $pid) )
      {
        mtr_debug("Process $pid died.");
        delete $pids{$pid};
      }
    }

    return if scalar keys %pids == 0;

    mtr_debug("Sleeping $timeout second(s) waiting for processes to die...");

    sleep($timeout);
  }

  mtr_debug("Process(es) " .
            join(' ', keys %pids) .
            " is still alive after $total_retries " .
            "of sending signal $signal.");
}

###########################################################################

sub mtr_im_load_pids($) {
  my $im= shift;

  mtr_debug("Loading PID files...");

  # Obtain mysqld-process pids.

  my $instances = $im->{'instances'};

  for ( my $idx= 0; $idx < 2; ++$idx )
  {
    mtr_debug("IM-guarded mysqld[$idx] PID file: '" .
              $instances->[$idx]->{'path_pid'} . "'.");

    my $mysqld_pid;

    if ( -r $instances->[$idx]->{'path_pid'} )
    {
      $mysqld_pid= mtr_get_pid_from_file($instances->[$idx]->{'path_pid'});
      mtr_debug("IM-guarded mysqld[$idx] PID: $mysqld_pid.");
    }
    else
    {
      $mysqld_pid= undef;
      mtr_debug("IM-guarded mysqld[$idx]: no PID file.");
    }

    $instances->[$idx]->{'pid'}= $mysqld_pid;
  }

  # Re-read Instance Manager PIDs from the file, since during tests Instance
  # Manager could have been restarted, so its PIDs could have been changed.

  #   - IM-main

  mtr_debug("IM-main PID file: '$im->{path_pid}'.");

  if ( -f $im->{'path_pid'} )
  {
    $im->{'pid'} =
      mtr_get_pid_from_file($im->{'path_pid'});

    mtr_debug("IM-main PID: $im->{pid}.");
  }
  else
  {
    mtr_debug("IM-main: no PID file.");
    $im->{'pid'}= undef;
  }

  #   - IM-angel

  mtr_debug("IM-angel PID file: '$im->{path_angel_pid}'.");

  if ( -f $im->{'path_angel_pid'} )
  {
    $im->{'angel_pid'} =
      mtr_get_pid_from_file($im->{'path_angel_pid'});

    mtr_debug("IM-angel PID: $im->{'angel_pid'}.");
  }
  else
  {
    mtr_debug("IM-angel: no PID file.");
    $im->{'angel_pid'} = undef;
  }
}

###########################################################################

sub mtr_im_terminate($) {
  my $im= shift;

  # Load pids from pid-files. We should do it first of all, because IM deletes
  # them on shutdown.

  mtr_im_load_pids($im);

  mtr_debug("Shutting Instance Manager down...");

  # Ignoring SIGCHLD so that all children could rest in peace.

  start_reap_all();

  # Send SIGTERM to IM-main.

  if ( defined $im->{'pid'} )
  {
    mtr_debug("IM-main pid: $im->{pid}.");
    mtr_debug("Stopping IM-main...");

    mtr_im_kill_process([ $im->{'pid'} ], 'TERM', 10, 1);
  }
  else
  {
    mtr_debug("IM-main pid: n/a.");
  }

  # If IM-angel was alive, wait for it to die.

  if ( defined $im->{'angel_pid'} )
  {
    mtr_debug("IM-angel pid: $im->{'angel_pid'}.");
    mtr_debug("Waiting for IM-angel to die...");

    my $total_attempts= 10;

    for ( my $cur_attempt=1; $cur_attempt <= $total_attempts; ++$cur_attempt )
    {
      unless ( kill (0, $im->{'angel_pid'}) )
      {
        mtr_debug("IM-angel died.");
        last;
      }

      sleep(1);
    }
  }
  else
  {
    mtr_debug("IM-angel pid: n/a.");
  }

  stop_reap_all();

  # Re-load PIDs.

  mtr_im_load_pids($im);
}

###########################################################################

sub mtr_im_check_alive($) {
  my $im= shift;

  mtr_debug("Checking whether IM-components are alive...");

  return 1 if mtr_im_check_main_alive($im);

  return 1 if mtr_im_check_angel_alive($im);

  return 1 if mtr_im_check_mysqlds_alive($im);

  return 0;
}

###########################################################################

sub mtr_im_check_main_alive($) {
  my $im= shift;

  # Check that the process, that we know to be IM's, is dead.

  if ( defined $im->{'pid'} )
  {
    if ( kill (0, $im->{'pid'}) )
    {
      mtr_debug("IM-main (PID: $im->{pid}) is alive.");
      return 1;
    }
    else
    {
      mtr_debug("IM-main (PID: $im->{pid}) is dead.");
    }
  }
  else
  {
    mtr_debug("No PID file for IM-main.");
  }

  # Check that IM does not accept client connections.

  if ( mtr_ping_port($im->{'port'}) )
  {
    mtr_debug("IM-main (port: $im->{port}) " .
              "is accepting connections.");

    mtr_im_errlog("IM-main is accepting connections on port " .
                  "$im->{port}, but there is no " .
                  "process information.");
    return 1;
  }
  else
  {
    mtr_debug("IM-main (port: $im->{port}) " .
              "does not accept connections.");
    return 0;
  }
}

###########################################################################

sub mtr_im_check_angel_alive($) {
  my $im= shift;

  # Check that the process, that we know to be the Angel, is dead.

  if ( defined $im->{'angel_pid'} )
  {
    if ( kill (0, $im->{'angel_pid'}) )
    {
      mtr_debug("IM-angel (PID: $im->{angel_pid}) is alive.");
      return 1;
    }
    else
    {
      mtr_debug("IM-angel (PID: $im->{angel_pid}) is dead.");
      return 0;
    }
  }
  else
  {
    mtr_debug("No PID file for IM-angel.");
    return 0;
  }
}

###########################################################################

sub mtr_im_check_mysqlds_alive($) {
  my $im= shift;

  mtr_debug("Checking for IM-guarded mysqld instances...");

  my $instances = $im->{'instances'};

  for ( my $idx= 0; $idx < 2; ++$idx )
  {
    mtr_debug("Checking mysqld[$idx]...");

    return 1
      if mtr_im_check_mysqld_alive($instances->[$idx]);
  }
}

###########################################################################

sub mtr_im_check_mysqld_alive($) {
  my $mysqld_instance= shift;

  # Check that the process is dead.

  if ( defined $mysqld_instance->{'pid'} )
  {
    if ( kill (0, $mysqld_instance->{'pid'}) )
    {
      mtr_debug("Mysqld instance (PID: $mysqld_instance->{pid}) is alive.");
      return 1;
    }
    else
    {
      mtr_debug("Mysqld instance (PID: $mysqld_instance->{pid}) is dead.");
    }
  }
  else
  {
    mtr_debug("No PID file for mysqld instance.");
  }

  # Check that mysqld does not accept client connections.

  if ( mtr_ping_port($mysqld_instance->{'port'}) )
  {
    mtr_debug("Mysqld instance (port: $mysqld_instance->{port}) " .
              "is accepting connections.");

    mtr_im_errlog("Mysqld is accepting connections on port " .
                  "$mysqld_instance->{port}, but there is no " .
                  "process information.");
    return 1;
  }
  else
  {
    mtr_debug("Mysqld instance (port: $mysqld_instance->{port}) " .
              "does not accept connections.");
    return 0;
  }
}

###########################################################################

sub mtr_im_cleanup($) {
  my $im= shift;

  mtr_im_rm_file($im->{'path_pid'});
  mtr_im_rm_file($im->{'path_sock'});

  mtr_im_rm_file($im->{'path_angel_pid'});

  for ( my $idx= 0; $idx < 2; ++$idx )
  {
    mtr_im_rm_file($im->{'instances'}->[$idx]->{'path_pid'});
    mtr_im_rm_file($im->{'instances'}->[$idx]->{'path_sock'});
  }
}

###########################################################################

sub mtr_im_rm_file($)
{
  my $file_path= shift;

  if ( -f $file_path )
  {
    mtr_debug("Removing '$file_path'...");

    unless ( unlink($file_path) )
    {
      mtr_warning("Can not remove '$file_path'.")
    }
  }
  else
  {
    mtr_debug("File '$file_path' does not exist already.");
  }
}

###########################################################################

sub mtr_im_errlog($) {
  my $msg= shift;

  # Complain in error log so that a warning will be shown.
  # 
  # TODO: unless BUG#20761 is fixed, we will print the warning to stdout, so
  # that it can be seen on console and does not produce pushbuild error.

  # my $errlog= "$opt_vardir/log/mysql-test-run.pl.err";
  # 
  # open (ERRLOG, ">>$errlog") ||
  #   mtr_error("Can not open error log ($errlog)");
  # 
  # my $ts= localtime();
  # print ERRLOG
  #   "Warning: [$ts] $msg\n";
  # 
  # close ERRLOG;

  my $ts= localtime();
  print "Warning: [$ts] $msg\n";
}

###########################################################################

sub mtr_im_kill($) {
  my $im= shift;

  # Re-load PIDs. That can be useful because some processes could have been
  # restarted.

  mtr_im_load_pids($im);

  # Ignoring SIGCHLD so that all children could rest in peace.

  start_reap_all();

  # Kill IM-angel first of all.

  if ( defined $im->{'angel_pid'} )
  {
    mtr_debug("Killing IM-angel (PID: $im->{angel_pid})...");
    mtr_im_kill_process([ $im->{'angel_pid'} ], 'KILL', 10, 1)
  }
  else
  {
    mtr_debug("IM-angel is dead.");
  }

  # Re-load PIDs again.

  mtr_im_load_pids($im);

  # Kill IM-main.
  
  if ( defined $im->{'pid'} )
  {
    mtr_debug("Killing IM-main (PID: $im->pid})...");
    mtr_im_kill_process([ $im->{'pid'} ], 'KILL', 10, 1);
  }
  else
  {
    mtr_debug("IM-main is dead.");
  }

  # Re-load PIDs again.

  mtr_im_load_pids($im);

  # Kill guarded mysqld instances.

  my @mysqld_pids;

  mtr_debug("Collecting PIDs of mysqld instances to kill...");

  for ( my $idx= 0; $idx < 2; ++$idx )
  {
    my $pid= $im->{'instances'}->[$idx]->{'pid'};

    unless ( defined $pid )
    {
      next;
    }

    mtr_debug("  - IM-guarded mysqld[$idx] PID: $pid.");

    push (@mysqld_pids, $pid);
  }

  if ( scalar @mysqld_pids > 0 )
  {
    mtr_debug("Killing IM-guarded mysqld instances...");
    mtr_im_kill_process(\@mysqld_pids, 'KILL', 10, 1);
  }

  # That's all.

  stop_reap_all();
}

##############################################################################

sub mtr_im_wait_for_connection($$$) {
  my $im= shift;
  my $total_attempts= shift;
  my $connect_timeout= shift;

  mtr_debug("Waiting for IM on port $im->{port} " .
            "to start accepting connections...");

  for ( my $cur_attempt= 1; $cur_attempt <= $total_attempts; ++$cur_attempt )
  {
    mtr_debug("Trying to connect to IM ($cur_attempt of $total_attempts)...");

    if ( mtr_ping_port($im->{'port'}) )
    {
      mtr_debug("IM is accepting connections " .
                "on port $im->{port}.");
      return 1;
    }

    mtr_debug("Sleeping $connect_timeout...");
    sleep($connect_timeout);
  }

  mtr_debug("IM does not accept connections " .
            "on port $im->{port} after " .
            ($total_attempts * $connect_timeout) . " seconds.");

  return 0;
}

##############################################################################

sub mtr_im_wait_for_mysqld($$$) {
  my $mysqld= shift;
  my $total_attempts= shift;
  my $connect_timeout= shift;

  mtr_debug("Waiting for IM-guarded mysqld on port $mysqld->{port} " .
            "to start accepting connections...");

  for ( my $cur_attempt= 1; $cur_attempt <= $total_attempts; ++$cur_attempt )
  {
    mtr_debug("Trying to connect to mysqld " .
              "($cur_attempt of $total_attempts)...");

    if ( mtr_ping_port($mysqld->{'port'}) )
    {
      mtr_debug("Mysqld is accepting connections " .
                "on port $mysqld->{port}.");
      return 1;
    }

    mtr_debug("Sleeping $connect_timeout...");
    sleep($connect_timeout);
  }

  mtr_debug("Mysqld does not accept connections " .
            "on port $mysqld->{port} after " .
            ($total_attempts * $connect_timeout) . " seconds.");

  return 0;
}

##############################################################################
#
#  Public operations.
#
##############################################################################

sub mtr_im_start($$) {
  my $im = shift;
  my $opts = shift;

  mtr_debug("Starting Instance Manager...");

  my $args;
  mtr_init_args(\$args);
  mtr_add_arg($args, "--defaults-file=%s", $im->{'defaults_file'});

  foreach my $opt ( @{$opts} )
  {
    mtr_add_arg($args, $opt);
  }

  $im->{'spawner_pid'} =
    mtr_spawn(
      $::exe_im,                        # path to the executable
      $args,                            # cmd-line args
      '',                               # stdin
      $im->{'path_log'},                # stdout
      $im->{'path_err'},                # stderr
      '',                               # pid file path (not used)
      { append_log_file => 1 }          # append log files
      );

  unless ( $im->{'spawner_pid'} )
  {
    mtr_error('Could not start Instance Manager.')
  }

  # Instance Manager can be run in daemon mode. In this case, it creates
  # several processes and the parent process, created by mtr_spawn(), exits just
  # after start. So, we have to obtain Instance Manager PID from the PID file.

  mtr_debug("Waiting for IM to create PID file (" .
            "path: '$im->{path_pid}'; " .
            "timeout: $im->{start_timeout})...");

  unless ( sleep_until_file_created($im->{'path_pid'},
                                    $im->{'start_timeout'},
                                    -1) ) # real PID is still unknown
  {
    mtr_debug("IM has not created PID file in $im->{start_timeout} secs.");
    mtr_debug("Aborting test suite...");

    mtr_kill_leftovers();

    mtr_report("IM has not created PID file in $im->{start_timeout} secs.");
    return 0;
  }

  $im->{'pid'}= mtr_get_pid_from_file($im->{'path_pid'});

  mtr_debug("Instance Manager started. PID: $im->{pid}.");

  # Wait until we can connect to IM.

  my $IM_CONNECT_TIMEOUT= 30;

  unless ( mtr_im_wait_for_connection($im,
                                      $IM_CONNECT_TIMEOUT, 1) )
  {
    mtr_debug("Can not connect to Instance Manager " .
              "in $IM_CONNECT_TIMEOUT seconds after start.");
    mtr_debug("Aborting test suite...");

    mtr_kill_leftovers();

    mtr_report("Can not connect to Instance Manager " .
               "in $IM_CONNECT_TIMEOUT seconds after start.");
    return 0;
  }

  # Wait for IM to start guarded instances:
  #   - wait for PID files;

  mtr_debug("Waiting for guarded mysqlds instances to create PID files...");

  for ( my $idx= 0; $idx < 2; ++$idx )
  {
    my $mysqld= $im->{'instances'}->[$idx];

    if ( exists $mysqld->{'nonguarded'} )
    {
      next;
    }

    mtr_debug("Waiting for mysqld[$idx] to create PID file (" .
              "path: '$mysqld->{path_pid}'; " .
              "timeout: $mysqld->{start_timeout})...");

    unless ( sleep_until_file_created($mysqld->{'path_pid'},
                                      $mysqld->{'start_timeout'},
                                      -1) ) # real PID is still unknown
    {
      mtr_debug("mysqld[$idx] has not created PID file in " .
                 "$mysqld->{start_timeout} secs.");
      mtr_debug("Aborting test suite...");

      mtr_kill_leftovers();

      mtr_report("mysqld[$idx] has not created PID file in " .
                 "$mysqld->{start_timeout} secs.");
      return 0;
    }

    mtr_debug("PID file for mysqld[$idx] ($mysqld->{path_pid} created.");
  }

  # Wait until we can connect to guarded mysqld-instances
  # (in other words -- wait for IM to start guarded instances).

  mtr_debug("Waiting for guarded mysqlds to start accepting connections...");

  for ( my $idx= 0; $idx < 2; ++$idx )
  {
    my $mysqld= $im->{'instances'}->[$idx];

    if ( exists $mysqld->{'nonguarded'} )
    {
      next;
    }

    mtr_debug("Waiting for mysqld[$idx] to accept connection...");

    unless ( mtr_im_wait_for_mysqld($mysqld, 30, 1) )
    {
      mtr_debug("Can not connect to mysqld[$idx] " .
                "in $IM_CONNECT_TIMEOUT seconds after start.");
      mtr_debug("Aborting test suite...");

      mtr_kill_leftovers();

      mtr_report("Can not connect to mysqld[$idx] " .
                 "in $IM_CONNECT_TIMEOUT seconds after start.");
      return 0;
    }

    mtr_debug("mysqld[$idx] started.");
  }

  mtr_debug("Instance Manager and its components are up and running.");

  return 1;
}

##############################################################################

sub mtr_im_stop($) {
  my $im= shift;

  mtr_debug("Stopping Instance Manager...");

  # Try graceful shutdown.

  mtr_im_terminate($im);

  # Check that all processes died.

  unless ( mtr_im_check_alive($im) )
  {
    mtr_debug("Instance Manager has been stopped successfully.");
    mtr_im_cleanup($im);
    return 1;
  }

  # Instance Manager don't want to die. We should kill it.

  mtr_im_errlog("Instance Manager did not shutdown gracefully.");

  mtr_im_kill($im);

  # Check again that all IM-related processes have been killed.

  my $im_is_alive= mtr_im_check_alive($im);

  mtr_im_cleanup($im);

  if ( $im_is_alive )
  {
    mtr_debug("Can not kill Instance Manager or its children.");
    return 0;
  }

  mtr_debug("Instance Manager has been killed successfully.");
  return 1;
}

###########################################################################

1;
