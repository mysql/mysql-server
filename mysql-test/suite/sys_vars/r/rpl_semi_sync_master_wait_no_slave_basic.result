INSTALL PLUGIN rpl_semi_sync_master SONAME 'semisync_master.so';
select @@global.rpl_semi_sync_master_wait_no_slave;
@@global.rpl_semi_sync_master_wait_no_slave
1
SET @start_global_value = @@global.rpl_semi_sync_master_wait_no_slave;
select @@global.rpl_semi_sync_master_wait_no_slave in (0,1);
@@global.rpl_semi_sync_master_wait_no_slave in (0,1)
1
select @@session.rpl_semi_sync_master_wait_no_slave;
ERROR HY000: Variable 'rpl_semi_sync_master_wait_no_slave' is a GLOBAL variable
show global variables like 'rpl_semi_sync_master_wait_no_slave';
Variable_name	Value
rpl_semi_sync_master_wait_no_slave	ON
show session variables like 'rpl_semi_sync_master_wait_no_slave';
Variable_name	Value
rpl_semi_sync_master_wait_no_slave	ON
select * from performance_schema.global_variables where variable_name='rpl_semi_sync_master_wait_no_slave';
VARIABLE_NAME	VARIABLE_VALUE
rpl_semi_sync_master_wait_no_slave	ON
select * from performance_schema.session_variables where variable_name='rpl_semi_sync_master_wait_no_slave';
VARIABLE_NAME	VARIABLE_VALUE
rpl_semi_sync_master_wait_no_slave	ON
set global rpl_semi_sync_master_wait_no_slave=0;
set session rpl_semi_sync_master_wait_no_slave=0;
ERROR HY000: Variable 'rpl_semi_sync_master_wait_no_slave' is a GLOBAL variable and should be set with SET GLOBAL
select @@global.rpl_semi_sync_master_wait_no_slave;
@@global.rpl_semi_sync_master_wait_no_slave
0
select @@session.rpl_semi_sync_master_wait_no_slave;
ERROR HY000: Variable 'rpl_semi_sync_master_wait_no_slave' is a GLOBAL variable
show global variables like 'rpl_semi_sync_master_wait_no_slave';
Variable_name	Value
rpl_semi_sync_master_wait_no_slave	OFF
show session variables like 'rpl_semi_sync_master_wait_no_slave';
Variable_name	Value
rpl_semi_sync_master_wait_no_slave	OFF
select * from performance_schema.global_variables where variable_name='rpl_semi_sync_master_wait_no_slave';
VARIABLE_NAME	VARIABLE_VALUE
rpl_semi_sync_master_wait_no_slave	OFF
select * from performance_schema.session_variables where variable_name='rpl_semi_sync_master_wait_no_slave';
VARIABLE_NAME	VARIABLE_VALUE
rpl_semi_sync_master_wait_no_slave	OFF
set global rpl_semi_sync_master_wait_no_slave=1;
set session rpl_semi_sync_master_wait_no_slave=1;
ERROR HY000: Variable 'rpl_semi_sync_master_wait_no_slave' is a GLOBAL variable and should be set with SET GLOBAL
select @@global.rpl_semi_sync_master_wait_no_slave;
@@global.rpl_semi_sync_master_wait_no_slave
1
select @@session.rpl_semi_sync_master_wait_no_slave;
ERROR HY000: Variable 'rpl_semi_sync_master_wait_no_slave' is a GLOBAL variable
show global variables like 'rpl_semi_sync_master_wait_no_slave';
Variable_name	Value
rpl_semi_sync_master_wait_no_slave	ON
show session variables like 'rpl_semi_sync_master_wait_no_slave';
Variable_name	Value
rpl_semi_sync_master_wait_no_slave	ON
select * from performance_schema.global_variables where variable_name='rpl_semi_sync_master_wait_no_slave';
VARIABLE_NAME	VARIABLE_VALUE
rpl_semi_sync_master_wait_no_slave	ON
select * from performance_schema.session_variables where variable_name='rpl_semi_sync_master_wait_no_slave';
VARIABLE_NAME	VARIABLE_VALUE
rpl_semi_sync_master_wait_no_slave	ON
set global rpl_semi_sync_master_wait_no_slave=1.1;
ERROR 42000: Incorrect argument type to variable 'rpl_semi_sync_master_wait_no_slave'
set global rpl_semi_sync_master_wait_no_slave=1e1;
ERROR 42000: Incorrect argument type to variable 'rpl_semi_sync_master_wait_no_slave'
set global rpl_semi_sync_master_wait_no_slave="some text";
ERROR 42000: Variable 'rpl_semi_sync_master_wait_no_slave' can't be set to the value of 'some text'
SET @@global.rpl_semi_sync_master_wait_no_slave = @start_global_value;
select @@global.rpl_semi_sync_master_wait_no_slave;
@@global.rpl_semi_sync_master_wait_no_slave
1
UNINSTALL PLUGIN rpl_semi_sync_master;
