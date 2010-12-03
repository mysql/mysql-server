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

eval SET SESSION DEFAULT_STORAGE_ENGINE = $engine_type;

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

#
# Bug#24989: The DEADLOCK error is improperly handled by InnoDB.
#
CREATE TABLE t1 (f1 int NOT NULL) ENGINE=InnoDB;
CREATE TABLE t2 (f2 int(11) NOT NULL PRIMARY KEY AUTO_INCREMENT) ENGINE=InnoDB;
DELIMITER |;
CREATE TRIGGER t1_bi before INSERT
    ON t1 FOR EACH ROW
BEGIN
  DECLARE CONTINUE HANDLER FOR SQLSTATE '40001' SET @a:= 'deadlock';
  DECLARE CONTINUE HANDLER FOR SQLEXCEPTION SET @a:= 'exception';
  INSERT INTO t2 (f2) VALUES (1);
  DELETE FROM t2 WHERE f2 = 1;
END;|

CREATE PROCEDURE proc24989()
BEGIN
  DECLARE CONTINUE HANDLER FOR SQLSTATE '40001' SET @b:= 'deadlock';
  DECLARE CONTINUE HANDLER FOR SQLEXCEPTION SET @a:= 'exception';
  INSERT INTO t2 (f2) VALUES (1);
  DELETE FROM t2 WHERE f2 = 1;
END;|

create procedure proc24989_2()
    deterministic
begin
  declare continue handler for sqlexception
    select 'Outer handler' as 'exception';

  insert into t1 values(1);
  select "continued";
end|

DELIMITER ;|

connect (con1,localhost,root,,);
connect (con2,localhost,root,,);

connection con1;
start transaction;
insert into t1 values(1);

connection con2;
start transaction;
insert into t2 values(123);
send insert into t1 values(1);

connection con1;
--sleep 1
insert into t1 values(1);

connection con2;
--error 1213
reap;
select @a;
# check that the whole transaction was rolled back
select * from t2;

connection con1;
commit;
start transaction;
insert into t1 values(1);

connection con2;
start transaction;
insert into t2 values(123);
send call proc24989();

connection con1;
--sleep 1
insert into t1 values(1);

connection con2;
reap;
select @a,@b;
# check that the whole transaction was rolled back
select * from t2;

connection con1;
commit;
start transaction;
insert into t1 values(1);

connection con2;
start transaction;
insert into t2 values(123);
send call proc24989_2();

connection con1;
--sleep 1
insert into t1 values(1);
commit;

connection con2;
reap;
# check that the whole transaction was rolled back
select * from t2;

disconnect con1;
disconnect con2;
connection default;
drop procedure proc24989;
drop procedure proc24989_2;
drop table t1,t2;

