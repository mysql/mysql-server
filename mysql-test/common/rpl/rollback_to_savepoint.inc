###############################################################################
# Bug#76727: SLAVE ASSERTION IN UNPACK_ROW WITH ROLLBACK TO
# SAVEPOINT IN ERROR HANDLER
#
# Problem:
# ========
# "SAVEPOINT", "ROLLBACK TO savepoint" wipe out table map on slave during
# execution binary log events. For trigger the map is written to binary log once
# for all trigger body and if trigger contains "SAVEPOINT" or
# "ROLLBACK TO savepoint" statements any trigger's events after these
# statements will not have table map. This results in an assert on slave.
#
# Test:
# =====
# Test case 1:
# Create a trigger with exception handler which rollsback to a savepoint.
# Test proves that there will not be any assert during execution of rolling
# back to savepoint.
#
# Test case 2:
# Create a trigger which calls a procedure which in turn calls an exception
# handler which rollsback to a savepoint. Prove that it doesn't cause any
# assertion during execution.
#
# Test case 3:
# Create a simple trigger which creates a SAVEPOINT and ROLLSBACK to savepoint
# and doesn't follow with any other DML statement. Prove that it doesn't cause
# any assertion during execution.
#
# Test case 4:
# Create a trigger with SAVEPOINT and follows with a DML without ROLLBACK TO
# savepoint. Ensure that data is replicated properly.
#
# Test case 5:
# Create a trigger with SAVEPOINT and it does nothing. Do few DMLS following
# the trigger ensure that the data is replicated properly
#
# Test case 6:
# Create a stored function which creates a SAVEPOINT and ROLLSBACK to
# savepoint. Do few inserts following the stored function call and ensure that
# no assert is generated on slave and all the rows are replicated to slave.
#
# Test case 7:
# Create a stored function which creates a SAVEPOINT alone and follows with
# DMLS without ROLLBACK TO savepoint. Ensure that data is replicated properly.
#
# Test case 8:
# Create a stored function which has SAVEPOINT inside it and does noting. It
# should follow with other DMLs. Ensure that data is replicated properly.
###############################################################################
--source include/not_group_replication_plugin.inc
--source include/rpl/set_privilege_checks_user_as_system_user.inc
--source include/rpl/init_source_replica.inc

--echo #Test case 1:
CREATE TABLE t1 (f1 INTEGER PRIMARY KEY);
CREATE TABLE t2 (f1 INTEGER PRIMARY KEY);
CREATE TABLE t3 (f1 INTEGER PRIMARY KEY);
DELIMITER |;

CREATE TRIGGER tr1 AFTER INSERT ON t1 FOR EACH ROW
BEGIN
        DECLARE EXIT HANDLER FOR SQLEXCEPTION
                BEGIN
                        ROLLBACK TO event_logging_1;
                        INSERT t3 VALUES (1);
                END;

        SAVEPOINT event_logging_1;

        INSERT INTO t2 VALUES (1);

        RELEASE SAVEPOINT event_logging_1;

END|
DELIMITER ;|

INSERT INTO t2 VALUES (1);
INSERT INTO t1 VALUES (1);

--source include/rpl/deprecated/show_binlog_events.inc

--sync_slave_with_master

--source include/rpl/connection_source.inc

DROP TRIGGER tr1;
DELETE FROM t1;
DELETE FROM t2;
DELETE FROM t3;

--echo # Test case 2:

DELIMITER |;

CREATE PROCEDURE p1()
BEGIN
        DECLARE EXIT HANDLER FOR SQLEXCEPTION
                BEGIN
                        ROLLBACK TO event_logging_2;
                        INSERT t3 VALUES (3);
                END;

        SAVEPOINT event_logging_2;

        INSERT INTO t2 VALUES (1);

        RELEASE SAVEPOINT event_logging_2;
END|

CREATE TRIGGER tr1 AFTER INSERT ON t1 FOR EACH ROW CALL p1()|

DELIMITER ;|

INSERT INTO t2 VALUES (1);
INSERT INTO t1 VALUES (1);

--source include/rpl/deprecated/show_binlog_events.inc

--sync_slave_with_master

--source include/rpl/connection_source.inc

DROP TABLE t1;
DROP TABLE t2;
DROP TABLE t3;

DROP PROCEDURE p1;

--echo # Test case 3:
--source include/rpl/reset.inc
--source include/rpl/connection_source.inc

CREATE TABLE t (f1 int(10) unsigned NOT NULL, PRIMARY KEY (f1)) ENGINE=InnoDB;

--delimiter |
CREATE TRIGGER t_insert_trig AFTER INSERT ON t
FOR EACH ROW
BEGIN

SAVEPOINT savepoint_1;
ROLLBACK TO savepoint_1;

END |
--delimiter ;

INSERT INTO t VALUES (2);
INSERT INTO t VALUES (3);

--source include/rpl/deprecated/show_binlog_events.inc

SELECT * FROM t;

--source include/rpl/sync_to_replica.inc

SELECT * FROM t;

--source include/rpl/connection_source.inc
DROP TABLE t;

--echo # Test case 4:
--source include/rpl/reset.inc
--source include/rpl/connection_source.inc
CREATE TABLE t (f1 int(10) unsigned NOT NULL) ENGINE=InnoDB;
CREATE TABLE t1 (f1 int(10) unsigned NOT NULL) ENGINE=InnoDB;

--delimiter |
CREATE TRIGGER t_insert_trig BEFORE INSERT ON t
FOR EACH ROW
BEGIN

SAVEPOINT savepoint_1;
INSERT INTO t1 VALUES (5);
END |
--delimiter ;

INSERT INTO t VALUES (2), (3);
INSERT INTO t1 VALUES (30);
--source include/rpl/deprecated/show_binlog_events.inc

SELECT * FROM t;
SELECT * FROM t1;
--source include/rpl/sync_to_replica.inc

SELECT * FROM t;
SELECT * FROM t1;

--source include/rpl/connection_source.inc
DROP TABLE t;
DROP TABLE t1;

--echo # Test case 5:
--source include/rpl/reset.inc
--source include/rpl/connection_source.inc
CREATE TABLE t (f1 int(10) unsigned NOT NULL) ENGINE=InnoDB;
CREATE TABLE t1 (f1 int(10) unsigned NOT NULL) ENGINE=InnoDB;

--delimiter |
CREATE TRIGGER t_insert_trig BEFORE INSERT ON t
FOR EACH ROW
BEGIN

SAVEPOINT savepoint_1;
END |

--delimiter ;

INSERT INTO t VALUES (2), (3);
INSERT INTO t1 VALUES (30);
--source include/rpl/deprecated/show_binlog_events.inc

SELECT * FROM t;
SELECT * FROM t1;
--source include/rpl/sync_to_replica.inc

SELECT * FROM t;
SELECT * FROM t1;

--source include/rpl/connection_source.inc
DROP TABLE t;
DROP TABLE t1;

--echo # Test case 6:
--source include/rpl/reset.inc
--source include/rpl/connection_source.inc
CREATE TABLE t1 (f1 INTEGER ) ENGINE=INNODB;
CREATE TABLE t2 (f1 INTEGER ) ENGINE=INNODB;

--delimiter |

CREATE FUNCTION f1() RETURNS INT
BEGIN
        SAVEPOINT event_logging_2;

        INSERT INTO t1 VALUES (1);

        ROLLBACK TO event_logging_2;
        RETURN 0;
END|

--delimiter ;

BEGIN;
INSERT INTO t2 VALUES (1), (f1()), (2), (4);
COMMIT;
INSERT INTO t2 VALUES (10);
--source include/rpl/deprecated/show_binlog_events.inc

--source include/rpl/connection_source.inc
SELECT * FROM t2;
SELECT * FROM t1;
--source include/rpl/sync_to_replica.inc
SELECT * FROM t2;
SELECT * FROM t1;

--source include/rpl/connection_source.inc
DROP TABLE t1;
DROP TABLE t2;
DROP FUNCTION f1;

--echo # Test case 7:
--source include/rpl/reset.inc
--source include/rpl/connection_source.inc
CREATE TABLE t1 (f1 INTEGER ) ENGINE=INNODB;
CREATE TABLE t2 (f1 INTEGER ) ENGINE=INNODB;

--delimiter |

CREATE FUNCTION f1() RETURNS INT
BEGIN
        SAVEPOINT event_logging_2;

        INSERT INTO t1 VALUES (1);

        RETURN 0;
END|

--delimiter ;

BEGIN;
INSERT INTO t2 VALUES (1), (f1()), (2), (4);
COMMIT;
INSERT INTO t2 VALUES (10);
--source include/rpl/deprecated/show_binlog_events.inc

--source include/rpl/connection_source.inc
SELECT * FROM t2;
SELECT * FROM t1;
--source include/rpl/sync_to_replica.inc
SELECT * FROM t2;
SELECT * FROM t1;

--source include/rpl/connection_source.inc
DROP TABLE t1;
DROP TABLE t2;
DROP FUNCTION f1;

--echo # Test case 8:
--source include/rpl/reset.inc
--source include/rpl/connection_source.inc
CREATE TABLE t1 (f1 INTEGER ) ENGINE=INNODB;

--delimiter |

CREATE FUNCTION f1() RETURNS INT
BEGIN
        SAVEPOINT event_logging_2;
        RETURN 0;
END|

--delimiter ;

BEGIN;
INSERT INTO t1 VALUES (1), (f1()), (2), (4);
COMMIT;
INSERT INTO t1 VALUES (10);
--source include/rpl/deprecated/show_binlog_events.inc

--source include/rpl/connection_source.inc
SELECT * FROM t1;
--source include/rpl/sync_to_replica.inc
SELECT * FROM t1;

--source include/rpl/connection_source.inc
DROP TABLE t1;
DROP FUNCTION f1;

--source include/rpl/deinit.inc
