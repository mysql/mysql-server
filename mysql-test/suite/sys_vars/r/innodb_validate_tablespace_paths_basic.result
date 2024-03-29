#
# Basic test for the innodb_validate_tablespace_paths variable
#
SELECT @@global.innodb_validate_tablespace_paths;
@@global.innodb_validate_tablespace_paths
1
set global innodb_validate_tablespace_paths = 0;
ERROR HY000: Variable 'innodb_validate_tablespace_paths' is a read only variable
SELECT @@global.innodb_validate_tablespace_paths;
@@global.innodb_validate_tablespace_paths
1
