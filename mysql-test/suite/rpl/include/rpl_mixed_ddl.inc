#########################################
# Author: Serge Kozlov skozlov@mysql.com
# Date: 07/10/2006
# Purpose: testing the replication in mixed mode
# Requirements: define binlog format for mysqld as in example below:
# ./mysql-test-run.pl --mysqld=--binlog-format=mixed
#########################################

--source include/master-slave.inc

# Check MIXED on both master and slave
connection master;
--echo ==========MASTER==========
--source suite/rpl/include/rpl_mixed_show_binlog_format.inc
connection slave;
--echo ==========SLAVE===========
--source suite/rpl/include/rpl_mixed_show_binlog_format.inc
connection master;


# DDL for databases
--echo
--echo ******************** DDL for database ********************
--disable_warnings
DROP DATABASE IF EXISTS test_rpl;
DROP DATABASE IF EXISTS test_rpl_1;
--enable_warnings
CREATE DATABASE test_rpl_1 CHARACTER SET utf8 COLLATE utf8_general_ci;
ALTER DATABASE test_rpl_1 CHARACTER SET latin1 COLLATE latin1_general_ci;
DROP DATABASE test_rpl_1;
CREATE DATABASE test_rpl CHARACTER SET utf8 COLLATE utf8_general_ci;
ALTER DATABASE test_rpl CHARACTER SET latin1 COLLATE latin1_swedish_ci;
--source suite/rpl/include/rpl_mixed_check_db.inc

USE test_rpl;

# DDL for tables
--echo
--echo ******************** DDL for tables ********************
eval CREATE TABLE t0 (a int auto_increment not null, c int not null, PRIMARY KEY(a), KEY index2 (c)) ENGINE=$engine_type;
ALTER TABLE t0 DROP INDEX index2;
ALTER TABLE t0 ADD COLUMN b char(254);
ALTER TABLE t0 ADD INDEX index1 (b);
ALTER TABLE t0 DROP COLUMN c;
RENAME TABLE t0 TO t1;
CREATE TABLE t2 LIKE t1;
--source suite/rpl/include/rpl_mixed_check_table.inc

# DDL for logfiles
# skipped

# DDL for tablespaces
# skipped

# DDL for indexes
--echo
--echo ******************** DDL for indexes ********************
ALTER TABLE t2 ADD COLUMN d datetime;
CREATE INDEX index2 on t2 (d);
CREATE INDEX index3 on t2 (a, d);
--source suite/rpl/include/rpl_mixed_check_table.inc
ALTER TABLE t2 DROP COLUMN d;


# BINLOG EVENTS
--echo
--echo
--echo ******************** SHOW BINLOG EVENTS ********************
source include/show_binlog_events.inc;
sync_slave_with_master;
# as we're using UUID we don't SELECT but use "diff" like in rpl_row_UUID
--exec $MYSQL_DUMP --compact --order-by-primary --skip-extended-insert --no-create-info test_rpl > $MYSQLTEST_VARDIR/tmp/rpl_switch_stm_row_mixed_master.sql
--exec $MYSQL_DUMP_SLAVE --compact --order-by-primary --skip-extended-insert --no-create-info test_rpl > $MYSQLTEST_VARDIR/tmp/rpl_switch_stm_row_mixed_slave.sql

connection master;
drop database test_rpl;
sync_slave_with_master;

# Let's compare. Note: If they match test will pass, if they do not match
# the test will show that the diff statement failed and not reject file
# will be created. You will need to go to the mysql-test dir and diff
# the files your self to see what is not matching

--diff_files $MYSQLTEST_VARDIR/tmp/rpl_switch_stm_row_mixed_master.sql $MYSQLTEST_VARDIR/tmp/rpl_switch_stm_row_mixed_slave.sql
