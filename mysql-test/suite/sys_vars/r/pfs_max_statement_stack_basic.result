select @@global.performance_schema_max_statement_stack;
@@global.performance_schema_max_statement_stack
10
select @@global.performance_schema_max_statement_stack;
@@global.performance_schema_max_statement_stack
10
select @@session.performance_schema_max_statement_stack;
ERROR HY000: Variable 'performance_schema_max_statement_stack' is a GLOBAL variable
show global variables like 'performance_schema_max_statement_stack';
Variable_name	Value
performance_schema_max_statement_stack	10
show session variables like 'performance_schema_max_statement_stack';
Variable_name	Value
performance_schema_max_statement_stack	10
select * from performance_schema.global_variables
where variable_name='performance_schema_max_statement_stack';
VARIABLE_NAME	VARIABLE_VALUE
performance_schema_max_statement_stack	10
select * from performance_schema.session_variables
where variable_name='performance_schema_max_statement_stack';
VARIABLE_NAME	VARIABLE_VALUE
performance_schema_max_statement_stack	10
set global performance_schema_max_statement_stack=1;
ERROR HY000: Variable 'performance_schema_max_statement_stack' is a read only variable
set session performance_schema_max_statement_stack=1;
ERROR HY000: Variable 'performance_schema_max_statement_stack' is a read only variable
SELECT @@GLOBAL.performance_schema_max_statement_stack = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='performance_schema_max_statement_stack';
@@GLOBAL.performance_schema_max_statement_stack = VARIABLE_VALUE
1
select @@global.performance_schema_max_statement_stack;
@@global.performance_schema_max_statement_stack
10
select * from performance_schema.global_variables
where variable_name='performance_schema_max_statement_stack';
VARIABLE_NAME	VARIABLE_VALUE
performance_schema_max_statement_stack	10
SELECT @@performance_schema_max_statement_stack = @@GLOBAL.performance_schema_max_statement_stack;
@@performance_schema_max_statement_stack = @@GLOBAL.performance_schema_max_statement_stack
1
