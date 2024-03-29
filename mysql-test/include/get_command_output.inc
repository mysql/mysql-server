# ==== Purpose ====
#
# Execute a command, and capture stdout in a mysqltest variable.
#
# ==== Usage ====
#
# --let $command= COMMAND
# --source include/get_command_output.inc
# --echo output was: $result

--let $include_filename= get_command_output.inc
--source include/begin_include_file.inc

if (!$command)
{
  --die !!!ERROR IN TEST: set $command first
}

--let $read_from_file= `SELECT UUID()`
--let $read_from_file= $MYSQLTEST_VARDIR/tmp/_command_output_$read_from_file

--exec $command > $read_from_file

--source include/read_file_to_var.inc

--let $read_from_file=

--let $include_filename= get_command_output.inc
--source include/end_include_file.inc
