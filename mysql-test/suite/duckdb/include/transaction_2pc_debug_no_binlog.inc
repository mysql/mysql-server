--echo #
--echo # Crash between binlog write and duckdb commit
--echo # May leave inconsistency of duckdb and binlog gtid
--echo #


CREATE TABLE t_duckdb (id int default 1 PRIMARY KEY, name varchar(10) default "NAME") ENGINE=duckdb;
CREATE TABLE t_innodb (id int default 1 PRIMARY KEY, name varchar(10) default "NAME") ENGINE=innodb;

--echo # ① Check duckdb transaction

SET GTID_NEXT = '0ccccccc-cccc-cccc-cccc-cccccccccccc:1';

BEGIN;
INSERT INTO t_duckdb VALUES (1, "1");
COMMIT;

SET SESSION DEBUG="+d,crash_before_duckdb_commit";
SET GTID_NEXT = '0ccccccc-cccc-cccc-cccc-cccccccccccc:2';

BEGIN;
INSERT INTO t_duckdb VALUES(2, '2');
--source include/expect_crash.inc

--error 0,CR_SERVER_LOST,ER_INTERNAL_ERROR
COMMIT;

--source include/start_mysqld.inc

SET SESSION DEBUG="-d,crash_before_duckdb_commit";

--echo # duckdb not support 2pc commit, so the data of '2' is lost by duckdb but binlog consist this.
--echo # We admit this inconsistency between duckdb and binlog is by design.
SELECT * FROM t_duckdb;
SELECT SUBSTRING_INDEX(SUBSTRING_INDEX(@@global.gtid_executed, ',', FIND_IN_SET('0ccccccc-cccc-cccc-cccc-cccccccccccc', REPLACE(@@global.gtid_executed, ':', ','))), ',', -1) AS filtered_gtid;


--echo # ② Check duckdb and innodb mix transaction

SET GTID_NEXT = '0ccccccc-cccc-cccc-cccc-cccccccccccc:3';

BEGIN;
INSERT INTO t_innodb VALUES (3, "3");
INSERT INTO t_duckdb VALUES (3, "3");

COMMIT;

SET SESSION DEBUG="+d,crash_before_duckdb_commit";
SET GTID_NEXT = '0ccccccc-cccc-cccc-cccc-cccccccccccc:4';

BEGIN;
INSERT INTO t_innodb VALUES (4, "4");
INSERT INTO t_duckdb VALUES (4, "4");

--source include/expect_crash.inc

--error 0,CR_SERVER_LOST,ER_INTERNAL_ERROR
COMMIT;

--source include/start_mysqld.inc

SET SESSION DEBUG="-d,crash_before_duckdb_commit";

--echo # This trancation consists both innodb and duckdb operation.
--echo # gtid_executed is persisted by innodb.
--echo # The commit order of duckdb and inoodb is undefined(mainly by the order of regist trx)
--echo # The case ③ is the situation where duckdb is committed before innodb.
--echo # So the data of 4 is lost by innodb and duckdb both, and gtid_execute also lost the :4 gtid.

SELECT * FROM t_innodb;
SELECT * FROM t_duckdb;
SELECT SUBSTRING_INDEX(SUBSTRING_INDEX(@@global.gtid_executed, ',', FIND_IN_SET('0ccccccc-cccc-cccc-cccc-cccccccccccc', REPLACE(@@global.gtid_executed, ':', ','))), ',', -1) AS filtered_gtid;


--echo # ④ Check duckdb and innodb mix transaction2

SET GTID_NEXT = '0ccccccc-cccc-cccc-cccc-cccccccccccc:5';

BEGIN;
INSERT INTO t_innodb VALUES (5, "5");
INSERT INTO t_duckdb VALUES (5, "5");

COMMIT;

SET SESSION DEBUG="+d,crash_before_duckdb_commit";
SET GTID_NEXT = '0ccccccc-cccc-cccc-cccc-cccccccccccc:6';

BEGIN;
INSERT INTO t_duckdb VALUES (6, "6");
INSERT INTO t_innodb VALUES (6, "6");

--source include/expect_crash.inc

--error 0,CR_SERVER_LOST,ER_INTERNAL_ERROR
COMMIT;

--source include/start_mysqld.inc

SET SESSION DEBUG="-d,crash_before_duckdb_commit";

--echo # This trancation consists both innodb and duckdb operation.
--echo # gtid_executed is persisted by innodb.
--echo # The commit order of duckdb and inoodb is undefined(mainly by the order of regist trx)
--echo # The case ④ is the situation where duckdb is committed after innodb.
--echo # So the data('6') is lost by duckdb only, innodb and gtid_execute have the gtid('6') remained.


SELECT * FROM t_innodb;
SELECT * FROM t_duckdb;
SELECT SUBSTRING_INDEX(SUBSTRING_INDEX(@@global.gtid_executed, ',', FIND_IN_SET('0ccccccc-cccc-cccc-cccc-cccccccccccc', REPLACE(@@global.gtid_executed, ':', ','))), ',', -1) AS filtered_gtid;


SET GTID_NEXT = AUTOMATIC;

DROP TABLE t_duckdb;
DROP TABLE t_innodb;

RESET BINARY LOGS AND GTIDS;
