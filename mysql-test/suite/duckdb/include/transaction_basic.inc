--echo #
--echo # Atomicity
--echo #

CREATE TABLE t_duckdb (id int default 1 PRIMARY KEY, name varchar(10) default "NAME") ENGINE=duckdb;

INSERT INTO t_duckdb(id, name) VALUES (1, '1');

BEGIN;
INSERT INTO t_duckdb(id, name) VALUES (2, '2');
INSERT INTO t_duckdb(id, name) VALUES (3, '3');
ROLLBACK;

SELECT * FROM t_duckdb;

--echo #
--echo # Durability after instance restart
--echo #
--source include/restart_mysqld.inc
SELECT * FROM t_duckdb;

--echo # Restart should rollback tranction
BEGIN;
INSERT INTO t_duckdb(id, name) VALUES (4, '4');
INSERT INTO t_duckdb(id, name) VALUES (5, '5');
--source include/restart_mysqld.inc
SELECT * FROM t_duckdb;

--echo #
--echo # Autocommit
--echo #
SET autocommit = 1;
INSERT INTO t_duckdb(id, name) VALUES (3, '3');
--source include/restart_mysqld.inc
SELECT * FROM t_duckdb;

DROP TABLE t_duckdb;
CREATE TABLE t_duckdb (id int default 1 PRIMARY KEY, name varchar(10) default "NAME") ENGINE=duckdb;

--echo #
--echo # Isolation
--echo #

--connect(conn1,localhost,root,,test)
--connect(conn2,localhost,root,,test)

--connection conn1
BEGIN;
INSERT INTO t_duckdb(id, name) VALUES (1, '1');

--connection conn2
--echo # should be empty
SELECT * FROM t_duckdb;

--connection conn1
COMMIT;

--connection conn2
--echo # should print 1
SELECT * FROM t_duckdb;

DROP TABLE t_duckdb;

--echo #
--echo # Test trans atomicity when consit of innodb and duckdb
--echo #


CREATE TABLE t_duckdb (id int default 1 PRIMARY KEY, name varchar(10) default "NAME") ENGINE=duckdb;
CREATE TABLE t_innodb (id int default 1 PRIMARY KEY, name varchar(10) default "NAME") ENGINE=innodb;

--echo # Atomicity
INSERT INTO t_duckdb(id, name) VALUES (1, '1');
INSERT INTO t_innodb(id, name) VALUES (1, '1');

BEGIN;
INSERT INTO t_duckdb(id, name) VALUES (2, '2');
INSERT INTO t_innodb(id, name) VALUES (2, '2');
ROLLBACK;

SELECT * FROM t_duckdb;
SELECT * FROM t_innodb;


--echo # Isolation
DROP TABLE t_duckdb;
CREATE TABLE t_duckdb (id int default 1 PRIMARY KEY, name varchar(10) default "NAME") ENGINE=duckdb;

DROP TABLE t_innodb;
CREATE TABLE t_innodb (id int default 1 PRIMARY KEY, name varchar(10) default "NAME") ENGINE=duckdb;

--connection conn1
BEGIN;
INSERT INTO t_duckdb(id, name) VALUES (3, '3');
INSERT INTO t_innodb(id, name) VALUES (3, '3');

--connection conn2
SELECT * FROM t_duckdb;
SELECT * FROM t_innodb;

--connection conn1
COMMIT;
--connection conn2
SELECT * FROM t_duckdb;
SELECT * FROM t_innodb;


--echo # Durability after instance restart
--connection default
INSERT INTO t_duckdb(id, name) VALUES (4, '4');
INSERT INTO t_innodb(id, name) VALUES (4, '4');

--source include/restart_mysqld.inc
SELECT * FROM t_duckdb;
SELECT * FROM t_innodb;

DROP TABLE t_duckdb;
DROP TABLE t_innodb;

--disconnect conn1
--disconnect conn2

