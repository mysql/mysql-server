# -*- cperl -*-

# This is a library file used by the Perl version of mysql-test-run,
# and is part of the translation of the Bourne shell script with the
# same name.

use strict;

use POSIX ":sys_wait_h";

sub mtr_run ($$$$$$);
sub mtr_spawn ($$$$$$);
sub mtr_stop_servers ($);
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

  # FIXME really needing a PATH???
  # $ENV{'PATH'}= "/bin:/usr/bin:/usr/local/bin:/usr/bsd:/usr/X11R6/bin:/usr/openwin/bin:/usr/bin/X11:$ENV{'PATH'}";

  $ENV{'TZ'}=             "GMT-3";         # for UNIX_TIMESTAMP tests to work
  $ENV{'LC_COLLATE'}=     "C";
  $ENV{'MYSQL_TEST_DIR'}= $::glob_mysql_test_dir;
  $ENV{'MASTER_MYPORT'}=  $::opt_master_myport;
  $ENV{'SLAVE_MYPORT'}=   $::opt_slave_myport;
# $ENV{'MYSQL_TCP_PORT'}= '@MYSQL_TCP_PORT@'; # FIXME
  $ENV{'MYSQL_TCP_PORT'}= 3306;
  $ENV{'MASTER_MYSOCK'}=  $::master->[0]->{'path_mysock'};

  if ( $::opt_script_debug )
  {
    print STDERR "-" x 78, "\n";
    print STDERR "STDIN  $input\n" if $input;
    print STDERR "STDOUT $output\n" if $output;
    print STDERR "STDERR $error\n" if $error;
    print STDERR "DAEMON\n" if !$join;
    print STDERR "EXEC $path ", join(" ",@$arg_list_t), "\n";
    print STDERR "-" x 78, "\n";
  }

  my $pid= fork();

  if ( $pid )
  {
    # Parent, i.e. the main script
    if ( $join )
    {
      # We run a command and wait for the result
      # FIXME this need to be improved
      waitpid($pid,0);
      my $exit_value=  $? >> 8;
      my $signal_num=  $? & 127;
      my $dumped_core= $? & 128;
      if ( $signal_num )
      {
        die("spawn got signal $signal_num");
      }
      if ( $dumped_core )
      {
        die("spawn dumped core");
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
      open(STDOUT,">",$output) or die "Can't redirect STDOUT to \"$output\": $!";
    }
    if ( $error )
    {
      if ( $output eq $error )
      {
        open(STDERR,">&STDOUT") or die "Can't dup STDOUT: $!";
      }
      else
      {
        open(STDERR,">",$error) or die "Can't redirect STDERR to \"$output\": $!";
      }
    }
    if ( $input )
    {
      open(STDIN,"<",$input) or die "Can't redirect STDIN to \"$input\": $!";
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
#    if ( $::master->[$idx]->{'pid'} )
#    {
      push(@args,
           $::master->[$idx]->{'path_mypid'},
           $::master->[$idx]->{'path_mysock'},
         );
#    }
  }

  for ( my $idx; $idx < 3; $idx++ )
  {
#    if ( $::slave->[$idx]->{'pid'} )
#    {
      push(@args,
           $::slave->[$idx]->{'path_mypid'},
           $::slave->[$idx]->{'path_mysock'},
         );
#    }
  }

  mtr_stop_servers(\@args);

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

sub mtr_stop_servers ($) {
  my $spec= shift;

  # First try nice normal shutdown using 'mysqladmin'

  {
    my @args= @$spec;
    while ( @args )
    {
      my $pidfile=  shift @args;        # FIXME not used here....
      my $sockfile= shift @args;

      if ( -f $sockfile )
      {

        # FIXME wrong log.....
        # FIXME, stderr.....
        # Shutdown time must be high as slave may be in reconnect
        my $opts= 
          [
           "--no-defaults",
           "-uroot",
           "--socket=$sockfile",
           "--connect_timeout=5",
           "--shutdown_timeout=70",
           "shutdown",
         ];
        # We don't wait for termination of mysqladmin
        mtr_spawn($::exe_mysqladmin, $opts,
                  "", $::path_manager_log, $::path_manager_log, "");
      }
    }
  }

  # Wait for them all to remove their socket file

 SOCKREMOVED:
  for (my $loop= $::opt_sleep_time_for_delete; $loop; $loop--)
  {
    my $sockfiles_left= 0;
    my @args= @$spec;
    while ( @args )
    {
      my $pidfile=  shift @args;
      my $sockfile= shift @args;
      if ( -f $sockfile or -f $pidfile )
      {
        $sockfiles_left++;              # Could be that pidfile is left
      }
    }
    if ( ! $sockfiles_left )
    {
      last SOCKREMOVED;
    }
    if ( $loop > 1 )
    {
      sleep(1);                 # One second
    }
  }

  # We may have killed all that left a socket, but we are not sure we got
  # them all killed. We now check the PID file, if any

  # Try nice kill with SIG_TERM

  {
    my @args= @$spec;
    while ( @args )
    {
      my $pidfile=  shift @args;
      my $sockfile= shift @args;
      if (-f $pidfile)
      {
        my $pid= mtr_get_pid_from_file($pidfile);
        mtr_warning("process $pid not cooperating with mysqladmin, " .
                    "will send TERM signal to process");
        kill(15,$pid);          # SIG_TERM
      }
    }
  }

  # Wait for them all to die

  for (my $loop= $::opt_sleep_time_for_delete; $loop; $loop--)
  {
    my $pidfiles_left= 0;
    my @args= @$spec;
    while ( @args )
    {
      my $pidfile=  shift @args;
      my $sockfile= shift @args;
      if ( -f $pidfile )
      {
        $pidfiles_left++;
      }
    }
    if ( ! $pidfiles_left )
    {
      return;
    }
    if ( $loop > 1 )
    {
      sleep(1);                 # One second
    }
  }

  # Try hard kill with SIG_KILL

  {
    my @args= @$spec;
    while ( @args )
    {
      my $pidfile=  shift @args;
      my $sockfile= shift @args;
      if (-f $pidfile)
      {
        my $pid= mtr_get_pid_from_file($pidfile);
        mtr_warning("$pid did not die from TERM signal, ",
                    "will send KILL signal to process");
        kill(9,$pid);
      }
    }
  }

  # We check with Perl "kill 0" if process still exists

 PIDFILES:
  for (my $loop= $::opt_sleep_time_for_delete; $loop; $loop--)
  {
    my $not_terminated= 0;
    my @args= @$spec;
    while ( @args )
    {
      my $pidfile=  shift @args;
      my $sockfile= shift @args;
      if (-f $pidfile)
      {
        my $pid= mtr_get_pid_from_file($pidfile);
        if ( ! kill(0,$pid) )
        {
          $not_terminated++;
          mtr_warning("could't kill $pid");
        }
      }
    }
    if ( ! $not_terminated )
    {
      last PIDFILES;
    }
    if ( $loop > 1 )
    {
      sleep(1);                 # One second
    }
  }

  {
    my $pidfiles_left= 0;
    my @args= @$spec;
    while ( @args )
    {
      my $pidfile=  shift @args;
      my $sockfile= shift @args;
      if ( -f $pidfile )
      {
        if ( ! unlink($pidfile) )
        {
          $pidfiles_left++;
          mtr_warning("could't delete $pidfile");
        }
      }
    }
    if ( $pidfiles_left )
    {
      mtr_error("one or more pid files could not be deleted");
    }
  }

  # FIXME We just assume they are all dead, we don't know....
}


1;
