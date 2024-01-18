--source include/have_log_bin.inc
--source include/force_myisam_default.inc
--source include/have_myisam.inc
--source include/not_hypergraph.inc  # Depends on delete-all-rows optimization.

--echo #
--echo # BUG#57373: Multi update+InnoDB reports ER_KEY_NOT_FOUND if a
--echo #            table is updated twice
--echo #

# Results differ between storage engines.
# See multi_update_innodb.test for the InnoDB variant of this test

CREATE TABLE t1(
  pk INT,
  a INT,
  PRIMARY KEY (pk)
) ENGINE=MyISAM;

INSERT INTO t1 VALUES (0,0);
UPDATE t1 AS A, t1 AS B SET A.pk = 1, B.a = 2;
--echo
--echo # Should be (1,2)
SELECT * FROM t1;
DROP TABLE t1;

--echo #
--echo # BUG#11882110: UPDATE REPORTS ER_KEY_NOT_FOUND IF TABLE IS
--echo #               UPDATED TWICE
--echo #

# Results differ between storage engines. This test is to verify that
# the bugfix did NOT change behavior for MyISAM.
# See multi_update_innodb.test for the InnoDB variant of this test
CREATE TABLE t1 (
  col_int_key int,
  pk int,
  col_int int,
  key(col_int_key),
  primary key (pk)
) ENGINE=MyISAM;
INSERT INTO t1 VALUES (1,2,3);

--echo
CREATE TABLE t2 (
  col_int_key int,
  pk_1 int,
  pk_2 int,
  col_int int,
  key(col_int_key),
  primary key (pk_1,pk_2)
) ENGINE=MyISAM;
INSERT INTO t2 VALUES (1,2,3,4);

--echo
UPDATE t1 AS A NATURAL JOIN t1 B SET A.pk=5,B.pk=7;

--echo
SELECT * FROM t1;

--echo
UPDATE t2 AS A NATURAL JOIN t2 B SET A.pk_1=5,B.pk_1=7;
--echo
UPDATE t2 AS A NATURAL JOIN t2 B SET A.pk_2=10,B.pk_2=11;

--echo
SELECT * FROM t2;

DROP TABLE t1,t2;

--echo #
--echo #Bug 11757486 - 49539: NON-DESCRIPTIVE ERR (ERROR 0 FROM STORAGE ENGINE)
--echo #                      WITH MULTI-TABLE UPDATE
--echo #

CREATE TABLE table_11757486 (field1 tinyint) ENGINE=MYISAM;
INSERT INTO table_11757486 VALUES (0),(0);
SET SESSION SQL_MODE=default;
UPDATE IGNORE (SELECT 128 as col1) x, table_11757486 SET field1=x.col1;

--error ER_WARN_DATA_OUT_OF_RANGE
UPDATE (SELECT 128 as col1) x, table_11757486 SET field1=x.col1;

SET SESSION SQL_MODE='';
UPDATE IGNORE (SELECT 128 as col1) x, table_11757486 SET field1=x.col1;
DROP TABLE table_11757486;

#
# Bug#27716 multi-update did partially and has not binlogged
#

CREATE TABLE `t1` (
  `a` int(11) NOT NULL auto_increment,
  `b` int(11) default NULL,
  PRIMARY KEY  (`a`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1 ;

CREATE TABLE `t2` (
  `a` int(11) NOT NULL auto_increment,
  `b` int(11) default NULL,
  PRIMARY KEY  (`a`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1 ;

# as the test is about to see erroed queries in binlog
set @sav_binlog_format=  @@session.binlog_format;
set @@session.binlog_format= mixed;


# A. testing multi_update::send_error() effective update
insert into t1 values (1,1),(2,2);
insert into t2 values (1,1),(4,4);
reset binary logs and gtids;
--error ER_DUP_ENTRY
UPDATE t2,t1 SET t2.a=t1.a+2;
# check
select * from t2 /* must be (3,1), (4,4) */;
source include/rpl/deprecated/show_binlog_events.inc;

# B. testing multi_update::send_error() ineffective update
# (as there is a policy described at mysql_update() still go to binlog)
delete from t1;
delete from t2;
insert into t1 values (1,2),(3,4),(4,4);
insert into t2 values (1,2),(3,4),(4,4);
reset binary logs and gtids;
--error ER_DUP_ENTRY
UPDATE t2,t1  SET t2.a=t2.b where t2.a=t1.a;
source include/rpl/deprecated/show_binlog_events.inc;

# cleanup
drop table t1, t2;
set @@session.binlog_format= @sav_binlog_format;

--echo # Bug#31640267: Assertion `trans_safe || updated_rows == 0 || thd->get_transaction()

CREATE TABLE t1 (
  pk int NOT NULL,
  col_int int DEFAULT NULL,
  PRIMARY KEY (pk)
) ENGINE=MyISAM;

INSERT INTO t1 VALUES (2,9), (6,NULL), (0,7);

PREPARE s FROM
'UPDATE t1 AS a NATURAL JOIN t1 AS b SET a.col_int = 9, b.col_int = 9';
EXECUTE s;
SELECT ROW_COUNT();
EXECUTE s;
SELECT ROW_COUNT();
DEALLOCATE PREPARE s;

DROP TABLE t1;
