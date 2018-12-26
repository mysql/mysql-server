#
# Include file that loads the error log into a table.
#

let $error_log_table_name=error_log;
let $log_error=`select @@log_error`;

disable_warnings;
eval CREATE TABLE IF NOT EXISTS $error_log_table_name ( line TEXT );
eval DELETE FROM $error_log_table_name;
enable_warnings;

replace_result $log_error ERROR_LOG;
eval LOAD DATA LOCAL INFILE '$log_error'
INTO TABLE $error_log_table_name FIELDS TERMINATED BY '\t';

# Fix line terminators for Windows.
eval UPDATE $error_log_table_name SET line = replace ( line, '\r', '' );

--disable_query_log
--disable_warnings
DROP FUNCTION IF EXISTS search_error_log;
--enable_warnings

DELIMITER //;

eval
CREATE FUNCTION search_error_log( level VARCHAR(10), pattern VARCHAR(100) )
RETURNS VARCHAR(400)
BEGIN
  SELECT DISTINCT substring( line FROM instr(line, level) ) INTO @res
  FROM $error_log_table_name WHERE line LIKE pattern;
  RETURN @res;
END//

DELIMITER ;//

--enable_query_log
