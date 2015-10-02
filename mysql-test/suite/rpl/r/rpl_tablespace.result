#########################################################################
#
# REPLICATION TABLESPACE RELATED TESTS.
#
#########################################################################
#
# Bug#21081898 INNODB: TABLESPACE RELATED DDLS ARE NOT COMMITTING ONGOING
# TRANSACTION
#
# Patch for this bug also fixes Bug#21082728: ASSERT
# `THD->VARIABLES.GTID_NEXT.TYPE != UNDEFINED_GROUP'
# FOR DML+TABLESPACE DDL
#
# #######################################################################
include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
[On MASTER]
# TEST Bug#21082728: Execute CREATE TABLESPACE inside active transaction
# with GTID_NEXT set. Check for proper error code and transaction state
# by verifying table content after issuing a ROLLBACK.
CREATE TABLE t1 (a INT PRIMARY KEY) ENGINE=INNODB;
SET @saved_gtid_next= @@session.gtid_next;
SET GTID_NEXT='aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1';
BEGIN;
INSERT INTO t1 VALUES (1);
CREATE TABLESPACE ts1 ADD DATAFILE './ts1.ibd' ENGINE=INNODB;
ERROR HY000: Cannot execute statements with implicit commit inside a transaction when @@SESSION.GTID_NEXT == 'UUID:NUMBER'.
ROLLBACK;
SET GTID_NEXT='aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:2';
# Select should return Empty result
SELECT * FROM t1;
a
# Issue ROLLBACK to release GTID
ROLLBACK;
# TEST Bug#21081898: Execute CREATE TABLESPACE inside active transaction
# with GTID_NEXT=automatic. Check table content for impliciti commit.
SET GTID_NEXT = 'AUTOMATIC';
BEGIN;
INSERT INTO t1 VALUES (1);
CREATE TABLESPACE ts1 ADD DATAFILE './ts1.ibd' ENGINE=INNODB;
ROLLBACK;
# Select should return one row inserted above
SELECT * FROM t1;
a
1
# Cleanup
DROP TABLE t1;
DROP TABLESPACE ts1;
SET GTID_NEXT= @saved_gtid_next;
# "STOP SLAVE" and verify that the SQL thread was properly stopped and
# left no gaps.
include/rpl_end.inc
