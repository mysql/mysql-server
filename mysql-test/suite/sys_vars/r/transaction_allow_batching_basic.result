SET @start_global_value = @@session.transaction_allow_batching;
SELECT @start_global_value as "Should be off by default";
Should be off by default
0
select @@global.transaction_allow_batching;
ERROR HY000: Variable 'transaction_allow_batching' is a SESSION variable
select @@session.transaction_allow_batching;
@@session.transaction_allow_batching
0
show global variables like 'transaction_allow_batching';
Variable_name	Value
show session variables like 'transaction_allow_batching';
Variable_name	Value
transaction_allow_batching	OFF
select * from performance_schema.global_variables where variable_name='transaction_allow_batching';
VARIABLE_NAME	VARIABLE_VALUE
select * from performance_schema.session_variables where variable_name='transaction_allow_batching';
VARIABLE_NAME	VARIABLE_VALUE
transaction_allow_batching	OFF
set global transaction_allow_batching = 1;
ERROR HY000: Variable 'transaction_allow_batching' is a SESSION variable and can't be used with SET GLOBAL
set session transaction_allow_batching = TRUE;
select @@session.transaction_allow_batching;
@@session.transaction_allow_batching
1
set transaction_allow_batching=1.1;
ERROR 42000: Incorrect argument type to variable 'transaction_allow_batching'
set transaction_allow_batching=1e1;
ERROR 42000: Incorrect argument type to variable 'transaction_allow_batching'
set transaction_allow_batching="foobar";
ERROR 42000: Variable 'transaction_allow_batching' can't be set to the value of 'foobar'
set session transaction_allow_batching = @start_global_value;
select @@session.transaction_allow_batching;
@@session.transaction_allow_batching
0
