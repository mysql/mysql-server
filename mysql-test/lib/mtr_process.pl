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

  # First, kill all masters and slaves that would conflict with
  # this run. Make sure to remove the PID file, if any.

  my @args;

  for ( my $idx; $idx < 2; $idx++ )
  {
    push(@args,{
                pid      => 0,          # We don't know the PID
                pidfile  => $::master->[$idx]->{'path_mypid'},
                sockfile => $::master->[$idx]->{'path_mysock'},
                port     => $::master->[$idx]->{'path_myport'},
               });
  }

  for ( my $idx; $idx < 3; $idx++ )
  {
    push(@args,{
                pid       => 0,         # We don't know the PID
                pidfile   => $::slave->[$idx]->{'path_mypid'},
                sockfile  => $::slave->[$idx]->{'path_mysock'},
                port      => $::slave->[$idx]->{'path_myport'},
               });
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
          mtr_debug("Sleep 1 second waiting for processes to die");
          sleep(1)                      # Wait one second
        } while ( $retries-- and  kill(0, @pids) );

        if ( kill(0, @pids) )           # Check if some left
        {
          # FIXME maybe just mtr_warning() ?
          mtr_error("can't kill process(es) " . join(" ", @pids));
        }
      }
    }
  }

  # We may have failed everything, bug we now check again if we have
  # the listen ports free to use, and if they are free, just go for it.

  foreach my $srv ( @args )
  {
    if ( mtr_ping_mysqld_server($srv->{'port'}, $srv->{'sockfile'}) )
    {
      mtr_error("can't kill old mysqld holding port $srv->{'port'}");
    }
  }
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

  foreach my $srv ( @$spec )
  {
    if ( mtr_ping_mysqld_server($srv->{'port'}, $srv->{'sockfile'}) )
    {
      push(@to_kill_specs, $srv);
    }
  }


  foreach my $srv ( @to_kill_specs )
  {
    # FIXME wrong log.....
    # FIXME, stderr.....
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
    # We don't wait for termination of mysqladmin
    my $pid= mtr_spawn($::exe_mysqladmin, $args,
                       "", $::path_manager_log, $::path_manager_log, "",
                       { append_log_file => 1 });
    $mysql_admin_pids{$pid}= 1;
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
 TIME:
  while ( $timeout-- )
  {
    foreach my $srv ( @to_kill_specs )
    {
      $res= 1;                          # We are optimistic
      if ( mtr_ping_mysqld_server($srv->{'port'}, $srv->{'sockfile'}) )
      {
        mtr_debug("Sleep 1 second waiting for processes to stop using port");
        sleep(1);                       # One second
        $res= 0;
        next TIME;
      }
    }
    last;                               # If we got here, we are done
  }

  $timeout or mtr_debug("At least one server is still listening to its port");

  sleep(5) if $::glob_win32;            # FIXME next startup fails if no sleep

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
  $SIG{CHLD}= 'IGNORE';                 # FIXME is this enough?
}

sub stop_reap_all {
  $SIG{CHLD}= 'DEFAULT';
}

sub mtr_ping_mysqld_server () {
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
  if ( connect(SOCK, $paddr) )
  {
    close(SOCK);                        # FIXME check error?
    return 1;
  }
  else
  {
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

  for ( my $loop= 1; $loop <= $timeout; $loop++ )
  {
    if ( -r $pidfile )
    {
      return $pid;
    }

    # Check if it died after the fork() was successful 
    if ( $pid > 0 && waitpid($pid,&WNOHANG) == $pid )
    {
      return 0;
    }

    mtr_debug("Sleep 1 second waiting for creation of $pidfile");

    if ( $loop % 60 == 0 )
    {
      my $left= $timeout - $loop;
      mtr_warning("Waited $loop seconds for $pidfile to be created, " .
                  "still waiting for $left seconds...");
    }

    sleep(1);
  }

  return 0;
}


sub mtr_kill_processes ($) {
  my $pids = shift;

  foreach my $sig (15,9)
  {
    my $retries= 20;                    # FIXME 20 seconds, this is silly!
    kill($sig, @{$pids});
    while ( $retries-- and  kill(0, @{$pids}) )
    {
      mtr_debug("Sleep 1 second waiting for processes to die");
      sleep(1)                      # Wait one second
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
  kill('HUP', -$$);
  sleep 2;
  exit($code);
}

1;
