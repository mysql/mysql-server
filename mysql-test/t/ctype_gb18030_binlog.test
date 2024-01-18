--source include/force_myisam_default.inc
--source include/have_myisam.inc

-- source include/rpl/force_binlog_format_statement.inc

RESET BINARY LOGS AND GTIDS;
SET NAMES gb18030;
--character_set gb18030

CREATE TABLE t1 (
  f1 BLOB
) DEFAULT CHARSET=gb18030;

delimiter |;
CREATE PROCEDURE p1(IN val BLOB)
BEGIN
     SET @tval = val;
     SET @sql_cmd = CONCAT_WS(' ', 'INSERT INTO t1(f1) VALUES(?)');
     PREPARE stmt FROM @sql_cmd;
     EXECUTE stmt USING @tval;
     DEALLOCATE PREPARE stmt;
END|
delimiter ;|

SET @`tcontent`:='测试binlog复制，测试四字节编码:㐂㐃㐄,焊䏷菡釬';
CALL p1(@`tcontent`);

FLUSH LOGS;
DROP PROCEDURE p1;
RENAME TABLE t1 to t2;

let $MYSQLD_DATADIR= `select @@datadir`;
copy_file $MYSQLD_DATADIR/binlog.000001 $MYSQLD_DATADIR/binlog-gb18030.saved;
# Reset GTIDs
RESET BINARY LOGS AND GTIDS;
--exec $MYSQL_BINLOG --force-if-open --short-form $MYSQLD_DATADIR/binlog-gb18030.saved | $MYSQL
SELECT hex(f1), f1 FROM t2;
SELECT hex(f1), f1 FROM t1;

DROP PROCEDURE p1;
DROP TABLE t1;
DROP TABLE t2;
--remove_file $MYSQLD_DATADIR/binlog-gb18030.saved

--source include/rpl/restore_default_binlog_format.inc
