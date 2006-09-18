# include/read_many_rows.inc
#
# Test how filesort and buffered-record-reads works
# This test needs a lot of time.
#
# The variables
#     $engine_type       -- storage engine to be tested
#     $other_engine_type -- storage engine <> $engine_type, if possible
#                           1. $other_engine_type must allow to store many rows
#                              without using non standard server options
#                              (does not need a t/read_many_rows_*-master.opt file)
#                           2. $other_engine_type must point to an all time
#                              available storage engine
#                              2006-08 MySQL 5.1 MyISAM and MEMORY only
# have to be set before sourcing this script.
#
# Last update:
# 2006-08-03 ML test refactored (MySQL 5.1)
#               main code t/innodb-big.test --> include/read_many_rows.inc
#

eval SET SESSION STORAGE_ENGINE = $engine_type;

--disable_warnings
DROP TABLE IF EXISTS t1, t2, t3, t4;
--enable_warnings

eval CREATE TABLE t1 (id INTEGER) ENGINE=$other_engine_type;
CREATE TABLE t2 (id INTEGER PRIMARY KEY);
CREATE TABLE t3 (a CHAR(32) PRIMARY KEY,id INTEGER);
eval CREATE TABLE t4 (a CHAR(32) PRIMARY KEY,id INTEGER) ENGINE=$other_engine_type;

INSERT INTO t1 (id) VALUES (1);
INSERT INTO t1 SELECT id+1 FROM t1;
INSERT INTO t1 SELECT id+2 FROM t1;
INSERT INTO t1 SELECT id+4 FROM t1;
INSERT INTO t1 SELECT id+8 FROM t1;
INSERT INTO t1 SELECT id+16 FROM t1;
INSERT INTO t1 SELECT id+32 FROM t1;
INSERT INTO t1 SELECT id+64 FROM t1;
INSERT INTO t1 SELECT id+128 FROM t1;
INSERT INTO t1 SELECT id+256 FROM t1;
INSERT INTO t1 SELECT id+512 FROM t1;
INSERT INTO t1 SELECT id+1024 FROM t1;
INSERT INTO t1 SELECT id+2048 FROM t1;
INSERT INTO t1 SELECT id+4096 FROM t1;
INSERT INTO t1 SELECT id+8192 FROM t1;
INSERT INTO t1 SELECT id+16384 FROM t1;
INSERT INTO t1 SELECT id+32768 FROM t1;
INSERT INTO t1 SELECT id+65536 FROM t1;
INSERT INTO t1 SELECT id+131072 FROM t1;
INSERT INTO t1 SELECT id+262144 FROM t1;
INSERT INTO t1 SELECT id+524288 FROM t1;
INSERT INTO t1 SELECT id+1048576 FROM t1;

INSERT INTO t2 SELECT * FROM t1;
INSERT INTO t3 SELECT CONCAT(id),id FROM t2 ORDER BY -id;
INSERT INTO t4 SELECT * FROM t3 ORDER BY CONCAT(a);
SELECT SUM(id) FROM t3;

DROP TABLE t1,t2,t3,t4;
