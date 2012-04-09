#
# Check if the binaries are compiled with error insert support
# by running ndb_mgm and check if it support the command "all error" which
# is only available when error insert is suppoerted.
#

--disable_query_log
let $dump_file = $MYSQLTEST_VARDIR/tmp/ndb_mgm_error.txt;

# Dump the output of ndb_mgm "all error 0" to file
--error 0,255
--exec $NDB_MGM --no-defaults --ndb-connectstring="$NDB_CONNECTSTRING" -e "all error 0" > $dump_file

# Load the file into a temporary table
CREATE TEMPORARY TABLE t37 (a varchar(512)) ENGINE = HEAP;

# needed for embedded
--chmod 0777 $dump_file
--replace_result $dump_file DUMP_FILE
eval LOAD DATA INFILE '$dump_file' INTO TABLE t37;
--remove_file $dump_file

# Query the temporary table
let $invalid_command_error =
  `SELECT count(*) FROM t37 WHERE a REGEXP 'Invalid command: error'`;

DROP TABLE t37;

if ($invalid_command_error)
{
  skip No error insert support;
}

--enable_query_log
