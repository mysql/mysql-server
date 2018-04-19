SET @orig = @@global.innodb_buffer_pool_filename;
SELECT @orig;
@orig
ib_buffer_pool
SELECT variable_value
FROM performance_schema.global_status
WHERE LOWER(variable_name) = 'innodb_buffer_pool_dump_status';
variable_value
Dumping of buffer pool not started
SET GLOBAL innodb_buffer_pool_filename = 'innodb_foobar_dump';
SET GLOBAL innodb_buffer_pool_dump_now = ON;
SET GLOBAL innodb_buffer_pool_filename = @orig;
