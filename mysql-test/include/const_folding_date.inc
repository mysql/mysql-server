let $table_name=tbl_date;
let $colname=d;
eval CREATE TABLE tbl_date($colname DATE);
INSERT INTO tbl_date VALUES ('0000-01-01'), ('2038-01-19'), ('9999-12-31');
ANALYZE TABLE tbl_date;
SET @OLD_SQL_MODE=@@SQL_MODE, @@SQL_MODE=ALLOW_INVALID_DATES;

--echo
--echo 1. String literals
--echo
--echo

--echo Semi supported
let $constant='0000-01-01';
--source include/const_folding_cmp_ops.inc
let $constant='2038-01-19';
--source include/const_folding_cmp_ops.inc
let $constant='1000-01-01';
--source include/const_folding_cmp_ops.inc
let $constant='9999-12-31';
--source include/const_folding_cmp_ops.inc

--echo Compare to a full datetime value instead of a date
let $constant='2038-01-19 12:45:00';
--source include/const_folding_cmp_ops.inc


--echo Zero date?
let $constant='0000-00-00';
--source include/const_folding_cmp_ops.inc

--echo Zero in date
let $constant='2018-00-03';
--source include/const_folding_cmp_ops.inc

--echo Strangely, this does not give an error nor warning, and does
--echo comparison using internal datetime comparison (pre-folding), so
--echo we do the same.
let $constant='2018-02-31';
--source include/const_folding_cmp_ops.inc

--echo
--echo 2. TIMESTAMP literals compared with DATE field
--echo
let $constant=TIMESTAMP'2038-01-19 00:00:00';
--source include/const_folding_cmp_ops.inc
let $constant=TIMESTAMP'2038-01-19 00:00:01';
--source include/const_folding_cmp_ops.inc
let $constant=TIMESTAMP'2038-01-19 00:00:00.000001';

--echo
--echo 3. Integer literals
--echo 

--echo in range
let $constant=20380119;
--source include/const_folding_cmp_ops.inc

--echo incorrect value; removes time part
let $constant=20380119124500;
--source include/const_folding_cmp_ops.inc

--echo out of range; interpreted as a value smaller than TIMESTAMP min
let $constant=2018;
--source include/const_folding_cmp_ops.inc

let $constant=0;
--source include/const_folding_cmp_ops.inc

--echo
--echo 3. Decimal literals
--echo
let $constant=20380119.00;
--source include/const_folding_cmp_ops.inc

--echo incorrect value; removes time part
let $constant=20380119061405.00;
--source include/const_folding_cmp_ops.inc

--echo out of range - compare using double
let $constant=2018.55;
--source include/const_folding_cmp_ops.inc

let $constant=0.00;
--source include/const_folding_cmp_ops.inc

--echo
--echo 3. Real literals: rounded up to whole seconds
--echo
--echo in range
let $constant=20380119E00;
--source include/const_folding_cmp_ops.inc

--echo incorrect value; removes time part
let $constant=20380119194500E00;
--source include/const_folding_cmp_ops.inc

--echo out of range compare using double
let $constant=2018.55E00;

let $constant=0.00E00;
--source include/const_folding_cmp_ops.inc

eval DROP TABLE $table_name;
SET SESSION sql_mode = DEFAULT;