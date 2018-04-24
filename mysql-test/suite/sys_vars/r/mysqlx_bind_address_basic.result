SELECT COUNT(@@GLOBAL.mysqlx_bind_address);
COUNT(@@GLOBAL.mysqlx_bind_address)
1
1 Expected
SET @@GLOBAL.mysqlx_bind_address=1;
ERROR HY000: Variable 'mysqlx_bind_address' is a read only variable
Expected error 'Read only variable'
SELECT VARIABLE_VALUE = ''
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='mysqlx_bind_address';
VARIABLE_VALUE = ''
0
0 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='mysqlx_bind_address';
COUNT(VARIABLE_VALUE)
1
1 Expected
SELECT @@mysqlx_bind_address is NULL AND @@GLOBAL.mysqlx_bind_address is NULL;
@@mysqlx_bind_address is NULL AND @@GLOBAL.mysqlx_bind_address is NULL
0
0 Expected
SELECT @@mysqlx_bind_address is NOT NULL AND @@GLOBAL.mysqlx_bind_address is NOT NULL;
@@mysqlx_bind_address is NOT NULL AND @@GLOBAL.mysqlx_bind_address is NOT NULL
1
1 Expected
SELECT COUNT(@@mysqlx_bind_address);
COUNT(@@mysqlx_bind_address)
1
1 Expected
SELECT COUNT(@@local.mysqlx_bind_address);
ERROR HY000: Variable 'mysqlx_bind_address' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.mysqlx_bind_address);
ERROR HY000: Variable 'mysqlx_bind_address' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.mysqlx_bind_address);
COUNT(@@GLOBAL.mysqlx_bind_address)
1
1 Expected
SELECT mysqlx_bind_address = @@SESSION.mysqlx_bind_address;
ERROR 42S22: Unknown column 'mysqlx_bind_address' in 'field list'
Expected error 'Readonly variable'
