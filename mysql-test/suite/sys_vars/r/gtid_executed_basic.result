RESET MASTER;
SET @start_global_value = @@global.gtid_executed;
SELECT @start_global_value;
@start_global_value

select @@global.gtid_executed;
@@global.gtid_executed

select @@session.gtid_executed;
ERROR HY000: Variable 'gtid_executed' is a GLOBAL variable
show global variables like 'gtid_executed';
Variable_name	Value
gtid_executed	
show session variables like 'gtid_executed';
Variable_name	Value
gtid_executed	
select * from performance_schema.global_variables where variable_name='gtid_executed';
VARIABLE_NAME	VARIABLE_VALUE
gtid_executed	
select * from performance_schema.session_variables where variable_name='gtid_executed';
VARIABLE_NAME	VARIABLE_VALUE
gtid_executed	
select @@global.gtid_executed;
@@global.gtid_executed

set @@global.gtid_executed = '';
ERROR HY000: Variable 'gtid_executed' is a read only variable
set @@session.gtid_executed = '';
ERROR HY000: Variable 'gtid_executed' is a read only variable
