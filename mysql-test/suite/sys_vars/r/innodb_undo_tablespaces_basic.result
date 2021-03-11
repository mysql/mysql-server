SELECT @@GLOBAL.innodb_undo_tablespaces >= 2;
@@GLOBAL.innodb_undo_tablespaces >= 2
1
SET GLOBAL innodb_undo_tablespaces=2;
SELECT @@innodb_undo_tablespaces;
@@innodb_undo_tablespaces
2
# 2 Expected
SET GLOBAL innodb_undo_tablespaces=1;
Warnings:
Warning	1292	Truncated incorrect innodb_undo_tablespaces value: '1'
# Expected warning 'Truncated incorrect innodb_undo_tablespaces value'
SELECT @@innodb_undo_tablespaces;
@@innodb_undo_tablespaces
2
# 2 Expected
SET GLOBAL innodb_undo_tablespaces=0;
Warnings:
Warning	1292	Truncated incorrect innodb_undo_tablespaces value: '0'
# Expected warning 'Truncated incorrect innodb_undo_tablespaces value'
SELECT @@innodb_undo_tablespaces;
@@innodb_undo_tablespaces
2
# 2 Expected
SET GLOBAL innodb_undo_tablespaces=-1;
Warnings:
Warning	1292	Truncated incorrect innodb_undo_tablespaces value: '-1'
# Expected warning 'Truncated incorrect innodb_undo_tablespaces value'
SELECT @@innodb_undo_tablespaces;
@@innodb_undo_tablespaces
2
# 2 Expected
SET @@GLOBAL.innodb_undo_tablespaces=ORIGINAL_VALUE;;
SELECT COUNT(@@GLOBAL.innodb_undo_tablespaces);
COUNT(@@GLOBAL.innodb_undo_tablespaces)
1
# 1 Expected
DIFFERENCE
0
0 Expected
SELECT @@innodb_undo_tablespaces = @@GLOBAL.innodb_undo_tablespaces;
@@innodb_undo_tablespaces = @@GLOBAL.innodb_undo_tablespaces
1
# 1 Expected
SELECT COUNT(@@innodb_undo_tablespaces);
COUNT(@@innodb_undo_tablespaces)
1
# 1 Expected
SELECT COUNT(@@local.innodb_undo_tablespaces);
ERROR HY000: Variable 'innodb_undo_tablespaces' is a GLOBAL variable
# Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@SESSION.innodb_undo_tablespaces);
ERROR HY000: Variable 'innodb_undo_tablespaces' is a GLOBAL variable
# Expected error 'Variable is a GLOBAL variable'
SELECT COUNT(@@GLOBAL.innodb_undo_tablespaces);
COUNT(@@GLOBAL.innodb_undo_tablespaces)
1
# 1 Expected
SELECT innodb_undo_tablespaces;
ERROR 42S22: Unknown column 'innodb_undo_tablespaces' in 'field list'
# Expected error 'Unknown column'
