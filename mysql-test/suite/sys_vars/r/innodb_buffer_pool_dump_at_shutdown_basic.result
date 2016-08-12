call mtr.add_suppression("Plugin \'MEMORY\'");
SET @orig = @@global.innodb_buffer_pool_dump_at_shutdown;
SELECT @orig;
@orig
1
SET GLOBAL innodb_buffer_pool_dump_at_shutdown = OFF;
SELECT @@global.innodb_buffer_pool_dump_at_shutdown;
@@global.innodb_buffer_pool_dump_at_shutdown
0
SET GLOBAL innodb_buffer_pool_dump_at_shutdown = ON;
SELECT @@global.innodb_buffer_pool_dump_at_shutdown;
@@global.innodb_buffer_pool_dump_at_shutdown
1
SET GLOBAL innodb_buffer_pool_dump_at_shutdown = 12.34;
Got one of the listed errors
SET GLOBAL innodb_buffer_pool_dump_at_shutdown = "string";
Got one of the listed errors
SET GLOBAL innodb_buffer_pool_dump_at_shutdown = 5;
Got one of the listed errors
# restart
SET GLOBAL innodb_buffer_pool_dump_at_shutdown = default;
