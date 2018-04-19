select @@global.ft_min_word_len;
@@global.ft_min_word_len
4
select @@session.ft_min_word_len;
ERROR HY000: Variable 'ft_min_word_len' is a GLOBAL variable
show global variables like 'ft_min_word_len';
Variable_name	Value
ft_min_word_len	4
show session variables like 'ft_min_word_len';
Variable_name	Value
ft_min_word_len	4
select * from performance_schema.global_variables where variable_name='ft_min_word_len';
VARIABLE_NAME	VARIABLE_VALUE
ft_min_word_len	4
select * from performance_schema.session_variables where variable_name='ft_min_word_len';
VARIABLE_NAME	VARIABLE_VALUE
ft_min_word_len	4
set global ft_min_word_len=1;
ERROR HY000: Variable 'ft_min_word_len' is a read only variable
set session ft_min_word_len=1;
ERROR HY000: Variable 'ft_min_word_len' is a read only variable
