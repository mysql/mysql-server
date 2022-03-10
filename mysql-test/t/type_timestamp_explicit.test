
 #Get deafult engine value
--let $DEFAULT_ENGINE = `select @@global.default_storage_engine`
let $explicit_defaults_for_timestamp=1;

--source type_timestamp.test
SET sql_mode = 'NO_ENGINE_SUBSTITUTION';
--echo #
--echo # WL6292 - Test cases to test new behavior with
--echo #          --explicit_defaults_for_timestamp
--echo # Almost all the scenario's required to test this WL, is already tested
--echo # by most of existing test case. Adding some basic tests here.
--echo #
CREATE TABLE t1 (f1 TIMESTAMP, f2 TIMESTAMP);
ALTER TABLE t1 ADD COLUMN (f3 TIMESTAMP NOT NULL);
ALTER TABLE t1 ADD COLUMN (f4 TIMESTAMP DEFAULT NULL);
ALTER TABLE t1 ADD COLUMN (f5 TIMESTAMP DEFAULT '0:0:0');
ALTER TABLE t1 ADD COLUMN (f6 TIMESTAMP ON UPDATE CURRENT_TIMESTAMP);


#Replace default engine value with static engine string
--replace_result $DEFAULT_ENGINE ENGINE
--echo # Following is expected out of SHOW CREATE TABLE
--echo #  `f1` timestamp NULL DEFAULT NULL,
--echo #  `f2` timestamp NULL DEFAULT NULL,
--echo #  `f3` timestamp NOT NULL,
--echo #  `f4` timestamp NULL DEFAULT NULL,
--echo #  `f5` timestamp NULL DEFAULT '0000-00-00 00:00:00'
--echo #  `f6` timestamp NULL DEFAULT NULL ON UPDATE CURRENT_TIMESTAMP

#Replace default engine value with static engine string
--replace_result $DEFAULT_ENGINE ENGINE
SHOW CREATE TABLE t1;

--echo # The new behavior affects CREATE SELECT with column definitions
--echo # before SELECT keyword. The columns f1,f2 in t2 do not get promoted
--echo # with the new behavior.

CREATE TABLE t2 (f1 TIMESTAMP, f2 TIMESTAMP) SELECT f1,f2,f3,f4,f5,f6 FROM t1;

#Replace default engine value with static engine string
--replace_result $DEFAULT_ENGINE ENGINE
SHOW CREATE TABLE t2;

DROP TABLE t1;
DROP TABLE t2;

--echo # With --explicit_defaults_for_timestamp,
--echo # inserting NULL in TIMESTAMP NOT NULL gives error, not NOW().
--echo # INT serves as model.

CREATE TABLE t1(
c1 TIMESTAMP NOT NULL,
c2 TIMESTAMP NOT NULL DEFAULT '2001-01-01 01:01:01',
c3 INT NOT NULL DEFAULT 42);

#Replace default engine value with static engine string
--replace_result $DEFAULT_ENGINE ENGINE
SHOW CREATE TABLE t1;
--error ER_BAD_NULL_ERROR
INSERT INTO t1 VALUES (NULL, DEFAULT, DEFAULT);
--error ER_BAD_NULL_ERROR
INSERT INTO t1 VALUES ('2005-05-05 06:06:06', NULL, DEFAULT);
--error ER_BAD_NULL_ERROR
INSERT INTO t1 VALUES ('2005-05-05 06:06:06', DEFAULT, NULL);
INSERT INTO t1 VALUES ('2005-05-05 06:06:06', DEFAULT, DEFAULT);
SELECT * FROM t1;
UPDATE t1 SET c1=NULL,c2=NULL,c3=NULL;
SELECT * FROM t1;
DROP TABLE t1;
SET sql_mode = default;

--echo #
--echo #Bug#19881933 : TABLE CREATION WITH TIMESTAMP IS REJECTED
--echo #

SET sql_mode=TRADITIONAL;

CREATE TABLE t1(
dummy INT,
i1_null_const INT NULL DEFAULT 42,
t1_null_now TIMESTAMP NULL DEFAULT CURRENT_TIMESTAMP,
t1_null_const TIMESTAMP NULL DEFAULT '2001-01-01 03:03:03',
i2_not_null_const INT NOT NULL DEFAULT 42,
t2_not_null_now TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
t2_not_null_const TIMESTAMP NOT NULL DEFAULT '2001-01-01 03:03:03',
i3_null INT NULL,
t3_null TIMESTAMP NULL,
i4_not_null INT NOT NULL,
t4_not_null TIMESTAMP NOT NULL);
SHOW CREATE TABLE t1;
DROP TABLE t1;

CREATE TABLE t1(a TIMESTAMP, b TIMESTAMP);
SHOW CREATE TABLE t1;
CREATE TABLE t2(a INT);
ALTER TABLE t2 ADD COLUMN b TIMESTAMP;
ALTER TABLE t2 ADD COLUMN c TIMESTAMP;
SHOW CREATE TABLE t2;
ALTER TABLE t2 ADD COLUMN d TIMESTAMP;
SHOW CREATE TABLE t2;
--error ER_INVALID_DEFAULT
ALTER TABLE t2 MODIFY b TIMESTAMP NOT NULL DEFAULT '0000-00-00 00:00:00';
--error ER_INVALID_DEFAULT
ALTER TABLE t2 MODIFY b TIMESTAMP DEFAULT '0000-00-00 00:00:00';
INSERT INTO t1 VALUES();
SET sql_mode='';
INSERT INTO t1 VALUES();
SET sql_mode=TRADITIONAL;
SELECT b FROM t1;
CREATE TABLE t3(a TIMESTAMP, b TIMESTAMP);
--echo # INSERT SELECT statement
INSERT INTO t3 SELECT * from t1;
--echo #CREATE SELECT with data in t1
CREATE TABLE t4 AS SELECT * FROM t1;
DELETE FROM t1;
DROP TABLE t4;
--echo #CREATE SELECT without data
CREATE TABLE t4 AS SELECT * FROM t1;
--echo #DML statements
INSERT INTO t1 VALUES(default, default);
INSERT INTO t1 VALUES(default(a), default(b));
UPDATE t1 SET b=default;
UPDATE t1 SET b=default(b);

CREATE TABLE t5(a TIMESTAMP NOT NULL);
CREATE TABLE t6(a TIMESTAMP, b TIMESTAMP NOT NULL);
DROP TABLE t1,t2,t3,t4,t5,t6;
SET sql_mode=default;


--echo #
--echo # BUG#27041502: ASSERTION `TYPE() != MYSQL_TYPE_TIMESTAMP'
--echo #               FAILED.

CREATE TABLE t1(fld1 TIMESTAMP,PRIMARY KEY (fld1));
CREATE TRIGGER trg BEFORE INSERT ON t1 FOR EACH ROW SET @a=1;

--echo # Without patch, an assert is triggered in the below
--echo # INSERT/UPDATE cases.
--error ER_BAD_NULL_ERROR
INSERT INTO t1 VALUES(NULL);
DROP TRIGGER trg;

CREATE TRIGGER trg AFTER INSERT ON t1 FOR EACH ROW SET @a=1;
--error ER_BAD_NULL_ERROR
INSERT INTO t1 VALUES(NULL);
DROP TRIGGER trg;

CREATE TRIGGER trg BEFORE UPDATE ON t1 FOR EACH ROW SET @a=1;
INSERT INTO t1 VALUES("20121231");
--error ER_BAD_NULL_ERROR
UPDATE t1 SET fld1=NULL WHERE fld1="20121231";
SELECT * from t1;
DROP TRIGGER trg;

CREATE TRIGGER trg AFTER UPDATE ON t1 FOR EACH ROW SET @a=1;
--error ER_BAD_NULL_ERROR
UPDATE t1 SET fld1=NULL WHERE fld1="20121231";
DROP TRIGGER trg;
DROP TABLE t1;
