include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
#
# Coverage for the user-level lock related functions
# GET_LOCK, RELEASE_LOCK, RELEASE_ALL_LOCKS, IS_USED_LOCK and IS_FREE_LOCK
# when replication with different binlog_formats is used.
# All these function are unsafe in statement based replication.
#
#
DROP TRIGGER IF EXISTS trig_t1_ins;
DROP TABLE IF EXISTS t1;
CREATE TABLE t1 (col1 INT, col2 INT);
# FT_RPL-1: Use all user-level lock related functions.
# FT_RPL-1.1: binlog_format = 'statement'
#             Reveal that all these statements get a warning about being
#             in that mode.
# FT_RPL-1.2: binlog_format = 'mixed'
#             Reveal that all these statements pass without warning.
# FT_RPL-1.3: binlog_format = 'row'
#             Reveal that all these statements pass without warning.
# The printing of warnings is temporary disabled. But there is some
# routine which checks the warnings and aborts in case of failure.
INSERT INTO t1 SELECT GET_LOCK('a',0),     1;
INSERT INTO t1 SELECT IS_USED_LOCK('a'),   2;
INSERT INTO t1 SELECT IS_FREE_LOCK('a'),   3;
INSERT INTO t1 SELECT RELEASE_LOCK('a'),   4;
INSERT INTO t1 SELECT RELEASE_ALL_LOCKS(), 5;
# FT_RPL-2: Check GET_LOCK within some INSERT TRIGGER
#           Reveal that statements harvest a warning or not according the
#           the binlog_format used.
CREATE TRIGGER trig_t1_ins BEFORE INSERT ON t1 FOR EACH ROW
BEGIN
SET @aux = GET_LOCK(2,0);
IF @aux <> 1 THEN
SIGNAL SQLSTATE '45000'
      SET MESSAGE_TEXT = 'LOCK 2 not got. Abort.', MYSQL_ERRNO = 9999;
END IF;
END;|
INSERT INTO t1 VALUES (6,6), (7,7), (8,8);
include/sync_slave_sql_with_master.inc
# FT_RPL-3: Check that there is no diff in table content between master
#           and slave in case binlog_format != 'statement'.
DROP TRIGGER trig_t1_ins;
DROP TABLE t1;
#
# End of tests
#
include/rpl_end.inc
