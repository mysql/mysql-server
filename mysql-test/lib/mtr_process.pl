# -*- cperl -*-

# This is a library file used by the Perl version of mysql-test-run,
# and is part of the translation of the Bourne shell script with the
# same name.

#use Carp qw(cluck);
use Socket;
use Errno;
use strict;

#use POSIX ":sys_wait_h";
use POSIX 'WNOHANG';

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
sub mtr_kill_process ($$$);

# static in C
sub spawn_impl ($$$$$$$$);

##############################################################################
#
#  Execute an external command
#
##############################################################################

# This function try to mimic the C version used in "netware/mysql_test_run.c"

sub mtr_run ($$$$$$;$) {
  my $path=       shift;
  my $arg_list_t= shift;
  my $input=      shift;
  my $output=     shift;
  my $error=      shift;
  my $pid_file=   shift;
  my $spawn_opts= shift;

  return spawn_impl($path,$arg_list_t,'run',$input,$output,$error,$pid_file,
    $spawn_opts);
}

sub mtr_run_test ($$$$$$;$) {
  my $path=       shift;
  my $arg_list_t= shift;
  my $input=      shift;
  my $output=     shift;
  my $error=      shift;
  my $pid_file=   shift;
  my $spawn_opts= shift;

  return spawn_impl($path,$arg_list_t,'test',$input,$output,$error,$pid_file,
    $spawn_opts);
}

sub mtr_spawn ($$$$$$;$) {
  my $path=       shift;
  my $arg_list_t= shift;
  my $input=      shift;
  my $output=     shift;
  my $error=      shift;
  my $pid_file=   shift;
  my $spawn_opts= shift;

  return spawn_impl($path,$arg_list_t,'spawn',$input,$output,$error,$pid_file,
    $spawn_opts);
}


##############################################################################
#
#  If $join is set, we return the error code, else we return the PID
#
##############################################################################

sub spawn_impl ($$$$$$$$) {
  my $path=       shift;
  my $arg_list_t= shift;
  my $mode=       shift;
  my $input=      shift;
  my $output=     shift;
  my $error=      shift;
  my $pid_file=   shift;                 # FIXME
  my $spawn_opts= shift;

  if ( $::opt_script_debug )
  {
    print STDERR "\n";
    print STDERR "#### ", "-" x 78, "\n";
    print STDERR "#### ", "STDIN  $input\n" if $input;
    print STDERR "#### ", "STDOUT $output\n" if $output;
    print STDERR "#### ", "STDERR $error\n" if $error;
    print STDERR "#### ", "$mode : $path ", join(" ",@$arg_list_t), "\n";
    print STDERR "#### ", "spawn options:\n";
    if ($spawn_opts)
    {
      foreach my $key (sort keys %{$spawn_opts})
      {
        print STDERR "#### ", "  - $key: $spawn_opts->{$key}\n";
      }
    }
    else
    {
      print STDERR "#### ", "  none\n";
    }
    print STDERR "#### ", "-" x 78, "\n";
  }

 FORK:
  {
    my $pid= fork();

    if ( ! defined $pid )
    {
      if ( $! == $!{EAGAIN} )           # See "perldoc Errno"
      {
        mtr_debug("Got EAGAIN from fork(), sleep 1 second and redo");
        sleep(1);
        redo FORK;
      }
      else
      {
        mtr_error("$path ($pid) can't be forked");
      }
    }

    if ( $pid )
    {
      spawn_parent_impl($pid,$mode,$path);
    }
    else
    {
      # Child, redirect output and exec
      # FIXME I tried POSIX::setsid() here to detach and, I hoped,
      # avoid zombies. But everything went wild, somehow the parent
      # became a deamon as well, and was hard to kill ;-)
      # Need to catch SIGCHLD and do waitpid or something instead......

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
          # Should be fixed so that the thread that is created with fork
          # executes the exe in another process and wait's for it to return.
          # In the meanwhile, we get all the output from mysqld's to screen
	}
        elsif ( ! open(STDOUT,$log_file_open_mode,$output) )
        {
          mtr_child_error("can't redirect STDOUT to \"$output\": $!");
        }
      }

      if ( $error )
      {
        if ( $output eq $error )
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
    }
  }
}


sub spawn_parent_impl {
  my $pid=  shift;
  my $mode= shift;
  my $path= shift;

  if ( $mode eq 'run' or $mode eq 'test' )
  {
    if ( $mode eq 'run' )
    {
      # Simple run of command, we wait for it to return
      my $ret_pid= waitpid($pid,0);
      if ( $ret_pid != $pid )
      {
        mtr_error("$path ($pid) got lost somehow");
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
      #
      # FIXME is this as it should be? Can't mysqld terminate
      # normally from running a test case?

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
            # FIXME we should only give up the suite, not all of the run?
            print STDERR "\n";
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

        # If one of the processes died, we want to
        # mark this, and kill the mysqltest process.

	mark_process_dead($ret_pid);
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

  my @kill_pids;
  my %admin_pids;
  my $pid;

  #Start shutdown of instance_managers, masters and slaves
  foreach my $srv ($::instance_manager,
		   @{$::instance_manager->{'instances'}},
		   @{$::master},@{$::slave})
  {
    $pid= mtr_mysqladmin_start($srv, "shutdown", 70);

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

  # Start shutdown of clusters
  foreach my $cluster (@{$::clusters})
  {
    $pid= mtr_ndbmgm_start($cluster, "shutdown");

    # Save the pid of the ndb_mgm process
    $admin_pids{$pid}= 1;

    push(@kill_pids,{
		     pid      => $cluster->{'pid'},
		     pidfile  => $cluster->{'path_pid'}
		    });

    $cluster->{'pid'}= 0; # Assume we are done with it


    foreach my $ndbd (@{$cluster->{'ndbds'}})
    {
      push(@kill_pids,{
		       pid      => $ndbd->{'pid'},
		       pidfile  => $ndbd->{'path_pid'},
		      });
      $ndbd->{'pid'}= 0; # Assume we are done with it
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

  # FIXME $path_run_dir or something
  my $rundir= "$::opt_vardir/run";

  if ( -d $rundir )
  {
    opendir(RUNDIR, $rundir)
      or mtr_error("can't open directory \"$rundir\": $!");

    my @pids;

    while ( my $elem= readdir(RUNDIR) )
    {
      my $pidfile= "$rundir/$elem";

      if ( -f $pidfile )
      {
        my $pid= mtr_get_pid_from_file($pidfile);

        # Race, could have been removed between I tested with -f
        # and the unlink() below, so I better check again with -f

        if ( ! unlink($pidfile) and -f $pidfile )
        {
          mtr_error("can't remove $pidfile");
        }

        if ( $::glob_cygwin_perl or kill(0, $pid) )
        {
          push(@pids, $pid);            # We know (cygwin guess) it exists
        }
      }
    }
    closedir(RUNDIR);

    if ( @pids )
    {
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
          kill(9, @pids);
          mtr_report("Sleep 1 second waiting for processes to die");
          sleep(1)                      # Wait one second
        } while ( $retries-- and  kill(0, @pids) );

        if ( kill(0, @pids) )           # Check if some left
        {
          mtr_warning("can't kill process(es) " . join(" ", @pids));
        }
      }
    }
  }

  # We may have failed everything, but we now check again if we have
  # the listen ports free to use, and if they are free, just go for it.

  foreach my $srv ( @kill_pids )
  {
    if ( defined $srv->{'port'} and mtr_ping_port($srv->{'port'}) )
    {
      mtr_warning("can't kill old process holding port $srv->{'port'}");
    }
  }
}


# Check that all processes in list are killed
# The argument is a list of 'ports', 'pids', 'pidfiles' and 'socketfiles'
# for which shutdown has been started. Make sure they all get killed
# in one way or the other.
#
# FIXME On Cygwin, and maybe some other platforms, $srv->{'pid'} and
# the pid in $srv->{'pidfile'} will not be the same PID. We need to try to kill
# both I think.

sub mtr_check_stop_servers ($) {
  my $spec=  shift;

  # Return if no processes are defined
  return if ! @$spec;

  #mtr_report("mtr_check_stop_servers");

  mtr_ping_with_timeout(\@$spec);

  # ----------------------------------------------------------------------
  # We loop with waitpid() nonblocking to see how many of the ones we
  # are to kill, actually got killed by mysqladmin or ndb_mgm
  #
  # Note that we don't rely on this, the mysqld server might have stopped
  # listening to the port, but still be alive. But it is a start.
  # ----------------------------------------------------------------------

  foreach my $srv ( @$spec )
  {
    my $ret_pid;
    if ( $srv->{'pid'} )
    {
      $ret_pid= waitpid($srv->{'pid'},&WNOHANG);
      if ($ret_pid == $srv->{'pid'})
      {
	mtr_verbose("Caught exit of process $ret_pid");
	$srv->{'pid'}= 0;
      }
      else
      {
	# mtr_warning("caught exit of unknown child $ret_pid");
      }
    }
  }

  # ----------------------------------------------------------------------
  # We know the process was started from this file, so there is a PID
  # saved, or else we have nothing to do.
  # Might be that is is recorded to be missing, but we failed to
  # take away the PID file earlier, then we do it now.
  # ----------------------------------------------------------------------

  my %mysqld_pids;

  foreach my $srv ( @$spec )
  {
    if ( $srv->{'pid'} )
    {
      $mysqld_pids{$srv->{'pid'}}= 1;
    }
    else
    {
      # Server is dead, we remove the pidfile if any
      # Race, could have been removed between I tested with -f
      # and the unlink() below, so I better check again with -f

      if ( -f $srv->{'pidfile'} and ! unlink($srv->{'pidfile'}) and
           -f $srv->{'pidfile'} )
      {
        mtr_error("can't remove $srv->{'pidfile'}");
      }
    }
  }

  # ----------------------------------------------------------------------
  # If all the processes in list already have been killed,
  # then we don't have to do anything.
  # ----------------------------------------------------------------------

  if ( ! keys %mysqld_pids )
  {
    return;
  }

  # ----------------------------------------------------------------------
  # In mtr_mysqladmin_shutdown() we only waited for the mysqld servers
  # not to listen to the port. But we are not sure we got them all
  # killed. If we suspect it lives, try nice kill with SIG_TERM. Note
  # that for true Win32 processes, kill(0,$pid) will not return 1.
  # ----------------------------------------------------------------------

  start_reap_all();                     # Avoid zombies

  my @mysqld_pids= keys %mysqld_pids;
  mtr_kill_processes(\@mysqld_pids);

  stop_reap_all();                      # Get into control again

  # ----------------------------------------------------------------------
  # Now, we check if all we can find using kill(0,$pid) are dead,
  # and just assume the rest are. We cleanup socket and PID files.
  # ----------------------------------------------------------------------

  {
    my $errors= 0;
    foreach my $srv ( @$spec )
    {
      if ( $srv->{'pid'} )
      {
        if ( kill(0,$srv->{'pid'}) )
        {
          # FIXME In Cygwin there seem to be some fast reuse
          # of PIDs, so dying may not be the right thing to do.
          $errors++;
          mtr_warning("can't kill process $srv->{'pid'}");
        }
        else
        {
          # We managed to kill it at last
          # FIXME In Cygwin, we will get here even if the process lives.

          # Not needed as we know the process is dead, but to be safe
          # we unlink and check success in two steps. We first unlink
          # without checking the error code, and then check if the
          # file still exists.

          foreach my $file ($srv->{'pidfile'}, $srv->{'sockfile'})
          {
            # Know it is dead so should be no race, careful anyway
            if ( defined $file and -f $file and ! unlink($file) and -f $file )
            {
              $errors++;
              mtr_warning("couldn't delete $file");
            }
          }
	  $srv->{'pid'}= 0;
        }
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
	mtr_verbose("All ports where free, continuing");
      }
    }
  }

  # FIXME We just assume they are all dead, for Cygwin we are not
  # really sure

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

# Start "mysqladmin shutdown" for a specific mysqld
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
  my $path_mysqladmin_log= "$::opt_vardir/log/mysqladmin.log";
  my $pid= mtr_spawn($::exe_mysqladmin, $args,
		     "", $path_mysqladmin_log, $path_mysqladmin_log, "",
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

  $timeout or mtr_report("At least one server is still listening to its port");

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

##############################################################################
#
#  The operating system will keep information about dead children, 
#  we read this information here, and if we have records the process
#  is alive, we mark it as dead.
#
##############################################################################

sub mtr_record_dead_children () {

  my $ret_pid;

  # Wait without blockinng to see if any processes had died
  # -1 or 0 means there are no more procesess to wait for
  while ( ($ret_pid= waitpid(-1,&WNOHANG)) != 0 and $ret_pid != -1)
  {
    mtr_warning("mtr_record_dead_children: $ret_pid");
    mark_process_dead($ret_pid);
  }
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
      return $pid;
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
    if ( $seconds > 1 and int($seconds) % 60 == 0 )
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

  mtr_verbose("mtr_kill_processes " . join(" ", @$pids));

  foreach my $pid (@$pids)
  {
    foreach my $sig (15, 9)
    {
      last if mtr_kill_process($pid, $sig, 10);
    }
  }
}


sub mtr_kill_process ($$$) {
  my $pid= shift;
  my $signal= shift;
  my $timeout= shift; # Seconds to wait for process
  my $max_loop= $timeout*10; # Sleeping 0.1 between each kill attempt

  for (my $cur_attempt= 1; $cur_attempt <= $max_loop; ++$cur_attempt)
  {
    mtr_debug("Sending $signal to $pid...");

    kill($signal, $pid);

    last unless kill (0, $pid) and $max_loop--;

    mtr_verbose("Sleep 0.1 second waiting for processes $pid to die");

    select(undef, undef, undef, 0.1);
  }

  return $max_loop;
}

##############################################################################
#
#  When we exit, we kill off all children
#
##############################################################################

# FIXME something is wrong, we sometimes terminate with "Hangup" written
# to tty, and no STDERR output telling us why.

# FIXME for some reason, setting HUP to 'IGNORE' will cause exit() to
# write out "Hangup", and maybe loose some output. We insert a sleep...

sub mtr_exit ($) {
  my $code= shift;
#  cluck("Called mtr_exit()");
  mtr_timer_stop_all($::glob_timers);
  local $SIG{HUP} = 'IGNORE';
  # ToDo: Signalling -$$ will only work if we are the process group
  # leader (in fact on QNX it will signal our session group leader,
  # which might be Do-compile or Pushbuild, causing tests to be
  # aborted). So we only do it if we are the group leader. We might
  # set ourselves as the group leader at startup (with
  # POSIX::setpgrp(0,0)), but then care must be needed to always do
  # proper child process cleanup.
  kill('HUP', -$$) if !$::glob_win32_perl and $$ == getpgrp();

  exit($code);
}

1;
