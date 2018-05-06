SET @start_global_value = @@global.lc_messages;
SELECT @start_global_value;
@start_global_value
en_US
select @@global.lc_messages;
@@global.lc_messages
en_US
select @@session.lc_messages;
@@session.lc_messages
en_US
show global variables like 'lc_messages';
Variable_name	Value
lc_messages	en_US
show session variables like 'lc_messages';
Variable_name	Value
lc_messages	en_US
select * from performance_schema.global_variables where variable_name='lc_messages';
VARIABLE_NAME	VARIABLE_VALUE
lc_messages	en_US
select * from performance_schema.session_variables where variable_name='lc_messages';
VARIABLE_NAME	VARIABLE_VALUE
lc_messages	en_US
set global lc_messages=1;
select @@global.lc_messages;
@@global.lc_messages
en_GB
set session lc_messages=2;
select @@session.lc_messages;
@@session.lc_messages
ja_JP
set global lc_messages="en_US";
set session lc_messages="en_GB";
select @@global.lc_messages;
@@global.lc_messages
en_US
select @@session.lc_messages;
@@session.lc_messages
en_GB
select * from performance_schema.global_variables where variable_name='lc_messages';
VARIABLE_NAME	VARIABLE_VALUE
lc_messages	en_US
select * from performance_schema.session_variables where variable_name='lc_messages';
VARIABLE_NAME	VARIABLE_VALUE
lc_messages	en_GB
set global lc_messages=1.1;
ERROR 42000: Incorrect argument type to variable 'lc_messages'
set global lc_messages=1e1;
ERROR 42000: Incorrect argument type to variable 'lc_messages'
SET @@global.lc_messages = @start_global_value;
SELECT @@global.lc_messages;
@@global.lc_messages
en_US
