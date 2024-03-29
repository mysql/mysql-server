SET @start_global_value = @@GLOBAL.innodb_open_files;
SELECT @start_global_value;
@start_global_value
4000
SHOW STATUS LIKE 'SYSTEM_VARIABLES_ADMIN';
Variable_name	Value
SELECT * FROM performance_schema.global_status WHERE variable_name like 'SYSTEM_VARIABLES_ADMIN';
VARIABLE_NAME	VARIABLE_VALUE
SELECT @@innodb_open_files;
@@innodb_open_files
4000
CREATE USER 'user1'@'localhost' IDENTIFIED BY '123';
GRANT SUPER ON *.* to 'user1'@'localhost';
Warnings:
Warning	1287	The SUPER privilege identifier is deprecated
SHOW GRANTS FOR 'user1'@'localhost';
Grants for user1@localhost
GRANT SUPER ON *.* TO `user1`@`localhost`
SELECT * FROM performance_schema.global_status WHERE variable_name like 'SYSTEM_VARIABLES_ADMIN';
VARIABLE_NAME	VARIABLE_VALUE
SELECT @@innodb_open_files ;
@@innodb_open_files
4000
SELECT innodb_set_open_files_limit(10);
innodb_set_open_files_limit(10)
10
SELECT @@innodb_open_files ;
@@innodb_open_files
10
DROP USER 'user1'@'localhost';
CREATE USER 'user1'@'localhost' IDENTIFIED BY '123';
GRANT SYSTEM_VARIABLES_ADMIN ON *.* to 'user1'@'localhost';
SHOW GRANTS FOR 'user1'@'localhost';
Grants for user1@localhost
GRANT USAGE ON *.* TO `user1`@`localhost`
GRANT SYSTEM_VARIABLES_ADMIN ON *.* TO `user1`@`localhost`
SELECT * FROM performance_schema.global_status WHERE variable_name like 'SYSTEM_VARIABLES_ADMIN';
VARIABLE_NAME	VARIABLE_VALUE
SELECT @@innodb_open_files ;
@@innodb_open_files
10
SELECT innodb_set_open_files_limit(2147483647);
innodb_set_open_files_limit(2147483647)
2147483647
SELECT @@innodb_open_files ;
@@innodb_open_files
2147483647
DROP USER 'user1'@'localhost';
CREATE USER 'user1'@'localhost' IDENTIFIED BY '123';
GRANT SUPER ON *.* to 'user1'@'localhost';
Warnings:
Warning	1287	The SUPER privilege identifier is deprecated
GRANT SYSTEM_VARIABLES_ADMIN ON *.* to 'user1'@'localhost';
SHOW GRANTS FOR 'user1'@'localhost';
Grants for user1@localhost
GRANT SUPER ON *.* TO `user1`@`localhost`
GRANT SYSTEM_VARIABLES_ADMIN ON *.* TO `user1`@`localhost`
SELECT * FROM performance_schema.global_status WHERE variable_name like 'SYSTEM_VARIABLES_ADMIN';
VARIABLE_NAME	VARIABLE_VALUE
SELECT @@innodb_open_files ;
@@innodb_open_files
2147483647
SELECT innodb_set_open_files_limit(4000);
innodb_set_open_files_limit(4000)
4000
SELECT @@innodb_open_files ;
@@innodb_open_files
4000
DROP USER 'user1'@'localhost';
CREATE USER 'user1'@'localhost' IDENTIFIED BY '123';
GRANT CREATE, SELECT, UPDATE, DELETE ON *.* TO 'user1'@'localhost';
SHOW GRANTS FOR 'user1'@'localhost';
Grants for user1@localhost
GRANT SELECT, UPDATE, DELETE, CREATE ON *.* TO `user1`@`localhost`
SELECT * FROM performance_schema.global_status WHERE variable_name like 'SYSTEM_VARIABLES_ADMIN';
VARIABLE_NAME	VARIABLE_VALUE
SELECT @@innodb_open_files ;
@@innodb_open_files
4000
SELECT innodb_set_open_files_limit(6000);
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN privilege(s) for this operation
SELECT @@innodb_open_files ;
@@innodb_open_files
4000
DROP USER 'user1'@'localhost';
SELECT innodb_set_open_files_limit(@start_global_value);
innodb_set_open_files_limit(@start_global_value)
4000
SELECT @@GLOBAL.innodb_open_files;
@@GLOBAL.innodb_open_files
4000
