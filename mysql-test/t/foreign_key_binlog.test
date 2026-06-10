--source include/have_log_bin.inc
--echo # FR 5) UPDATE and DELETE operations on parent of foreign key table must
--echo # generate binlog for CASCADE operations
--echo # FR 5.1) Test foreign key cascade generates child binlogs with row-based binary logging.
--echo # Set BINLOG_FORMAT to 'ROW'.
--echo # Create tables t1, t2, t22, t3, and t4 with foreign key constraints.
--echo # Insert data into the tables.
--echo # Perform DELETE and UPDATE operations on t1.
--echo # Verify that the binary log is generated correctly.
--echo # Check the contents of the binary log file fk_binlog.sql.
--echo # Apply the binary log to the database and verify the results.
SET @saved_binlog_format = (SELECT @@global.binlog_format);

SET BINLOG_FORMAT='ROW';
let $MYSQLD_DATADIR= `select @@datadir`;
# Deletes all the binary logs
reset binary logs and gtids;

CREATE TABLE t1 (f1 INT PRIMARY KEY);
CREATE TABLE t2 (f1 INT PRIMARY KEY, f2 INT, UNIQUE KEY(f2), FOREIGN KEY (f2) REFERENCES t1(f1) ON DELETE CASCADE ON UPDATE CASCADE);
CREATE TABLE t22 (f1 INT PRIMARY KEY, f2 INT, FOREIGN KEY (f2) REFERENCES t1(f1) ON DELETE SET NULL ON UPDATE SET NULL);
CREATE TABLE t3 (f1 INT PRIMARY KEY, f2 INT, UNIQUE KEY(f2), FOREIGN KEY (f2) REFERENCES t2(f2) ON DELETE CASCADE ON UPDATE CASCADE);
CREATE TABLE t4 (f1 INT PRIMARY KEY, f2 INT, FOREIGN KEY (f2) REFERENCES t3(f2) ON DELETE CASCADE ON UPDATE CASCADE);
INSERT INTO t1 VALUES (1),(2),(3);
INSERT INTO t2 VALUES (1, 1),(2, 2),(3, 3);
INSERT INTO t22 VALUES (1, 1),(2, 2),(3, 3);
INSERT INTO t3 VALUES (1, 1),(2, 2),(3, 3);
INSERT INTO t4 VALUES (1, 1),(2, 2),(3, 3);
DELETE FROM t1 WHERE f1=2;
UPDATE t1 SET f1=5 WHERE f1=3;
let $master_binlog_file= query_get_value(SHOW BINARY LOG STATUS, File, 1);
flush logs;
echo mysqlbinlog var/log/master_binlog_file > var/tmp/fk_binlog.sql;
exec $MYSQL_BINLOG $MYSQLD_DATADIR/$master_binlog_file > $MYSQLTEST_VARDIR/tmp/fk_binlog.sql;
DELETE FROM t2;
DELETE FROM t22;
DELETE FROM t3;
DELETE FROM t4;
DROP TABLE t4, t3, t22, t2, t1;

# Search for message in the result file
--let SEARCH_FILE= $MYSQLTEST_VARDIR/tmp/fk_binlog.sql
--let SEARCH_PATTERN= NO_FOREIGN_KEY_CHECKS_F
--source include/search_pattern.inc

--let SEARCH_PATTERN= USE_SQL_FOREIGN_KEY_F
--source include/search_pattern.inc

# Deletes all the binary logs to avoid transactions from
# fk_binlog.sql to be skipped.
reset binary logs and gtids;

exec $MYSQL < $MYSQLTEST_VARDIR/tmp/fk_binlog.sql;
SELECT * FROM t2 ORDER BY f1;
SELECT * FROM t22 ORDER BY f1;
SELECT * FROM t3 ORDER BY f1;
SELECT * FROM t4 ORDER BY f1;
--remove_file $MYSQLTEST_VARDIR/tmp/fk_binlog.sql
DROP TABLE t4, t3, t22, t2, t1;

SET BINLOG_FORMAT='STATEMENT';
let $MYSQLD_DATADIR= `select @@datadir`;
# Deletes all the binary logs
reset binary logs and gtids;

--echo # FR 5.2) Test foreign key support with mixed statement and row-based binary logging.
--echo # Set BINLOG_FORMAT to 'STATEMENT' initially.
--echo # Create tables t1, t2, and t3 with foreign key constraints.
--echo # Insert data into the tables.
--echo # Perform DELETE operation on t1 with BINLOG_FORMAT set to 'STATEMENT'.
--echo # Switch to BINLOG_FORMAT 'ROW' and perform UPDATE operation on t1.
--echo # Switch back to BINLOG_FORMAT 'STATEMENT' and perform another DELETE operation on t1.
--echo # Switch to BINLOG_FORMAT 'ROW' again and perform another UPDATE operation on t1.
--echo # Verify that the binary log is generated correctly.
--echo # Check the contents of the binary log file fk_both_binlog.sql.
--echo # Apply the binary log to the database and verify the results.
CREATE TABLE t1 (f1 INT PRIMARY KEY);
CREATE TABLE t2 (f1 INT PRIMARY KEY, f2 INT, FOREIGN KEY (f2) REFERENCES t1(f1) ON DELETE CASCADE ON UPDATE CASCADE);
CREATE TABLE t3 (f1 INT PRIMARY KEY, f2 INT, FOREIGN KEY (f2) REFERENCES t1(f1) ON DELETE SET NULL ON UPDATE SET NULL);
INSERT INTO t1 VALUES (1),(2),(3),(4),(5);
INSERT INTO t2 VALUES (1, 1),(2, 2),(3, 3), (4, 4), (5, 5);
INSERT INTO t3 VALUES (1, 1),(2, 2),(3, 3), (4, 4), (5, 5);
DELETE FROM t1 WHERE f1=2;
SET BINLOG_FORMAT='ROW';
UPDATE t1 SET f1=9 WHERE f1=3;
SET BINLOG_FORMAT='STATEMENT';
DELETE FROM t1 WHERE f1=4;
SET BINLOG_FORMAT='ROW';
UPDATE t1 SET f1=10 WHERE f1=5;
let $master_binlog_file= query_get_value(SHOW BINARY LOG STATUS, File, 1);
flush logs;
echo mysqlbinlog var/log/master_binlog_file > var/tmp/fk_both_binlog.sql;
exec $MYSQL_BINLOG $MYSQLD_DATADIR/$master_binlog_file > $MYSQLTEST_VARDIR/tmp/fk_both_binlog.sql;
DELETE FROM t2;
DELETE FROM t3;
SELECT * FROM t2;
SELECT * FROM t3;
DROP TABLE t3, t2, t1;

# Deletes all the binary logs to avoid transactions from
# fk_binlog.sql to be skipped.
reset binary logs and gtids;

exec $MYSQL < $MYSQLTEST_VARDIR/tmp/fk_both_binlog.sql;
SELECT * FROM t2 ORDER BY f1;
SELECT * FROM t3 ORDER BY f1;
--remove_file $MYSQLTEST_VARDIR/tmp/fk_both_binlog.sql
DROP TABLE t3, t2, t1;

--echo # WL#17024 Fire child trigger during cascade action
--echo # child table binlog should not contain FKC binlog flag for
--echo # tables mentioned in it's trigger.
--echo # FR 9: Binary Logging
--echo # Source must log all the changes performed inside cascade-induced triggers
--echo # of child table
--echo # case 1: ROW format
SET BINLOG_FORMAT='ROW';
let $MYSQLD_DATADIR= `select @@datadir`;
# Deletes all the binary logs
reset binary logs and gtids;

CREATE TABLE t1 (f1 INT PRIMARY KEY);
CREATE TABLE t2 (f1 INT PRIMARY KEY, f2 INT, UNIQUE KEY(f2), FOREIGN KEY (f2) REFERENCES t1(f1) ON DELETE CASCADE ON UPDATE CASCADE);
CREATE TABLE logtable (action VARCHAR(128), oldval INT, newval INT);
INSERT INTO t1 VALUES (1),(2),(3);
INSERT INTO t2 VALUES (1, 1),(2, 2),(3, 3);

delimiter //;
CREATE TRIGGER trg1 AFTER DELETE ON t1
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("t1-AFTER-DELETE", OLD.f1, 0);
END//

CREATE TRIGGER trg2 AFTER DELETE ON t2
FOR EACH ROW BEGIN
    SET FOREIGN_KEY_CHECKS = ON;
    INSERT INTO logtable VALUES("t2-AFTER-DELETE", OLD.f2, 0);
END//

CREATE TRIGGER trg3 AFTER UPDATE ON t1
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("t1-AFTER-UPDATE", OLD.f1, NEW.f1);
END//

CREATE TRIGGER trg4 AFTER UPDATE ON t2
FOR EACH ROW BEGIN
    SET FOREIGN_KEY_CHECKS = ON;
    INSERT INTO logtable VALUES("t2-AFTER-UPDATE", OLD.f2, NEW.f2);
END//
delimiter ;//

SET enable_cascade_triggers = ON;
DELETE FROM t1 WHERE f1=2;
UPDATE t1 SET f1=5 WHERE f1=3;
SET enable_cascade_triggers = OFF;
SELECT * FROM t1 ORDER BY f1;
SELECT * FROM t2 ORDER BY f1;
SELECT * FROM logtable;
let $master_binlog_file= query_get_value(SHOW BINARY LOG STATUS, File, 1);
flush logs;
echo mysqlbinlog var/log/master_binlog_file > var/tmp/fk_binlog.sql;
exec $MYSQL_BINLOG $MYSQLD_DATADIR/$master_binlog_file > $MYSQLTEST_VARDIR/tmp/fk_binlog.sql;
DROP TABLE t2, t1, logtable;

--echo #NO_FOREIGN_KEY_CHECKS_F flag should appear only twice
--echo #once for DELETE cascade and once for UPDATE CASCADE
--echo #write_rows event from logtable INSERT in trigger should not have this flag.
--echo # Show binlog events
source include/rpl/deprecated/show_binlog_events.inc;

# Deletes all the binary logs to avoid transactions from
# fk_binlog.sql to be skipped.
reset binary logs and gtids;

exec $MYSQL < $MYSQLTEST_VARDIR/tmp/fk_binlog.sql;
SELECT * FROM t1 ORDER BY f1;
SELECT * FROM t2 ORDER BY f1;
SELECT * FROM logtable;
DROP TABLE t2, t1, logtable;

--remove_file $MYSQLTEST_VARDIR/tmp/fk_binlog.sql
let $MYSQLD_DATADIR= `select @@datadir`;
# Deletes all the binary logs
reset binary logs and gtids;

--echo # case 2: STATEMENT format
SET BINLOG_FORMAT='STATEMENT';
let $MYSQLD_DATADIR= `select @@datadir`;
# Deletes all the binary logs
reset binary logs and gtids;

CREATE TABLE t1 (f1 INT PRIMARY KEY);
CREATE TABLE t2 (f1 INT PRIMARY KEY, f2 INT, UNIQUE KEY(f2), FOREIGN KEY (f2) REFERENCES t1(f1) ON DELETE CASCADE ON UPDATE CASCADE);
CREATE TABLE logtable (action VARCHAR(128), oldval INT, newval INT);
INSERT INTO t1 VALUES (1),(2),(3);
INSERT INTO t2 VALUES (1, 1),(2, 2),(3, 3);

delimiter //;
CREATE TRIGGER trg1 AFTER DELETE ON t1
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("t1-AFTER-DELETE", OLD.f1, 0);
END//

CREATE TRIGGER trg2 AFTER DELETE ON t2
FOR EACH ROW BEGIN
    SET FOREIGN_KEY_CHECKS = ON;
    INSERT INTO logtable VALUES("t2-AFTER-DELETE", OLD.f2, 0);
END//

CREATE TRIGGER trg3 AFTER UPDATE ON t1
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("t1-AFTER-UPDATE", OLD.f1, NEW.f1);
END//

CREATE TRIGGER trg4 AFTER UPDATE ON t2
FOR EACH ROW BEGIN
    SET FOREIGN_KEY_CHECKS = ON;
    INSERT INTO logtable VALUES("t2-AFTER-UPDATE", OLD.f2, NEW.f2);
END//
delimiter ;//

SET enable_cascade_triggers = ON;
DELETE FROM t1 WHERE f1=2;
UPDATE t1 SET f1=5 WHERE f1=3;

SELECT * FROM t1 ORDER BY f1;
SELECT * FROM t2 ORDER BY f1;
SELECT * FROM logtable;
let $master_binlog_file= query_get_value(SHOW BINARY LOG STATUS, File, 1);
flush logs;
echo mysqlbinlog var/log/master_binlog_file > var/tmp/fk_binlog.sql;
exec $MYSQL_BINLOG $MYSQLD_DATADIR/$master_binlog_file > $MYSQLTEST_VARDIR/tmp/fk_binlog.sql;
DROP TABLE t2, t1, logtable;

--echo #NO_FOREIGN_KEY_CHECKS_F flag should not appear in stmt binlog mode
--echo #check if logtable is present 5(create+4 triggers) times in binlog
--echo # Show binlog events
source include/rpl/deprecated/show_binlog_events.inc;

# Deletes all the binary logs to avoid transactions from
# fk_binlog.sql to be skipped.
reset binary logs and gtids;

exec $MYSQL < $MYSQLTEST_VARDIR/tmp/fk_binlog.sql;
SELECT * FROM t1 ORDER BY f1;
SELECT * FROM t2 ORDER BY f1;
SELECT * FROM logtable;
DROP TABLE t2, t1, logtable;

--remove_file $MYSQLTEST_VARDIR/tmp/fk_binlog.sql
let $MYSQLD_DATADIR= `select @@datadir`;
# Deletes all the binary logs
reset binary logs and gtids;

--echo # case 3: Enabling enable_cascade_triggers in STATEMENT format with
--echo #         InnoDB FK handling should give warning
--exec echo "restart: --innodb_native_foreign_keys=ON" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--let $restart_parameters=restart: --innodb_native_foreign_keys=ON
--source include/restart_mysqld.inc
--source include/wait_until_connected_again.inc
--disable_query_log
call mtr.add_suppression("Executing triggers on foreign key cascade operations is supported only with SQL Foreign Key Handling. State of the variable 'enable_cascade_triggers' is logged as OFF in binlog for an event in this case.");
--enable_query_log

SET BINLOG_FORMAT='STATEMENT';
let $MYSQLD_DATADIR= `select @@datadir`;
# Deletes all the binary logs
reset binary logs and gtids;

CREATE TABLE t1 (f1 INT PRIMARY KEY);
CREATE TABLE t2 (f1 INT PRIMARY KEY, f2 INT, UNIQUE KEY(f2),
FOREIGN KEY (f2) REFERENCES t1(f1) ON DELETE CASCADE ON UPDATE CASCADE);
CREATE TABLE logtable (action VARCHAR(128), oldval INT, newval INT);
INSERT INTO t1 VALUES (1),(2),(3);
INSERT INTO t2 VALUES (1, 1),(2, 2),(3, 3);

delimiter //;
CREATE TRIGGER trg1 AFTER DELETE ON t1
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("t1-AFTER-DELETE", OLD.f1, 0);
END//

CREATE TRIGGER trg2 AFTER DELETE ON t2
FOR EACH ROW BEGIN
    INSERT INTO logtable VALUES("t2-AFTER-DELETE", OLD.f2, 0);
END//

delimiter ;//

SET enable_cascade_triggers = ON;
DELETE FROM t1 WHERE f1=2;
DROP TABLE t1, t2, logtable;
--let $assert_text = "Check for InnoDB FK enable_cascade_trigger warning in the server log"
--let $assert_file = $MYSQLTEST_VARDIR/log/mysqld.1.err
--let $assert_select = Executing triggers on foreign key cascade operations is supported only with SQL Foreign Key Handling
--let $assert_count = 1
--source include/assert_grep.inc

--echo # cleanup: restart without innodb_native_foreign_keys variable
--exec echo "restart:" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--let $restart_parameters=restart:
--source include/restart_mysqld.inc
