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
sub mtr_stop_mysqld_servers ($);
sub mtr_kill_leftovers ();
sub mtr_record_dead_children ();
sub mtr_exit ($);
sub sleep_until_file_created ($$$);
sub mtr_kill_processes ($);
sub mtr_ping_mysqld_server ($);

# Private IM-related operations.

sub mtr_im_kill_process ($$$$);
sub mtr_im_load_pids ($);
sub mtr_im_terminate ($);
sub mtr_im_check_alive ($);
sub mtr_im_check_main_alive ($);
sub mtr_im_check_angel_alive ($);
sub mtr_im_check_mysqlds_alive ($);
sub mtr_im_check_mysqld_alive ($$);
sub mtr_im_cleanup ($);
sub mtr_im_rm_file ($);
sub mtr_im_errlog ($);
sub mtr_im_kill ($);
sub mtr_im_wait_for_connection ($$$);
sub mtr_im_wait_for_mysqld($$$);

# Public IM-related operations.

sub mtr_im_start ($$);
sub mtr_im_stop ($);

# static in C
sub spawn_impl ($$$$$$$$);

##############################################################################
#
#  Execute an external command
#
##############################################################################

# This function try to mimic the C version used in "netware/mysql_test_run.c"
# FIXME learn it to handle append mode as well, a "new" flag or a "append"

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

      if ( $::glob_cygwin_shell and $mode eq 'test' )
      {
        # Programs started from mysqltest under Cygwin, are to
        # execute them within Cygwin. Else simple things in test
        # files like
        # --system "echo 1 > file"
        # will fail.
        # FIXME not working :-(
#       $ENV{'COMSPEC'}= "$::glob_cygwin_shell -c";
      }

      my $log_file_open_mode = '>';

      if ($spawn_opts and $spawn_opts->{'append_log_file'})
      {
        $log_file_open_mode = '>>';
      }

      if ( $output )
      {
        if ( ! open(STDOUT,$log_file_open_mode,$output) )
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

      if ( $ret_pid <= 0 )
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
        # 

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

        # If one of the mysqld processes died, we want to
        # mark this, and kill the mysqltest process.

        foreach my $idx (0..1)
        {
          if ( $::master->[$idx]->{'pid'} eq $ret_pid )
          {
            mtr_debug("child $ret_pid was master[$idx], " .
                      "exit during mysqltest run");
            $::master->[$idx]->{'pid'}= 0;
            last;
          }
        }

        foreach my $idx (0..2)
        {
          if ( $::slave->[$idx]->{'pid'} eq $ret_pid )
          {
            mtr_debug("child $ret_pid was slave[$idx], " .
                      "exit during mysqltest run");
            $::slave->[$idx]->{'pid'}= 0;
            last;
          }
        }

        mtr_debug("waitpid() catched exit of unknown child $ret_pid, " .
                  "exit during mysqltest run");
      }

      if ( $ret_pid != $pid )
      {
        # We terminated the waiting because a "mysqld" process died.
        # Kill the mysqltest process.

        kill(9,$pid);

        $ret_pid= waitpid($pid,0);

        if ( $ret_pid == -1 )
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

# We just "ping" on the ports, and if we can't do a socket connect
# we assume the server is dead. So we don't *really* know a server
# is dead, we just hope that it after letting the listen port go,
# it is dead enough for us to start a new server.

sub mtr_kill_leftovers () {

  mtr_debug("mtr_kill_leftovers(): started.");

  mtr_im_stop($::instance_manager);

  # Kill mysqld servers (masters and slaves) that would conflict with this
  # run. Make sure to remove the PID file, if any.
  # Don't touch IM-managed mysqld instances -- they should be stopped by
  # mtr_im_stop().

  mtr_debug("Collecting mysqld-instances to shutdown...");

  my @args;

  for ( my $idx= 0; $idx < 2; $idx++ )
  {
    my $pidfile= $::master->[$idx]->{'path_mypid'};
    my $sockfile= $::master->[$idx]->{'path_mysock'};
    my $port= $::master->[$idx]->{'path_myport'};

    push(@args,{
                pid      => 0,          # We don't know the PID
                pidfile  => $pidfile,
                sockfile => $sockfile,
                port     => $port,
               });

    mtr_debug("  - Master mysqld " .
              "(idx: $idx; pid: '$pidfile'; socket: '$sockfile'; port: $port)");
    $::master->[$idx]->{'pid'}= 0; # Assume we are done with it
  }

  for ( my $idx= 0; $idx < 3; $idx++ )
  {
    my $pidfile= $::slave->[$idx]->{'path_mypid'};
    my $sockfile= $::slave->[$idx]->{'path_mysock'};
    my $port= $::slave->[$idx]->{'path_myport'};

    push(@args,{
                pid       => 0,         # We don't know the PID
                pidfile   => $pidfile,
                sockfile  => $sockfile,
                port      => $port,
               });

    mtr_debug("  - Slave mysqld " .
              "(idx: $idx; pid: '$pidfile'; socket: '$sockfile'; port: $port)");

    $::slave->[$idx]->{'pid'}= 0; # Assume we are done with it
  }

  mtr_mysqladmin_shutdown(\@args, 20);

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

  mtr_debug("Processing PID files in directory '$rundir'...");

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
        mtr_debug("Processing PID file: '$pidfile'...");

        my $pid= mtr_get_pid_from_file($pidfile);

        mtr_debug("Got pid: $pid from file '$pidfile'");

        # Race, could have been removed between I tested with -f
        # and the unlink() below, so I better check again with -f

        if ( ! unlink($pidfile) and -f $pidfile )
        {
          mtr_error("can't remove $pidfile");
        }

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
          mtr_debug("Sleep 1 second waiting for processes to die");
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

  # We may have failed everything, bug we now check again if we have
  # the listen ports free to use, and if they are free, just go for it.

  mtr_debug("Checking known mysqld servers...");

  foreach my $srv ( @args )
  {
    if ( mtr_ping_mysqld_server($srv->{'port'}) )
    {
      mtr_warning("can't kill old mysqld holding port $srv->{'port'}");
    }
  }

  mtr_debug("mtr_kill_leftovers(): finished.");
}

##############################################################################
#
#  Shut down mysqld servers we have started from this run of this script
#
##############################################################################

# To speed things we kill servers in parallel. The argument is a list
# of 'ports', 'pids', 'pidfiles' and 'socketfiles'.

# FIXME On Cygwin, and maybe some other platforms, $srv->{'pid'} and
# $srv->{'pidfile'} will not be the same PID. We need to try to kill
# both I think.

sub mtr_stop_mysqld_servers ($) {
  my $spec=  shift;

  # ----------------------------------------------------------------------
  # First try nice normal shutdown using 'mysqladmin'
  # ----------------------------------------------------------------------

  # Shutdown time must be high as slave may be in reconnect
  mtr_mysqladmin_shutdown($spec, 70);

  # ----------------------------------------------------------------------
  # We loop with waitpid() nonblocking to see how many of the ones we
  # are to kill, actually got killed by mtr_mysqladmin_shutdown().
  # Note that we don't rely on this, the mysqld server might have stop
  # listening to the port, but still be alive. But it is a start.
  # ----------------------------------------------------------------------

  foreach my $srv ( @$spec )
  {
    if ( $srv->{'pid'} and (waitpid($srv->{'pid'},&WNOHANG) == $srv->{'pid'}) )
    {
      $srv->{'pid'}= 0;
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
  # If the processes where started from this script, and we had no PIDS
  # then we don't have to do anything.
  # ----------------------------------------------------------------------

  if ( ! keys %mysqld_pids )
  {
    # cluck "This is how we got here!";
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
            if ( -f $file and ! unlink($file) and -f $file )
            {
              $errors++;
              mtr_warning("couldn't delete $file");
            }
          }
        }
      }
    }
    if ( $errors )
    {
      # We are in trouble, just die....
      mtr_error("we could not kill or clean up all processes");
    }
  }

  # FIXME We just assume they are all dead, for Cygwin we are not
  # really sure
    
}


##############################################################################
#
#  Shut down mysqld servers using "mysqladmin ... shutdown".
#  To speed this up, we start them in parallel and use waitpid() to
#  catch their termination. Note that this doesn't say the servers
#  are terminated, just that 'mysqladmin' is terminated.
#
#  Note that mysqladmin will ask the server about what PID file it uses,
#  and mysqladmin will wait for it to be removed before it terminates
#  (unless passes timeout).
#
#  This function will take at most about 20 seconds, and we still are not
#  sure we killed them all. If none is responding to ping, we return 1,
#  else we return 0.
#
##############################################################################

sub mtr_mysqladmin_shutdown {
  my $spec= shift;
  my $adm_shutdown_tmo= shift;

  my %mysql_admin_pids;
  my @to_kill_specs;

  mtr_debug("mtr_mysqladmin_shutdown(): starting...");
  mtr_debug("Collecting mysqld-instances to shutdown...");

  foreach my $srv ( @$spec )
  {
    if ( mtr_ping_mysqld_server($srv->{'port'}) )
    {
      mtr_debug("Mysqld (port: $srv->{port}) needs to be stopped.");

      push(@to_kill_specs, $srv);
    }
  }


  foreach my $srv ( @to_kill_specs )
  {
    # Shutdown time must be high as slave may be in reconnect
    my $args;

    mtr_init_args(\$args);

    mtr_add_arg($args, "--no-defaults");
    mtr_add_arg($args, "--user=%s", $::opt_user);
    mtr_add_arg($args, "--password=");
    if ( -e $srv->{'sockfile'} )
    {
      mtr_add_arg($args, "--socket=%s", $srv->{'sockfile'});
    }
    if ( $srv->{'port'} )
    {
      mtr_add_arg($args, "--port=%s", $srv->{'port'});
    }
    if ( $srv->{'port'} and ! -e $srv->{'sockfile'} )
    {
      mtr_add_arg($args, "--protocol=tcp"); # Needed if no --socket
    }
    mtr_add_arg($args, "--connect_timeout=5");
    mtr_add_arg($args, "--shutdown_timeout=$adm_shutdown_tmo");
    mtr_add_arg($args, "shutdown");

    mtr_debug("Shutting down mysqld " .
              "(port: $srv->{port}; socket: '$srv->{sockfile}')...");

    my $path_mysqladmin_log= "$::opt_vardir/log/mysqladmin.log";
    my $pid= mtr_spawn($::exe_mysqladmin, $args,
                       "", $path_mysqladmin_log, $path_mysqladmin_log, "",
                       { append_log_file => 1 });
    $mysql_admin_pids{$pid}= 1;

    # We don't wait for termination of mysqladmin
  }

  # As mysqladmin is such a simple program, we trust it to terminate.
  # I.e. we wait blocking, and wait wait for them all before we go on.
  while (keys %mysql_admin_pids)
  {
    foreach my $pid (keys %mysql_admin_pids)
    {
      if ( waitpid($pid,0) > 0 )
      {
        delete $mysql_admin_pids{$pid};
      }
    }
  }

  # If we trusted "mysqladmin --shutdown_timeout= ..." we could just
  # terminate now, but we don't (FIXME should be debugged).
  # So we try again to ping and at least wait the same amount of time
  # mysqladmin would for all to die.

  my $timeout= 20;                      # 20 seconds max
  my $res= 1;                           # If we just fall through, we are done
                                        # in the sense that the servers don't
                                        # listen to their ports any longer

  mtr_debug("Waiting for mysqld servers to stop...");

 TIME:
  while ( $timeout-- )
  {
    foreach my $srv ( @to_kill_specs )
    {
      $res= 1;                          # We are optimistic
      if ( mtr_ping_mysqld_server($srv->{'port'}) )
      {
        mtr_debug("Mysqld (port: $srv->{port}) is still alive.");
        mtr_debug("Sleep 1 second waiting for processes to stop using port");
        sleep(1);                       # One second
        $res= 0;
        next TIME;
      }
    }
    last;                               # If we got here, we are done
  }

  if ($res)
  {
    mtr_debug("mtr_mysqladmin_shutdown(): All mysqld instances are down.");
  }
  else
  {
    mtr_debug("mtr_mysqladmin_shutdown(): At least one server is alive.");
  }

  return $res;
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

  # FIXME the man page says to wait for -1 to terminate,
  # but on OS X we get '0' all the time...
  while ( ($ret_pid= waitpid(-1,&WNOHANG)) > 0 )
  {
    mtr_debug("waitpid() catched exit of child $ret_pid");
    foreach my $idx (0..1)
    {
      if ( $::master->[$idx]->{'pid'} eq $ret_pid )
      {
        mtr_debug("child $ret_pid was master[$idx]");
        $::master->[$idx]->{'pid'}= 0;
      }
    }

    foreach my $idx (0..2)
    {
      if ( $::slave->[$idx]->{'pid'} eq $ret_pid )
      {
        mtr_debug("child $ret_pid was slave[$idx]");
        $::slave->[$idx]->{'pid'}= 0;
        last;
      }
    }
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
  while(waitpid(-1, &WNOHANG) > 0) { };
}

sub stop_reap_all {
  $SIG{CHLD}= 'DEFAULT';
}

sub mtr_ping_mysqld_server ($) {
  my $port= shift;

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
    mtr_debug("Server (port: $port) is alive.");
    close(SOCK);                        # FIXME check error?
    return 1;
  }
  else
  {
    mtr_debug("Server (port: $port) is dead.");
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
      return 0;
    }

    mtr_debug("Sleep $sleeptime milliseconds waiting for ".
	      "creation of $pidfile");

    # Print extra message every 60 seconds
    my $seconds= ($loop * $sleeptime) / 1000;
    if ( $seconds > 1 and $seconds % 60 == 0 )
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

  foreach my $sig (15, 9)
  {
    my $retries= 10;
    while (1)
    {
      kill($sig, @{$pids});
      last unless kill (0, @{$pids}) and $retries--;

      mtr_debug("Sleep 2 second waiting for processes to die");
      sleep(2);
    }
  }
}

##############################################################################
#
#  When we exit, we kill off all children
#
##############################################################################

# FIXME something is wrong, we sometimes terminate with "Hangup" written
# to tty, and no STDERR output telling us why.

# FIXME for some readon, setting HUP to 'IGNORE' will cause exit() to
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
  kill('HUP', -$$) if $$ == getpgrp();
  sleep 2;
  exit($code);
}

##############################################################################
#
#  Instance Manager management routines.
#
##############################################################################

sub mtr_im_kill_process ($$$$) {
  my $pid_lst= shift;
  my $signal= shift;
  my $total_retries= shift;
  my $timeout= shift;

  my %pids;

  foreach my $pid (@{$pid_lst})
  {
    $pids{$pid}= 1;
  }

  for (my $cur_attempt= 1; $cur_attempt <= $total_retries; ++$cur_attempt)
  {
    foreach my $pid (keys %pids)
    {
      mtr_debug("Sending $signal to $pid...");

      kill($signal, $pid);

      unless (kill (0, $pid))
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
  my $instance_manager= shift;

  mtr_debug("Loading PID files...");

  # Obtain mysqld-process pids.

  my $instances = $instance_manager->{'instances'};

  for (my $idx= 0; $idx < 2; ++$idx)
  {
    mtr_debug("IM-guarded mysqld[$idx] PID file: '" .
              $instances->[$idx]->{'path_pid'} . "'.");

    my $mysqld_pid;

    if (-r $instances->[$idx]->{'path_pid'})
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

  mtr_debug("IM-main PID file: '$instance_manager->{path_pid}'.");

  if (-f $instance_manager->{'path_pid'})
  {
    $instance_manager->{'pid'} =
      mtr_get_pid_from_file($instance_manager->{'path_pid'});

    mtr_debug("IM-main PID: $instance_manager->{pid}.");
  }
  else
  {
    mtr_debug("IM-main: no PID file.");
    $instance_manager->{'pid'}= undef;
  }

  #   - IM-angel

  mtr_debug("IM-angel PID file: '$instance_manager->{path_angel_pid}'.");

  if (-f $instance_manager->{'path_angel_pid'})
  {
    $instance_manager->{'angel_pid'} =
      mtr_get_pid_from_file($instance_manager->{'path_angel_pid'});

    mtr_debug("IM-angel PID: $instance_manager->{'angel_pid'}.");
  }
  else
  {
    mtr_debug("IM-angel: no PID file.");
    $instance_manager->{'angel_pid'} = undef;
  }
}

###########################################################################

sub mtr_im_terminate($) {
  my $instance_manager= shift;

  # Load pids from pid-files. We should do it first of all, because IM deletes
  # them on shutdown.

  mtr_im_load_pids($instance_manager);

  mtr_debug("Shutting Instance Manager down...");

  # Ignoring SIGCHLD so that all children could rest in peace.

  start_reap_all();

  # Send SIGTERM to IM-main.

  if (defined $instance_manager->{'pid'})
  {
    mtr_debug("IM-main pid: $instance_manager->{pid}.");
    mtr_debug("Stopping IM-main...");

    mtr_im_kill_process([ $instance_manager->{'pid'} ], 'TERM', 10, 1);
  }
  else
  {
    mtr_debug("IM-main pid: n/a.");
  }

  # If IM-angel was alive, wait for it to die.

  if (defined $instance_manager->{'angel_pid'})
  {
    mtr_debug("IM-angel pid: $instance_manager->{'angel_pid'}.");
    mtr_debug("Waiting for IM-angel to die...");

    my $total_attempts= 10;

    for (my $cur_attempt=1; $cur_attempt <= $total_attempts; ++$cur_attempt)
    {
      unless (kill (0, $instance_manager->{'angel_pid'}))
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

  mtr_im_load_pids($instance_manager);
}

###########################################################################

sub mtr_im_check_alive($) {
  my $instance_manager= shift;

  mtr_debug("Checking whether IM-components are alive...");

  return 1 if mtr_im_check_main_alive($instance_manager);

  return 1 if mtr_im_check_angel_alive($instance_manager);

  return 1 if mtr_im_check_mysqlds_alive($instance_manager);

  return 0;
}

###########################################################################

sub mtr_im_check_main_alive($) {
  my $instance_manager= shift;

  # Check that the process, that we know to be IM's, is dead.

  if (defined $instance_manager->{'pid'})
  {
    if (kill (0, $instance_manager->{'pid'}))
    {
      mtr_debug("IM-main (PID: $instance_manager->{pid}) is alive.");
      return 1;
    }
    else
    {
      mtr_debug("IM-main (PID: $instance_manager->{pid}) is dead.");
    }
  }
  else
  {
    mtr_debug("No PID file for IM-main.");
  }

  # Check that IM does not accept client connections.

  if (mtr_ping_mysqld_server($instance_manager->{'port'}))
  {
    mtr_debug("IM-main (port: $instance_manager->{port}) " .
              "is accepting connections.");

    mtr_im_errlog("IM-main is accepting connections on port " .
                  "$instance_manager->{port}, but there is no " .
                  "process information.");
    return 1;
  }
  else
  {
    mtr_debug("IM-main (port: $instance_manager->{port}) " .
              "does not accept connections.");
    return 0;
  }
}

###########################################################################

sub mtr_im_check_angel_alive($) {
  my $instance_manager= shift;

  # Check that the process, that we know to be the Angel, is dead.

  if (defined $instance_manager->{'angel_pid'})
  {
    if (kill (0, $instance_manager->{'angel_pid'}))
    {
      mtr_debug("IM-angel (PID: $instance_manager->{angel_pid}) is alive.");
      return 1;
    }
    else
    {
      mtr_debug("IM-angel (PID: $instance_manager->{angel_pid}) is dead.");
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
  my $instance_manager= shift;

  mtr_debug("Checking for IM-guarded mysqld instances...");

  my $instances = $instance_manager->{'instances'};

  for (my $idx= 0; $idx < 2; ++$idx)
  {
    mtr_debug("Checking mysqld[$idx]...");

    return 1
      if mtr_im_check_mysqld_alive($instance_manager, $instances->[$idx]);
  }
}

###########################################################################

sub mtr_im_check_mysqld_alive($$) {
  my $instance_manager= shift;
  my $mysqld_instance= shift;

  # Check that the process is dead.

  if (defined $instance_manager->{'pid'})
  {
    if (kill (0, $instance_manager->{'pid'}))
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

  if (mtr_ping_mysqld_server($mysqld_instance->{'port'}))
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
  my $instance_manager= shift;

  mtr_im_rm_file($instance_manager->{'path_pid'});
  mtr_im_rm_file($instance_manager->{'path_sock'});

  mtr_im_rm_file($instance_manager->{'path_angel_pid'});

  for (my $idx= 0; $idx < 2; ++$idx)
  {
    mtr_im_rm_file($instance_manager->{'instances'}->[$idx]->{'path_pid'});
    mtr_im_rm_file($instance_manager->{'instances'}->[$idx]->{'path_sock'});
  }
}

###########################################################################

sub mtr_im_rm_file($)
{
  my $file_path= shift;

  if (-f $file_path)
  {
    mtr_debug("Removing '$file_path'...");

    mtr_warning("Can not remove '$file_path'.")
      unless unlink($file_path);
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
  my $instance_manager= shift;

  # Re-load PIDs. That can be useful because some processes could have been
  # restarted.

  mtr_im_load_pids($instance_manager);

  # Ignoring SIGCHLD so that all children could rest in peace.

  start_reap_all();

  # Kill IM-angel first of all.

  if (defined $instance_manager->{'angel_pid'})
  {
    mtr_debug("Killing IM-angel (PID: $instance_manager->{angel_pid})...");
    mtr_im_kill_process([ $instance_manager->{'angel_pid'} ], 'KILL', 10, 1)
  }
  else
  {
    mtr_debug("IM-angel is dead.");
  }

  # Re-load PIDs again.

  mtr_im_load_pids($instance_manager);

  # Kill IM-main.
  
  if (defined $instance_manager->{'pid'})
  {
    mtr_debug("Killing IM-main (PID: $instance_manager->pid})...");
    mtr_im_kill_process([ $instance_manager->{'pid'} ], 'KILL', 10, 1);
  }
  else
  {
    mtr_debug("IM-main is dead.");
  }

  # Re-load PIDs again.

  mtr_im_load_pids($instance_manager);

  # Kill guarded mysqld instances.

  my @mysqld_pids;

  mtr_debug("Collecting PIDs of mysqld instances to kill...");

  for (my $idx= 0; $idx < 2; ++$idx)
  {
    my $pid= $instance_manager->{'instances'}->[$idx]->{'pid'};

    next unless defined $pid;

    mtr_debug("  - IM-guarded mysqld[$idx] PID: $pid.");

    push (@mysqld_pids, $pid);
  }

  if (scalar @mysqld_pids > 0)
  {
    mtr_debug("Killing IM-guarded mysqld instances...");
    mtr_im_kill_process(\@mysqld_pids, 'KILL', 10, 1);
  }

  # That's all.

  stop_reap_all();
}

##############################################################################

sub mtr_im_wait_for_connection($$$) {
  my $instance_manager= shift;
  my $total_attempts= shift;
  my $connect_timeout= shift;

  mtr_debug("Waiting for IM on port $instance_manager->{port} " .
            "to start accepting connections...");

  for (my $cur_attempt= 1; $cur_attempt <= $total_attempts; ++$cur_attempt)
  {
    mtr_debug("Trying to connect to IM ($cur_attempt of $total_attempts)...");

    if (mtr_ping_mysqld_server($instance_manager->{'port'}))
    {
      mtr_debug("IM is accepting connections " .
                "on port $instance_manager->{port}.");
      return 1;
    }

    mtr_debug("Sleeping $connect_timeout...");
    sleep($connect_timeout);
  }

  mtr_debug("IM does not accept connections " .
            "on port $instance_manager->{port} after " .
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

  for (my $cur_attempt= 1; $cur_attempt <= $total_attempts; ++$cur_attempt)
  {
    mtr_debug("Trying to connect to mysqld " .
              "($cur_attempt of $total_attempts)...");

    if (mtr_ping_mysqld_server($mysqld->{'port'}))
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

sub mtr_im_start($$) {
  my $instance_manager = shift;
  my $opts = shift;

  mtr_debug("Starting Instance Manager...");

  my $args;
  mtr_init_args(\$args);
  mtr_add_arg($args, "--defaults-file=%s",
              $instance_manager->{'defaults_file'});

  foreach my $opt (@{$opts})
  {
    mtr_add_arg($args, $opt);
  }

  $instance_manager->{'pid'} =
    mtr_spawn(
      $::exe_im,                        # path to the executable
      $args,                            # cmd-line args
      '',                               # stdin
      $instance_manager->{'path_log'},  # stdout
      $instance_manager->{'path_err'},  # stderr
      '',                               # pid file path (not used)
      { append_log_file => 1 }          # append log files
      );

  if ( ! $instance_manager->{'pid'} )
  {
    mtr_report('Could not start Instance Manager');
    return;
  }

  # Instance Manager can be run in daemon mode. In this case, it creates
  # several processes and the parent process, created by mtr_spawn(), exits just
  # after start. So, we have to obtain Instance Manager PID from the PID file.

  if ( ! sleep_until_file_created(
                                  $instance_manager->{'path_pid'},
                                  $instance_manager->{'start_timeout'},
                                  -1)) # real PID is still unknown
  {
    mtr_report("Instance Manager PID file is missing");
    return;
  }

  $instance_manager->{'pid'} =
    mtr_get_pid_from_file($instance_manager->{'path_pid'});

  mtr_debug("Instance Manager started. PID: $instance_manager->{pid}.");

  # Wait until we can connect to IM.

  my $IM_CONNECT_TIMEOUT= 30;

  unless (mtr_im_wait_for_connection($instance_manager,
                                     $IM_CONNECT_TIMEOUT, 1))
  {
    mtr_debug("Can not connect to Instance Manager " .
              "in $IM_CONNECT_TIMEOUT seconds after start.");
    mtr_debug("Aborting test suite...");

    mtr_kill_leftovers();

    mtr_error("Can not connect to Instance Manager " .
              "in $IM_CONNECT_TIMEOUT seconds after start.");
  }

  # Wait until we can connect to guarded mysqld-instances
  # (in other words -- wait for IM to start guarded instances).

  for (my $idx= 0; $idx < 2; ++$idx)
  {
    my $mysqld= $instance_manager->{'instances'}->[$idx];

    next if exists $mysqld->{'nonguarded'};

    mtr_debug("Waiting for mysqld[$idx] to start...");

    unless (mtr_im_wait_for_mysqld($mysqld, 30, 1))
    {
      mtr_debug("Can not connect to mysqld[$idx] " .
                "in $IM_CONNECT_TIMEOUT seconds after start.");
      mtr_debug("Aborting test suite...");

      mtr_kill_leftovers();

      mtr_error("Can not connect to mysqld[$idx] " .
                "in $IM_CONNECT_TIMEOUT seconds after start.");
    }

    mtr_debug("mysqld[$idx] started.");
  }

  mtr_debug("Instance Manager started.");
}

##############################################################################

sub mtr_im_stop($) {
  my $instance_manager= shift;

  mtr_debug("Stopping Instance Manager...");

  # Try graceful shutdown.

  mtr_im_terminate($instance_manager);

  # Check that all processes died.

  unless (mtr_im_check_alive($instance_manager))
  {
    mtr_debug("Instance Manager has been stopped successfully.");
    mtr_im_cleanup($instance_manager);
    return 1;
  }

  # Instance Manager don't want to die. We should kill it.

  mtr_im_errlog("Instance Manager did not shutdown gracefully.");

  mtr_im_kill($instance_manager);

  # Check again that all IM-related processes have been killed.

  my $im_is_alive= mtr_im_check_alive($instance_manager);

  mtr_im_cleanup($instance_manager);

  if ($im_is_alive)
  {
    mtr_error("Can not kill Instance Manager or its children.");
    return 0;
  }

  mtr_debug("Instance Manager has been killed successfully.");
  return 1;
}

###########################################################################

1;
