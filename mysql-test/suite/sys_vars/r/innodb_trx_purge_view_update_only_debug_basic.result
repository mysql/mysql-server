SET @start_global_value = @@global.innodb_trx_purge_view_update_only_debug;
SELECT @start_global_value;
@start_global_value
0
select @@global.innodb_trx_purge_view_update_only_debug in (0, 1);
@@global.innodb_trx_purge_view_update_only_debug in (0, 1)
1
select @@global.innodb_trx_purge_view_update_only_debug;
@@global.innodb_trx_purge_view_update_only_debug
0
select @@session.innodb_trx_purge_view_update_only_debug;
ERROR HY000: Variable 'innodb_trx_purge_view_update_only_debug' is a GLOBAL variable
show global variables like 'innodb_trx_purge_view_update_only_debug';
Variable_name	Value
innodb_trx_purge_view_update_only_debug	OFF
show session variables like 'innodb_trx_purge_view_update_only_debug';
Variable_name	Value
innodb_trx_purge_view_update_only_debug	OFF
select * from performance_schema.global_variables where variable_name='innodb_trx_purge_view_update_only_debug';
VARIABLE_NAME	VARIABLE_VALUE
innodb_trx_purge_view_update_only_debug	OFF
select * from performance_schema.session_variables where variable_name='innodb_trx_purge_view_update_only_debug';
VARIABLE_NAME	VARIABLE_VALUE
innodb_trx_purge_view_update_only_debug	OFF
set global innodb_trx_purge_view_update_only_debug=1;
select @@global.innodb_trx_purge_view_update_only_debug;
@@global.innodb_trx_purge_view_update_only_debug
1
select * from performance_schema.global_variables where variable_name='innodb_trx_purge_view_update_only_debug';
VARIABLE_NAME	VARIABLE_VALUE
innodb_trx_purge_view_update_only_debug	ON
select * from performance_schema.session_variables where variable_name='innodb_trx_purge_view_update_only_debug';
VARIABLE_NAME	VARIABLE_VALUE
innodb_trx_purge_view_update_only_debug	ON
set @@global.innodb_trx_purge_view_update_only_debug=0;
select @@global.innodb_trx_purge_view_update_only_debug;
@@global.innodb_trx_purge_view_update_only_debug
0
select * from performance_schema.global_variables where variable_name='innodb_trx_purge_view_update_only_debug';
VARIABLE_NAME	VARIABLE_VALUE
innodb_trx_purge_view_update_only_debug	OFF
select * from performance_schema.session_variables where variable_name='innodb_trx_purge_view_update_only_debug';
VARIABLE_NAME	VARIABLE_VALUE
innodb_trx_purge_view_update_only_debug	OFF
set session innodb_trx_purge_view_update_only_debug='some';
ERROR HY000: Variable 'innodb_trx_purge_view_update_only_debug' is a GLOBAL variable and should be set with SET GLOBAL
set @@session.innodb_trx_purge_view_update_only_debug='some';
ERROR HY000: Variable 'innodb_trx_purge_view_update_only_debug' is a GLOBAL variable and should be set with SET GLOBAL
set global innodb_trx_purge_view_update_only_debug=1.1;
ERROR 42000: Incorrect argument type to variable 'innodb_trx_purge_view_update_only_debug'
set global innodb_trx_purge_view_update_only_debug='foo';
ERROR 42000: Variable 'innodb_trx_purge_view_update_only_debug' can't be set to the value of 'foo'
set global innodb_trx_purge_view_update_only_debug=-2;
ERROR 42000: Variable 'innodb_trx_purge_view_update_only_debug' can't be set to the value of '-2'
set global innodb_trx_purge_view_update_only_debug=1e1;
ERROR 42000: Incorrect argument type to variable 'innodb_trx_purge_view_update_only_debug'
set global innodb_trx_purge_view_update_only_debug=2;
ERROR 42000: Variable 'innodb_trx_purge_view_update_only_debug' can't be set to the value of '2'
SET @@global.innodb_trx_purge_view_update_only_debug = @start_global_value;
SELECT @@global.innodb_trx_purge_view_update_only_debug;
@@global.innodb_trx_purge_view_update_only_debug
0
