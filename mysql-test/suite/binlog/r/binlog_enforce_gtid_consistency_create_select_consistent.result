include/begin_replace_gtid_combination.inc [gtid_mode=off/on -> default_storage_engine=InnoDB/MyISAM]
include/rpl_init.inc [topology=none]
SET @old_sql_notes= @@GLOBAL.SQL_NOTES;
SET GLOBAL SQL_NOTES= 0;
SET SESSION SQL_NOTES= 0;
CALL mtr.add_suppression('Statement violates GTID consistency:');
CALL mtr.add_suppression('Unsafe statement written to the binary log');
SET GLOBAL ENFORCE_GTID_CONSISTENCY = 0;
SET GLOBAL GTID_MODE = OFF_PERMISSIVE;
SET GLOBAL GTID_MODE = ON_PERMISSIVE;
SET GLOBAL GTID_MODE = 2;
######## ENFORCE_GTID_CONSISTENCY=OFF GTID_MODE=ON_PERMISSIVE GTID_NEXT=AUTOMATIC ########
CREATE TABLE empty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE empty_myisam (a INT) ENGINE = MyISAM;
CREATE TABLE nonempty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE nonempty_myisam (a INT) ENGINE = MyISAM;
INSERT INTO nonempty_innodb VALUES (1);
INSERT INTO nonempty_myisam VALUES (1);
---- CREATE TEMPORARY ... SELECT (value, 1 row) ----
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 1 row) ----
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 1 row) ----
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 0 row) ----
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 0 row) ----
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
SET SQL_LOG_BIN = 0;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (value, 1 row) ----
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 1 row) ----
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 1 row) ----
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 0 row) ----
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 0 row) ----
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
DROP TABLE empty_innodb, empty_myisam, nonempty_innodb, nonempty_myisam;
SET SQL_LOG_BIN = 1;
######## ENFORCE_GTID_CONSISTENCY=OFF GTID_MODE=ON_PERMISSIVE GTID_NEXT=ANONYMOUS ########
CREATE TABLE empty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE empty_myisam (a INT) ENGINE = MyISAM;
CREATE TABLE nonempty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE nonempty_myisam (a INT) ENGINE = MyISAM;
INSERT INTO nonempty_innodb VALUES (1);
INSERT INTO nonempty_myisam VALUES (1);
---- CREATE TEMPORARY ... SELECT (value, 1 row) ----
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 1 row) ----
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 1 row) ----
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 0 row) ----
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 0 row) ----
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
SET SQL_LOG_BIN = 0;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (value, 1 row) ----
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 1 row) ----
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 1 row) ----
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 0 row) ----
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 0 row) ----
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
DROP TABLE empty_innodb, empty_myisam, nonempty_innodb, nonempty_myisam;
SET SQL_LOG_BIN = 1;
######## ENFORCE_GTID_CONSISTENCY=OFF GTID_MODE=ON_PERMISSIVE GTID_NEXT=GTID ########
CREATE TABLE empty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE empty_myisam (a INT) ENGINE = MyISAM;
CREATE TABLE nonempty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE nonempty_myisam (a INT) ENGINE = MyISAM;
INSERT INTO nonempty_innodb VALUES (1);
INSERT INTO nonempty_myisam VALUES (1);
---- CREATE TEMPORARY ... SELECT (value, 1 row) ----
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 1 row) ----
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 1 row) ----
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 0 row) ----
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 0 row) ----
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
SET SQL_LOG_BIN = 0;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (value, 1 row) ----
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 1 row) ----
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 1 row) ----
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 0 row) ----
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 0 row) ----
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
DROP TABLE empty_innodb, empty_myisam, nonempty_innodb, nonempty_myisam;
SET SQL_LOG_BIN = 1;
SET GLOBAL GTID_MODE = 1;
######## ENFORCE_GTID_CONSISTENCY=OFF GTID_MODE=OFF_PERMISSIVE GTID_NEXT=AUTOMATIC ########
CREATE TABLE empty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE empty_myisam (a INT) ENGINE = MyISAM;
CREATE TABLE nonempty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE nonempty_myisam (a INT) ENGINE = MyISAM;
INSERT INTO nonempty_innodb VALUES (1);
INSERT INTO nonempty_myisam VALUES (1);
---- CREATE TEMPORARY ... SELECT (value, 1 row) ----
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 1 row) ----
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 1 row) ----
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 0 row) ----
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 0 row) ----
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
SET SQL_LOG_BIN = 0;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (value, 1 row) ----
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 1 row) ----
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 1 row) ----
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 0 row) ----
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 0 row) ----
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
DROP TABLE empty_innodb, empty_myisam, nonempty_innodb, nonempty_myisam;
SET SQL_LOG_BIN = 1;
######## ENFORCE_GTID_CONSISTENCY=OFF GTID_MODE=OFF_PERMISSIVE GTID_NEXT=ANONYMOUS ########
CREATE TABLE empty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE empty_myisam (a INT) ENGINE = MyISAM;
CREATE TABLE nonempty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE nonempty_myisam (a INT) ENGINE = MyISAM;
INSERT INTO nonempty_innodb VALUES (1);
INSERT INTO nonempty_myisam VALUES (1);
---- CREATE TEMPORARY ... SELECT (value, 1 row) ----
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 1 row) ----
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 1 row) ----
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 0 row) ----
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 0 row) ----
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
SET SQL_LOG_BIN = 0;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (value, 1 row) ----
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 1 row) ----
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 1 row) ----
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 0 row) ----
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 0 row) ----
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
DROP TABLE empty_innodb, empty_myisam, nonempty_innodb, nonempty_myisam;
SET SQL_LOG_BIN = 1;
######## ENFORCE_GTID_CONSISTENCY=OFF GTID_MODE=OFF_PERMISSIVE GTID_NEXT=GTID ########
CREATE TABLE empty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE empty_myisam (a INT) ENGINE = MyISAM;
CREATE TABLE nonempty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE nonempty_myisam (a INT) ENGINE = MyISAM;
INSERT INTO nonempty_innodb VALUES (1);
INSERT INTO nonempty_myisam VALUES (1);
---- CREATE TEMPORARY ... SELECT (value, 1 row) ----
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 1 row) ----
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 1 row) ----
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 0 row) ----
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 0 row) ----
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
SET SQL_LOG_BIN = 0;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (value, 1 row) ----
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 1 row) ----
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 1 row) ----
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 0 row) ----
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 0 row) ----
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
DROP TABLE empty_innodb, empty_myisam, nonempty_innodb, nonempty_myisam;
SET SQL_LOG_BIN = 1;
SET GLOBAL GTID_MODE = 0;
######## ENFORCE_GTID_CONSISTENCY=OFF GTID_MODE=OFF GTID_NEXT=AUTOMATIC ########
CREATE TABLE empty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE empty_myisam (a INT) ENGINE = MyISAM;
CREATE TABLE nonempty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE nonempty_myisam (a INT) ENGINE = MyISAM;
INSERT INTO nonempty_innodb VALUES (1);
INSERT INTO nonempty_myisam VALUES (1);
---- CREATE TEMPORARY ... SELECT (value, 1 row) ----
# enforce_gtid_consistency=0 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 1 row) ----
# enforce_gtid_consistency=0 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 1 row) ----
# enforce_gtid_consistency=0 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 0 row) ----
# enforce_gtid_consistency=0 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 0 row) ----
# enforce_gtid_consistency=0 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
SET SQL_LOG_BIN = 0;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (value, 1 row) ----
# enforce_gtid_consistency=0 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 1 row) ----
# enforce_gtid_consistency=0 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 1 row) ----
# enforce_gtid_consistency=0 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 0 row) ----
# enforce_gtid_consistency=0 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 0 row) ----
# enforce_gtid_consistency=0 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
DROP TABLE empty_innodb, empty_myisam, nonempty_innodb, nonempty_myisam;
SET SQL_LOG_BIN = 1;
######## ENFORCE_GTID_CONSISTENCY=OFF GTID_MODE=OFF GTID_NEXT=ANONYMOUS ########
CREATE TABLE empty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE empty_myisam (a INT) ENGINE = MyISAM;
CREATE TABLE nonempty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE nonempty_myisam (a INT) ENGINE = MyISAM;
INSERT INTO nonempty_innodb VALUES (1);
INSERT INTO nonempty_myisam VALUES (1);
---- CREATE TEMPORARY ... SELECT (value, 1 row) ----
# enforce_gtid_consistency=0 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 1 row) ----
# enforce_gtid_consistency=0 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 1 row) ----
# enforce_gtid_consistency=0 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 0 row) ----
# enforce_gtid_consistency=0 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 0 row) ----
# enforce_gtid_consistency=0 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
SET SQL_LOG_BIN = 0;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (value, 1 row) ----
# enforce_gtid_consistency=0 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 1 row) ----
# enforce_gtid_consistency=0 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 1 row) ----
# enforce_gtid_consistency=0 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 0 row) ----
# enforce_gtid_consistency=0 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 0 row) ----
# enforce_gtid_consistency=0 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
DROP TABLE empty_innodb, empty_myisam, nonempty_innodb, nonempty_myisam;
SET SQL_LOG_BIN = 1;
SET GLOBAL ENFORCE_GTID_CONSISTENCY = 1;
SET GLOBAL GTID_MODE = OFF_PERMISSIVE;
SET GLOBAL GTID_MODE = ON_PERMISSIVE;
SET GLOBAL GTID_MODE = ON;
SET GLOBAL GTID_MODE = 3;
######## ENFORCE_GTID_CONSISTENCY=ERROR GTID_MODE=ON GTID_NEXT=AUTOMATIC ########
CREATE TABLE empty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE empty_myisam (a INT) ENGINE = MyISAM;
CREATE TABLE nonempty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE nonempty_myisam (a INT) ENGINE = MyISAM;
INSERT INTO nonempty_innodb VALUES (1);
INSERT INTO nonempty_myisam VALUES (1);
---- CREATE TEMPORARY ... SELECT (value, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=3 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=3 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=3 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 0 row) ----
# enforce_gtid_consistency=1 gtid_mode=3 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 0 row) ----
# enforce_gtid_consistency=1 gtid_mode=3 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
SET SQL_LOG_BIN = 0;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (value, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=3 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=3 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=3 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 0 row) ----
# enforce_gtid_consistency=1 gtid_mode=3 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 0 row) ----
# enforce_gtid_consistency=1 gtid_mode=3 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
DROP TABLE empty_innodb, empty_myisam, nonempty_innodb, nonempty_myisam;
SET SQL_LOG_BIN = 1;
######## ENFORCE_GTID_CONSISTENCY=ERROR GTID_MODE=ON GTID_NEXT=GTID ########
CREATE TABLE empty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE empty_myisam (a INT) ENGINE = MyISAM;
CREATE TABLE nonempty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE nonempty_myisam (a INT) ENGINE = MyISAM;
INSERT INTO nonempty_innodb VALUES (1);
INSERT INTO nonempty_myisam VALUES (1);
---- CREATE TEMPORARY ... SELECT (value, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=3 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=3 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=3 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 0 row) ----
# enforce_gtid_consistency=1 gtid_mode=3 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 0 row) ----
# enforce_gtid_consistency=1 gtid_mode=3 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
SET SQL_LOG_BIN = 0;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (value, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=3 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=3 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=3 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 0 row) ----
# enforce_gtid_consistency=1 gtid_mode=3 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 0 row) ----
# enforce_gtid_consistency=1 gtid_mode=3 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
DROP TABLE empty_innodb, empty_myisam, nonempty_innodb, nonempty_myisam;
SET SQL_LOG_BIN = 1;
SET GLOBAL GTID_MODE = 2;
######## ENFORCE_GTID_CONSISTENCY=ERROR GTID_MODE=ON_PERMISSIVE GTID_NEXT=AUTOMATIC ########
CREATE TABLE empty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE empty_myisam (a INT) ENGINE = MyISAM;
CREATE TABLE nonempty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE nonempty_myisam (a INT) ENGINE = MyISAM;
INSERT INTO nonempty_innodb VALUES (1);
INSERT INTO nonempty_myisam VALUES (1);
---- CREATE TEMPORARY ... SELECT (value, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 0 row) ----
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 0 row) ----
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
SET SQL_LOG_BIN = 0;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (value, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 0 row) ----
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 0 row) ----
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
DROP TABLE empty_innodb, empty_myisam, nonempty_innodb, nonempty_myisam;
SET SQL_LOG_BIN = 1;
######## ENFORCE_GTID_CONSISTENCY=ERROR GTID_MODE=ON_PERMISSIVE GTID_NEXT=ANONYMOUS ########
CREATE TABLE empty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE empty_myisam (a INT) ENGINE = MyISAM;
CREATE TABLE nonempty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE nonempty_myisam (a INT) ENGINE = MyISAM;
INSERT INTO nonempty_innodb VALUES (1);
INSERT INTO nonempty_myisam VALUES (1);
---- CREATE TEMPORARY ... SELECT (value, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 0 row) ----
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 0 row) ----
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
SET SQL_LOG_BIN = 0;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (value, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 0 row) ----
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 0 row) ----
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
DROP TABLE empty_innodb, empty_myisam, nonempty_innodb, nonempty_myisam;
SET SQL_LOG_BIN = 1;
######## ENFORCE_GTID_CONSISTENCY=ERROR GTID_MODE=ON_PERMISSIVE GTID_NEXT=GTID ########
CREATE TABLE empty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE empty_myisam (a INT) ENGINE = MyISAM;
CREATE TABLE nonempty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE nonempty_myisam (a INT) ENGINE = MyISAM;
INSERT INTO nonempty_innodb VALUES (1);
INSERT INTO nonempty_myisam VALUES (1);
---- CREATE TEMPORARY ... SELECT (value, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 0 row) ----
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 0 row) ----
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
SET SQL_LOG_BIN = 0;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (value, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 0 row) ----
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 0 row) ----
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
DROP TABLE empty_innodb, empty_myisam, nonempty_innodb, nonempty_myisam;
SET SQL_LOG_BIN = 1;
SET GLOBAL GTID_MODE = 1;
######## ENFORCE_GTID_CONSISTENCY=ERROR GTID_MODE=OFF_PERMISSIVE GTID_NEXT=AUTOMATIC ########
CREATE TABLE empty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE empty_myisam (a INT) ENGINE = MyISAM;
CREATE TABLE nonempty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE nonempty_myisam (a INT) ENGINE = MyISAM;
INSERT INTO nonempty_innodb VALUES (1);
INSERT INTO nonempty_myisam VALUES (1);
---- CREATE TEMPORARY ... SELECT (value, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 0 row) ----
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 0 row) ----
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
SET SQL_LOG_BIN = 0;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (value, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 0 row) ----
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 0 row) ----
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
DROP TABLE empty_innodb, empty_myisam, nonempty_innodb, nonempty_myisam;
SET SQL_LOG_BIN = 1;
######## ENFORCE_GTID_CONSISTENCY=ERROR GTID_MODE=OFF_PERMISSIVE GTID_NEXT=ANONYMOUS ########
CREATE TABLE empty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE empty_myisam (a INT) ENGINE = MyISAM;
CREATE TABLE nonempty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE nonempty_myisam (a INT) ENGINE = MyISAM;
INSERT INTO nonempty_innodb VALUES (1);
INSERT INTO nonempty_myisam VALUES (1);
---- CREATE TEMPORARY ... SELECT (value, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 0 row) ----
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 0 row) ----
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
SET SQL_LOG_BIN = 0;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (value, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 0 row) ----
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 0 row) ----
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
DROP TABLE empty_innodb, empty_myisam, nonempty_innodb, nonempty_myisam;
SET SQL_LOG_BIN = 1;
######## ENFORCE_GTID_CONSISTENCY=ERROR GTID_MODE=OFF_PERMISSIVE GTID_NEXT=GTID ########
CREATE TABLE empty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE empty_myisam (a INT) ENGINE = MyISAM;
CREATE TABLE nonempty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE nonempty_myisam (a INT) ENGINE = MyISAM;
INSERT INTO nonempty_innodb VALUES (1);
INSERT INTO nonempty_myisam VALUES (1);
---- CREATE TEMPORARY ... SELECT (value, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 0 row) ----
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 0 row) ----
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
SET SQL_LOG_BIN = 0;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (value, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 0 row) ----
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 0 row) ----
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
DROP TABLE empty_innodb, empty_myisam, nonempty_innodb, nonempty_myisam;
SET SQL_LOG_BIN = 1;
SET GLOBAL GTID_MODE = 0;
######## ENFORCE_GTID_CONSISTENCY=ERROR GTID_MODE=OFF GTID_NEXT=AUTOMATIC ########
CREATE TABLE empty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE empty_myisam (a INT) ENGINE = MyISAM;
CREATE TABLE nonempty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE nonempty_myisam (a INT) ENGINE = MyISAM;
INSERT INTO nonempty_innodb VALUES (1);
INSERT INTO nonempty_myisam VALUES (1);
---- CREATE TEMPORARY ... SELECT (value, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 0 row) ----
# enforce_gtid_consistency=1 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 0 row) ----
# enforce_gtid_consistency=1 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
SET SQL_LOG_BIN = 0;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (value, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 0 row) ----
# enforce_gtid_consistency=1 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 0 row) ----
# enforce_gtid_consistency=1 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
DROP TABLE empty_innodb, empty_myisam, nonempty_innodb, nonempty_myisam;
SET SQL_LOG_BIN = 1;
######## ENFORCE_GTID_CONSISTENCY=ERROR GTID_MODE=OFF GTID_NEXT=ANONYMOUS ########
CREATE TABLE empty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE empty_myisam (a INT) ENGINE = MyISAM;
CREATE TABLE nonempty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE nonempty_myisam (a INT) ENGINE = MyISAM;
INSERT INTO nonempty_innodb VALUES (1);
INSERT INTO nonempty_myisam VALUES (1);
---- CREATE TEMPORARY ... SELECT (value, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 0 row) ----
# enforce_gtid_consistency=1 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 0 row) ----
# enforce_gtid_consistency=1 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
SET SQL_LOG_BIN = 0;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (value, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 1 row) ----
# enforce_gtid_consistency=1 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 0 row) ----
# enforce_gtid_consistency=1 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 0 row) ----
# enforce_gtid_consistency=1 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
DROP TABLE empty_innodb, empty_myisam, nonempty_innodb, nonempty_myisam;
SET SQL_LOG_BIN = 1;
SET GLOBAL ENFORCE_GTID_CONSISTENCY = 2;
SET GLOBAL GTID_MODE = OFF_PERMISSIVE;
SET GLOBAL GTID_MODE = ON_PERMISSIVE;
SET GLOBAL GTID_MODE = 2;
######## ENFORCE_GTID_CONSISTENCY=WARN GTID_MODE=ON_PERMISSIVE GTID_NEXT=AUTOMATIC ########
CREATE TABLE empty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE empty_myisam (a INT) ENGINE = MyISAM;
CREATE TABLE nonempty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE nonempty_myisam (a INT) ENGINE = MyISAM;
INSERT INTO nonempty_innodb VALUES (1);
INSERT INTO nonempty_myisam VALUES (1);
---- CREATE TEMPORARY ... SELECT (value, 1 row) ----
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 1 row) ----
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 1 row) ----
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 0 row) ----
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 0 row) ----
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
SET SQL_LOG_BIN = 0;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (value, 1 row) ----
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 1 row) ----
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 1 row) ----
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 0 row) ----
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 0 row) ----
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
DROP TABLE empty_innodb, empty_myisam, nonempty_innodb, nonempty_myisam;
SET SQL_LOG_BIN = 1;
######## ENFORCE_GTID_CONSISTENCY=WARN GTID_MODE=ON_PERMISSIVE GTID_NEXT=ANONYMOUS ########
CREATE TABLE empty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE empty_myisam (a INT) ENGINE = MyISAM;
CREATE TABLE nonempty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE nonempty_myisam (a INT) ENGINE = MyISAM;
INSERT INTO nonempty_innodb VALUES (1);
INSERT INTO nonempty_myisam VALUES (1);
---- CREATE TEMPORARY ... SELECT (value, 1 row) ----
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 1 row) ----
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 1 row) ----
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 0 row) ----
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 0 row) ----
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
SET SQL_LOG_BIN = 0;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (value, 1 row) ----
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 1 row) ----
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 1 row) ----
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 0 row) ----
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 0 row) ----
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
DROP TABLE empty_innodb, empty_myisam, nonempty_innodb, nonempty_myisam;
SET SQL_LOG_BIN = 1;
######## ENFORCE_GTID_CONSISTENCY=WARN GTID_MODE=ON_PERMISSIVE GTID_NEXT=GTID ########
CREATE TABLE empty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE empty_myisam (a INT) ENGINE = MyISAM;
CREATE TABLE nonempty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE nonempty_myisam (a INT) ENGINE = MyISAM;
INSERT INTO nonempty_innodb VALUES (1);
INSERT INTO nonempty_myisam VALUES (1);
---- CREATE TEMPORARY ... SELECT (value, 1 row) ----
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 1 row) ----
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 1 row) ----
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 0 row) ----
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 0 row) ----
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
SET SQL_LOG_BIN = 0;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (value, 1 row) ----
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 1 row) ----
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 1 row) ----
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 0 row) ----
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 0 row) ----
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
DROP TABLE empty_innodb, empty_myisam, nonempty_innodb, nonempty_myisam;
SET SQL_LOG_BIN = 1;
SET GLOBAL GTID_MODE = 1;
######## ENFORCE_GTID_CONSISTENCY=WARN GTID_MODE=OFF_PERMISSIVE GTID_NEXT=AUTOMATIC ########
CREATE TABLE empty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE empty_myisam (a INT) ENGINE = MyISAM;
CREATE TABLE nonempty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE nonempty_myisam (a INT) ENGINE = MyISAM;
INSERT INTO nonempty_innodb VALUES (1);
INSERT INTO nonempty_myisam VALUES (1);
---- CREATE TEMPORARY ... SELECT (value, 1 row) ----
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 1 row) ----
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 1 row) ----
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 0 row) ----
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 0 row) ----
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
SET SQL_LOG_BIN = 0;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (value, 1 row) ----
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 1 row) ----
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 1 row) ----
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 0 row) ----
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 0 row) ----
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
DROP TABLE empty_innodb, empty_myisam, nonempty_innodb, nonempty_myisam;
SET SQL_LOG_BIN = 1;
######## ENFORCE_GTID_CONSISTENCY=WARN GTID_MODE=OFF_PERMISSIVE GTID_NEXT=ANONYMOUS ########
CREATE TABLE empty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE empty_myisam (a INT) ENGINE = MyISAM;
CREATE TABLE nonempty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE nonempty_myisam (a INT) ENGINE = MyISAM;
INSERT INTO nonempty_innodb VALUES (1);
INSERT INTO nonempty_myisam VALUES (1);
---- CREATE TEMPORARY ... SELECT (value, 1 row) ----
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 1 row) ----
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 1 row) ----
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 0 row) ----
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 0 row) ----
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
SET SQL_LOG_BIN = 0;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (value, 1 row) ----
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 1 row) ----
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 1 row) ----
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 0 row) ----
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 0 row) ----
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
DROP TABLE empty_innodb, empty_myisam, nonempty_innodb, nonempty_myisam;
SET SQL_LOG_BIN = 1;
######## ENFORCE_GTID_CONSISTENCY=WARN GTID_MODE=OFF_PERMISSIVE GTID_NEXT=GTID ########
CREATE TABLE empty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE empty_myisam (a INT) ENGINE = MyISAM;
CREATE TABLE nonempty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE nonempty_myisam (a INT) ENGINE = MyISAM;
INSERT INTO nonempty_innodb VALUES (1);
INSERT INTO nonempty_myisam VALUES (1);
---- CREATE TEMPORARY ... SELECT (value, 1 row) ----
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 1 row) ----
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 1 row) ----
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 0 row) ----
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 0 row) ----
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
SET SQL_LOG_BIN = 0;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (value, 1 row) ----
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 1 row) ----
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 1 row) ----
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 0 row) ----
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 0 row) ----
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
DROP TABLE empty_innodb, empty_myisam, nonempty_innodb, nonempty_myisam;
SET SQL_LOG_BIN = 1;
SET GLOBAL GTID_MODE = 0;
######## ENFORCE_GTID_CONSISTENCY=WARN GTID_MODE=OFF GTID_NEXT=AUTOMATIC ########
CREATE TABLE empty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE empty_myisam (a INT) ENGINE = MyISAM;
CREATE TABLE nonempty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE nonempty_myisam (a INT) ENGINE = MyISAM;
INSERT INTO nonempty_innodb VALUES (1);
INSERT INTO nonempty_myisam VALUES (1);
---- CREATE TEMPORARY ... SELECT (value, 1 row) ----
# enforce_gtid_consistency=2 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 1 row) ----
# enforce_gtid_consistency=2 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 1 row) ----
# enforce_gtid_consistency=2 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 0 row) ----
# enforce_gtid_consistency=2 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 0 row) ----
# enforce_gtid_consistency=2 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
SET SQL_LOG_BIN = 0;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (value, 1 row) ----
# enforce_gtid_consistency=2 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 1 row) ----
# enforce_gtid_consistency=2 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 1 row) ----
# enforce_gtid_consistency=2 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 0 row) ----
# enforce_gtid_consistency=2 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 0 row) ----
# enforce_gtid_consistency=2 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
DROP TABLE empty_innodb, empty_myisam, nonempty_innodb, nonempty_myisam;
SET SQL_LOG_BIN = 1;
######## ENFORCE_GTID_CONSISTENCY=WARN GTID_MODE=OFF GTID_NEXT=ANONYMOUS ########
CREATE TABLE empty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE empty_myisam (a INT) ENGINE = MyISAM;
CREATE TABLE nonempty_innodb (a INT) ENGINE = InnoDB;
CREATE TABLE nonempty_myisam (a INT) ENGINE = MyISAM;
INSERT INTO nonempty_innodb VALUES (1);
INSERT INTO nonempty_myisam VALUES (1);
---- CREATE TEMPORARY ... SELECT (value, 1 row) ----
# enforce_gtid_consistency=2 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 1 row) ----
# enforce_gtid_consistency=2 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 1 row) ----
# enforce_gtid_consistency=2 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (InnoDB, 0 row) ----
# enforce_gtid_consistency=2 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE TEMPORARY ... SELECT (MyISAM, 0 row) ----
# enforce_gtid_consistency=2 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
SET SQL_LOG_BIN = 0;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (value, 1 row) ----
# enforce_gtid_consistency=2 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT 1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 1 row) ----
# enforce_gtid_consistency=2 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 1 row) ----
# enforce_gtid_consistency=2 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM nonempty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (InnoDB, 0 row) ----
# enforce_gtid_consistency=2 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_innodb;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
---- CREATE ... SELECT, SQL_LOG_BIN=0 (MyISAM, 0 row) ----
# enforce_gtid_consistency=2 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TABLE t1 (a INT) SELECT * FROM empty_myisam;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TABLE IF EXISTS t1;
DROP TABLE empty_innodb, empty_myisam, nonempty_innodb, nonempty_myisam;
SET SQL_LOG_BIN = 1;
SET GLOBAL ENFORCE_GTID_CONSISTENCY = OFF;
SET GLOBAL SQL_NOTES = @old_sql_notes;
include/rpl_end.inc
include/end_replace_gtid_combination.inc [gtid_mode=off/on -> default_storage_engine=InnoDB/MyISAM]
