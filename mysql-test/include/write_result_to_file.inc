# ==== Purpose ====
#
# Execute a statement and write the result to a file.  This is useful
# if the output needs more advanced parsing than can be done by
# mysqltest commands.
#
# ==== Usage ====
#
# --let $statement= <STATEMENT>
# --let $output_file= {<FILE>|GENERATE}
# [--let $server_number= <NUMBER>]
# [--let $dont_print_statement= 1]
# --source include/write_result_to_file.inc
#
# Parameters:
#   $statement
#     The statement to execute.
#
#   $server_number
#     1 for the master, 2 for the slave, 3 for next server, etc.
#     If omitted, uses current connection.
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

--let _WRTF_SERVER_NUMBER= $server_number
if (!$server_number)
{
  --let _WRTF_SERVER_NUMBER= `SELECT 1 + @@PORT - $MASTER_MYPORT`
}

--let $_write_result_msg= [server=$_WRTF_SERVER_NUMBER]
if (!$dont_print_statement)
{
  --let $_write_result_msg= [server=$_WRTF_SERVER_NUMBER statement=$statement]
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

perl;
  my $stmt= $ENV{'_WRTF_STATEMENT'};
  my $server_number= $ENV{'_WRTF_SERVER_NUMBER'};
  my $mysql = $ENV{'MYSQL'};
  $mysql =~ s/( --|$)/ --defaults-group-suffix=.$server_number$1/;
  my $outfile = $ENV{'_WRTF_OUTPUT_FILE'};
  open MYSQL, "| $mysql > $outfile" or die "Failed to open pipe to client on server '$server_number': $!";
  print MYSQL $stmt, ';' or die "Error printing statement to MYSQL pipe: $!";
  close MYSQL or die "Error closing MYSQL pipe: $!";
EOF

--let $include_filename= write_result_to_file.inc [$write_result_msg]
--source include/end_include_file.inc
