SET @start_global_value = @@global.eq_range_index_dive_limit;
SELECT @start_global_value;
@start_global_value
200
select @@global.eq_range_index_dive_limit;
@@global.eq_range_index_dive_limit
200
select @@session.eq_range_index_dive_limit;
@@session.eq_range_index_dive_limit
200
show global variables like 'eq_range_index_dive_limit';
Variable_name	Value
eq_range_index_dive_limit	200
show session variables like 'eq_range_index_dive_limit';
Variable_name	Value
eq_range_index_dive_limit	200
select * 
from performance_schema.global_variables 
where variable_name='eq_range_index_dive_limit';
VARIABLE_NAME	VARIABLE_VALUE
eq_range_index_dive_limit	200
select * 
from performance_schema.session_variables 
where variable_name='eq_range_index_dive_limit';
VARIABLE_NAME	VARIABLE_VALUE
eq_range_index_dive_limit	200
set global eq_range_index_dive_limit=10;
select @@global.eq_range_index_dive_limit;
@@global.eq_range_index_dive_limit
10
set session eq_range_index_dive_limit=10;
select @@session.eq_range_index_dive_limit;
@@session.eq_range_index_dive_limit
10
set global eq_range_index_dive_limit=0;
select @@global.eq_range_index_dive_limit;
@@global.eq_range_index_dive_limit
0
set session eq_range_index_dive_limit=0;
select @@session.eq_range_index_dive_limit;
@@session.eq_range_index_dive_limit
0
set global eq_range_index_dive_limit=4294967295;
select @@global.eq_range_index_dive_limit;
@@global.eq_range_index_dive_limit
4294967295
set session eq_range_index_dive_limit=4294967295;
select @@session.eq_range_index_dive_limit;
@@session.eq_range_index_dive_limit
4294967295
set session eq_range_index_dive_limit=default;
select @@session.eq_range_index_dive_limit;
@@session.eq_range_index_dive_limit
4294967295
set global eq_range_index_dive_limit=default;
select @@global.eq_range_index_dive_limit;
@@global.eq_range_index_dive_limit
200
set session eq_range_index_dive_limit=default;
select @@session.eq_range_index_dive_limit;
@@session.eq_range_index_dive_limit
200
set global eq_range_index_dive_limit=-1;
Warnings:
Warning	1292	Truncated incorrect eq_range_index_dive_limit value: '-1'
select @@global.eq_range_index_dive_limit;
@@global.eq_range_index_dive_limit
0
set session eq_range_index_dive_limit=-1;
Warnings:
Warning	1292	Truncated incorrect eq_range_index_dive_limit value: '-1'
select @@session.eq_range_index_dive_limit;
@@session.eq_range_index_dive_limit
0
set global eq_range_index_dive_limit=4294967296;
Warnings:
Warning	1292	Truncated incorrect eq_range_index_dive_limit value: '4294967296'
select @@global.eq_range_index_dive_limit;
@@global.eq_range_index_dive_limit
4294967295
set session eq_range_index_dive_limit=4294967296;
Warnings:
Warning	1292	Truncated incorrect eq_range_index_dive_limit value: '4294967296'
select @@session.eq_range_index_dive_limit;
@@session.eq_range_index_dive_limit
4294967295
set global eq_range_index_dive_limit=1.1;
ERROR 42000: Incorrect argument type to variable 'eq_range_index_dive_limit'
set global eq_range_index_dive_limit=1e1;
ERROR 42000: Incorrect argument type to variable 'eq_range_index_dive_limit'
set global eq_range_index_dive_limit="foobar";
ERROR 42000: Incorrect argument type to variable 'eq_range_index_dive_limit'
SET @@global.eq_range_index_dive_limit = @start_global_value;
SELECT @@global.eq_range_index_dive_limit;
@@global.eq_range_index_dive_limit
200
