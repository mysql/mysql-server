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
---- CREATE TEMPORARY in trx (InnoDB, BEGIN) ----
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, BEGIN) ----
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- DROP TEMPORARY in trx (InnoDB, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (MyISAM, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
---- DROP TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
Warnings:
Note	1051	Unknown table 'test.t1'
######## ENFORCE_GTID_CONSISTENCY=OFF GTID_MODE=ON_PERMISSIVE GTID_NEXT=ANONYMOUS ########
---- CREATE TEMPORARY in trx (InnoDB, BEGIN) ----
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, BEGIN) ----
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- DROP TEMPORARY in trx (InnoDB, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (MyISAM, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
---- DROP TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
Warnings:
Note	1051	Unknown table 'test.t1'
######## ENFORCE_GTID_CONSISTENCY=OFF GTID_MODE=ON_PERMISSIVE GTID_NEXT=GTID ########
---- CREATE TEMPORARY in trx (InnoDB, BEGIN) ----
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, BEGIN) ----
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- DROP TEMPORARY in trx (InnoDB, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (MyISAM, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
---- DROP TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=0 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
Warnings:
Note	1051	Unknown table 'test.t1'
SET GLOBAL GTID_MODE = 1;
######## ENFORCE_GTID_CONSISTENCY=OFF GTID_MODE=OFF_PERMISSIVE GTID_NEXT=AUTOMATIC ########
---- CREATE TEMPORARY in trx (InnoDB, BEGIN) ----
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, BEGIN) ----
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- DROP TEMPORARY in trx (InnoDB, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (MyISAM, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
---- DROP TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
Warnings:
Note	1051	Unknown table 'test.t1'
######## ENFORCE_GTID_CONSISTENCY=OFF GTID_MODE=OFF_PERMISSIVE GTID_NEXT=ANONYMOUS ########
---- CREATE TEMPORARY in trx (InnoDB, BEGIN) ----
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, BEGIN) ----
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- DROP TEMPORARY in trx (InnoDB, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (MyISAM, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
---- DROP TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
Warnings:
Note	1051	Unknown table 'test.t1'
######## ENFORCE_GTID_CONSISTENCY=OFF GTID_MODE=OFF_PERMISSIVE GTID_NEXT=GTID ########
---- CREATE TEMPORARY in trx (InnoDB, BEGIN) ----
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, BEGIN) ----
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- DROP TEMPORARY in trx (InnoDB, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (MyISAM, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
---- DROP TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=0 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
Warnings:
Note	1051	Unknown table 'test.t1'
SET GLOBAL GTID_MODE = 0;
######## ENFORCE_GTID_CONSISTENCY=OFF GTID_MODE=OFF GTID_NEXT=AUTOMATIC ########
---- CREATE TEMPORARY in trx (InnoDB, BEGIN) ----
# enforce_gtid_consistency=0 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, BEGIN) ----
# enforce_gtid_consistency=0 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=0 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=0 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- DROP TEMPORARY in trx (InnoDB, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
# enforce_gtid_consistency=0 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (MyISAM, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
# enforce_gtid_consistency=0 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=0 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
---- DROP TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=0 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
Warnings:
Note	1051	Unknown table 'test.t1'
######## ENFORCE_GTID_CONSISTENCY=OFF GTID_MODE=OFF GTID_NEXT=ANONYMOUS ########
---- CREATE TEMPORARY in trx (InnoDB, BEGIN) ----
# enforce_gtid_consistency=0 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, BEGIN) ----
# enforce_gtid_consistency=0 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=0 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=0 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- DROP TEMPORARY in trx (InnoDB, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
# enforce_gtid_consistency=0 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (MyISAM, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
# enforce_gtid_consistency=0 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=0 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
---- DROP TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=0 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
Warnings:
Note	1051	Unknown table 'test.t1'
SET GLOBAL ENFORCE_GTID_CONSISTENCY = 1;
SET GLOBAL GTID_MODE = OFF_PERMISSIVE;
SET GLOBAL GTID_MODE = ON_PERMISSIVE;
SET GLOBAL GTID_MODE = ON;
SET GLOBAL GTID_MODE = 3;
######## ENFORCE_GTID_CONSISTENCY=ERROR GTID_MODE=ON GTID_NEXT=AUTOMATIC ########
---- CREATE TEMPORARY in trx (InnoDB, BEGIN) ----
# enforce_gtid_consistency=1 gtid_mode=3 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, BEGIN) ----
# enforce_gtid_consistency=1 gtid_mode=3 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=1 gtid_mode=3 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=1 gtid_mode=3 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- DROP TEMPORARY in trx (InnoDB, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
# enforce_gtid_consistency=1 gtid_mode=3 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (MyISAM, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
# enforce_gtid_consistency=1 gtid_mode=3 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=1 gtid_mode=3 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
---- DROP TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=1 gtid_mode=3 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
Warnings:
Note	1051	Unknown table 'test.t1'
######## ENFORCE_GTID_CONSISTENCY=ERROR GTID_MODE=ON GTID_NEXT=GTID ########
---- CREATE TEMPORARY in trx (InnoDB, BEGIN) ----
# enforce_gtid_consistency=1 gtid_mode=3 gtid_next=GTID
SET GTID_NEXT = 'GTID';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, BEGIN) ----
# enforce_gtid_consistency=1 gtid_mode=3 gtid_next=GTID
SET GTID_NEXT = 'GTID';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=1 gtid_mode=3 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=1 gtid_mode=3 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- DROP TEMPORARY in trx (InnoDB, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
# enforce_gtid_consistency=1 gtid_mode=3 gtid_next=GTID
SET GTID_NEXT = 'GTID';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (MyISAM, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
# enforce_gtid_consistency=1 gtid_mode=3 gtid_next=GTID
SET GTID_NEXT = 'GTID';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=1 gtid_mode=3 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
---- DROP TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=1 gtid_mode=3 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
Warnings:
Note	1051	Unknown table 'test.t1'
SET GLOBAL GTID_MODE = 2;
######## ENFORCE_GTID_CONSISTENCY=ERROR GTID_MODE=ON_PERMISSIVE GTID_NEXT=AUTOMATIC ########
---- CREATE TEMPORARY in trx (InnoDB, BEGIN) ----
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, BEGIN) ----
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- DROP TEMPORARY in trx (InnoDB, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (MyISAM, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
---- DROP TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
Warnings:
Note	1051	Unknown table 'test.t1'
######## ENFORCE_GTID_CONSISTENCY=ERROR GTID_MODE=ON_PERMISSIVE GTID_NEXT=ANONYMOUS ########
---- CREATE TEMPORARY in trx (InnoDB, BEGIN) ----
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, BEGIN) ----
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- DROP TEMPORARY in trx (InnoDB, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (MyISAM, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
---- DROP TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
Warnings:
Note	1051	Unknown table 'test.t1'
######## ENFORCE_GTID_CONSISTENCY=ERROR GTID_MODE=ON_PERMISSIVE GTID_NEXT=GTID ########
---- CREATE TEMPORARY in trx (InnoDB, BEGIN) ----
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, BEGIN) ----
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- DROP TEMPORARY in trx (InnoDB, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (MyISAM, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
---- DROP TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=1 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
Warnings:
Note	1051	Unknown table 'test.t1'
SET GLOBAL GTID_MODE = 1;
######## ENFORCE_GTID_CONSISTENCY=ERROR GTID_MODE=OFF_PERMISSIVE GTID_NEXT=AUTOMATIC ########
---- CREATE TEMPORARY in trx (InnoDB, BEGIN) ----
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, BEGIN) ----
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- DROP TEMPORARY in trx (InnoDB, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (MyISAM, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
---- DROP TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
Warnings:
Note	1051	Unknown table 'test.t1'
######## ENFORCE_GTID_CONSISTENCY=ERROR GTID_MODE=OFF_PERMISSIVE GTID_NEXT=ANONYMOUS ########
---- CREATE TEMPORARY in trx (InnoDB, BEGIN) ----
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, BEGIN) ----
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- DROP TEMPORARY in trx (InnoDB, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (MyISAM, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
---- DROP TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
Warnings:
Note	1051	Unknown table 'test.t1'
######## ENFORCE_GTID_CONSISTENCY=ERROR GTID_MODE=OFF_PERMISSIVE GTID_NEXT=GTID ########
---- CREATE TEMPORARY in trx (InnoDB, BEGIN) ----
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, BEGIN) ----
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- DROP TEMPORARY in trx (InnoDB, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (MyISAM, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
---- DROP TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=1 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
Warnings:
Note	1051	Unknown table 'test.t1'
SET GLOBAL GTID_MODE = 0;
######## ENFORCE_GTID_CONSISTENCY=ERROR GTID_MODE=OFF GTID_NEXT=AUTOMATIC ########
---- CREATE TEMPORARY in trx (InnoDB, BEGIN) ----
# enforce_gtid_consistency=1 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, BEGIN) ----
# enforce_gtid_consistency=1 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=1 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=1 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- DROP TEMPORARY in trx (InnoDB, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
# enforce_gtid_consistency=1 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (MyISAM, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
# enforce_gtid_consistency=1 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=1 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
---- DROP TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=1 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
Warnings:
Note	1051	Unknown table 'test.t1'
######## ENFORCE_GTID_CONSISTENCY=ERROR GTID_MODE=OFF GTID_NEXT=ANONYMOUS ########
---- CREATE TEMPORARY in trx (InnoDB, BEGIN) ----
# enforce_gtid_consistency=1 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, BEGIN) ----
# enforce_gtid_consistency=1 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=1 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=1 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- DROP TEMPORARY in trx (InnoDB, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
# enforce_gtid_consistency=1 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (MyISAM, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
# enforce_gtid_consistency=1 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=1 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
---- DROP TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=1 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
Warnings:
Note	1051	Unknown table 'test.t1'
SET GLOBAL ENFORCE_GTID_CONSISTENCY = 2;
SET GLOBAL GTID_MODE = OFF_PERMISSIVE;
SET GLOBAL GTID_MODE = ON_PERMISSIVE;
SET GLOBAL GTID_MODE = 2;
######## ENFORCE_GTID_CONSISTENCY=WARN GTID_MODE=ON_PERMISSIVE GTID_NEXT=AUTOMATIC ########
---- CREATE TEMPORARY in trx (InnoDB, BEGIN) ----
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, BEGIN) ----
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- DROP TEMPORARY in trx (InnoDB, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (MyISAM, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
---- DROP TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
Warnings:
Note	1051	Unknown table 'test.t1'
######## ENFORCE_GTID_CONSISTENCY=WARN GTID_MODE=ON_PERMISSIVE GTID_NEXT=ANONYMOUS ########
---- CREATE TEMPORARY in trx (InnoDB, BEGIN) ----
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, BEGIN) ----
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- DROP TEMPORARY in trx (InnoDB, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (MyISAM, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
---- DROP TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
Warnings:
Note	1051	Unknown table 'test.t1'
######## ENFORCE_GTID_CONSISTENCY=WARN GTID_MODE=ON_PERMISSIVE GTID_NEXT=GTID ########
---- CREATE TEMPORARY in trx (InnoDB, BEGIN) ----
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, BEGIN) ----
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- DROP TEMPORARY in trx (InnoDB, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (MyISAM, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
---- DROP TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=2 gtid_mode=2 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
Warnings:
Note	1051	Unknown table 'test.t1'
SET GLOBAL GTID_MODE = 1;
######## ENFORCE_GTID_CONSISTENCY=WARN GTID_MODE=OFF_PERMISSIVE GTID_NEXT=AUTOMATIC ########
---- CREATE TEMPORARY in trx (InnoDB, BEGIN) ----
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, BEGIN) ----
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- DROP TEMPORARY in trx (InnoDB, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (MyISAM, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
---- DROP TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
Warnings:
Note	1051	Unknown table 'test.t1'
######## ENFORCE_GTID_CONSISTENCY=WARN GTID_MODE=OFF_PERMISSIVE GTID_NEXT=ANONYMOUS ########
---- CREATE TEMPORARY in trx (InnoDB, BEGIN) ----
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, BEGIN) ----
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- DROP TEMPORARY in trx (InnoDB, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (MyISAM, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
---- DROP TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
Warnings:
Note	1051	Unknown table 'test.t1'
######## ENFORCE_GTID_CONSISTENCY=WARN GTID_MODE=OFF_PERMISSIVE GTID_NEXT=GTID ########
---- CREATE TEMPORARY in trx (InnoDB, BEGIN) ----
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, BEGIN) ----
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- DROP TEMPORARY in trx (InnoDB, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (MyISAM, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
---- DROP TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=2 gtid_mode=1 gtid_next=GTID
SET GTID_NEXT = 'GTID';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
Warnings:
Note	1051	Unknown table 'test.t1'
SET GLOBAL GTID_MODE = 0;
######## ENFORCE_GTID_CONSISTENCY=WARN GTID_MODE=OFF GTID_NEXT=AUTOMATIC ########
---- CREATE TEMPORARY in trx (InnoDB, BEGIN) ----
# enforce_gtid_consistency=2 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, BEGIN) ----
# enforce_gtid_consistency=2 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=2 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=2 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- DROP TEMPORARY in trx (InnoDB, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
# enforce_gtid_consistency=2 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (MyISAM, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
# enforce_gtid_consistency=2 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=2 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
---- DROP TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=2 gtid_mode=0 gtid_next=AUTOMATIC
SET GTID_NEXT = 'AUTOMATIC';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
Warnings:
Note	1051	Unknown table 'test.t1'
######## ENFORCE_GTID_CONSISTENCY=WARN GTID_MODE=OFF GTID_NEXT=ANONYMOUS ########
---- CREATE TEMPORARY in trx (InnoDB, BEGIN) ----
# enforce_gtid_consistency=2 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, BEGIN) ----
# enforce_gtid_consistency=2 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
BEGIN;
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=2 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = InnoDB;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- CREATE TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=2 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] CREATE TEMPORARY TABLE t1 (a INT) ENGINE = MyISAM;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
ROLLBACK;
Warnings:
Warning	1751	The creation of some temporary tables could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
---- DROP TEMPORARY in trx (InnoDB, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
# enforce_gtid_consistency=2 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (MyISAM, BEGIN) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
# enforce_gtid_consistency=2 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
BEGIN;
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
---- DROP TEMPORARY in trx (InnoDB, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = InnoDB;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=2 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
---- DROP TEMPORARY in trx (MyISAM, AUTOCOMMIT=0) ----
CREATE TEMPORARY TABLE IF NOT EXISTS t1 (a INT) ENGINE = MyISAM;
SET AUTOCOMMIT = 0;
# enforce_gtid_consistency=2 gtid_mode=0 gtid_next=ANONYMOUS
SET GTID_NEXT = 'ANONYMOUS';
[START] DROP TEMPORARY TABLE t1;
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
[END] DROP TEMPORARY TABLE t1;
include/assert.inc [No warning or error should be generated.]
include/assert.inc [ONGOING_AUTOMATIC_GTID_VIOLATING_TRANSACTION_COUNT should be 0]
include/assert.inc [ONGOING_ANONYMOUS_GTID_VIOLATING_TRANSACTION_COUNT should be 1 (stm) / 0 (row / mix)]
ROLLBACK;
Warnings:
Warning	1752	Some temporary tables were dropped, but these operations could not be rolled back.
SET GTID_NEXT = 'AUTOMATIC';
include/assert.inc [Both counters should be 0]
SET AUTOCOMMIT = 1;
DROP TEMPORARY TABLE IF EXISTS t1;
Warnings:
Note	1051	Unknown table 'test.t1'
SET GLOBAL ENFORCE_GTID_CONSISTENCY = OFF;
SET GLOBAL SQL_NOTES = @old_sql_notes;
include/rpl_end.inc
