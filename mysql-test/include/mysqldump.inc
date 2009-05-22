################################################################################
# mysqldump.inc
#
# SUMMARY: include file to facilitate testing the quality of mysqldump output
#
# INPUTS:  Two variables:
#          $table_name - the name of the table that was dumped
#          $mysqldumpfile - the name of the file that captured mysqldump output
#
# OUTPUTS: minor echo data:
#          We 'echo' some stage information to the .result file:
#          'altering original table', 'restoring from dumpfile', 'comparing'
#
# OTHER FILES:  We use include/diff_tables.inc to compare the original, renamed
#               table with the 'restored' one. 
#
# DESCRIPTION: This file works by being fed the name of the original table
#              and a mysqldump output file.  The original table is then renamed
#              to <table_name>_orig, the mysqldump file is used to recreate the
#              table, then diff_tables.inc is called to compare them.
#
# LIMITATIONS:  Does *NOT* work with xml output!
#
# AUTHOR: pcrews 2009-05-21
#                Bug#40465 mysqldump.test does no checking of dump or restore
#
# LAST CHANGE: 2009-05-21 
#
################################################################################

--echo # Begin testing mysqldump output + restore
--echo # Create 'original table name - <table>_orig
# NOTE: We use SET then let as query_get_value has issues with the extra commas
#       used in the CONCAT statement.
eval SET @orig_table_name = CONCAT('$table_name', '_orig');
let $orig_table_name = query_get_value(SELECT @orig_table_name,@orig_table_name,1);
--echo # Rename original table
eval ALTER TABLE $table_name RENAME to $orig_table_name;
--echo # Recreate table from mysqldump output
--exec $MYSQL test < $mysqldumpfile
--echo # Compare original and recreated tables
--echo # Recreated table: $table_name
--echo # Original table: $orig_table_name
let $diff_table_1 = $table_name;
let $diff_table_2 = $orig_table_name;
--source include/diff_tables.inc
--echo # Cleanup
--remove_file $mysqldumpfile
eval DROP TABLE $table_name, $orig_table_name;

