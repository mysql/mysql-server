# ==== Purpose ====
#
# The sql statement containing REPLACE_STRING_VAL string is replaced by given
# list of parameters and executed.
#
# ==== Usage ====
#
# --let $execute_list= <param1>,<param2>,<param3>
# --let $execute_sql_statement= SELECT REPLACE_STRING_VAL
# --source include/execute_list_command.inc
#
# Parameters:
#
#   $execute_list
#     The REPLACE_STRING_VAL string in execute_sql_statement statement is
#     replaced by list of parameters and executed.
#
#   $execute_sql_statement
#     The sql statement containing REPLACE_STRING_VAL string is replaced by
#     given list of parameters and executed.


--let $result_file = $MYSQLTEST_VARDIR/tmp/exec_list_tmp.inc
--let _RESULT_FILE = $result_file
--let _STATEMENT=$execute_sql_statement
--let $_list= $execute_list

while ($_list)
{
  # Set $_rpl_diff_server_i to the first number in the list
  --let $_list_i= `SELECT SUBSTRING_INDEX('$_list', ',', 1)`
  # Remove $_rpl_diff_server_i from the list
  --let $_list= `SELECT SUBSTRING('$_list', LENGTH('$_list_i') + 2)`

  --let _REPLACE_STRING_VAL=$_list_i

--perl
  use strict;
  my $_statement= $ENV{'_STATEMENT'} or die "execute_sql_statement is not set";
  my $_replace= $ENV{'_REPLACE_STRING_VAL'} or die "execute_list is not set properly";
  my $_search = "REPLACE_STRING_VAL";
  my $outfile = $ENV{'_RESULT_FILE'};

  $_statement =~ s/$_search/$_replace/;

  open(FILE, ">", $outfile) or die "Error $? opening $outfile: $!";
  print FILE $_statement or die "Error $? writing to $outfile: $!";
  close FILE or die "Error $? closing $outfile: $!";
  chmod 0777, $outfile;
EOF

  --let $exec_sql_string = `SELECT LOAD_FILE('$result_file')`
  --eval $exec_sql_string
}

--remove_file $result_file
