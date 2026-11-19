# Create the basic InnoDB source table and convert the replica to DuckDB.
# Requires an initialized source/replica topology and the source connection.
# Leaves the replica connected.

CREATE TABLE t1 (
    id BIGINT NOT NULL,
    c0 VARCHAR(20) NOT NULL,
    c1 CHAR(10) DEFAULT 'duckdb',
    PRIMARY KEY(id, c0)
) ENGINE=InnoDB;
SHOW CREATE TABLE t1;

--source include/rpl/sync.inc
--source include/rpl/connection_replica.inc
ALTER TABLE t1 ENGINE=DuckDB;
SHOW CREATE TABLE t1;
