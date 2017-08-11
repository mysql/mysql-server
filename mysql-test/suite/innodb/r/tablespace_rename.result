SET GLOBAL innodb_file_per_table=ON;
#
# Tablespace rename
#
CREATE TABLE t1 (c1 int KEY) ENGINE=Innodb;
INSERT INTO t1 VALUES (1);
SELECT * FROM t1;
c1
1
SHOW TABLES;
Tables_in_test
t1
#
# Test crash before writing MLOG_FILE_RENAME t2.
# On recovery the rename never happened.
#
SET SESSION debug="+d,ib_crash_rename_log_1";
RENAME TABLE t1 TO t2;
ERROR HY000: Lost connection to MySQL server during query
# restart
SET SESSION debug="-d,ib_crash_rename_log_1";
SHOW TABLES;
Tables_in_test
t1
INSERT INTO t1 VALUES (2);
SELECT * FROM t1;
c1
1
2
#
# Cleanup
#
DROP TABLE t1;
