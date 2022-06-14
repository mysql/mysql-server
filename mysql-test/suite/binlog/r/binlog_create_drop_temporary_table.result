SET @@session.binlog_format= MIXED;
SELECT @@session.binlog_format;
@@session.binlog_format
MIXED
CREATE TABLE t1(a INT) ENGINE=InnoDB;
CREATE TEMPORARY TABLE tmp1 ENGINE=InnoDB SELECT * FROM t1;
DROP TEMPORARY TABLE tmp1;
CREATE TEMPORARY TABLE tmp5 (a INT) ENGINE=InnoDB;
DROP TEMPORARY TABLE tmp5;
CREATE TEMPORARY TABLE tmp7 LIKE t1;
DROP TEMPORARY TABLE tmp7;
include/assert.inc [CREATE/DROP TEMPORARY TABLE is not written into binlog when @@session.binlog_format= MIXED.]
SET @@session.binlog_format= ROW;
SELECT @@session.binlog_format;
@@session.binlog_format
ROW
CREATE TEMPORARY TABLE tmp1 ENGINE=InnoDB SELECT * FROM t1;
DROP TEMPORARY TABLE tmp1;
CREATE TEMPORARY TABLE tmp5 (a INT) ENGINE=InnoDB;
DROP TEMPORARY TABLE tmp5;
CREATE TEMPORARY TABLE tmp7 LIKE t1;
DROP TEMPORARY TABLE tmp7;
include/assert.inc [CREATE/DROP TEMPORARY TABLE is not written into binlog when @@session.binlog_format= ROW.]
DROP TABLE t1;
