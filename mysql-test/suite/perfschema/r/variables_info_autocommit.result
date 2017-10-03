select @@autocommit, @@big_tables;
@@autocommit	@@big_tables
0	1
select * from performance_schema.variables_info
where VARIABLE_NAME in ("autocommit", "big_tables");
VARIABLE_NAME	VARIABLE_SOURCE	VARIABLE_PATH	MIN_VALUE	MAX_VALUE	SET_TIME	SET_USER	SET_HOST
autocommit	COMMAND_LINE		0	0	NULL	NULL	NULL
big_tables	COMMAND_LINE		0	0	NULL	NULL	NULL
