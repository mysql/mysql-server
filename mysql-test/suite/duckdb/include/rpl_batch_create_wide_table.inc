# Create the typed InnoDB source table and convert the replica to DuckDB.
# Requires an initialized source/replica topology. Leaves the replica connected.

--source include/rpl/connection_source.inc
CREATE TABLE t1 (
    id BIGINT NOT NULL,
    c0 VARCHAR(20) NOT NULL,
    c1 CHAR(10) DEFAULT 'duckdb',
    c2 DECIMAL(5,2),
    c3 DATETIME,
    c4 TIMESTAMP,
    c5 BLOB,
    PRIMARY KEY(id, c0)
) ENGINE=InnoDB;
SHOW CREATE TABLE t1;

--source include/rpl/sync.inc
--source include/rpl/connection_replica.inc
ALTER TABLE t1 ENGINE=DuckDB;
SHOW CREATE TABLE t1;
