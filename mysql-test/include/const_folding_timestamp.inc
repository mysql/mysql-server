eval CREATE TABLE $table_name (t $ts_type$decimals $nullness);
eval INSERT INTO $table_name VALUES ($min_ts), ($ca_now), ($max_ts);
eval ANALYZE TABLE $table_name;
eval SELECT * FROM $table_name;
let $colname=t;
SET @OLD_SQL_MODE=@@SQL_MODE, @@SQL_MODE=ALLOW_INVALID_DATES;

--echo
--echo 1. String literals
--echo
--echo TIMESTAMP: Some out of range constants (adjusted to TZ +0300
--echo for MTR)
--echo DATETIME: 1970, 2038 just fine.
let $constant='0000-00-00 10:20:30';
--source include/const_folding_cmp_ops.inc
let $constant='2038-01-19 06:14:07.999999';
--source include/const_folding_cmp_ops.inc
let $constant='2038-01-19 06:14:08.000000';
--source include/const_folding_cmp_ops.inc
let $constant='1970-01-01 00:00:01';
--source include/const_folding_cmp_ops.inc
let $constant='1970-01-01 03:00:00.999999';
--source include/const_folding_cmp_ops.inc

--echo Zero date?
let $constant='0000-00-00 00:00:00';
--source include/const_folding_cmp_ops.inc

--echo Zero in date
let $constant='2018-00-03 00:00:00';
--source include/const_folding_cmp_ops.inc

--echo
--echo Some in range string literals
--echo
let $constant='2038-01-19 06:14:07';
--source include/const_folding_cmp_ops.inc
let $constant='1970-01-01 03:00:01';
--source include/const_folding_cmp_ops.inc

--echo
--echo 2. TIMESTAMP literals
--echo These are the same as above, but with TIMESTAMP prefix.
--echo
--echo TIMESTAMP field: Some out of range constants (adjusted to TZ
--echo +0300 for MTR)
--echo DATETIME field: in range

# Gives error: let $constant=TIMESTAMP'0000-00-00 10:20:30';

let $constant=TIMESTAMP'2038-01-19 06:14:07.999999';
--source include/const_folding_cmp_ops.inc
let $constant=TIMESTAMP'2038-01-19 06:14:08.000000';
--source include/const_folding_cmp_ops.inc

# Gives error: let $constant=TIMESTAMP'12038-01-19 06:14:08.000000';

let $constant=TIMESTAMP'1970-01-01 00:00:01';
--source include/const_folding_cmp_ops.inc
let $constant=TIMESTAMP'1970-01-01 03:00:00.999999';
--source include/const_folding_cmp_ops.inc

# Gives error: let $constant=TIMESTAMP'0000-00-00 00:00:00';
# Gives error: let $constant=TIMESTAMP'2018-02-31 00:00:00';


--echo
--echo Some in range constants
--echo
let $constant=TIMESTAMP'2038-01-19 06:14:07';
--source include/const_folding_cmp_ops.inc
let $constant=TIMESTAMP'1970-01-01 03:00:01';
--source include/const_folding_cmp_ops.inc

--echo
--echo 3. Integer literals
--echo 

# in range
let $constant=20380119061407;
--source include/const_folding_cmp_ops.inc

# out of range; interpreted as a value smaller than TIMESTAMP min
let $constant=2018;
--source include/const_folding_cmp_ops.inc

let $constant=0;
--source include/const_folding_cmp_ops.inc

--echo
--echo 3. Decimal literals: rounded up to whole seconds
--echo 
# in range
let $constant=20380119061405.55;
--source include/const_folding_cmp_ops.inc

# out of range compare using double
let $constant=2018.55;
--source include/const_folding_cmp_ops.inc

let $constant=0.00;
--source include/const_folding_cmp_ops.inc

--echo
--echo 3. Real literals: rounded up to whole seconds
--echo
# in range
let $constant=20380119061405.55E00;
--source include/const_folding_cmp_ops.inc
# out of range compare using double
let $constant=2018.55E00;

let $constant=0.00E00;
--source include/const_folding_cmp_ops.inc

eval DROP TABLE $table_name;
