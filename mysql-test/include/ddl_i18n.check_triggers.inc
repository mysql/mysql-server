#   - Check SHOW CREATE statement;

--echo
--echo

SHOW CREATE TRIGGER trg1|
--echo
SHOW CREATE TRIGGER trg2|
--echo
SHOW CREATE TRIGGER mysqltest2.trg3|
--echo
SHOW CREATE TRIGGER mysqltest2.trg4|

#   - Check SHOW statement;

--echo
--echo

SHOW TRIGGERS|

--echo

use mysqltest2|

--echo

SHOW TRIGGERS|

use mysqltest1|

#   - Check INFORMATION_SCHEMA;

--echo
--echo

--replace_column 17 CREATED
SELECT * FROM INFORMATION_SCHEMA.TRIGGERS WHERE trigger_name = 'trg1'|

--echo
--replace_column 17 CREATED
SELECT * FROM INFORMATION_SCHEMA.TRIGGERS WHERE trigger_name = 'trg2'|

--echo
--replace_column 17 CREATED
SELECT * FROM INFORMATION_SCHEMA.TRIGGERS WHERE trigger_name = 'trg3'|

--echo
--replace_column 17 CREATED
SELECT * FROM INFORMATION_SCHEMA.TRIGGERS WHERE trigger_name = 'trg4'|

#   - Initialize the used variables (actual values don't matter);

--echo
--echo

SET @a1 = '1'|
SET @a2 = '1'|
SET @a3 = '1'|

SET @b1 = '2'|
SET @b2 = '2'|
SET @b3 = '2'|

#   - Execute the triggers;

--echo
--echo

INSERT INTO t1 VALUES(1)|

--echo
--echo ---> Log:
SELECT msg FROM log|

--echo
SELECT
  COLLATION(@a1) AS ca1,
  COLLATION(@a2) AS ca2,
  COLLATION(@a3) AS ca3,
  COLLATION(@b1) AS cb1,
  COLLATION(@b2) AS cb2,
  COLLATION(@b3) AS cb3|

--echo
DELETE FROM log|

--echo
--echo

INSERT INTO mysqltest2.t1 VALUES(1)|

--echo
--echo ---> Log:
SELECT msg FROM mysqltest2.log|

--echo
SELECT
  COLLATION(@a1) AS ca1,
  COLLATION(@a2) AS ca2,
  COLLATION(@a3) AS ca3,
  COLLATION(@b1) AS cb1,
  COLLATION(@b2) AS cb2,
  COLLATION(@b3) AS cb3|

--echo
DELETE FROM mysqltest2.log|
