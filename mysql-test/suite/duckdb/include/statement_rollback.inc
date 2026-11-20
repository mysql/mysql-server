# A failed DuckDB statement must roll back the complete transaction, including
# preceding successful statements, other transactional engines and buffered rows.
CREATE TABLE rollback_duckdb(id INT PRIMARY KEY, v INT NOT NULL) ENGINE=DuckDB;
CREATE TABLE rollback_innodb(id INT PRIMARY KEY, v INT NOT NULL) ENGINE=InnoDB;
INSERT INTO rollback_duckdb VALUES(100,100);
INSERT INTO rollback_innodb VALUES(100,100);

--echo # Explicit transaction: discard earlier writes in both engines.
SET @rollback_gtid=@@GLOBAL.gtid_executed;
START TRANSACTION;
INSERT INTO rollback_innodb VALUES(1,1);
INSERT INTO rollback_duckdb VALUES(1,1);
--error ER_BAD_NULL_ERROR
INSERT INTO rollback_duckdb VALUES(2,2),(3,NULL);
COMMIT;
SELECT @@GLOBAL.gtid_executed=@rollback_gtid AS gtid_unchanged;
SELECT * FROM rollback_duckdb ORDER BY id;
SELECT * FROM rollback_innodb ORDER BY id;

--echo # Autocommit disabled: discard all writes and close the transaction.
SET @rollback_gtid=@@GLOBAL.gtid_executed;
SET autocommit=0;
INSERT INTO rollback_duckdb VALUES(4,4);
INSERT INTO rollback_innodb VALUES(4,4);
--error ER_BAD_NULL_ERROR
INSERT INTO rollback_duckdb VALUES(5,5),(6,NULL);
COMMIT;
SELECT @@GLOBAL.gtid_executed=@rollback_gtid AS gtid_unchanged;
SELECT * FROM rollback_duckdb ORDER BY id;
SELECT * FROM rollback_innodb ORDER BY id;
SET autocommit=1;

--echo # An autocommit failure must not retain a partial insert either.
SET @rollback_gtid=@@GLOBAL.gtid_executed;
--error ER_BAD_NULL_ERROR
INSERT INTO rollback_duckdb VALUES(7,7),(8,NULL);
SELECT @@GLOBAL.gtid_executed=@rollback_gtid AS gtid_unchanged;
SELECT * FROM rollback_duckdb ORDER BY id;

--echo # The same connection can commit a fresh transaction after the failure.
START TRANSACTION;
INSERT INTO rollback_duckdb VALUES(9,9);
INSERT INTO rollback_innodb VALUES(9,9);
COMMIT;
SELECT * FROM rollback_duckdb ORDER BY id;
SELECT * FROM rollback_innodb ORDER BY id;

DROP TABLE rollback_duckdb, rollback_innodb;
