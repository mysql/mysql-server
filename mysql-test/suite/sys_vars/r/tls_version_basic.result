SELECT COUNT(@@GLOBAL.tls_version);
COUNT(@@GLOBAL.tls_version)
1
1 Expected
SET @@GLOBAL.tls_version=1;
ERROR 42000: Incorrect argument type to variable 'tls_version'
Expected error 'Incorrect argument type'
SELECT COUNT(@@GLOBAL.tls_version);
COUNT(@@GLOBAL.tls_version)
1
1 Expected
SELECT @@GLOBAL.tls_version = VARIABLE_VALUE
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='tls_version';
@@GLOBAL.tls_version = VARIABLE_VALUE
1
1 Expected
SELECT COUNT(@@GLOBAL.tls_version);
COUNT(@@GLOBAL.tls_version)
1
1 Expected
SELECT COUNT(VARIABLE_VALUE)
FROM performance_schema.global_variables
WHERE VARIABLE_NAME='tls_version';
COUNT(VARIABLE_VALUE)
1
1 Expected
SELECT @@tls_version = @@GLOBAL.tls_version;
@@tls_version = @@GLOBAL.tls_version
1
1 Expected
SELECT COUNT(@@tls_version);
COUNT(@@tls_version)
1
1 Expected
SELECT COUNT(@@local.tls_version);
ERROR HY000: Variable 'tls_version' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.tls_version);
ERROR HY000: Variable 'tls_version' is a GLOBAL variable
Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.tls_version);
COUNT(@@GLOBAL.tls_version)
1
1 Expected
SET tls_version = @@GLOBAL.tls_version;
ERROR HY000: Variable 'tls_version' is a GLOBAL variable and should be set with SET GLOBAL
Expected error 'Variable is a GLOBAL variable'
SET @@GLOBAL.tls_version='TLSv1.2';
SELECT @@GLOBAL.tls_version;
@@GLOBAL.tls_version
TLSv1.2
SET @@GLOBAL.tls_version='TLSv1.3';
SELECT @@GLOBAL.tls_version;
@@GLOBAL.tls_version
TLSv1.3
SET @@GLOBAL.tls_version='TLSv1.2,TLSv1.3';
SELECT @@GLOBAL.tls_version;
@@GLOBAL.tls_version
TLSv1.2,TLSv1.3
SET @@GLOBAL.tls_version='tlsv1.2';
SELECT @@GLOBAL.tls_version;
@@GLOBAL.tls_version
tlsv1.2
SET @@GLOBAL.tls_version='tlsv1.3';
SELECT @@GLOBAL.tls_version;
@@GLOBAL.tls_version
tlsv1.3
SET @@GLOBAL.tls_version='tlsv1.2,tlsv1.3';
SELECT @@GLOBAL.tls_version;
@@GLOBAL.tls_version
tlsv1.2,tlsv1.3
SET @@GLOBAL.tls_version='TLSv1.0';
ERROR 42000: Variable 'tls_version' can't be set to the value of 'TLSv1.0'
SET @@GLOBAL.tls_version='TLSv1.1';
ERROR 42000: Variable 'tls_version' can't be set to the value of 'TLSv1.1'
SET @@GLOBAL.tls_version='TLSv1.0,TLSv1.1';
ERROR 42000: Variable 'tls_version' can't be set to the value of 'TLSv1.0,TLSv1.1'
SET @@GLOBAL.tls_version='SSLv2';
ERROR 42000: Variable 'tls_version' can't be set to the value of 'SSLv2'
SET @@GLOBAL.tls_version='SSLv3';
ERROR 42000: Variable 'tls_version' can't be set to the value of 'SSLv3'
SET @@GLOBAL.tls_version=default;
