 --echo #
--echo # Crash between binlog write and duckdb commit
--echo # It should not cause inconsistency.
--echo #


CREATE TABLE t_duckdb (id int default 1 PRIMARY KEY, name varchar(10) default "NAME") ENGINE=duckdb;
CREATE TABLE t_innodb (id int default 1 PRIMARY KEY, name varchar(10) default "NAME") ENGINE=innodb;

--echo # Check duckdb transaction

SET GTID_NEXT = '0ccccccc-cccc-cccc-cccc-cccccccccccc:1';

BEGIN;
INSERT INTO t_duckdb VALUES (1, "1");
COMMIT;

SET SESSION DEBUG="+d,crash_after_binlog_sync";
SET GTID_NEXT = '0ccccccc-cccc-cccc-cccc-cccccccccccc:2';

BEGIN;
INSERT INTO t_duckdb VALUES(2, '2');
--source include/expect_crash.inc

--error 0,CR_SERVER_LOST,ER_INTERNAL_ERROR
COMMIT;

--source include/start_mysqld.inc

SET SESSION DEBUG="-d,crash_after_binlog_sync";

--echo # Duckdb doesn't support 2pc, but we have a mechanism to keep the
--echo # consistenty between duckdb engine, innodb engine and binlog.
SELECT * FROM t_duckdb;
SELECT SUBSTRING_INDEX(SUBSTRING_INDEX(@@global.gtid_executed, ',', FIND_IN_SET('0ccccccc-cccc-cccc-cccc-cccccccccccc', REPLACE(@@global.gtid_executed, ':', ','))), ',', -1) AS filtered_gtid;

--echo # Check innodb transaction

SET GTID_NEXT = '0ccccccc-cccc-cccc-cccc-cccccccccccc:3';

BEGIN;
INSERT INTO t_innodb VALUES (1, "1");
COMMIT;

SET SESSION DEBUG="+d,crash_after_binlog_sync";
SET GTID_NEXT = '0ccccccc-cccc-cccc-cccc-cccccccccccc:4';

BEGIN;
INSERT INTO t_innodb VALUES(2, '2');
--source include/expect_crash.inc

--error 0,CR_SERVER_LOST,ER_INTERNAL_ERROR
COMMIT;

--source include/start_mysqld.inc

SET SESSION DEBUG="-d,crash_after_binlog_sync";

--echo # innodb can keep consistent between innodb and binlog.
SELECT * FROM t_innodb;
SELECT SUBSTRING_INDEX(SUBSTRING_INDEX(@@global.gtid_executed, ',', FIND_IN_SET('0ccccccc-cccc-cccc-cccc-cccccccccccc', REPLACE(@@global.gtid_executed, ':', ','))), ',', -1) AS filtered_gtid;

--echo # Check duckdb and innodb mix transaction

SET GTID_NEXT = '0ccccccc-cccc-cccc-cccc-cccccccccccc:5';

BEGIN;
INSERT INTO t_innodb VALUES (100, "100");
INSERT INTO t_duckdb VALUES (100, "100");

COMMIT;

SET SESSION DEBUG="+d,crash_after_binlog_sync";
SET GTID_NEXT = '0ccccccc-cccc-cccc-cccc-cccccccccccc:6';

BEGIN;
INSERT INTO t_innodb VALUES (101, "101");
INSERT INTO t_duckdb VALUES (101, "101");

--source include/expect_crash.inc

--error 0,CR_SERVER_LOST,ER_INTERNAL_ERROR
COMMIT;

--source include/start_mysqld.inc

SET SESSION DEBUG="-d,crash_after_binlog_sync";

--echo # This trancation consists both innodb and duckdb operation.
--echo # The whole transaction is set_no_2pc .
--echo # So innodb and duckdb both lost the data of 101.
--echo # This is also by design.

SELECT * FROM t_innodb;
SELECT * FROM t_duckdb;
SELECT SUBSTRING_INDEX(SUBSTRING_INDEX(@@global.gtid_executed, ',', FIND_IN_SET('0ccccccc-cccc-cccc-cccc-cccccccccccc', REPLACE(@@global.gtid_executed, ':', ','))), ',', -1) AS filtered_gtid;

SET GTID_NEXT = AUTOMATIC;

DROP TABLE t_duckdb;
DROP TABLE t_innodb;

RESET BINARY LOGS AND GTIDS;
