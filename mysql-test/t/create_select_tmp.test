# Testcase for BUG#4551
# The bug was that when the table was TEMPORARY, it was not deleted if
# the CREATE SELECT failed (the code intended too, but it actually
# didn't). And as the CREATE TEMPORARY TABLE was not written to the
# binlog if it was a transactional table, it resulted in an
# inconsistency between binlog and the internal list of temp tables.

# This does not work for RBR yet.
--source include/rpl/force_binlog_format_statement.inc

--disable_query_log
CALL mtr.add_suppression("Unsafe statement written to the binary log using statement format since BINLOG_FORMAT = STATEMENT");
--enable_query_log

CREATE TABLE t1 ( a int );
INSERT INTO t1 VALUES (1),(2),(1);
--error ER_DUP_ENTRY
CREATE TABLE t2 ( PRIMARY KEY (a) ) ENGINE=INNODB SELECT a FROM t1;
--error ER_NO_SUCH_TABLE 
select * from t2;
--error ER_DUP_ENTRY
CREATE TEMPORARY TABLE t2 ( PRIMARY KEY (a) ) ENGINE=INNODB SELECT a FROM t1;
--error ER_NO_SUCH_TABLE
select * from t2;
drop table t1;

--source include/rpl/restore_default_binlog_format.inc
