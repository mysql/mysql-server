# -*- cperl -*-
# Copyright (C) 2004-2006 MySQL AB
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

use Socket;
use Errno;
use strict;

use POSIX qw(WNOHANG SIGHUP);

sub mtr_run ($$$$$$;$);
sub mtr_spawn ($$$$$$;$);
sub mtr_check_stop_servers ($);
sub mtr_kill_leftovers ();
sub mtr_wait_blocking ($);
sub mtr_record_dead_children ();
sub mtr_ndbmgm_start($$);
sub mtr_mysqladmin_start($$$);
sub mtr_exit ($);
sub sleep_until_file_created ($$$);
sub mtr_kill_processes ($);
sub mtr_ping_with_timeout($);
sub mtr_ping_port ($);

# Local function
sub spawn_impl ($$$$$$$);

##############################################################################
#
#  Execute an external command
#
##############################################################################

sub mtr_run ($$$$$$;$) {
  my $path=       shift;
  my $arg_list_t= shift;
  my $input=      shift;
  my $output=     shift;
  my $error=      shift;
  my $pid_file=   shift; # Not used
  my $spawn_opts= shift;

  return spawn_impl($path,$arg_list_t,'run',$input,$output,$error,
    $spawn_opts);
}

sub mtr_run_test ($$$$$$;$) {
  my $path=       shift;
  my $arg_list_t= shift;
  my $input=      shift;
  my $output=     shift;
  my $error=      shift;
  my $pid_file=   shift; # Not used
  my $spawn_opts= shift;

  return spawn_impl($path,$arg_list_t,'test',$input,$output,$error,
    $spawn_opts);
}

sub mtr_spawn ($$$$$$;$) {
  my $path=       shift;
  my $arg_list_t= shift;
  my $input=      shift;
  my $output=     shift;
  my $error=      shift;
  my $pid_file=   shift; # Not used
  my $spawn_opts= shift;

  return spawn_impl($path,$arg_list_t,'spawn',$input,$output,$error,
    $spawn_opts);
}



sub spawn_impl ($$$$$$$) {
  my $path=       shift;
  my $arg_list_t= shift;
  my $mode=       shift;
  my $input=      shift;
  my $output=     shift;
  my $error=      shift;
  my $spawn_opts= shift;

  if ( $::opt_script_debug )
  {
    mtr_report("");
    mtr_debug("-" x 73);
    mtr_debug("STDIN  $input") if $input;
    mtr_debug("STDOUT $output") if $output;
    mtr_debug("STDERR $error") if $error;
    mtr_debug("$mode: $path ", join(" ",@$arg_list_t));
    mtr_debug("spawn options:");
    if ($spawn_opts)
    {
      foreach my $key (sort keys %{$spawn_opts})
      {
        mtr_debug("  - $key: $spawn_opts->{$key}");
      }
    }
    else
    {
      mtr_debug("  none");
    }
    mtr_debug("-" x 73);
    mtr_report("");
  }

  mtr_error("Can't spawn with empty \"path\"") unless defined $path;


 FORK:
  {
    my $pid= fork();

    if ( ! defined $pid )
    {
      if ( $! == $!{EAGAIN} )           # See "perldoc Errno"
      {
        mtr_warning("Got EAGAIN from fork(), sleep 1 second and redo");
        sleep(1);
        redo FORK;
      }

      mtr_error("$path ($pid) can't be forked, error: $!");

    }

    if ( $pid )
    {
      select(STDOUT) if $::glob_win32_perl;
      return spawn_parent_impl($pid,$mode,$path);
    }
    else
    {
      # Child, redirect output and exec

      $SIG{INT}= 'DEFAULT';         # Parent do some stuff, we don't

      my $log_file_open_mode = '>';

      if ($spawn_opts and $spawn_opts->{'append_log_file'})
      {
        $log_file_open_mode = '>>';
      }

      if ( $output )
      {
	if ( $::glob_win32_perl )
	{
	  # Don't redirect stdout on ActiveState perl since this is
          # just another thread in the same process.
	}
        elsif ( ! open(STDOUT,$log_file_open_mode,$output) )
        {
          mtr_child_error("can't redirect STDOUT to \"$output\": $!");
        }
      }

      if ( $error )
      {
        if ( !$::glob_win32_perl and $output eq $error )
        {
          if ( ! open(STDERR,">&STDOUT") )
          {
            mtr_child_error("can't dup STDOUT: $!");
          }
        }
        else
        {
          if ( ! open(STDERR,$log_file_open_mode,$error) )
          {
            mtr_child_error("can't redirect STDERR to \"$error\": $!");
          }
        }
      }

      if ( $input )
      {
        if ( ! open(STDIN,"<",$input) )
        {
          mtr_child_error("can't redirect STDIN to \"$input\": $!");
        }
      }

      if ( ! exec($path,@$arg_list_t) )
      {
        mtr_child_error("failed to execute \"$path\": $!");
      }
      mtr_error("Should never come here 1!");
    }
    mtr_error("Should never come here 2!");
  }
  mtr_error("Should never come here 3!");
}


sub spawn_parent_impl {
  my $pid=  shift;
  my $mode= shift;
  my $path= shift;

  if ( $mode eq 'run' or $mode eq 'test' )
  {
    if ( $mode eq 'run' )
    {
      # Simple run of command, wait blocking for it to return
      my $ret_pid= waitpid($pid,0);
      if ( $ret_pid != $pid )
      {
	# The "simple" waitpid has failed, print debug info
	# and try to handle the error
        mtr_warning("waitpid($pid, 0) returned $ret_pid " .
		    "when waiting for '$path', error: '$!'");
	if ( $ret_pid == -1 )
	{
	  # waitpid returned -1, that would indicate the process
	  # no longer exist and waitpid couldn't wait for it.
	  return 1;
	}
	mtr_error("Error handling failed");
      }

      return mtr_process_exit_status($?);
    }
    else
    {
      # We run mysqltest and wait for it to return. But we try to
      # catch dying mysqld processes as well.
      #
      # We do blocking waitpid() until we get the return from the
      # "mysqltest" call. But if a mysqld process dies that we
      # started, we take this as an error, and kill mysqltest.


      my $exit_value= -1;
      my $saved_exit_value;
      my $ret_pid;                      # What waitpid() returns

      while ( ($ret_pid= waitpid(-1,0)) != -1 )
      {
        # Someone terminated, don't know who. Collect
        # status info first before $? is lost,
        # but not $exit_value, this is flagged from

        my $timer_name= mtr_timer_timeout($::glob_timers, $ret_pid);
        if ( $timer_name )
        {
          if ( $timer_name eq "suite" )
          {
            # We give up here
            print STDERR "\n";
            kill(9, $pid); # Kill mysqltest
            mtr_kill_leftovers(); # Kill servers the hard way
            mtr_error("Test suite timeout");
          }
          elsif ( $timer_name eq "testcase" )
          {
            $saved_exit_value=  63;       # Mark as timeout
            kill(9, $pid);                # Kill mysqltest
            next;                         # Go on and catch the termination
          }
        }

        if ( $ret_pid == $pid )
        {
          # We got termination of mysqltest, we are done
          $exit_value= mtr_process_exit_status($?);
          last;
        }

        # One of the child processes died, unless this was expected
	# mysqltest should be killed and test aborted

	check_expected_crash_and_restart($ret_pid);
      }

      if ( $ret_pid != $pid )
      {
        # We terminated the waiting because a "mysqld" process died.
        # Kill the mysqltest process.
	mtr_verbose("Kill mysqltest because another process died");
        kill(9,$pid);

        $ret_pid= waitpid($pid,0);

        if ( $ret_pid != $pid )
        {
          mtr_error("$path ($pid) got lost somehow");
        }
      }

      return $saved_exit_value || $exit_value;
    }
  }
  else
  {
    # We spawned a process we don't wait for
    return $pid;
  }
}


# ----------------------------------------------------------------------
# We try to emulate how an Unix shell calculates the exit code
# ----------------------------------------------------------------------

sub mtr_process_exit_status {
  my $raw_status= shift;

  if ( $raw_status & 127 )
  {
    return ($raw_status & 127) + 128;  # Signal num + 128
  }
  else
  {
    return $raw_status >> 8;           # Exit code
  }
}


##############################################################################
#
#  Kill processes left from previous runs
#
##############################################################################


# Kill all processes(mysqld, ndbd, ndb_mgmd and im) that would conflict with
# this run
# Make sure to remove the PID file, if any.
# kill IM manager first, else it will restart the servers
sub mtr_kill_leftovers () {

  mtr_report("Killing Possible Leftover Processes");
  mtr_debug("mtr_kill_leftovers(): started.");

  my @kill_pids;
  my %admin_pids;

  foreach my $srv (@{$::master}, @{$::slave})
  {
    mtr_debug("  - mysqld " .
              "(pid: $srv->{pid}; " .
              "pid file: '$srv->{path_pid}'; " .
              "socket: '$srv->{path_sock}'; ".
              "port: $srv->{port})");

    my $pid= mtr_mysqladmin_start($srv, "shutdown", 20);

    # Save the pid of the mysqladmin process
    $admin_pids{$pid}= 1;

    push(@kill_pids,{
		     pid      => $srv->{'pid'},
		     pidfile  => $srv->{'path_pid'},
		     sockfile => $srv->{'path_sock'},
		     port     => $srv->{'port'},
		    });
    $srv->{'pid'}= 0; # Assume we are done with it
  }

  if ( ! $::opt_skip_ndbcluster )
  {

    foreach my $cluster (@{$::clusters})
    {

      # Don't shut down a "running" cluster
      next if $cluster->{'use_running'};

      mtr_debug("  - cluster " .
		"(pid: $cluster->{pid}; " .
		"pid file: '$cluster->{path_pid})");

      my $pid= mtr_ndbmgm_start($cluster, "shutdown");

      # Save the pid of the ndb_mgm process
      $admin_pids{$pid}= 1;

      push(@kill_pids,{
		       pid      => $cluster->{'pid'},
		       pidfile  => $cluster->{'path_pid'}
		      });

      $cluster->{'pid'}= 0; # Assume we are done with it

      foreach my $ndbd (@{$cluster->{'ndbds'}})
      {
	mtr_debug("    - ndbd " .
		  "(pid: $ndbd->{pid}; " .
		  "pid file: '$ndbd->{path_pid})");

	push(@kill_pids,{
			 pid      => $ndbd->{'pid'},
			 pidfile  => $ndbd->{'path_pid'},
			});
	$ndbd->{'pid'}= 0; # Assume we are done with it
      }
    }
  }

  # Wait for all the admin processes to complete
  mtr_wait_blocking(\%admin_pids);

  # If we trusted "mysqladmin --shutdown_timeout= ..." we could just
  # terminate now, but we don't (FIXME should be debugged).
  # So we try again to ping and at least wait the same amount of time
  # mysqladmin would for all to die.

  mtr_ping_with_timeout(\@kill_pids);

  # We now have tried to terminate nice. We have waited for the listen
  # port to be free, but can't really tell if the mysqld process died
  # or not. We now try to find the process PID from the PID file, and
  # send a kill to that process. Note that Perl let kill(0,@pids) be
  # a way to just return the numer of processes the kernel can send
  # signals to. So this can be used (except on Cygwin) to determine
  # if there are processes left running that we cound out might exists.
  #
  # But still after all this work, all we know is that we have
  # the ports free.

  # We scan the "var/run/" directory for other process id's to kill

  my $rundir= "$::opt_vardir/run";

  mtr_debug("Processing PID files in directory '$rundir'...");

  if ( -d $rundir )
  {
    opendir(RUNDIR, $rundir)
      or mtr_error("can't open directory \"$rundir\": $!");

    my @pids;

    while ( my $elem= readdir(RUNDIR) )
    {
      # Only read pid from files that end with .pid
      if ( $elem =~ /.*[.]pid$/)
      {
	my $pidfile= "$rundir/$elem";

	if ( -f $pidfile )
	{
	  mtr_debug("Processing PID file: '$pidfile'...");

	  my $pid= mtr_get_pid_from_file($pidfile);

	  mtr_debug("Got pid: $pid from file '$pidfile'");

	  if ( $::glob_cygwin_perl or kill(0, $pid) )
	  {
	    mtr_debug("There is process with pid $pid -- scheduling for kill.");
	    push(@pids, $pid);            # We know (cygwin guess) it exists
	  }
	  else
	  {
	    mtr_debug("There is no process with pid $pid -- skipping.");
	  }
	}
      }
      else
      {
	mtr_warning("Found non pid file $elem in $rundir")
	  if -f "$rundir/$elem";
	next;
      }
    }
    closedir(RUNDIR);

    if ( @pids )
    {
      mtr_debug("Killing the following processes with PID files: " .
                join(' ', @pids) . "...");

      start_reap_all();

      if ( $::glob_cygwin_perl )
      {
        # We have no (easy) way of knowing the Cygwin controlling
        # process, in the PID file we only have the Windows process id.
        system("kill -f " . join(" ",@pids)); # Hope for the best....
        mtr_debug("Sleep 5 seconds waiting for processes to die");
        sleep(5);
      }
      else
      {
        my $retries= 10;                    # 10 seconds
        do
        {
          mtr_debug("Sending SIGKILL to pids: " . join(' ', @pids));
          kill(9, @pids);
          mtr_report("Sleep 1 second waiting for processes to die");
          sleep(1)                      # Wait one second
        } while ( $retries-- and  kill(0, @pids) );

        if ( kill(0, @pids) )           # Check if some left
        {
          mtr_warning("can't kill process(es) " . join(" ", @pids));
        }
      }

      stop_reap_all();
    }
  }
  else
  {
    mtr_debug("Directory for PID files ($rundir) does not exist.");
  }

  # We may have failed everything, but we now check again if we have
  # the listen ports free to use, and if they are free, just go for it.

  mtr_debug("Checking known mysqld servers...");

  foreach my $srv ( @kill_pids )
  {
    if ( defined $srv->{'port'} and mtr_ping_port($srv->{'port'}) )
    {
      mtr_warning("can't kill old process holding port $srv->{'port'}");
    }
  }

  mtr_debug("mtr_kill_leftovers(): finished.");
}


#
# Check that all processes in "spec" are shutdown gracefully
# else kill them off hard
#
sub mtr_check_stop_servers ($) {
  my $spec=  shift;

  # Return if no processes are defined
  return if ! @$spec;

  mtr_verbose("mtr_check_stop_servers");

  # ----------------------------------------------------------------------
  # Wait until servers in "spec" has stopped listening
  # to their ports or timeout occurs
  # ----------------------------------------------------------------------
  mtr_ping_with_timeout(\@$spec);

  # ----------------------------------------------------------------------
  # Use waitpid() nonblocking for a little while, to see how
  # many process's will exit sucessfully.
  # This is the normal case.
  # ----------------------------------------------------------------------
  my $wait_counter= 50; # Max number of times to redo the loop
  foreach my $srv ( @$spec )
  {
    my $pid= $srv->{'pid'};
    my $ret_pid;
    if ( $pid )
    {
      $ret_pid= waitpid($pid,&WNOHANG);
      if ($ret_pid == $pid)
      {
	mtr_verbose("Caught exit of process $ret_pid");
	$srv->{'pid'}= 0;
      }
      elsif ($ret_pid == 0)
      {
	mtr_verbose("Process $pid is still alive");
	if ($wait_counter-- > 0)
	{
	  # Give the processes more time to exit
	  select(undef, undef, undef, (0.1));
	  redo;
	}
      }
      else
      {
	mtr_warning("caught exit of unknown child $ret_pid");
      }
    }
  }

  # ----------------------------------------------------------------------
  # The processes that haven't yet exited need to
  # be killed hard, put them in "kill_pids" hash
  # ----------------------------------------------------------------------
  my %kill_pids;
  foreach my $srv ( @$spec )
  {
    my $pid= $srv->{'pid'};
    if ( $pid )
    {
      # Server is still alive, put it in list to be hard killed
      if ($::glob_win32_perl)
      {
	# Kill the real process if it's known
	$pid= $srv->{'real_pid'} if ($srv->{'real_pid'});
      }
      $kill_pids{$pid}= 1;

      # Write a message to the process's error log (if it has one)
      # that it's being killed hard.
      if ( defined $srv->{'errfile'} )
      {
	mtr_tofile($srv->{'errfile'}, "Note: Forcing kill of process $pid\n");
      }
      mtr_warning("Forcing kill of process $pid");

    }
    else
    {
      # Server is dead, remove the pidfile if it exists
      #
      # Race, could have been removed between test with -f
      # and the unlink() below, so better check again with -f
      if ( -f $srv->{'pidfile'} and ! unlink($srv->{'pidfile'}) and
           -f $srv->{'pidfile'} )
      {
        mtr_error("can't remove $srv->{'pidfile'}");
      }
    }
  }

  if ( ! keys %kill_pids )
  {
    # All processes has exited gracefully
    return;
  }

  mtr_kill_processes(\%kill_pids);

  # ----------------------------------------------------------------------
  # All processes are killed, cleanup leftover files
  # ----------------------------------------------------------------------
  {
    my $errors= 0;
    foreach my $srv ( @$spec )
    {
      if ( $srv->{'pid'} )
      {
	# Server has been hard killed, clean it's resources
	foreach my $file ($srv->{'pidfile'}, $srv->{'sockfile'})
        {
	  # Know it is dead so should be no race, careful anyway
	  if ( defined $file and -f $file and ! unlink($file) and -f $file )
          {
	    $errors++;
	    mtr_warning("couldn't delete $file");
	  }
	}

	if ($::glob_win32_perl and $srv->{'real_pid'})
	{
	  # Wait for the pseudo pid - if the real_pid was known
	  # the pseudo pid has not been waited for yet, wai blocking
	  # since it's "such a simple program"
	  mtr_verbose("Wait for pseudo process $srv->{'pid'}");
	  my $ret_pid= waitpid($srv->{'pid'}, 0);
	  mtr_verbose("Pseudo process $ret_pid died");
	}

	$srv->{'pid'}= 0;
      }
    }
    if ( $errors )
    {
      # There where errors killing processes
      # do one last attempt to ping the servers
      # and if they can't be pinged, assume they are dead
      if ( ! mtr_ping_with_timeout( \@$spec ) )
      {
	mtr_error("we could not kill or clean up all processes");
      }
      else
      {
	mtr_verbose("All ports were free, continuing");
      }
    }
  }
}


# Wait for all the process in the list to terminate
sub mtr_wait_blocking($) {
  my $admin_pids= shift;


  # Return if no processes defined
  return if ! %$admin_pids;

  mtr_verbose("mtr_wait_blocking");

  # Wait for all the started processes to exit
  # As mysqladmin is such a simple program, we trust it to terminate itself.
  # I.e. we wait blocking, and wait for them all before we go on.
  foreach my $pid (keys %{$admin_pids})
  {
    my $ret_pid= waitpid($pid,0);

  }
}

# Start "mysqladmin <command>" for a specific mysqld
sub mtr_mysqladmin_start($$$) {
  my $srv= shift;
  my $command= shift;
  my $adm_shutdown_tmo= shift;

  my $args;
  mtr_init_args(\$args);

  mtr_add_arg($args, "--no-defaults");
  mtr_add_arg($args, "--user=%s", $::opt_user);
  mtr_add_arg($args, "--password=");
  mtr_add_arg($args, "--silent");
  if ( -e $srv->{'path_sock'} )
  {
    mtr_add_arg($args, "--socket=%s", $srv->{'path_sock'});
  }
  if ( $srv->{'port'} )
  {
    mtr_add_arg($args, "--port=%s", $srv->{'port'});
  }
  if ( $srv->{'port'} and ! -e $srv->{'path_sock'} )
  {
    mtr_add_arg($args, "--protocol=tcp"); # Needed if no --socket
  }
  mtr_add_arg($args, "--connect_timeout=5");

  # Shutdown time must be high as slave may be in reconnect
  mtr_add_arg($args, "--shutdown_timeout=$adm_shutdown_tmo");
  mtr_add_arg($args, "$command");
  my $pid= mtr_spawn($::exe_mysqladmin, $args,
		     "", "", "", "",
		     { append_log_file => 1 });
  mtr_verbose("mtr_mysqladmin_start, pid: $pid");
  return $pid;

}

# Start "ndb_mgm shutdown" for a specific cluster, it will
# shutdown all data nodes and leave the ndb_mgmd running
sub mtr_ndbmgm_start($$) {
  my $cluster= shift;
  my $command= shift;

  my $args;

  mtr_init_args(\$args);

  mtr_add_arg($args, "--no-defaults");
  mtr_add_arg($args, "--core");
  mtr_add_arg($args, "--try-reconnect=1");
  mtr_add_arg($args, "--ndb_connectstring=%s", $cluster->{'connect_string'});
  mtr_add_arg($args, "-e");
  mtr_add_arg($args, "$command");

  my $pid= mtr_spawn($::exe_ndb_mgm, $args,
		     "", "/dev/null", "/dev/null", "",
		     {});
  mtr_verbose("mtr_ndbmgm_start, pid: $pid");
  return $pid;

}


# Ping all servers in list, exit when none of them answers
# or when timeout has passed
sub mtr_ping_with_timeout($) {
  my $spec= shift;
  my $timeout= 200;                     # 20 seconds max
  my $res= 1;                           # If we just fall through, we are done
                                        # in the sense that the servers don't
                                        # listen to their ports any longer

  mtr_debug("Waiting for mysqld servers to stop...");

 TIME:
  while ( $timeout-- )
  {
    foreach my $srv ( @$spec )
    {
      $res= 1;                          # We are optimistic
      if ( $srv->{'pid'} and defined $srv->{'port'} )
      {
	if ( mtr_ping_port($srv->{'port'}) )
	{
	  mtr_verbose("waiting for process $srv->{'pid'} to stop ".
		      "using port $srv->{'port'}");

	  # Millisceond sleep emulated with select
	  select(undef, undef, undef, (0.1));
	  $res= 0;
	  next TIME;
	}
	else
	{
	  # Process was not using port
	}
      }
    }
    last;                               # If we got here, we are done
  }

  if ($res)
  {
    mtr_debug("mtr_ping_with_timeout(): All mysqld instances are down.");
  }
  else
  {
    mtr_report("mtr_ping_with_timeout(): At least one server is alive.");
  }

  return $res;
}


#
# Loop through our list of processes and look for and entry
# with the provided pid
# Set the pid of that process to 0 if found
#
sub mark_process_dead($)
{
  my $ret_pid= shift;

  foreach my $mysqld (@{$::master}, @{$::slave})
  {
    if ( $mysqld->{'pid'} eq $ret_pid )
    {
      mtr_verbose("$mysqld->{'type'} $mysqld->{'idx'} exited, pid: $ret_pid");
      $mysqld->{'pid'}= 0;
      return;
    }
  }

  foreach my $cluster (@{$::clusters})
  {
    if ( $cluster->{'pid'} eq $ret_pid )
    {
      mtr_verbose("$cluster->{'name'} cluster ndb_mgmd exited, pid: $ret_pid");
      $cluster->{'pid'}= 0;
      return;
    }

    foreach my $ndbd (@{$cluster->{'ndbds'}})
    {
      if ( $ndbd->{'pid'} eq $ret_pid )
      {
	mtr_verbose("$cluster->{'name'} cluster ndbd exited, pid: $ret_pid");
	$ndbd->{'pid'}= 0;
	return;
      }
    }
  }
  mtr_warning("mark_process_dead couldn't find an entry for pid: $ret_pid");

}

#
# Loop through our list of processes and look for and entry
# with the provided pid, if found check for the file indicating
# expected crash and restart it.
#
sub check_expected_crash_and_restart($)
{
  my $ret_pid= shift;

  foreach my $mysqld (@{$::master}, @{$::slave})
  {
    if ( $mysqld->{'pid'} eq $ret_pid )
    {
      mtr_verbose("$mysqld->{'type'} $mysqld->{'idx'} exited, pid: $ret_pid");
      $mysqld->{'pid'}= 0;

      # Check if crash expected and restart if it was
      my $expect_file= "$::opt_vardir/tmp/" . "$mysqld->{'type'}" .
	"$mysqld->{'idx'}" . ".expect";
      if ( -f $expect_file )
      {
	mtr_verbose("Crash was expected, file $expect_file exists");
	mysqld_start($mysqld, $mysqld->{'start_opts'},
		     $mysqld->{'start_slave_master_info'});
	unlink($expect_file);
      }

      return;
    }
  }

  foreach my $cluster (@{$::clusters})
  {
    if ( $cluster->{'pid'} eq $ret_pid )
    {
      mtr_verbose("$cluster->{'name'} cluster ndb_mgmd exited, pid: $ret_pid");
      $cluster->{'pid'}= 0;

      # Check if crash expected and restart if it was
      my $expect_file= "$::opt_vardir/tmp/ndb_mgmd_" . "$cluster->{'type'}" .
	".expect";
      if ( -f $expect_file )
      {
	mtr_verbose("Crash was expected, file $expect_file exists");
	ndbmgmd_start($cluster);
	unlink($expect_file);
      }
      return;
    }

    foreach my $ndbd (@{$cluster->{'ndbds'}})
    {
      if ( $ndbd->{'pid'} eq $ret_pid )
      {
	mtr_verbose("$cluster->{'name'} cluster ndbd exited, pid: $ret_pid");
	$ndbd->{'pid'}= 0;

	# Check if crash expected and restart if it was
	my $expect_file= "$::opt_vardir/tmp/ndbd_" . "$cluster->{'type'}" .
	  "$ndbd->{'idx'}" . ".expect";
	if ( -f $expect_file )
	{
	  mtr_verbose("Crash was expected, file $expect_file exists");
	  ndbd_start($cluster, $ndbd->{'idx'},
		     $ndbd->{'start_extra_args'});
	  unlink($expect_file);
	}
	return;
      }
    }
  }

  if ($::instance_manager->{'spawner_pid'} eq $ret_pid)
  {
    return;
  }

  mtr_warning("check_expected_crash_and_restart couldn't find an entry for pid: $ret_pid");

}

##############################################################################
#
#  The operating system will keep information about dead children, 
#  we read this information here, and if we have records the process
#  is alive, we mark it as dead.
#
##############################################################################

sub mtr_record_dead_children () {

  my $process_died= 0;
  my $ret_pid;

  # Wait without blockinng to see if any processes had died
  # -1 or 0 means there are no more procesess to wait for
  while ( ($ret_pid= waitpid(-1,&WNOHANG)) != 0 and $ret_pid != -1)
  {
    mtr_warning("mtr_record_dead_children: $ret_pid");
    mark_process_dead($ret_pid);
    $process_died= 1;
  }
  return $process_died;
}

sub start_reap_all {
  # This causes terminating processes to not become zombies, avoiding
  # the need for (or possibility of) explicit waitpid().
  $SIG{CHLD}= 'IGNORE';

  # On some platforms (Linux, QNX, OSX, ...) there is potential race
  # here. If a process terminated before setting $SIG{CHLD} (but after
  # any attempt to waitpid() it), it will still be a zombie. So we
  # have to handle any such process here.
  my $pid;
  while(($pid= waitpid(-1, &WNOHANG)) != 0 and $pid != -1)
  {
    mtr_warning("start_reap_all pid: $pid");
    mark_process_dead($pid);
  };
}

sub stop_reap_all {
  $SIG{CHLD}= 'DEFAULT';
}


sub mtr_ping_port ($) {
  my $port= shift;

  mtr_verbose("mtr_ping_port: $port");

  my $remote= "localhost";
  my $iaddr=  inet_aton($remote);
  if ( ! $iaddr )
  {
    mtr_error("can't find IP number for $remote");
  }
  my $paddr=  sockaddr_in($port, $iaddr);
  my $proto=  getprotobyname('tcp');
  if ( ! socket(SOCK, PF_INET, SOCK_STREAM, $proto) )
  {
    mtr_error("can't create socket: $!");
  }

  mtr_debug("Pinging server (port: $port)...");

  if ( connect(SOCK, $paddr) )
  {
    close(SOCK);                        # FIXME check error?
    mtr_verbose("USED");
    return 1;
  }
  else
  {
    mtr_verbose("FREE");
    return 0;
  }
}

##############################################################################
#
#  Wait for a file to be created
#
##############################################################################

# FIXME check that the pidfile contains the expected pid!

sub sleep_until_file_created ($$$) {
  my $pidfile= shift;
  my $timeout= shift;
  my $pid=     shift;
  my $sleeptime= 100; # Milliseconds
  my $loops= ($timeout * 1000) / $sleeptime;

  for ( my $loop= 1; $loop <= $loops; $loop++ )
  {
    if ( -r $pidfile )
    {
      return 1;
    }

    # Check if it died after the fork() was successful
    if ( $pid != 0 && waitpid($pid,&WNOHANG) == $pid )
    {
      mtr_warning("Process $pid died");
      return 0;
    }

    mtr_debug("Sleep $sleeptime milliseconds waiting for $pidfile");

    # Print extra message every 60 seconds
    my $seconds= ($loop * $sleeptime) / 1000;
    if ( $seconds > 1 and int($seconds * 10) % 600 == 0 )
    {
      my $left= $timeout - $seconds;
      mtr_warning("Waited $seconds seconds for $pidfile to be created, " .
                  "still waiting for $left seconds...");
    }

    # Millisceond sleep emulated with select
    select(undef, undef, undef, ($sleeptime/1000));
  }

  return 0;
}


sub mtr_kill_processes ($) {
  my $pids = shift;

  mtr_verbose("mtr_kill_processes (" . join(" ", keys %{$pids}) . ")");

  foreach my $pid (keys %{$pids})
  {

    if ($pid <= 0)
    {
      mtr_warning("Trying to kill illegal pid: $pid");
      next;
    }

    my $signaled_procs= kill(9, $pid);
    if ($signaled_procs == 0)
    {
      # No such process existed, assume it's killed
      mtr_verbose("killed $pid(no such process)");
    }
    else
    {
      my $ret_pid= waitpid($pid,0);
      if ($ret_pid == $pid)
      {
	mtr_verbose("killed $pid(got the pid)");
      }
      elsif ($ret_pid == -1)
      {
	mtr_verbose("killed $pid(got -1)");
      }
    }
  }
  mtr_verbose("done killing processes");
}


##############################################################################
#
#  When we exit, we kill off all children
#
##############################################################################

sub mtr_exit ($) {
  my $code= shift;
  mtr_timer_stop_all($::glob_timers);
  local $SIG{HUP} = 'IGNORE';
  # ToDo: Signalling -$$ will only work if we are the process group
  # leader (in fact on QNX it will signal our session group leader,
  # which might be Do-compile or Pushbuild, causing tests to be
  # aborted). So we only do it if we are the group leader. We might
  # set ourselves as the group leader at startup (with
  # POSIX::setpgrp(0,0)), but then care must be needed to always do
  # proper child process cleanup.
  POSIX::kill(SIGHUP, -$$) if !$::glob_win32_perl and $$ == getpgrp();

  exit($code);
}

###########################################################################

1;
