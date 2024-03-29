#
# Testing WL#7524 - Import from SDI files with --secure-file-priv=''
#
# Setup test environment
# IM-NEG-4: Must obey the secure_file_priv setting, e.g. for .sdi
# files outside the schema directory.
# Verify that access to non-existent directories is detected
# and diagnosed appropriately (as this can only be done when
# secure_file_priv is empty)
IMPORT TABLE FROM '...';
Got one of the listed errors
#
# IM-POS-2: Export, drop and import back table
#
CREATE TABLE t1 (i int) ENGINE=MYISAM;
INSERT INTO t1 VALUES (1), (3), (5);
SELECT * FROM t1;
i
1
3
5
CREATE TABLE t2 (i int) ENGINE=MYISAM;
INSERT INTO t2 VALUES (2), (4), (6);
SELECT * FROM t2;
i
2
4
6
FLUSH TABLES WITH READ LOCK;
# Copy t1 files to export dir
UNLOCK TABLES;
DROP TABLE t1;
DROP TABLE t2;
# Copy MYD and MYI back into datadir
# Verify that importing the same file twice, triggers error
IMPORT TABLE FROM '...', '...';
ERROR HY000: Table 'test.t1', referenced in SDI, already exists.
IMPORT TABLE FROM '...', '...';
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `i` int DEFAULT NULL
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SHOW CREATE TABLE t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `i` int DEFAULT NULL
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT * FROM t1;
i
1
3
5
SELECT * FROM t2;
i
2
4
6
DROP TABLE t1;
DROP TABLE t2;
# Clean SDI files in $EXPORT_DIR
# Remove $EXPORT_DIR
