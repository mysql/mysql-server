# t/merge_innodb.test
#
# Tests with MERGE tables over InnoDB tables
#

--source include/force_myisam_default.inc
--source include/have_myisam.inc


--disable_warnings
DROP TABLE IF EXISTS t1, t2, t3, t4, t5;
--enable_warnings

#
# Bug#30491 - MERGE doesn't report error when one table is Innodb
#
CREATE TABLE t1 (c1 varchar(100)) ENGINE=MyISAM;
CREATE TABLE t2 (c1 varchar(100)) ENGINE=MyISAM;
CREATE TABLE t3 (c1 varchar(100)) ENGINE=InnoDB;
INSERT INTO t1 VALUES ('Ann'), ('Alice');
INSERT INTO t2 VALUES ('Bob'), ('Brian');
INSERT INTO t3 VALUES ('Chris'), ('Charlie');
CREATE TABLE t4 (c1 varchar(100)) ENGINE=MRG_MYISAM UNION=(t1,t2)
  INSERT_METHOD=LAST;
CREATE TABLE t5 (c1 varchar(100)) ENGINE=MRG_MYISAM UNION=(t1,t3)
  INSERT_METHOD=LAST;
--error ER_WRONG_MRG_TABLE
SELECT * FROM t5;
SELECT * FROM t4;
ALTER TABLE t2 ENGINE=InnoDB;
--error ER_WRONG_MRG_TABLE
SELECT * FROM t4;
DELETE FROM t2 LIMIT 1;
--error ER_WRONG_MRG_TABLE
SELECT * FROM t4;
--error ER_WRONG_MRG_TABLE
INSERT INTO t4 VALUES ('Beware');
--error ER_WRONG_MRG_TABLE
SELECT * FROM t4;
SELECT * FROM t2;
SELECT * FROM t1;
DROP TABLE t1, t2, t3, t4, t5;

