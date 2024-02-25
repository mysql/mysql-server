CREATE PROCEDURE check_memory(IN psi_key CHAR(64))
BEGIN
SELECT ROUND((SUM_NUMBER_OF_BYTES_ALLOC-SUM_NUMBER_OF_BYTES_FREE)/1024.0)
FROM performance_schema.memory_summary_global_by_event_name
WHERE event_name = CONCAT("memory/innodb/", psi_key);
END;|
#
# Test memory allocations related to innodb_log_buffer_size changes
#
CALL check_memory('log_buffer_memory');
ROUND((SUM_NUMBER_OF_BYTES_ALLOC-SUM_NUMBER_OF_BYTES_FREE)/1024.0)
16385
SET GLOBAL innodb_log_buffer_size = 32*1024*1024;
CALL check_memory('log_buffer_memory');
ROUND((SUM_NUMBER_OF_BYTES_ALLOC-SUM_NUMBER_OF_BYTES_FREE)/1024.0)
32769
SET GLOBAL innodb_log_buffer_size = 64*1024*1024;
CALL check_memory('log_buffer_memory');
ROUND((SUM_NUMBER_OF_BYTES_ALLOC-SUM_NUMBER_OF_BYTES_FREE)/1024.0)
65537
SET GLOBAL innodb_log_buffer_size = 4*1024*1024;
CALL check_memory('log_buffer_memory');
ROUND((SUM_NUMBER_OF_BYTES_ALLOC-SUM_NUMBER_OF_BYTES_FREE)/1024.0)
4097
SET GLOBAL innodb_log_buffer_size = 16777216;
CALL check_memory('log_buffer_memory');
ROUND((SUM_NUMBER_OF_BYTES_ALLOC-SUM_NUMBER_OF_BYTES_FREE)/1024.0)
16385
DROP PROCEDURE check_memory;
