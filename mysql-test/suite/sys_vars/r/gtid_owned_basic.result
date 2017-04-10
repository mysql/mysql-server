SET @start_global_value = @@global.gtid_owned;
SELECT @start_global_value;
@start_global_value

select @@global.gtid_owned;
@@global.gtid_owned

select @@session.gtid_owned;
@@session.gtid_owned

show global variables like 'gtid_owned';
Variable_name	Value
gtid_owned	
show session variables like 'gtid_owned';
Variable_name	Value
gtid_owned	
select * from performance_schema.global_variables where variable_name='gtid_owned';
VARIABLE_NAME	VARIABLE_VALUE
gtid_owned	
select * from performance_schema.session_variables where variable_name='gtid_owned';
VARIABLE_NAME	VARIABLE_VALUE
gtid_owned	
set @@global.gtid_owned = 1;
ERROR HY000: Variable 'gtid_owned' is a read only variable
set @@session.gtid_owned = 1;
ERROR HY000: Variable 'gtid_owned' is a read only variable
set @@session.gtid_next = 'anonymous';
select @@session.gtid_owned;
@@session.gtid_owned
ANONYMOUS
select @@global.gtid_owned;
@@global.gtid_owned

ROLLBACK;
set @@session.gtid_next = 'automatic';
select @@session.gtid_owned;
@@session.gtid_owned

select @@global.gtid_owned;
@@global.gtid_owned

