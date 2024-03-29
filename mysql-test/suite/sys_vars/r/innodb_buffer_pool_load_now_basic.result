SET @orig = @@global.innodb_buffer_pool_load_now;
SELECT @orig;
@orig
0
SET GLOBAL innodb_buffer_pool_dump_now = ON;
SET GLOBAL innodb_fast_shutdown=0;
# restart
SET GLOBAL innodb_buffer_pool_load_now = ON;
SELECT variable_value
FROM performance_schema.global_status
WHERE LOWER(variable_name) = 'innodb_buffer_pool_load_status';
variable_value
Buffer pool load completed or still running
