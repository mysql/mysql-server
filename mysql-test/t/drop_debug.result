#
# Bug#21625393 : Assert condition (e->usage() == 1) failure in
#                dd::cache::Shared_multi_map<T>::remove()
#
#
# Create MyISAM table, and drop it, but make drop fail
# before the object is deleted from the dd tables. Now,
# the object exists in the global data dictionary, but
# not in the SE.
CREATE TABLE t1 (i INT) ENGINE=MyISAM;
connect  con1, localhost, root;
SET SESSION DEBUG='+d,fail_while_dropping_dd_object';
DROP TABLE t1;
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
SET SESSION DEBUG='-d,fail_while_dropping_dd_object';
connection default;
# Drop the table for real. Use IF EXISTS clause to ignore
# the fact that table does not exist in SE.
# Without the fix this statement will fail with assert.
DROP TABLE IF EXISTS t1;
Warnings:
Warning	1017	Can't find file: 't1' (errno: 2 - No such file or directory)
connection con1;
disconnect con1;
connection default;
#
# WL#7743 "New data dictionary: changes to DDL-related parts of SE API"
#
# Systematic test coverage for changes in DROP TABLES and DROP DATABASE
# behavior.
#
# 1) Error handling by DROP TABLES.
#
#
# 1.a) DROP TABLES statement which fails due to missing table
#      should not have any side-effects.
CREATE TABLE t_m (t_m INT) ENGINE=MyISAM;
CREATE TABLE t_i (t_i INT) ENGINE=InnoDB;
CREATE TEMPORARY TABLE tt_m (tt_m INT) ENGINE=MyISAM;
CREATE TEMPORARY TABLE tt_i (tt_i INT) ENGINE=InnoDB;
# Notice that all missing tables are reported.
DROP TABLES t_m, t_i, tt_m, tt_i, t_no_such_table, t_no_such_table_either;
ERROR 42S02: Unknown table 'test.t_no_such_table,test.t_no_such_table_either'
# All tables are still there.
SELECT * FROM t_m, t_i, tt_m, tt_i;
t_m	t_i	tt_m	tt_i
# Notice that all missing tables are reported.
DROP TEMPORARY TABLES tt_m, tt_i, tt_no_such_table, tt_no_such_table_either;
ERROR 42S02: Unknown table 'test.tt_no_such_table,test.tt_no_such_table_either'
# All tables are still there.
SELECT * FROM tt_m, tt_i;
tt_m	tt_i
#
# 1.b) DROP TABLES IF EXISTS should ignore missing tables
#      as expected and drop existing tables.
#
# Notice that all missing tables are reported in warning.
DROP TABLES IF EXISTS t_m, t_i, tt_m, tt_i, t_no_such_table, t_no_such_table_either;
Warnings:
Note	1051	Unknown table 'test.t_no_such_table'
Note	1051	Unknown table 'test.t_no_such_table_either'
# All existing tables are dropped.
SELECT * FROM t_m;
ERROR 42S02: Table 'test.t_m' doesn't exist
SELECT * FROM t_i;
ERROR 42S02: Table 'test.t_i' doesn't exist
CREATE TEMPORARY TABLE tt_m (tt_m INT) ENGINE=MyISAM;
CREATE TEMPORARY TABLE tt_i (tt_i INT) ENGINE=InnoDB;
# Notice that all missing tables are reported in warning.
DROP TEMPORARY TABLES IF EXISTS tt_m, tt_i, tt_no_such_table, tt_no_such_table_either;
Warnings:
Note	1051	Unknown table 'test.tt_no_such_table'
Note	1051	Unknown table 'test.tt_no_such_table_either'
# All existing tables are dropped.
SELECT * FROM tt_m;
ERROR 42S02: Table 'test.tt_m' doesn't exist
SELECT * FROM tt_i;
ERROR 42S02: Table 'test.tt_i' doesn't exist
#
# 1.c) DROP TABLES which fails due to foreign key error does
#      not have side effect.
CREATE TABLE t_m (t_m INT) ENGINE=MyISAM;
CREATE TABLE t_i_1 (t_i_1 INT) ENGINE=InnoDB;
CREATE TABLE t_i_2 (t_i_2 INT PRIMARY KEY) ENGINE=InnoDB;
CREATE TABLE t_i_3 (t_i_3 INT, FOREIGN KEY(t_i_3) REFERENCES t_i_2(t_i_2)) ENGINE=InnoDB;
DROP TABLES t_m, t_i_1, t_i_2;
ERROR HY000: Cannot drop table 't_i_2' referenced by a foreign key constraint 't_i_3_ibfk_1' on table 't_i_3'.
# All tables are still there.
SELECT * FROM t_m;
t_m
SELECT * FROM t_i_1;
t_i_1
SELECT * FROM t_i_2;
t_i_2
#
# 1.d) DROP TABLES which fails due to SE error might have side
#      effect. Tables in engines which do not support atomic DDL
#      which we have managed to drop before error stay dropped.
#      Removal of InnoDB tables should be rolled back.
SET SESSION DEBUG='+d,rm_table_no_locks_abort_after_atomic_tables';
DROP TABLES t_m, t_i_1;
ERROR HY000: Unknown error
SET SESSION DEBUG='-d,rm_table_no_locks_abort_after_atomic_tables';
SELECT * FROM t_m;
ERROR 42S02: Table 'test.t_m' doesn't exist
SELECT * FROM t_i_1;
t_i_1
#
# 1.e) DROP TABLES which fails due to SE error and involves only
#      tables in engines supporting atomic DDL should not have
#      side effects/should be rolled back.
SET SESSION DEBUG='+d,rm_table_no_locks_abort_after_atomic_tables';
DROP TABLES t_i_1, t_i_3;
ERROR HY000: Unknown error
SET SESSION DEBUG='-d,rm_table_no_locks_abort_after_atomic_tables';
SELECT * FROM t_i_1;
t_i_1
SELECT * FROM t_i_3;
t_i_3
# Clean-up.
DROP TABLES t_i_1, t_i_3, t_i_2;
#
# 2) Binary logging and GTID handling for DROP TABLES statements.
#
# 2.a) Binary logging for successfull DROP TABLES statement is
#      covered by binlog_stm_mix_innodb_myisam,
#      rpl_mixed_drop_create_temp_table and other similar
#      tests.
#
# 2.b) Binary logging for failed DROP TABLES statement are
#      additionally covered by rpl_binlog_failed_drop_table,
#      rpl_gtid/no_gtid_split_statements tests.
#
# 2.c) GTID handling for DROP TABLES statement is covered by
#      rpl_gtid_split_statements and 
#      no_binlog_gtid_next_partially_failed_stmts tests.
#
#
# 3) DROP TABLES IF EXISTS should be able to delete tables with
#    entries in the data-dictionary, but absent from SE.
#
CREATE TABLE t1 (a INT) ENGINE=MyISAM;
# Remove table from SE manually.
FLUSH TABLE t1;
# Plain DROP TABLES reports an error.
DROP TABLE t1;
ERROR HY000: Storage engine can't drop table 'test.t1' because it is missing. Use DROP TABLE IF EXISTS to remove it from data-dictionary.
# DROP TABLES IF EXISTS successfully drops table.
DROP TABLE IF EXISTS t1;
Warnings:
Warning	1017	Can't find file: 't1' (errno: 2 - No such file or directory)
#
# 4) Error handling by DROP DATABASE.
#
#
# 4.a) DROP DATABASE which fails due to foreign key error should
#      not have side effect.
CREATE DATABASE mysqltest;
CREATE TABLE mysqltest.t_m (t_m INT) ENGINE=MyISAM;
CREATE TABLE mysqltest.t_i_1 (t_i_1 INT) ENGINE= InnoDB;
CREATE TABLE mysqltest.t_i_2 (t_i_2 INT PRIMARY KEY) ENGINE= InnoDB;
CREATE FUNCTION mysqltest.f1() RETURNS INT RETURN 0;
CREATE TABLE t1 (fk INT,
FOREIGN KEY (fk) REFERENCES mysqltest.t_i_2(t_i_2))
ENGINE=InnoDB;
DROP DATABASE mysqltest;
ERROR HY000: Cannot drop table 't_i_2' referenced by a foreign key constraint 't1_ibfk_1' on table 't1'.
# Database and all tables are still there.
SELECT * FROM mysqltest.t_m;
t_m
SELECT * FROM mysqltest.t_i_1;
t_i_1
SELECT * FROM mysqltest.t_i_2;
t_i_2
# Stored function f1() is still there too.
SELECT mysqltest.f1();
mysqltest.f1()
0
#
# 4.b) DROP DATABASE which fails due to SE error might have side
#      effect. Tables in engines which do not support atomic DDL
#      which we have managed to drop before error stay dropped.
#      Removal of InnoDB tables should be rolled back.
DROP TABLE t1;
SET SESSION DEBUG='+d,rm_db_fail_after_dropping_tables';
DROP DATABASE mysqltest;
ERROR HY000: Unknown error
SET SESSION DEBUG='-d,rm_db_fail_after_dropping_tables';
SELECT * FROM mysqltest.t_m;
ERROR 42S02: Table 'mysqltest.t_m' doesn't exist
# Database and tables t_i_1, t_i_2 are still there.
SELECT * FROM mysqltest.t_i_1;
t_i_1
SELECT * FROM mysqltest.t_i_2;
t_i_2
# Stored function f1() is still there too.
SELECT mysqltest.f1();
mysqltest.f1()
0
#
# 4.c) DROP DATABASE which fails due to SE error and involves only
#      tables in engines supporting atomic DDL should not have side
#      effects/should be rolled back.
SET SESSION DEBUG='+d,rm_db_fail_after_dropping_tables';
DROP DATABASE mysqltest;
ERROR HY000: Unknown error
SET SESSION DEBUG='-d,rm_db_fail_after_dropping_tables';
# Database and tables t_i_1, t_i_2 are still there.
SELECT * FROM mysqltest.t_i_1;
t_i_1
SELECT * FROM mysqltest.t_i_2;
t_i_2
# Stored function f1() is still there too.
SELECT mysqltest.f1();
mysqltest.f1()
0
#
# 4.d) DROP DATABASE which fails due to failure to drop routine
#      might have side effect. Tables in engines which do not
#      support atomic DDL stay dropped. Removal of InnoDB tables
#      should be rolled back.
CREATE TABLE mysqltest.t_m (t_m INT) ENGINE=MyISAM;
SET SESSION DEBUG='+d,fail_drop_db_routines';
DROP DATABASE mysqltest;
ERROR HY000: Failed to DROP ROUTINE 
SET SESSION DEBUG='-d,fail_drop_db_routines';
SELECT * FROM mysqltest.t_m;
ERROR 42S02: Table 'mysqltest.t_m' doesn't exist
# Database and tables t_i_1, t_i_2 are still there.
SELECT * FROM mysqltest.t_i_1;
t_i_1
SELECT * FROM mysqltest.t_i_2;
t_i_2
# Stored function f1() is still there too.
SELECT mysqltest.f1();
mysqltest.f1()
0
DROP DATABASE mysqltest;
#
# 5) Binary logging and GTID handling for DROP DATABASE.
#
# GTID handling and Binary logging for successfull and
# failed DROP DATABASE statement are covered by
# rpl_gtid/no_gtid_split_statements_debug tests.
#
#
# Additional coverage for hidden tables handling by DROP DATABASE.
#
CREATE DATABASE mysqltest;
# Create hidden '#sql...' table in mysqltest by starting
# non-atomic ALTER TABLE and crashing the server in the
# middle of it.
CREATE TABLE mysqltest.t1 (i INT) ENGINE=MYISAM;
SET DEBUG='+d,crash_copy_before_commit';
ALTER TABLE mysqltest.t1 ADD COLUMN j INT;
ERROR HY000: Lost connection to MySQL server during query
# restart
# Check that after restart this hidden table is there.
#sqlXXXX.MYD
t1.MYD
# And that DROP DATABASE can remove this table, without problems.
DROP DATABASE mysqltest;
#
# Bug#28923782: DD CRASHES ON ASSERT IF HA_COMMIT_TRANS() RETURNS ERROR
#
# Check result of ha_commit_trans() before committing changes of the DD
# objects.
#
CREATE TABLE t(i INT);
SET debug = '+d,simulate_failure_in_before_commit_hook';
# This test case works only with binlogging, because it depends on the
# 2pc coordinated by the binlog. Hence, to avoid failure when running
# the test with --skip-log-bin, we temporarily suppress the query- and
# result log.
SET debug = '-d,simulate_failure_in_before_commit_hook';
DROP TABLE t;
