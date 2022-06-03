CREATE TABLE t1(a INT PRIMARY KEY) ENGINE=InnoDB;
# Kill the server
# Fault 1: Extra file in innodb_data_file_path
SHOW TABLES;
Tables_in_test
t1
# Recovery and clean shutdown with old data file path
Pattern "\[ERROR\] \[[^]]*\] \[[^]]*\] unknown option '--innodb-nonexistent-option'" found
# Start with new data file path
INSERT INTO t1 VALUES (1);
# Kill the server
# Fault 2: Missing file in innodb_data_file_path
SHOW TABLES;
Tables_in_test
t1
# Recovery and shutdown with innodb-force-recovery (ignore unknown file)
Pattern "\[ERROR\] \[[^]]*\] \[[^]]*\] unknown option '--innodb-nonexistent-option'" found
# Cleanup
# No log should be generated while persistent tablespaces are idle
# FIXME: Something occasionally violates this (log_write_requests=1)
CREATE TEMPORARY TABLE t (a SERIAL) ENGINE=InnoDB;
SELECT * FROM t1;
a
1
DROP TABLE t1;
SELECT count>0 FROM information_schema.innodb_metrics
WHERE name='log_write_requests';
count>0
1
# restart
#
# Bug#33508534: ADDITION OF A NEW DATAFILE TO SYSTEM TABLESPACE IS NOT
#               REFLECTED IN I_S.FILES
#
SELECT @@innodb_data_file_path;
@@innodb_data_file_path
ibdata1:12M:autoextend
SELECT FILE_NAME FROM INFORMATION_SCHEMA.FILES
WHERE TABLESPACE_NAME='innodb_system';
FILE_NAME
./ibdata1
# restart: --datadir=MYSQLTEST_VARDIR/tmp/mydatadir --log-error=MYSQLD_LOG --innodb-data-file-path=ibdata1:12M;ibdata2:12M:autoextend
SELECT @@innodb_data_file_path;
@@innodb_data_file_path
ibdata1:12M;ibdata2:12M:autoextend
SELECT FILE_NAME FROM INFORMATION_SCHEMA.FILES
WHERE TABLESPACE_NAME='innodb_system';
FILE_NAME
./ibdata1
./ibdata2
Pattern "\[ERROR\] .* File .*foo: 'open' returned OS error 71. Cannot continue operation" found
# restart:
