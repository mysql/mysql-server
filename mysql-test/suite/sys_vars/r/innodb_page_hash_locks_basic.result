select @@global.innodb_page_hash_locks between 1 and 1024;
@@global.innodb_page_hash_locks between 1 and 1024
1
select @@global.innodb_page_hash_locks;
@@global.innodb_page_hash_locks
16
select @@session.innodb_page_hash_locks;
ERROR HY000: Variable 'innodb_page_hash_locks' is a GLOBAL variable
show global variables like 'innodb_page_hash_locks';
Variable_name	Value
innodb_page_hash_locks	16
show session variables like 'innodb_page_hash_locks';
Variable_name	Value
innodb_page_hash_locks	16
select * from performance_schema.global_variables where variable_name='innodb_page_hash_locks';
VARIABLE_NAME	VARIABLE_VALUE
innodb_page_hash_locks	16
select * from performance_schema.session_variables where variable_name='innodb_page_hash_locks';
VARIABLE_NAME	VARIABLE_VALUE
innodb_page_hash_locks	16
set global innodb_page_hash_locks=1;
ERROR HY000: Variable 'innodb_page_hash_locks' is a read only variable
set @@session.innodb_page_hash_locks='some';
ERROR HY000: Variable 'innodb_page_hash_locks' is a read only variable
