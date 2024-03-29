SELECT  @@global.innodb_default_row_format;
@@global.innodb_default_row_format
dynamic
SET GLOBAL innodb_default_row_format = 'redundant';
SELECT @@global.innodb_default_row_format;
@@global.innodb_default_row_format
redundant
SET GLOBAL innodb_default_row_format = 'dynamic';
SELECT @@global.innodb_default_row_format;
@@global.innodb_default_row_format
dynamic
SET GLOBAL innodb_default_row_format = 'compact';
SELECT @@global.innodb_default_row_format;
@@global.innodb_default_row_format
compact
SET GLOBAL innodb_default_row_format = 'compressed';
ERROR 42000: Variable 'innodb_default_row_format' can't be set to the value of 'compressed'
SELECT @@global.innodb_default_row_format;
@@global.innodb_default_row_format
compact
SET GLOBAL innodb_default_row_format = 'foobar';
ERROR 42000: Variable 'innodb_default_row_format' can't be set to the value of 'foobar'
SELECT @@global.innodb_default_row_format;
@@global.innodb_default_row_format
compact
SET GLOBAL innodb_default_row_format = 0;
SELECT @@global.innodb_default_row_format;
@@global.innodb_default_row_format
redundant
SET GLOBAL innodb_default_row_format = 1;
SELECT @@global.innodb_default_row_format;
@@global.innodb_default_row_format
compact
SET GLOBAL innodb_default_row_format = 2;
SELECT @@global.innodb_default_row_format;
@@global.innodb_default_row_format
dynamic
SET GLOBAL innodb_default_row_format = 3;
ERROR 42000: Variable 'innodb_default_row_format' can't be set to the value of '3'
SELECT @@global.innodb_default_row_format;
@@global.innodb_default_row_format
dynamic
SET GLOBAL innodb_default_row_format = 123;
ERROR 42000: Variable 'innodb_default_row_format' can't be set to the value of '123'
SELECT @@global.innodb_default_row_format;
@@global.innodb_default_row_format
dynamic
SET GLOBAL innodb_default_row_format = default;
