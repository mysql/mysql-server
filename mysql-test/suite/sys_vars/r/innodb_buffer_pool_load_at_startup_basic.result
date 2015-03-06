SET @orig = @@global.innodb_buffer_pool_load_at_startup;
SELECT @orig;
@orig
1
SET GLOBAL innodb_buffer_pool_load_at_startup = OFF;
ERROR HY000: Variable 'innodb_buffer_pool_load_at_startup' is a read only variable
SET GLOBAL innodb_buffer_pool_load_at_startup = ON;
ERROR HY000: Variable 'innodb_buffer_pool_load_at_startup' is a read only variable
