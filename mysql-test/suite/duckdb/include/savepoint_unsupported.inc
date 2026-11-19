--echo # DuckDB must reject a savepoint after joining the transaction.
START TRANSACTION;
INSERT INTO savepoint_duckdb VALUES(1);
--error ER_CHECK_NOT_IMPLEMENTED
SAVEPOINT unsupported;
--error ER_SP_DOES_NOT_EXIST
ROLLBACK TO SAVEPOINT unsupported;
ROLLBACK;
SELECT COUNT(*) AS duckdb_rows FROM savepoint_duckdb;

--echo # An earlier savepoint can roll back all work in a newly joined engine.
START TRANSACTION;
INSERT INTO savepoint_innodb VALUES(1);
SAVEPOINT before_duckdb;
INSERT INTO savepoint_duckdb VALUES(2);
INSERT INTO savepoint_innodb VALUES(2);
ROLLBACK TO SAVEPOINT before_duckdb;
COMMIT;
SELECT COUNT(*) AS duckdb_rows FROM savepoint_duckdb;
SELECT * FROM savepoint_innodb ORDER BY id;
DELETE FROM savepoint_innodb;

--echo # Native InnoDB savepoints remain supported.
START TRANSACTION;
INSERT INTO savepoint_innodb VALUES(3);
SAVEPOINT native;
INSERT INTO savepoint_innodb VALUES(4);
ROLLBACK TO SAVEPOINT native;
RELEASE SAVEPOINT native;
COMMIT;
SELECT * FROM savepoint_innodb ORDER BY id;
DELETE FROM savepoint_innodb;
