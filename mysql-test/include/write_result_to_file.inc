# ==== Purpose ====
#
# Execute a statement and write the result to a file.  This is useful
# if the output needs more advanced parsing than can be done by
# mysqltest commands.
#
# The statement is sent to mysqld on current connection using
# the mysql command line client.
#
# ==== Usage ====
#
# --let $statement= <STATEMENT>
# --let $output_file= {<FILE>|GENERATE}
# [--let $dont_print_statement= 1]
# [--let $allow_error= 1]
# [--let $append= 1]
# --source include/write_result_to_file.inc
#
# Parameters:
#   $statement
#     The statement to execute.
#
#   $output_file
#     Name of file to write. If omitted, generates a new filename and
#     stores the name both in the mysqltest variable $output_file and
#     in the environment variable $OUTPUT_FILE.
#
#   $dont_print_statement
#     By default, the statement is echoed to the result log.  If the
#     statement contains non-deterministic output, set this variable
#     to suppress it.
#
#   $allow_error
#     By default, this script causes the test to fail if the statement
#     generates an error.  If $allow_error is set, errors are ignored.
#
#   $append
#     By default, any existing file is overwritten. If $append is
#     specified, and the file exists, it appends to the file.

# Get the port and socket used by mysqld on current connection
--let _WRTF_SERVER_PORT= `SELECT @@PORT`
--let _WRTF_SERVER_SOCKET= `SELECT @@SOCKET`

--let $_write_result_msg= [connection=$CURRENT_CONNECTION]
if (!$dont_print_statement)
{
  --let $_write_result_msg= [connection=$CURRENT_CONNECTION statement=$statement]
}

--let $include_filename= write_result_to_file.inc $_write_result_msg
--source include/begin_include_file.inc

if ($statement == '')
{
  --die !!!ERROR IN TEST: mysqltest variable 'statement' not set in write_result_to_file.inc
}
--let _WRTF_STATEMENT= $statement

if (!$output_file)
{
  --die !!!ERROR IN TEST: mysqltest variable 'output_file' not set in write_result_to_file.inc
}
if ($output_file == GENERATE)
{
  --let $output_file= `SELECT UUID()`
  --let $output_file= $MYSQLTEST_VARDIR/tmp/_stmt_file_$output_file
}
--let _WRTF_OUTPUT_FILE= $output_file

if ($allow_error)
{
  --let _WRTF_ALLOW_ERROR= 1
}
if (!$allow_error)
{
  --let _WRTF_ALLOW_ERROR= 0
}
if ($append)
{
  --let _WRTF_APPEND= 1
}
if (!$append)
{
  --let _WRTF_APPEND= 0
}

perl;
  use strict;
  my $stmt= $ENV{'_WRTF_STATEMENT'};
  # Connecting mysql to same mysqld as current connectiona
  # by overriding port and socket
  my $mysql= $ENV{'MYSQL'};
  my $server_port= $ENV{'_WRTF_SERVER_PORT'};
  my $server_socket= $ENV{'_WRTF_SERVER_SOCKET'};
  my $redirection_type= $ENV{'_WRTF_APPEND'} ? '>>' : '>';
  $mysql .= " --port=$server_port --socket=$server_socket";
  my $outfile = $ENV{'_WRTF_OUTPUT_FILE'};
  open MYSQL, "| $mysql $redirection_type $outfile" or die "Error $? opening MYSQL pipe '| $mysql > $outfile': $!";
  print MYSQL $stmt, ';' or die "Error $? printing statement '$stmt' to MYSQL pipe '| $mysql > $outfile': $!";
  close MYSQL or $ENV{'_WRTF_ALLOW_ERROR'} or die "Error $? closing MYSQL pipe '| $mysql > $outfile' reading '$stmt': $!";
EOF

--let $include_filename= write_result_to_file.inc [$write_result_msg]
--source include/end_include_file.inc
