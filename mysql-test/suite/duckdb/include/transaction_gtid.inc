--source include/restart_mysqld.inc

CREATE TABLE t_duckdb (id int default 1 PRIMARY KEY, name varchar(10) default "NAME") ENGINE=duckdb;
CREATE TABLE t_innodb (id int default 1 PRIMARY KEY, name varchar(10) default "NAME") ENGINE=innodb;

--echo # Test gtid_executed of duckdb transaction

SET GTID_NEXT = 'cccccccc-cccc-cccc-cccc-cccccccccccc:1';
BEGIN;
INSERT INTO t_duckdb VALUES (1, "duckdb");
COMMIT;

--let $assert_cond= "[SELECT @@GLOBAL.GTID_EXECUTED]" LIKE "%cccccccc-cccc-cccc-cccc-cccccccccccc:1%"
--let $assert_text= "[SELECT @@GLOBAL.GTID_EXECUTED] ERROR."
--source include/assert.inc

SET GTID_NEXT = 'AUTOMATIC';

--source include/restart_mysqld.inc

--let $assert_cond= "[SELECT @@GLOBAL.GTID_EXECUTED]" LIKE "%cccccccc-cccc-cccc-cccc-cccccccccccc:1%"
--let $assert_text= "[SELECT @@GLOBAL.GTID_EXECUTED] ERROR."
--source include/assert.inc

--echo # Test gtid_executed of duckdb and innodb mix transaction
SET GTID_NEXT = 'cccccccc-cccc-cccc-cccc-cccccccccccc:2';
BEGIN;
INSERT INTO t_duckdb VALUES (2, "duckdb");
INSERT INTO t_innodb VALUES (1, "innodb");
COMMIT;

--let $assert_cond= "[SELECT @@GLOBAL.GTID_EXECUTED]" LIKE "%cccccccc-cccc-cccc-cccc-cccccccccccc:1-2%"
--let $assert_text= "[SELECT @@GLOBAL.GTID_EXECUTED] ERROR."
--source include/assert.inc

SET GTID_NEXT = 'AUTOMATIC';

--source include/restart_mysqld.inc

--let $assert_cond= "[SELECT @@GLOBAL.GTID_EXECUTED]" LIKE "%cccccccc-cccc-cccc-cccc-cccccccccccc:1-2%"
--let $assert_text= "[SELECT @@GLOBAL.GTID_EXECUTED] ERROR."
--source include/assert.inc

SET GTID_NEXT = 'AUTOMATIC';
DROP TABLE t_duckdb;
DROP TABLE t_innodb;
RESET BINARY LOGS AND GTIDS;
