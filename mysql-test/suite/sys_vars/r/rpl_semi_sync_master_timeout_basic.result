INSTALL PLUGIN rpl_semi_sync_master SONAME 'semisync_master.so';
select @@global.rpl_semi_sync_master_timeout;
@@global.rpl_semi_sync_master_timeout
10000
SET @start_global_value = @@global.rpl_semi_sync_master_timeout;
Assuming value will not be more then 100 sec
select @@global.rpl_semi_sync_master_timeout between 1 and 100000;
@@global.rpl_semi_sync_master_timeout between 1 and 100000
1
select @@session.rpl_semi_sync_master_timeout;
ERROR HY000: Variable 'rpl_semi_sync_master_timeout' is a GLOBAL variable
show global variables like 'rpl_semi_sync_master_timeout';
Variable_name	Value
rpl_semi_sync_master_timeout	10000
show session variables like 'rpl_semi_sync_master_timeout';
Variable_name	Value
rpl_semi_sync_master_timeout	10000
select * from performance_schema.global_variables where variable_name='rpl_semi_sync_master_timeout';
VARIABLE_NAME	VARIABLE_VALUE
rpl_semi_sync_master_timeout	10000
select * from performance_schema.session_variables where variable_name='rpl_semi_sync_master_timeout';
VARIABLE_NAME	VARIABLE_VALUE
rpl_semi_sync_master_timeout	10000
set global rpl_semi_sync_master_timeout=42;
set session rpl_semi_sync_master_timeout=99;
ERROR HY000: Variable 'rpl_semi_sync_master_timeout' is a GLOBAL variable and should be set with SET GLOBAL
select @@global.rpl_semi_sync_master_timeout;
@@global.rpl_semi_sync_master_timeout
42
select @@session.rpl_semi_sync_master_timeout;
ERROR HY000: Variable 'rpl_semi_sync_master_timeout' is a GLOBAL variable
show global variables like 'rpl_semi_sync_master_timeout';
Variable_name	Value
rpl_semi_sync_master_timeout	42
show session variables like 'rpl_semi_sync_master_timeout';
Variable_name	Value
rpl_semi_sync_master_timeout	42
select * from performance_schema.global_variables where variable_name='rpl_semi_sync_master_timeout';
VARIABLE_NAME	VARIABLE_VALUE
rpl_semi_sync_master_timeout	42
select * from performance_schema.session_variables where variable_name='rpl_semi_sync_master_timeout';
VARIABLE_NAME	VARIABLE_VALUE
rpl_semi_sync_master_timeout	42
set global rpl_semi_sync_master_timeout=1.1;
ERROR 42000: Incorrect argument type to variable 'rpl_semi_sync_master_timeout'
set global rpl_semi_sync_master_timeout=1e1;
ERROR 42000: Incorrect argument type to variable 'rpl_semi_sync_master_timeout'
set global rpl_semi_sync_master_timeout="some text";
ERROR 42000: Incorrect argument type to variable 'rpl_semi_sync_master_timeout'
SET @@global.rpl_semi_sync_master_timeout = @start_global_value;
select @@global.rpl_semi_sync_master_timeout;
@@global.rpl_semi_sync_master_timeout
10000
UNINSTALL PLUGIN rpl_semi_sync_master;
