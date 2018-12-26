set @save_slave_sql_verify_checksum = @@global.slave_sql_verify_checksum;
select @@global.slave_sql_verify_checksum  as 'must be one because of default';
must be one because of default
1
select @@session.slave_sql_verify_checksum  as 'no session var';
ERROR HY000: Variable 'slave_sql_verify_checksum' is a GLOBAL variable
set @@global.slave_sql_verify_checksum = 0;
set @@global.slave_sql_verify_checksum = default;
set @@global.slave_sql_verify_checksum = 2;
ERROR 42000: Variable 'slave_sql_verify_checksum' can't be set to the value of '2'
set @@global.slave_sql_verify_checksum = @save_slave_sql_verify_checksum;
