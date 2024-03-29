SELECT @@innodb_stats_include_delete_marked;
@@innodb_stats_include_delete_marked
0
SET GLOBAL innodb_stats_include_delete_marked=1;
SELECT @@innodb_stats_include_delete_marked;
@@innodb_stats_include_delete_marked
1
SET SESSION innodb_stats_include_delete_marked=1;
ERROR HY000: Variable 'innodb_stats_include_delete_marked' is a GLOBAL variable and should be set with SET GLOBAL
SET GLOBAL innodb_stats_include_delete_marked=100;
ERROR 42000: Variable 'innodb_stats_include_delete_marked' can't be set to the value of '100'
SET GLOBAL innodb_stats_include_delete_marked=foo;
ERROR 42000: Variable 'innodb_stats_include_delete_marked' can't be set to the value of 'foo'
SET GLOBAL innodb_stats_include_delete_marked=OFF ;
SELECT @@innodb_stats_include_delete_marked;
@@innodb_stats_include_delete_marked
0
SET GLOBAL innodb_stats_include_delete_marked=ON ;
SELECT @@innodb_stats_include_delete_marked;
@@innodb_stats_include_delete_marked
1
SET GLOBAL innodb_stats_include_delete_marked=Default ;
SELECT @@innodb_stats_include_delete_marked;
@@innodb_stats_include_delete_marked
0
