INSTALL PLUGIN rpl_semi_sync_source SONAME 'semisync_source.so';
select @@global.rpl_semi_sync_source_enabled;
@@global.rpl_semi_sync_source_enabled
0
SET @start_global_value = @@global.rpl_semi_sync_source_enabled;
select @@global.rpl_semi_sync_source_enabled in (0,1);
@@global.rpl_semi_sync_source_enabled in (0,1)
1
select @@session.rpl_semi_sync_source_enabled;
ERROR HY000: Variable 'rpl_semi_sync_source_enabled' is a GLOBAL variable
show global variables like 'rpl_semi_sync_source_enabled';
Variable_name	Value
rpl_semi_sync_source_enabled	OFF
show session variables like 'rpl_semi_sync_source_enabled';
Variable_name	Value
rpl_semi_sync_source_enabled	OFF
select * from performance_schema.global_variables where variable_name='rpl_semi_sync_source_enabled';
VARIABLE_NAME	VARIABLE_VALUE
rpl_semi_sync_source_enabled	OFF
select * from performance_schema.session_variables where variable_name='rpl_semi_sync_source_enabled';
VARIABLE_NAME	VARIABLE_VALUE
rpl_semi_sync_source_enabled	OFF
set global rpl_semi_sync_source_enabled=0;
set session rpl_semi_sync_source_enabled=0;
ERROR HY000: Variable 'rpl_semi_sync_source_enabled' is a GLOBAL variable and should be set with SET GLOBAL
select @@global.rpl_semi_sync_source_enabled;
@@global.rpl_semi_sync_source_enabled
0
select @@session.rpl_semi_sync_source_enabled;
ERROR HY000: Variable 'rpl_semi_sync_source_enabled' is a GLOBAL variable
show global variables like 'rpl_semi_sync_source_enabled';
Variable_name	Value
rpl_semi_sync_source_enabled	OFF
show session variables like 'rpl_semi_sync_source_enabled';
Variable_name	Value
rpl_semi_sync_source_enabled	OFF
select * from performance_schema.global_variables where variable_name='rpl_semi_sync_source_enabled';
VARIABLE_NAME	VARIABLE_VALUE
rpl_semi_sync_source_enabled	OFF
select * from performance_schema.session_variables where variable_name='rpl_semi_sync_source_enabled';
VARIABLE_NAME	VARIABLE_VALUE
rpl_semi_sync_source_enabled	OFF
set global rpl_semi_sync_source_enabled=1;
set session rpl_semi_sync_source_enabled=1;
ERROR HY000: Variable 'rpl_semi_sync_source_enabled' is a GLOBAL variable and should be set with SET GLOBAL
select @@global.rpl_semi_sync_source_enabled;
@@global.rpl_semi_sync_source_enabled
1
select @@session.rpl_semi_sync_source_enabled;
ERROR HY000: Variable 'rpl_semi_sync_source_enabled' is a GLOBAL variable
show global variables like 'rpl_semi_sync_source_enabled';
Variable_name	Value
rpl_semi_sync_source_enabled	ON
show session variables like 'rpl_semi_sync_source_enabled';
Variable_name	Value
rpl_semi_sync_source_enabled	ON
select * from performance_schema.global_variables where variable_name='rpl_semi_sync_source_enabled';
VARIABLE_NAME	VARIABLE_VALUE
rpl_semi_sync_source_enabled	ON
select * from performance_schema.session_variables where variable_name='rpl_semi_sync_source_enabled';
VARIABLE_NAME	VARIABLE_VALUE
rpl_semi_sync_source_enabled	ON
set global rpl_semi_sync_source_enabled=1.1;
ERROR 42000: Incorrect argument type to variable 'rpl_semi_sync_source_enabled'
set global rpl_semi_sync_source_enabled=1e1;
ERROR 42000: Incorrect argument type to variable 'rpl_semi_sync_source_enabled'
set global rpl_semi_sync_source_enabled="some text";
ERROR 42000: Variable 'rpl_semi_sync_source_enabled' can't be set to the value of 'some text'
SET @@global.rpl_semi_sync_source_enabled = @start_global_value;
select @@global.rpl_semi_sync_source_enabled;
@@global.rpl_semi_sync_source_enabled
0
UNINSTALL PLUGIN rpl_semi_sync_source;
