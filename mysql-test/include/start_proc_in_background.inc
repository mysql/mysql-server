# ==== Purpose ====
#
# Start process in background. Optionally store its PID in the file
#
# This functionality cannot be achieved by mysqltest built-in --exec_in_background
# because we need to start it like:
# cmd param1 param2 > output.log & echo $! > cmd.pid
#
# The way to pass $! through Perl to mysqltest binary is to escape $! like \$!
# This will prevent attemption to resolve $! variable on Perl layer,
# but will pass down \$! to mysqltest.
# mysqltest handles \$! literally (recognizes that $ has been escaped
# and adds escape character to the constructed command, so we end up with:
# cmd param1 param2 > output.log & echo \$! > cmd.pid
#
# ==== Usage ====
#
# --let $command = process_to_be_executed
# [--let $command_opt = opt1 opt2 ...]
# [--let $output_file = output_file]
# [--let $pid_file = pid_file]
# [--let $redirect_stderr = 0 | 1 ]
# --source include/start_proc_in_backcground.inc
#
# Parameters:
#   $command
#     Process to be executed in background.
#
#   $command_opt
#     Options to be passed to the process via command line at its startup.
#
#   $output_file
#     Redirect process output to this file.
#
#   $pid_file
#     Store started process PID in this file.
#


if (!$command)
{
  --die ERROR IN TEST: 'command' parameter not specified
}

--let $line = $command $command_opt

if ($output_file)
{
  if ($redirect_stderr == 1)
  {
    --let $line = $line 2> $output_file
  }
  if ($redirect_stderr == 0)
  {
    --let $line = $line > $output_file
  }
}

--let $line = $line &

if ($pid_file)
{
  --let $line = $line echo \$! > $pid_file
}

--let _LINE = $line
--perl
   my $cmd= $ENV{'_LINE'};
   system("$cmd");
EOF

