# trigger-single-table.test

# Place all trigger tests that must be run with a single table instance here.
# These are tests that are used to provoke errors when two consecutive
# sessions are forced to run with the same table entry, hence they will
# use the same triggers attached to that table.

# Save the initial number of concurrent sessions
--source include/count_sessions.inc

--echo # Bug#34009876: When table_open_cache_instances = 1, the result of
--echo #               CONNECTION_ID() called on the trigger is wrong.

--echo Must be 1
SELECT @@GLOBAL.table_open_cache_instances;

DELIMITER //;
CREATE FUNCTION transaction_id()
RETURNS BIGINT
BEGIN
  RETURN connection_id();
END;
//
DELIMITER ;//

CREATE TABLE t1
(
  id  BIGINT NOT NULL PRIMARY KEY,
  tr1 BIGINT NOT NULL,
  tr2 BIGINT NOT NULL
);

DELIMITER //;
CREATE TRIGGER t1_insert_trigger
BEFORE INSERT ON t1 FOR EACH ROW
BEGIN
    SET NEW.tr1 = connection_id();
    SET NEW.tr2 = transaction_id();
END;
//
DELIMITER ;//

INSERT INTO t1(id) VALUES (1);
SELECT id,
       tr1 = connection_id() AS "tr1 valid",
       tr2 = connection_id() AS "tr2 valid"
FROM t1
WHERE id=1;

--enable_connect_log
connect (conn1, localhost, root, , test);
connection conn1;

INSERT INTO t1(id) VALUES (2);
SELECT id,
       tr1 = connection_id() AS "tr1 valid", 
       tr2 = connection_id() AS "tr2 valid"
FROM t1
WHERE id=2;

# Verify that we got different connection ids in the two inserts
SELECT COUNT(DISTINCT tr1), COUNT(DISTINCT tr2) FROM t1;

disconnect conn1;
connection default;

DROP TABLE t1;
DROP FUNCTION transaction_id;

# Wait till all disconnects are completed
--source include/wait_until_count_sessions.inc
