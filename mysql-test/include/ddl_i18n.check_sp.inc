#   - Check SHOW CREATE statement;

--echo
--echo

SHOW CREATE PROCEDURE p1|
--echo
SHOW CREATE PROCEDURE p2|
--echo
SHOW CREATE PROCEDURE mysqltest2.p3|
--echo
SHOW CREATE PROCEDURE mysqltest2.p4|

#   - Check SHOW statement;

--echo
--echo

--replace_column 5 MODIFIED 6 CREATED
SHOW PROCEDURE STATUS LIKE 'p1'|

--echo
--replace_column 5 MODIFIED 6 CREATED
SHOW PROCEDURE STATUS LIKE 'p2'|

--echo
--replace_column 5 MODIFIED 6 CREATED
SHOW PROCEDURE STATUS LIKE 'p3'|

--echo
--replace_column 5 MODIFIED 6 CREATED
SHOW PROCEDURE STATUS LIKE 'p4'|

#   - Check INFORMATION_SCHEMA;

--echo
--echo

--replace_column 16 CREATED 17 ALTERED
SELECT * FROM INFORMATION_SCHEMA.ROUTINES WHERE routine_name = 'p1'|

--echo
--replace_column 16 CREATED 17 ALTERED
SELECT * FROM INFORMATION_SCHEMA.ROUTINES WHERE routine_name = 'p2'|

--echo
--replace_column 16 CREATED 17 ALTERED
SELECT * FROM INFORMATION_SCHEMA.ROUTINES WHERE routine_name = 'p3'|

--echo
--replace_column 16 CREATED 17 ALTERED
SELECT * FROM INFORMATION_SCHEMA.ROUTINES WHERE routine_name = 'p4'|

#   - Initialize the used variables (actual values don't matter);

--echo
--echo

SET @a = '1'|
SET @b = '2'|

#   - Execute the routines;

--echo
--echo

CALL p1(@a, @b)|
SELECT COLLATION(@a) AS ca, COLLATION(@b) cb|

--echo

CALL p2(@a, @b)|
SELECT COLLATION(@a) AS ca, COLLATION(@b) cb|

--echo

CALL mysqltest2.p3(@a, @b)|
SELECT COLLATION(@a) AS ca, COLLATION(@b) cb|

--echo

CALL mysqltest2.p4(@a, @b)|
SELECT COLLATION(@a) AS ca, COLLATION(@b) cb|
