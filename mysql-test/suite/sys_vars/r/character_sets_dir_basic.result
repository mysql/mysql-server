select @@global.character_sets_dir;
@@global.character_sets_dir
MYSQL_CHARSETSDIR
select @@session.character_sets_dir;
ERROR HY000: Variable 'character_sets_dir' is a GLOBAL variable
show global variables like 'character_sets_dir';
Variable_name	Value
character_sets_dir	MYSQL_CHARSETSDIR
show session variables like 'character_sets_dir';
Variable_name	Value
character_sets_dir	MYSQL_CHARSETSDIR
select * from performance_schema.global_variables where variable_name='character_sets_dir';
VARIABLE_NAME	VARIABLE_VALUE
character_sets_dir	MYSQL_CHARSETSDIR
select * from performance_schema.session_variables where variable_name='character_sets_dir';
VARIABLE_NAME	VARIABLE_VALUE
character_sets_dir	MYSQL_CHARSETSDIR
set global character_sets_dir="foo";
ERROR HY000: Variable 'character_sets_dir' is a read only variable
set session character_sets_dir="foo";
ERROR HY000: Variable 'character_sets_dir' is a read only variable
