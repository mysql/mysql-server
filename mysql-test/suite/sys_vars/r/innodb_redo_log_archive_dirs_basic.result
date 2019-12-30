SET @innodb_redo_log_archive_dirs_sav= @@global.innodb_redo_log_archive_dirs;
SELECT @@global.innodb_redo_log_archive_dirs;
@@global.innodb_redo_log_archive_dirs
NULL
SET @@global.innodb_redo_log_archive_dirs="label1:/dir1";
SELECT @@global.innodb_redo_log_archive_dirs;
@@global.innodb_redo_log_archive_dirs
label1:/dir1
SET @@global.innodb_redo_log_archive_dirs="label1:/dir1;label2:dir2";
SELECT @@global.innodb_redo_log_archive_dirs;
@@global.innodb_redo_log_archive_dirs
label1:/dir1;label2:dir2
SET @@global.innodb_redo_log_archive_dirs="label1:/dir1;";
SELECT @@global.innodb_redo_log_archive_dirs;
@@global.innodb_redo_log_archive_dirs
label1:/dir1;
SET @@global.innodb_redo_log_archive_dirs="label1:dir1;label2:/dir2;";
SELECT @@global.innodb_redo_log_archive_dirs;
@@global.innodb_redo_log_archive_dirs
label1:dir1;label2:/dir2;
SET @@global.innodb_redo_log_archive_dirs=":/dir1";
SELECT @@global.innodb_redo_log_archive_dirs;
@@global.innodb_redo_log_archive_dirs
:/dir1
SET @@global.innodb_redo_log_archive_dirs=":dir1;:dir1";
SELECT @@global.innodb_redo_log_archive_dirs;
@@global.innodb_redo_log_archive_dirs
:dir1;:dir1
SET @@global.innodb_redo_log_archive_dirs=";;;:/dir1";
SELECT @@global.innodb_redo_log_archive_dirs;
@@global.innodb_redo_log_archive_dirs
;;;:/dir1
SET @@global.innodb_redo_log_archive_dirs="label::::";
SELECT @@global.innodb_redo_log_archive_dirs;
@@global.innodb_redo_log_archive_dirs
label::::
SET @@global.innodb_redo_log_archive_dirs=";;;::::;;;;::::;";
SELECT @@global.innodb_redo_log_archive_dirs;
@@global.innodb_redo_log_archive_dirs
;;;::::;;;;::::;
SET @@global.innodb_redo_log_archive_dirs="";
SELECT @@global.innodb_redo_log_archive_dirs;
@@global.innodb_redo_log_archive_dirs

SET @@global.innodb_redo_log_archive_dirs=NULL;
SELECT @@global.innodb_redo_log_archive_dirs;
@@global.innodb_redo_log_archive_dirs
NULL
SET PERSIST innodb_redo_log_archive_dirs="label1:/dir1";
SELECT @@global.innodb_redo_log_archive_dirs;
@@global.innodb_redo_log_archive_dirs
label1:/dir1
RESET PERSIST innodb_redo_log_archive_dirs;
SET innodb_redo_log_archive_dirs="label1:/dir1";
ERROR HY000: Variable 'innodb_redo_log_archive_dirs' is a GLOBAL variable and should be set with SET GLOBAL
SET @@local.innodb_redo_log_archive_dirs="label1:/dir1";
ERROR HY000: Variable 'innodb_redo_log_archive_dirs' is a GLOBAL variable and should be set with SET GLOBAL
SET @@innodb_redo_log_archive_dirs="label1:/dir1";
ERROR HY000: Variable 'innodb_redo_log_archive_dirs' is a GLOBAL variable and should be set with SET GLOBAL
SET @@session.innodb_redo_log_archive_dirs="label1:/dir1";
ERROR HY000: Variable 'innodb_redo_log_archive_dirs' is a GLOBAL variable and should be set with SET GLOBAL
SET @@global.innodb_redo_log_archive_dirs="/dir1";
ERROR 42000: Variable 'innodb_redo_log_archive_dirs' can't be set to the value of '/dir1'
SET @@global.innodb_redo_log_archive_dirs=":";
ERROR 42000: Variable 'innodb_redo_log_archive_dirs' can't be set to the value of ':'
SET @@global.innodb_redo_log_archive_dirs="l1:";
ERROR 42000: Variable 'innodb_redo_log_archive_dirs' can't be set to the value of 'l1:'
SET @@global.innodb_redo_log_archive_dirs=":d1;;";
ERROR 42000: Variable 'innodb_redo_log_archive_dirs' can't be set to the value of ':d1;;'
SET @@global.innodb_redo_log_archive_dirs=":d1;d2";
ERROR 42000: Variable 'innodb_redo_log_archive_dirs' can't be set to the value of ':d1;d2'
SET @@global.innodb_redo_log_archive_dirs="l1:d1;l2:";
ERROR 42000: Variable 'innodb_redo_log_archive_dirs' can't be set to the value of 'l1:d1;l2:'
SET @@global.innodb_redo_log_archive_dirs="l1:d1;l2:;";
ERROR 42000: Variable 'innodb_redo_log_archive_dirs' can't be set to the value of 'l1:d1;l2:;'
DROP PROCEDURE IF EXISTS test.innodb_redo_log_archive_dirs;
CREATE PROCEDURE test.innodb_redo_log_archive_dirs (IN start BIGINT, IN nam CHAR(20))
BEGIN
DECLARE CONTINUE HANDLER FOR 13178 SET @x2 = 1;
SET @idx=start;
SET @n=nam;
WHILE (@idx > 0) DO
SET @s=CONCAT("SET @@global.innodb_redo_log_archive_dirs='1:../www/",@n,@idx,"'");
Prepare stmt from @s;
Execute stmt;
SET @@global.innodb_redo_log_archive_dirs="";
SET @idx = @idx - 1;
END WHILE;
END|
CALL test.innodb_redo_log_archive_dirs(100,'s');
DROP PROCEDURE test.innodb_redo_log_archive_dirs;
SET @@global.innodb_redo_log_archive_dirs= @innodb_redo_log_archive_dirs_sav;
