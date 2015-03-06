SELECT @@global.innodb_buffer_pool_dump_pct;
@@global.innodb_buffer_pool_dump_pct
25
SET GLOBAL innodb_buffer_pool_dump_pct=20;
SELECT @@global.innodb_buffer_pool_dump_pct;
@@global.innodb_buffer_pool_dump_pct
20
SET GLOBAL innodb_buffer_pool_dump_pct=1;
SELECT @@global.innodb_buffer_pool_dump_pct;
@@global.innodb_buffer_pool_dump_pct
1
SET GLOBAL innodb_buffer_pool_dump_pct=100;
SELECT @@global.innodb_buffer_pool_dump_pct;
@@global.innodb_buffer_pool_dump_pct
100
SET GLOBAL innodb_buffer_pool_dump_pct=101;
Warnings:
Warning	1292	Truncated incorrect innodb_buffer_pool_dump_pct value: '101'
SELECT @@global.innodb_buffer_pool_dump_pct;
@@global.innodb_buffer_pool_dump_pct
100
SET GLOBAL innodb_buffer_pool_dump_pct=-1;
Warnings:
Warning	1292	Truncated incorrect innodb_buffer_pool_dump_pct value: '-1'
SELECT @@global.innodb_buffer_pool_dump_pct;
@@global.innodb_buffer_pool_dump_pct
1
SET GLOBAL innodb_buffer_pool_dump_pct=Default;
SELECT @@global.innodb_buffer_pool_dump_pct;
@@global.innodb_buffer_pool_dump_pct
25
SET GLOBAL innodb_buffer_pool_dump_pct='foo';
ERROR 42000: Incorrect argument type to variable 'innodb_buffer_pool_dump_pct'
SET innodb_buffer_pool_dump_pct=50;
ERROR HY000: Variable 'innodb_buffer_pool_dump_pct' is a GLOBAL variable and should be set with SET GLOBAL
