SELECT @@innodb_stats_auto_recalc;
@@innodb_stats_auto_recalc
1
SET GLOBAL innodb_stats_auto_recalc=ON;
SELECT @@innodb_stats_auto_recalc;
@@innodb_stats_auto_recalc
1
SET GLOBAL innodb_stats_auto_recalc=OFF;
SELECT @@innodb_stats_auto_recalc;
@@innodb_stats_auto_recalc
0
SET GLOBAL innodb_stats_auto_recalc=1;
SELECT @@innodb_stats_auto_recalc;
@@innodb_stats_auto_recalc
1
SET GLOBAL innodb_stats_auto_recalc=0;
SELECT @@innodb_stats_auto_recalc;
@@innodb_stats_auto_recalc
0
SET GLOBAL innodb_stats_auto_recalc=123;
ERROR 42000: Variable 'innodb_stats_auto_recalc' can't be set to the value of '123'
SET GLOBAL innodb_stats_auto_recalc='foo';
ERROR 42000: Variable 'innodb_stats_auto_recalc' can't be set to the value of 'foo'
SET GLOBAL innodb_stats_auto_recalc=default;
