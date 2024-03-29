### t/log_tables_debug.test
#
# Log-related tests requiring a debug-build server.
#

# extra clean-up required due to Bug#38124, set to 1 when behavior has
# changed (see explanation in log_state.test)
let $fixed_bug38124 = 0;

--source include/have_debug.inc

# Several subtests modify global variables. Save the initial values only here,
# but reset to the initial values per subtest.
SET @old_log_output= @@global.log_output;
SET @old_general_log= @@global.general_log;
SET @old_general_log_file= @@global.general_log_file;
SET @old_slow_query_log= @@global.slow_query_log;
SET @old_slow_query_log_file= @@global.slow_query_log_file;


--echo #
--echo # Bug#45387 Information about statement id for prepared
--echo #           statements missed from general log
--echo #

let MYSQLD_DATADIR= `SELECT @@datadir`;

# set logging to our specific bug log to control the entries added
SET @@global.log_output = 'FILE,TABLE';
SET @@global.general_log = ON;
SET @@global.general_log_file = 'bug45387_general.log';

let CONN_ID= `SELECT CONNECTION_ID()`;
FLUSH LOGS;

# reset log settings
SET @@global.general_log = @old_general_log;
SET @@global.general_log_file = @old_general_log_file;

perl;
  # get the relevant info from the surrounding perl invocation
  $datadir= $ENV{'MYSQLD_DATADIR'};
  $conn_id= $ENV{'CONN_ID'};

  # loop through the log file looking for the stmt querying for conn id
  open(FILE, "$datadir/bug45387_general.log") or
    die("Unable to read log file $datadir/bug45387_general.log: $!\n");
  while(<FILE>) {
#   if (/\d{6}\s+\d+:\d+:\d+[ \t]+(\d+)[ \t]+Query[ \t]+SELECT CONNECTION_ID/) {
    if (/\d{4}-\d+-\d+T\d+:\d+:\d+\.\d{6}Z[ \t]+(\d+)[ \t]+Query[ \t]+SELECT CONNECTION_ID/) {
      $found= $1;
      break;
    }
  }

  # print the result
  if ($found == $conn_id) {
    print "Bug#45387: ID match.\n";
  } else {
    print "Bug#45387: Expected ID '$conn_id', found '$found' in log file.\n";
    print "Contents of log file:\n";
    seek(FILE, 0, 0);
    while($line= <FILE>) {
      print $line;
    }
  }

  close(FILE);
EOF

--remove_file $MYSQLD_DATADIR/bug45387_general.log

--echo End of 5.1 tests

--echo #
--echo # Bug#11748692: LOGGING TO SLOW_LOG TABLE FAILS WITH VERY SLOW QUERIES
--echo #

# While fatal issues can arise while trying to log to a tab (such as
# being unable to open or write to the table), show that we fail gracefully
# in other cases (e.g. if setting a field fails because the value for
# rows_examined is larger in the server than the data-type in the table
# can hold, we can still write the line and should in fact do so as the
# other information would still be correct and valuable). Below, we
# articifically max out the internal value to demonstrate the aforementioned
# behaviour.

SELECT @@session.long_query_time INTO @old_long_query_time;
SET GLOBAL slow_query_log = 1;
TRUNCATE mysql.slow_log;
--echo # this should get logged. 0 rows are examined.
SET SESSION long_query_time=0;
--echo # rows_examined should overflow without the patch, and max out with it.
SET @@session.debug = '+d,slow_log_table_max_rows_examined';
SET @@session.long_query_time = @old_long_query_time;
SET @@session.debug = '-d,slow_log_table_max_rows_examined';
SELECT rows_examined,db,query_time,lock_time,sql_text FROM mysql.slow_log WHERE rows_examined > 0;
# If the above overflows either TIME field, run "REPAIR TABLE mysql.slow_log;"

--echo End of 8.0 tests

--echo #
--echo # Cleanup
--echo #

# Reset global system variables to initial values if forgotten somewhere above.
SET global log_output = @old_log_output;
SET global general_log = @old_general_log;
SET global general_log_file = @old_general_log_file;
SET global slow_query_log = @old_slow_query_log;
SET global slow_query_log_file = @old_slow_query_log_file;
if(!$fixed_bug38124)
{
   --disable_query_log
   let $my_var = `SELECT @old_general_log_file`;
   eval SET @@global.general_log_file = '$my_var';
   let $my_var = `SELECT @old_slow_query_log_file`;
   eval SET @@global.slow_query_log_file = '$my_var';
   --enable_query_log
}
TRUNCATE TABLE mysql.general_log;
TRUNCATE TABLE mysql.slow_log;
