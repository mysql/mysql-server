SET global innodb_print_ddl_logs = ON;

# In CONNECTION - DEFAULT
CREATE TABLE t (col1 INT PRIMARY KEY, col2 INT);
INSERT INTO t VALUES(1,1), (2,2), (3,3), (4,4);
SET DEBUG_SYNC= 'alter_table_inplace_after_lock_downgrade SIGNAL do_insert WAIT_FOR resume_ddl';
ALTER TABLE t DROP COLUMN col2;

# In CONNECTION - 1
SET DEBUG_SYNC= 'now WAIT_FOR do_insert';
START TRANSACTION;
INSERT INTO t VALUES(10,10);
set session debug = "+d, crash_after_flush_engine_log";
# Crash right after flushing InnoDB redo log
COMMIT;
ERROR HY000: Lost connection to MySQL server during query

# In CONNECTION - DEFAULT
ERROR HY000: Lost connection to MySQL server during query

# restart
SELECT * FROM t ORDER BY col1;
col1	col2
1	1
2	2
3	3
4	4
DROP TABLE t;
