# -*- cperl -*-

# This is a library file used by the Perl version of mysql-test-run,
# and is part of the translation of the Bourne shell script with the
# same name.

#use Carp qw(cluck);
use strict;

use POSIX ":sys_wait_h";

sub mtr_run ($$$$$$);
sub mtr_spawn ($$$$$$);
sub mtr_stop_mysqld_servers ($$);
sub mtr_kill_leftovers ();

# static in C
sub spawn_impl ($$$$$$$);

##############################################################################
#
#  Execute an external command
#
##############################################################################

# This function try to mimic the C version used in "netware/mysql_test_run.c"
# FIXME learn it to handle append mode as well, a "new" flag or a "append"

sub mtr_run ($$$$$$) {
  my $path=       shift;
  my $arg_list_t= shift;
  my $input=      shift;
  my $output=     shift;
  my $error=      shift;
  my $pid_file=   shift;

  return spawn_impl($path,$arg_list_t,1,$input,$output,$error,$pid_file);
}

sub mtr_spawn ($$$$$$) {
  my $path=       shift;
  my $arg_list_t= shift;
  my $input=      shift;
  my $output=     shift;
  my $error=      shift;
  my $pid_file=   shift;

  return spawn_impl($path,$arg_list_t,0,$input,$output,$error,$pid_file);
}


##############################################################################
#
#  If $join is set, we return the error code, else we return the PID
#
##############################################################################

sub spawn_impl ($$$$$$$) {
  my $path=       shift;
  my $arg_list_t= shift;
  my $join=       shift;
  my $input=      shift;
  my $output=     shift;
  my $error=      shift;
  my $pid_file=   shift;                 # FIXME

  if ( $::opt_script_debug )
  {
    print STDERR "\n";
    print STDERR "#### ", "-" x 78, "\n";
    print STDERR "#### ", "STDIN  $input\n" if $input;
    print STDERR "#### ", "STDOUT $output\n" if $output;
    print STDERR "#### ", "STDERR $error\n" if $error;
    if ( $join )
    {
      print STDERR "#### ", "RUN  ";
    }
    else
    {
      print STDERR "#### ", "SPAWN ";
    }
    print STDERR "$path ", join(" ",@$arg_list_t), "\n";
    print STDERR "#### ", "-" x 78, "\n";
  }

  my $pid= fork();
  if ( ! defined $pid )
  {
    mtr_error("$path ($pid) can't be forked");
  }

  if ( $pid )
  {
    # Parent, i.e. the main script
    if ( $join )
    {
      # We run a command and wait for the result
      # FIXME this need to be improved
      my $res= waitpid($pid,0);

      if ( $res == -1 )
      {
        mtr_error("$path ($pid) got lost somehow");
      }
      my $exit_value=  $? >> 8;
      my $signal_num=  $? & 127;
      my $dumped_core= $? & 128;
      if ( $signal_num )
      {
        mtr_error("$path ($pid) got signal $signal_num");
      }
      if ( $dumped_core )
      {
        mtr_error("$path ($pid) dumped core");
      }
      return $exit_value;
    }
    else
    {
      # We spawned a process we don't wait for
      return $pid;
    }
  }
  else
  {
    # Child, redirect output and exec
    # FIXME I tried POSIX::setsid() here to detach and, I hoped,
    # avoid zombies. But everything went wild, somehow the parent
    # became a deamon as well, and was hard to kill ;-)
    # Need to catch SIGCHLD and do waitpid or something instead......

    $SIG{INT}= 'DEFAULT';         # Parent do some stuff, we don't

    if ( $output )
    {
      if ( ! open(STDOUT,">",$output) )
      {
        mtr_error("can't redirect STDOUT to \"$output\": $!");
      }
    }
    if ( $error )
    {
      if ( $output eq $error )
      {
        if ( ! open(STDERR,">&STDOUT") )
        {
          mtr_error("can't dup STDOUT: $!");
        }
      }
      else
      {
        if ( ! open(STDERR,">",$error) )
        {
          mtr_error("can't redirect STDERR to \"$output\": $!");
        }
      }
    }
    if ( $input )
    {
      if ( ! open(STDIN,"<",$input) )
      {
        mtr_error("can't redirect STDIN to \"$input\": $!");
      }
    }
    exec($path,@$arg_list_t);
  }
}

##############################################################################
#
#  Kill processes left from previous runs
#
##############################################################################

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

  mtr_stop_mysqld_servers(\@args, 1);

  # We scan the "var/run/" directory for other process id's to kill
  my $rundir= "$::glob_mysql_test_dir/var/run"; # FIXME $path_run_dir or something

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
        if ( ! unlink($pidfile) )
        {
          mtr_error("can't remove $pidfile");
        }
        push(@pids, $pid);
      }
    }
    closedir(RUNDIR);

    start_reap_all();

    if ( $::glob_cygwin_perl )
    {
      # We have no (easy) way of knowing the Cygwin controlling
      # process, in the PID file we only have the Windows process id.
      system("kill -f " . join(" ",@pids)); # Hope for the best....
    }
    else
    {
      my $retries= 10;                    # 10 seconds
      do
      {
        kill(9, @pids);
      } while ( $retries-- and  kill(0, @pids) );

      if ( kill(0, @pids) )
      {
        mtr_error("can't kill processes " . join(" ", @pids));
      }
    }

    stop_reap_all();
  }
}

##############################################################################
#
#  Shut down mysqld servers
#
##############################################################################

# To speed things we kill servers in parallel.
# The argument is a list of 'pidfiles' and 'socketfiles'.
# We use the pidfiles and socketfiles to try to terminate the servers.
# This is not perfect, there could still be other server processes
# left.

# Force flag is to be set only for killing mysqld servers this script
# didn't create in this run, i.e. initial cleanup before we start working.
# If force flag is set, we try to kill all with mysqladmin, and
# give up if we have no PIDs.

# FIXME On some operating systems, $srv->{'pid'} and $srv->{'pidfile'}
#       will not be the same PID. We need to try to kill both I think.

sub mtr_stop_mysqld_servers ($$) {
  my $spec=  shift;
  my $force= shift;

  # ----------------------------------------------------------------------
  # If the process was not started from this file, we got no PID,
  # we try to find it in the PID file.
  # ----------------------------------------------------------------------

  my $any_pid= 0;                     # If we have any PIDs

  foreach my $srv ( @$spec )
  {
    if ( ! $srv->{'pid'} and -f $srv->{'pidfile'} )
    {
      $srv->{'pid'}= mtr_get_pid_from_file($srv->{'pidfile'});
    }
    if ( $srv->{'pid'} )
    {
      $any_pid= 1;
    }
  }

  # If the processes where started from this script, and we know
  # no PIDs, then we don't have to do anything.

  if ( ! $any_pid and ! $force )
  {
    # cluck "This is how we got here!";
    return;
  }

  # ----------------------------------------------------------------------
  # First try nice normal shutdown using 'mysqladmin'
  # ----------------------------------------------------------------------

  start_reap_all();                   # Don't require waitpid() of children

  foreach my $srv ( @$spec )
  {
    if ( -e $srv->{'sockfile'} or $srv->{'port'} )
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
      mtr_add_arg($args, "--connect_timeout=5");
      mtr_add_arg($args, "--shutdown_timeout=20");
      mtr_add_arg($args, "--protocol=tcp"); # FIXME new thing, will it help?!
      mtr_add_arg($args, "shutdown");
      # We don't wait for termination of mysqladmin
      mtr_spawn($::exe_mysqladmin, $args,
                "", $::path_manager_log, $::path_manager_log, "");
    }
  }

  # Wait for them all to remove their pid and socket file

 PIDSOCKFILEREMOVED:
  for (my $loop= $::opt_sleep_time_for_delete; $loop; $loop--)
  {
    my $pidsockfiles_left= 0;
    foreach my $srv ( @$spec )
    {
      if ( -e $srv->{'sockfile'} or -f $srv->{'pidfile'} )
      {
        $pidsockfiles_left++;          # Could be that pidfile is left
      }
    }
    if ( ! $pidsockfiles_left )
    {
      last PIDSOCKFILEREMOVED;
    }
    if ( $loop % 20 == 1 )
    {
      mtr_warning("Still processes alive after 10 seconds, retrying for $loop seconds...");
    }
    mtr_debug("Sleep for 1 second waiting for pid and socket file removal");
    sleep(1);                          # One second
  }

  # ----------------------------------------------------------------------
  # If no known PIDs, we have nothing more to try
  # ----------------------------------------------------------------------

  if ( ! $any_pid )
  {
    stop_reap_all();
    return;
  }

  # ----------------------------------------------------------------------
  # We may have killed all that left a socket, but we are not sure we got
  # them all killed. If we suspect it lives, try nice kill with SIG_TERM.
  # Note that for true Win32 processes, kill(0,$pid) will not return 1.
  # ----------------------------------------------------------------------

 SIGNAL:
  foreach my $sig (15,9)
  {
    my $process_left= 0;
    foreach my $srv ( @$spec )
    {
      if ( $srv->{'pid'} and
           ( -f $srv->{'pidfile'} or kill(0,$srv->{'pid'}) ) )
      {
        $process_left++;
        mtr_warning("process $srv->{'pid'} not cooperating, " .
                    "will send signal $sig to process");
        kill($sig,$srv->{'pid'});       # SIG_TERM
      }
      if ( ! $process_left )
      {
        last SIGNAL;
      }
    }
    mtr_debug("Sleep for 5 seconds waiting for processes to die");
    sleep(5);                           # We wait longer than usual
  }

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
            unlink($file);
            if ( -e $file )
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

  stop_reap_all();

  # FIXME We just assume they are all dead, we don't know....
}

sub start_reap_all {
  $SIG{CHLD}= 'IGNORE';                 # FIXME is this enough?
}

sub stop_reap_all {
  $SIG{CHLD}= 'DEFAULT';
}

##############################################################################
#
#  Wait for a file to be created
#
##############################################################################


sub sleep_until_file_created ($$) {
  my $pidfile= shift;
  my $timeout= shift;

  my $loop=  $timeout;
  while ( $loop-- )
  {
    if ( -r $pidfile )
    {
      return;
    }
    mtr_debug("Sleep for 1 second waiting for creation of $pidfile");

    if ( $loop % 20 == 1 )
    {
      mtr_warning("Waiting for $pidfile to be created, still trying for $loop seconds...");
    }

    sleep(1);
  }

  if ( ! -r $pidfile )
  {
    mtr_error("No $pidfile was created");
  }
}



1;
