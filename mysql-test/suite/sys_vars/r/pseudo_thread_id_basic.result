select @@global.pseudo_thread_id;
ERROR HY000: Variable 'pseudo_thread_id' is a SESSION variable
select @@session.pseudo_thread_id between 1 and 10000;
@@session.pseudo_thread_id between 1 and 10000
1
should be empty
show global variables like 'pseudo_thread_id';
Variable_name	Value
show session variables like 'pseudo_thread_id';
Variable_name	Value
pseudo_thread_id	#
should be empty
select * from performance_schema.global_variables where variable_name='pseudo_thread_id';
VARIABLE_NAME	VARIABLE_VALUE
select @@session.pseudo_thread_id = variable_value from performance_schema.session_variables where variable_name='pseudo_thread_id';
@@session.pseudo_thread_id = variable_value
1
set session pseudo_thread_id=42;
select @@session.pseudo_thread_id;
@@session.pseudo_thread_id
42
select * from performance_schema.global_variables where variable_name='pseudo_thread_id';
VARIABLE_NAME	VARIABLE_VALUE
select variable_value from performance_schema.session_variables where variable_name='pseudo_thread_id';
variable_value
42
set global pseudo_thread_id=1;
ERROR HY000: Variable 'pseudo_thread_id' is a SESSION variable and can't be used with SET GLOBAL
set session pseudo_thread_id=1.1;
ERROR 42000: Incorrect argument type to variable 'pseudo_thread_id'
set session pseudo_thread_id=1e1;
ERROR 42000: Incorrect argument type to variable 'pseudo_thread_id'
set session pseudo_thread_id="foo";
ERROR 42000: Incorrect argument type to variable 'pseudo_thread_id'
