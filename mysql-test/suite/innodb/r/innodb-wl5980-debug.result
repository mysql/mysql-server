SET GLOBAL innodb_file_per_table=ON;
#
# WL5980 Remote tablespace debug error injection tests.
#
CREATE TABLE t1 (a int KEY, b text) ENGINE=Innodb DATA DIRECTORY='MYSQL_TMP_DIR/alt_dir' ;
INSERT INTO t1 VALUES (1, "tablespace");
SELECT * FROM t1;
a	b
1	tablespace
#
# Test the first injection point in fil_rename_tablespace().
# Make sure the table is useable after this failure.
#
SET SESSION debug="+d,fil_rename_tablespace_failure_1";
RENAME TABLE t1 TO t2;
SET SESSION debug="-d,fil_rename_tablespace_failure_1";
INSERT INTO t1 VALUES (2, "tablespace");
SELECT * FROM t1;
a	b
1	tablespace
2	tablespace
#
# Test the second injection point in fil_rename_tablespace().
# Make sure the table is useable after this failure.
#
SET SESSION debug="+d,fil_rename_tablespace_failure_2";
RENAME TABLE t1 TO t2;
SET SESSION debug="-d,fil_rename_tablespace_failure_2";
INSERT INTO t1 VALUES (3, "tablespace");
SELECT * FROM t1;
a	b
1	tablespace
2	tablespace
3	tablespace
#
# Test the injection point in dict_table_rename_in_cache().
# Make sure the table is useable after this failure.
#
SET SESSION debug="+d,dict_table_rename_in_cache_failure";
RENAME TABLE t1 TO t2;
SET SESSION debug="-d,dict_table_rename_in_cache_failure";
INSERT INTO t1 VALUES (4, "tablespace");
SELECT * FROM t1;
a	b
1	tablespace
2	tablespace
3	tablespace
4	tablespace
#
# Cleanup
#
DROP TABLE t1;
