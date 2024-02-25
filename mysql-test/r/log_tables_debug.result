SET @old_log_output= @@global.log_output;
SET @old_general_log= @@global.general_log;
SET @old_general_log_file= @@global.general_log_file;
SET @old_slow_query_log= @@global.slow_query_log;
SET @old_slow_query_log_file= @@global.slow_query_log_file;
#
# Bug#45387 Information about statement id for prepared
#           statements missed from general log
#
SET @@global.log_output = 'FILE,TABLE';
SET @@global.general_log = ON;
SET @@global.general_log_file = 'bug45387_general.log';
FLUSH LOGS;
SET @@global.general_log = @old_general_log;
SET @@global.general_log_file = @old_general_log_file;
Bug#45387: ID match.
End of 5.1 tests
#
# Bug#11748692: LOGGING TO SLOW_LOG TABLE FAILS WITH VERY SLOW QUERIES
#
SELECT @@session.long_query_time INTO @old_long_query_time;
SET GLOBAL slow_query_log = 1;
TRUNCATE mysql.slow_log;
# this should get logged. 0 rows are examined.
SET SESSION long_query_time=0;
# rows_examined should overflow without the patch, and max out with it.
SET @@session.debug = '+d,slow_log_table_max_rows_examined';
SET @@session.long_query_time = @old_long_query_time;
SET @@session.debug = '-d,slow_log_table_max_rows_examined';
SELECT rows_examined,db,query_time,lock_time,sql_text FROM mysql.slow_log WHERE rows_examined > 0;
rows_examined	db	query_time	lock_time	sql_text
2147483647	test	838:59:59.000000	838:59:59.000000	SET @@session.debug = '+d,slow_log_table_max_rows_examined'
End of 8.0 tests
#
# Cleanup
#
SET global log_output = @old_log_output;
SET global general_log = @old_general_log;
SET global general_log_file = @old_general_log_file;
SET global slow_query_log = @old_slow_query_log;
SET global slow_query_log_file = @old_slow_query_log_file;
TRUNCATE TABLE mysql.general_log;
TRUNCATE TABLE mysql.slow_log;
