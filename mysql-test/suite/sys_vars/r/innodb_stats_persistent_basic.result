SELECT @@innodb_stats_persistent;
@@innodb_stats_persistent
1
SET GLOBAL innodb_stats_persistent=ON;
SELECT @@innodb_stats_persistent;
@@innodb_stats_persistent
1
SET GLOBAL innodb_stats_persistent=OFF;
SELECT @@innodb_stats_persistent;
@@innodb_stats_persistent
0
SET GLOBAL innodb_stats_persistent=1;
SELECT @@innodb_stats_persistent;
@@innodb_stats_persistent
1
SET GLOBAL innodb_stats_persistent=0;
SELECT @@innodb_stats_persistent;
@@innodb_stats_persistent
0
SET GLOBAL innodb_stats_persistent=123;
ERROR 42000: Variable 'innodb_stats_persistent' can't be set to the value of '123'
SET GLOBAL innodb_stats_persistent='foo';
ERROR 42000: Variable 'innodb_stats_persistent' can't be set to the value of 'foo'
SET GLOBAL innodb_stats_persistent=default;
