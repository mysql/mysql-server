SET @start_global_value = @@global.innodb_trx_rseg_n_slots_debug;
SELECT @start_global_value;
@start_global_value
0
select @@global.innodb_trx_rseg_n_slots_debug between 0 and 1024;
@@global.innodb_trx_rseg_n_slots_debug between 0 and 1024
1
select @@global.innodb_trx_rseg_n_slots_debug;
@@global.innodb_trx_rseg_n_slots_debug
0
select @@session.innodb_trx_rseg_n_slots_debug;
ERROR HY000: Variable 'innodb_trx_rseg_n_slots_debug' is a GLOBAL variable
show global variables like 'innodb_trx_rseg_n_slots_debug';
Variable_name	Value
innodb_trx_rseg_n_slots_debug	0
show session variables like 'innodb_trx_rseg_n_slots_debug';
Variable_name	Value
innodb_trx_rseg_n_slots_debug	0
select * from performance_schema.global_variables where variable_name='innodb_trx_rseg_n_slots_debug';
VARIABLE_NAME	VARIABLE_VALUE
innodb_trx_rseg_n_slots_debug	0
select * from performance_schema.session_variables where variable_name='innodb_trx_rseg_n_slots_debug';
VARIABLE_NAME	VARIABLE_VALUE
innodb_trx_rseg_n_slots_debug	0
set global innodb_trx_rseg_n_slots_debug=1;
select @@global.innodb_trx_rseg_n_slots_debug;
@@global.innodb_trx_rseg_n_slots_debug
1
select * from performance_schema.global_variables where variable_name='innodb_trx_rseg_n_slots_debug';
VARIABLE_NAME	VARIABLE_VALUE
innodb_trx_rseg_n_slots_debug	1
select * from performance_schema.session_variables where variable_name='innodb_trx_rseg_n_slots_debug';
VARIABLE_NAME	VARIABLE_VALUE
innodb_trx_rseg_n_slots_debug	1
set @@global.innodb_trx_rseg_n_slots_debug=0;
select @@global.innodb_trx_rseg_n_slots_debug;
@@global.innodb_trx_rseg_n_slots_debug
0
select * from performance_schema.global_variables where variable_name='innodb_trx_rseg_n_slots_debug';
VARIABLE_NAME	VARIABLE_VALUE
innodb_trx_rseg_n_slots_debug	0
select * from performance_schema.session_variables where variable_name='innodb_trx_rseg_n_slots_debug';
VARIABLE_NAME	VARIABLE_VALUE
innodb_trx_rseg_n_slots_debug	0
set session innodb_trx_rseg_n_slots_debug='some';
ERROR HY000: Variable 'innodb_trx_rseg_n_slots_debug' is a GLOBAL variable and should be set with SET GLOBAL
set @@session.innodb_trx_rseg_n_slots_debug='some';
ERROR HY000: Variable 'innodb_trx_rseg_n_slots_debug' is a GLOBAL variable and should be set with SET GLOBAL
set global innodb_trx_rseg_n_slots_debug=1.1;
ERROR 42000: Incorrect argument type to variable 'innodb_trx_rseg_n_slots_debug'
set global innodb_trx_rseg_n_slots_debug='foo';
ERROR 42000: Incorrect argument type to variable 'innodb_trx_rseg_n_slots_debug'
set global innodb_trx_rseg_n_slots_debug=-2;
Warnings:
Warning	1292	Truncated incorrect innodb_trx_rseg_n_slots_debug value: '-2'
set global innodb_trx_rseg_n_slots_debug=1e1;
ERROR 42000: Incorrect argument type to variable 'innodb_trx_rseg_n_slots_debug'
set global innodb_trx_rseg_n_slots_debug=1024;
set global innodb_trx_rseg_n_slots_debug=1025;
Warnings:
Warning	1292	Truncated incorrect innodb_trx_rseg_n_slots_debug value: '1025'
SET @@global.innodb_trx_rseg_n_slots_debug = @start_global_value;
SELECT @@global.innodb_trx_rseg_n_slots_debug;
@@global.innodb_trx_rseg_n_slots_debug
0
