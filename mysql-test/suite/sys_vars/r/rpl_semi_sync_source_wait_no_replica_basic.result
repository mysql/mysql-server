INSTALL PLUGIN rpl_semi_sync_source SONAME 'semisync_source.so';
select @@global.rpl_semi_sync_source_wait_no_replica;
@@global.rpl_semi_sync_source_wait_no_replica
1
SET @start_global_value = @@global.rpl_semi_sync_source_wait_no_replica;
select @@global.rpl_semi_sync_source_wait_no_replica in (0,1);
@@global.rpl_semi_sync_source_wait_no_replica in (0,1)
1
select @@session.rpl_semi_sync_source_wait_no_replica;
ERROR HY000: Variable 'rpl_semi_sync_source_wait_no_replica' is a GLOBAL variable
show global variables like 'rpl_semi_sync_source_wait_no_replica';
Variable_name	Value
rpl_semi_sync_source_wait_no_replica	ON
show session variables like 'rpl_semi_sync_source_wait_no_replica';
Variable_name	Value
rpl_semi_sync_source_wait_no_replica	ON
select * from performance_schema.global_variables where variable_name='rpl_semi_sync_source_wait_no_replica';
VARIABLE_NAME	VARIABLE_VALUE
rpl_semi_sync_source_wait_no_replica	ON
select * from performance_schema.session_variables where variable_name='rpl_semi_sync_source_wait_no_replica';
VARIABLE_NAME	VARIABLE_VALUE
rpl_semi_sync_source_wait_no_replica	ON
set global rpl_semi_sync_source_wait_no_replica=0;
set session rpl_semi_sync_source_wait_no_replica=0;
ERROR HY000: Variable 'rpl_semi_sync_source_wait_no_replica' is a GLOBAL variable and should be set with SET GLOBAL
select @@global.rpl_semi_sync_source_wait_no_replica;
@@global.rpl_semi_sync_source_wait_no_replica
0
select @@session.rpl_semi_sync_source_wait_no_replica;
ERROR HY000: Variable 'rpl_semi_sync_source_wait_no_replica' is a GLOBAL variable
show global variables like 'rpl_semi_sync_source_wait_no_replica';
Variable_name	Value
rpl_semi_sync_source_wait_no_replica	OFF
show session variables like 'rpl_semi_sync_source_wait_no_replica';
Variable_name	Value
rpl_semi_sync_source_wait_no_replica	OFF
select * from performance_schema.global_variables where variable_name='rpl_semi_sync_source_wait_no_replica';
VARIABLE_NAME	VARIABLE_VALUE
rpl_semi_sync_source_wait_no_replica	OFF
select * from performance_schema.session_variables where variable_name='rpl_semi_sync_source_wait_no_replica';
VARIABLE_NAME	VARIABLE_VALUE
rpl_semi_sync_source_wait_no_replica	OFF
set global rpl_semi_sync_source_wait_no_replica=1;
set session rpl_semi_sync_source_wait_no_replica=1;
ERROR HY000: Variable 'rpl_semi_sync_source_wait_no_replica' is a GLOBAL variable and should be set with SET GLOBAL
select @@global.rpl_semi_sync_source_wait_no_replica;
@@global.rpl_semi_sync_source_wait_no_replica
1
select @@session.rpl_semi_sync_source_wait_no_replica;
ERROR HY000: Variable 'rpl_semi_sync_source_wait_no_replica' is a GLOBAL variable
show global variables like 'rpl_semi_sync_source_wait_no_replica';
Variable_name	Value
rpl_semi_sync_source_wait_no_replica	ON
show session variables like 'rpl_semi_sync_source_wait_no_replica';
Variable_name	Value
rpl_semi_sync_source_wait_no_replica	ON
select * from performance_schema.global_variables where variable_name='rpl_semi_sync_source_wait_no_replica';
VARIABLE_NAME	VARIABLE_VALUE
rpl_semi_sync_source_wait_no_replica	ON
select * from performance_schema.session_variables where variable_name='rpl_semi_sync_source_wait_no_replica';
VARIABLE_NAME	VARIABLE_VALUE
rpl_semi_sync_source_wait_no_replica	ON
set global rpl_semi_sync_source_wait_no_replica=1.1;
ERROR 42000: Incorrect argument type to variable 'rpl_semi_sync_source_wait_no_replica'
set global rpl_semi_sync_source_wait_no_replica=1e1;
ERROR 42000: Incorrect argument type to variable 'rpl_semi_sync_source_wait_no_replica'
set global rpl_semi_sync_source_wait_no_replica="some text";
ERROR 42000: Variable 'rpl_semi_sync_source_wait_no_replica' can't be set to the value of 'some text'
SET @@global.rpl_semi_sync_source_wait_no_replica = @start_global_value;
select @@global.rpl_semi_sync_source_wait_no_replica;
@@global.rpl_semi_sync_source_wait_no_replica
1
UNINSTALL PLUGIN rpl_semi_sync_source;
