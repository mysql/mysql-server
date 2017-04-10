select @@global.innodb_version;
@@global.innodb_version
x.y.z
select @@session.innodb_version;
ERROR HY000: Variable 'innodb_version' is a GLOBAL variable
show global variables like 'innodb_version' disabled so to not change with every version;
show session variables like 'innodb_version' disabled so to not change with every version;
select VARIABLE_VALUE=@@global.innodb_version from performance_schema.global_variables where variable_name='innodb_version';
VARIABLE_VALUE=@@global.innodb_version
1
select VARIABLE_VALUE=@@global.innodb_version from performance_schema.session_variables where variable_name='innodb_version';
VARIABLE_VALUE=@@global.innodb_version
1
set global innodb_version=1;
ERROR HY000: Variable 'innodb_version' is a read only variable
set session innodb_version=1;
ERROR HY000: Variable 'innodb_version' is a read only variable
