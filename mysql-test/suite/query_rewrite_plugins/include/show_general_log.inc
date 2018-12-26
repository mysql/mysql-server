CREATE TABLE test_log ( line TEXT );

--replace_result $MYSQLTEST_VARDIR ...
eval LOAD DATA LOCAL INFILE '$MYSQLTEST_VARDIR/log/rewrite_general.log'
     INTO TABLE test_log FIELDS TERMINATED BY '\n'
     IGNORE 3 LINES;

SELECT substring_index( line, ' ', -1 ) AS incoming_query
FROM test_log
WHERE line LIKE ( '%SELECT%' );

DROP TABLE test_log;

SET GLOBAL general_log = DEFAULT;
SET GLOBAL general_log_file=  @old_general_log_file;
SET GLOBAL general_log=       @old_general_log;
SET GLOBAL log_output=        @old_log_output;
