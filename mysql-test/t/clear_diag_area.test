--echo # WL#5928: Most statements should clear the diagnostic area

SELECT @@max_heap_table_size INTO @old_max_heap_table_size;
SET sql_mode = 'NO_ENGINE_SUBSTITUTION';
--echo #
--echo # Throw error, run some queries that shouldn't change diagnostics.
--echo #

--error ER_BAD_TABLE_ERROR
DROP TABLE no_such_table;

SHOW ERRORS;
SHOW WARNINGS;
SHOW COUNT(*) ERRORS;
SHOW COUNT(*) WARNINGS;
GET DIAGNOSTICS @n = NUMBER;
GET DIAGNOSTICS CONDITION 1 @err_no = MYSQL_ERRNO, @err_txt = MESSAGE_TEXT;
SELECT @n, @err_no, @err_txt;



--echo
--echo #
--echo # Contrary to SHOW, these will now reset the diagnostics area:
--echo #

# This will fail on --ps-protocol because that does a {prepare, execute}
# sequence for each command (rather than preparing earlier). This means
# that there is a prepare "SELECT @@warning_count" in between the execute
# of the throwing statement and that of the SELECT @@warning_count. Thus,
# the prepare will clear the diagnostics before the execute can see them.
# To prevent hard to find errors due to counter-intuitive semantics, we
# fail @@warning_count and @@error_count with ER_UNSUPPORTED_PS during
# prepare!
--disable_ps_protocol

--error ER_BAD_TABLE_ERROR
DROP TABLE no_such_table;
SELECT @@error_count;
SELECT @@error_count;
--error ER_BAD_TABLE_ERROR
DROP TABLE no_such_table;
SELECT @@warning_count;
SELECT @@warning_count;

CREATE TABLE IF NOT EXISTS t2 (f1 INT);
CREATE TABLE IF NOT EXISTS t2 (f1 INT);
SELECT @@warning_count;

DROP TABLE t2;

--enable_ps_protocol



--echo
--echo #
--echo # Parse-error
--echo #
--echo # Errors may occur during the parse-stage -- before we know whether
--echo # the current statement is a diagnostics statement and must preserve the
--echo # previous statement's diagnostics area!  Show that we handle this right.
--echo #
--error ER_BAD_TABLE_ERROR
DROP TABLE no_such_table;

--echo parser may use message for ER_SYNTAX_ERROR, but always uses number
--echo ER_PARSE_ERROR instead.
--echo Only sql_binlog.cc actually uses number ER_SYNTAX_ERROR.
--error ER_PARSE_ERROR
GET DIAGNOSTICS;

GET DIAGNOSTICS @n = NUMBER;
GET DIAGNOSTICS CONDITION 1 @err_no = MYSQL_ERRNO, @err_txt = MESSAGE_TEXT;
SELECT @n, @err_no, @err_txt;

--echo Here is another error the parser can throw, though:
--error ER_UNKNOWN_SYSTEM_VARIABLE
SET GLOBAL wombat = 'pangolin';



--echo
--echo #
--echo # SP: Stored Procedures
--echo #

# neutral:
# - DECLARE CONDITION
# - DECLARE HANDLER
# - BEGIN..END

DELIMITER |;

CREATE PROCEDURE p0_proc_with_warning ()
BEGIN
  SELECT CAST('2001-01-01' AS SIGNED INT);
END|

CREATE PROCEDURE p1_declare_handler_preserves ()
BEGIN
  DECLARE CONTINUE HANDLER FOR NOT FOUND
  BEGIN

    /* Nes gadol hayah poh -- first handler should have been called.
       DECLARE another handler. This should NOT clear the DA! */

    DECLARE red CONDITION FOR 1051;
    DECLARE CONTINUE HANDLER FOR red
    BEGIN
      GET DIAGNOSTICS @n0 = NUMBER;
      GET DIAGNOSTICS CONDITION 1 @e0 = MYSQL_ERRNO, @t0 = MESSAGE_TEXT;
    END;

    /* The important bit here is that there are two diagnostics statements
       in a row, so we can show that the first one does not clear the
       diagnostics area. */

    GET DIAGNOSTICS @n1 = NUMBER;
    GET DIAGNOSTICS CONDITION 1 @e1 = MYSQL_ERRNO, @t1 = MESSAGE_TEXT;
  END;

  SET @n1 = 0, @e1 = 0, @t1 = 'handler was not run or GET DIAGNOSTICS failed';

  SIGNAL SQLSTATE '02000' SET MESSAGE_TEXT = 'signal message';

  /* Show handler was called, and DA was read intact despite of other
     DECLAREs on the way: */

  SELECT @n1, @e1, @t1;
END|



CREATE PROCEDURE p2_declare_variable_clears ()
BEGIN
  DECLARE CONTINUE HANDLER FOR NOT FOUND
  BEGIN

    /* DECLARE a variable. This will clear the diagnostics area, so
       the subsequent GET DIAGNOSTICS will fail. It in turn will flag
       a warning (not an exception), which will remain unseen, as it
       in turn gets cleared by the next statement (SELECT). */

    DECLARE v1 INT;

    GET DIAGNOSTICS @n2 = NUMBER;
    GET DIAGNOSTICS CONDITION 1 @e2 = MYSQL_ERRNO, @t2 = MESSAGE_TEXT;
  END;

  SET @n2 = 0, @e2 = 0, @t2 = 'handler was not run or GET DIAGNOSTICS failed';

  SIGNAL SQLSTATE '02000' SET MESSAGE_TEXT = 'signal message';

  /* Show handler was called, and DA was NOT read intact because of DECLARE VARIABLE. */

  SELECT @n2, @e2, @t2;
END|



CREATE PROCEDURE p6_bubble_warning ()
BEGIN
  DECLARE CONTINUE HANDLER FOR NOT FOUND
  BEGIN

    /* Absurdly high CONDITION number will cause GET DIAG to fail.
       As it is the last statement, warning should bubble up to caller. */

    GET DIAGNOSTICS CONDITION 99 @e6 = MYSQL_ERRNO, @t6 = MESSAGE_TEXT;
  END;

  SET @n2 = 0, @e2 = 0, @t2 = 'handler was not run or GET DIAGNOSTICS failed';

  SIGNAL SQLSTATE '02000' SET MESSAGE_TEXT = 'signal message';
END|



CREATE PROCEDURE p5_declare_variable_clears ()
BEGIN
  DECLARE CONTINUE HANDLER FOR NOT FOUND
  BEGIN

    /* DECLARE a VARIABLE with a broken DEFAULT. This will throw a
       warning at runtime, which GET DIAGNOSTICS will see instead of
       the previous condition (the SIGNAL). */

    DECLARE v1 INT DEFAULT 'meow';

    GET DIAGNOSTICS @n5 = NUMBER;
    GET DIAGNOSTICS CONDITION 1 @e5= MYSQL_ERRNO, @t5 = MESSAGE_TEXT;
  END;

  SET @n5 = 0, @e5 = 0, @t5 = 'handler was not run or GET DIAGNOSTICS failed';

  SIGNAL SQLSTATE '02000' SET MESSAGE_TEXT = 'signal message';

  /* Show handler was called, and DA was NOT read intact because of DECLARE VARIABLE. */

  SELECT @n5, @e5, @t5;
  SELECT "still here, we got a warning, not an exception!";
END|



CREATE PROCEDURE p3_non_diagnostics_stmt_clears ()
BEGIN
  DECLARE CONTINUE HANDLER FOR NOT FOUND
  BEGIN

    /* Do some stuff before using GET (CURRENT, not STACKED) DIAGNOSTICS.
       This will clear the DA.
       show that handler was run, even if GET DIAG below fails! */

    SET @t3 = 'handler was run, but GET DIAGNOSTICS failed';
    SELECT 1 FROM DUAL;

    GET CURRENT DIAGNOSTICS @n3 = NUMBER;
    GET CURRENT DIAGNOSTICS CONDITION 1 @e3 = MYSQL_ERRNO, @t3 = MESSAGE_TEXT;
  END;

  SET @n3 = 0, @e3 = 0, @t3 = 'handler was not run or GET DIAGNOSTICS failed';

  SIGNAL SQLSTATE '02000' SET MESSAGE_TEXT = 'signal message';

  /* Show handler was called. */

  SELECT @n3, @e3, @t3;
END|



CREATE PROCEDURE p4_unhandled_exception_returned ()
BEGIN

  /* This will throw an exception which we do not handle,
     so execution will abort, and the caller will see
     the error. */

  DROP TABLE no_such_table;
  SELECT "we should never get here";
END|



# this guards against a regression to MySQL Bug#4902 (see sp.test),
# a failure to handle MULTI_RESULTS correctly. That's a crashing bug.
CREATE PROCEDURE p7_show_warnings ()
BEGIN
  SHOW VARIABLES LIKE 'foo';
  SHOW WARNINGS;
  SELECT "(SHOW WARNINGS does not have to come last)";
END|

CREATE PROCEDURE p8a_empty ()
BEGIN
END|

CREATE PROCEDURE p8b_show_warnings ()
BEGIN
  SHOW WARNINGS;
END|


DELIMITER ;|


--echo
--echo # warning bubbles up as it is thrown in the last statement (END does not count).
--echo # 1292, "truncated"
CALL p0_proc_with_warning;
DROP PROCEDURE p0_proc_with_warning;


--echo
--echo # DECLARE HANDLER does not reset DA.
--echo # 1643, "signal message"
CALL p1_declare_handler_preserves;
DROP PROCEDURE p1_declare_handler_preserves;


--echo
--echo # DECLARE VARIABLE clears DA.
--echo # 0 (but "GET DIAGNOSTICS failed")
CALL p2_declare_variable_clears;
DROP PROCEDURE p2_declare_variable_clears;

--echo
--echo # DECLARE VARIABLE with broken DEFAULT throws error.
--echo # 1366, "Incorrect integer value"
CALL p5_declare_variable_clears;
DROP PROCEDURE p5_declare_variable_clears;

--echo
--echo # show that GET DIAGNOSTICS produces warnings on failure.
--echo # 1758, "GET DIAGNOSTICS failed: Invalid condition number"
CALL p6_bubble_warning;
DROP PROCEDURE p6_bubble_warning;

--echo
--echo # non-diagnostics statement in handler clears DA before GET DIAGNOSTICS
--echo # 0 (but "GET DIAGNOSTICS failed")
CALL p3_non_diagnostics_stmt_clears;
DROP PROCEDURE p3_non_diagnostics_stmt_clears;

--echo
--echo # unhandled exception terminates SP, bubbles up
--echo # 1051, "unknown table"
--error ER_BAD_TABLE_ERROR
CALL p4_unhandled_exception_returned;
DROP PROCEDURE p4_unhandled_exception_returned;

--echo
--echo # SHOW WARNINGS is flagged MULTI_RESULTS. Show that we can handle that.
--echo # 1051, "unknown table"
CALL p7_show_warnings;
DROP PROCEDURE p7_show_warnings;

--echo
--echo # CALL is a procedure statement, so a called procedure won't see
--echo # warnings generated in its caller, and a post-CALL statement
--echo # won't see warnings from before the CALL even if the procedure
--echo # was empty.
--error ER_BAD_TABLE_ERROR
DROP TABLE no_such_table;
CALL p8a_empty;
SHOW WARNINGS;
DROP PROCEDURE p8a_empty;

--error ER_BAD_TABLE_ERROR
DROP TABLE no_such_table;
CALL p8b_show_warnings;
DROP PROCEDURE p8b_show_warnings;

--echo
--echo #
--echo # SF: Stored Functions
--echo #

DELIMITER |;
CREATE FUNCTION f2_unseen_warnings() RETURNS INT
BEGIN
  /* throw a warning */
  SET @@max_heap_table_size= 1;
  /* RETURN counts as a statement as per the standard, so clears DA */
  RETURN 1;
END|
DELIMITER ;|


DELIMITER |;
CREATE FUNCTION f3_stacking_warnings() RETURNS TEXT
BEGIN
  /* throw a warning */
  RETURN CAST('2001-01-01' AS SIGNED INT);
END|
DELIMITER ;|


# show we didn't break this
DELIMITER |;
--error ER_SP_NO_RETSET
CREATE FUNCTION f4_show_warnings() RETURNS TEXT
BEGIN
  SHOW WARNINGS;
  RETURN "yeah, not so much";
END|
DELIMITER ;|



--echo
--echo # SF: warnings within remain unseen, except when from last statement (RETURN)
SELECT f2_unseen_warnings();
SHOW WARNINGS;
SET @@max_heap_table_size= 1;
DROP FUNCTION f2_unseen_warnings;

--echo
--echo # SF: warnings within bubble up, if from last statement (RETURN).
--echo #     warnings from multiple function calls are all preserved.
SELECT f3_stacking_warnings(),f3_stacking_warnings(),f3_stacking_warnings();
DROP FUNCTION f3_stacking_warnings;



--echo
--echo # PS
--echo #

--echo # show prepared statements still throw warnings OK
PREPARE stmt1 FROM 'create table if not exists t1 (f1 int)';
EXECUTE stmt1;
EXECUTE stmt1;
DEALLOCATE PREPARE stmt1;
DROP TABLE t1;

--echo # show prepared statements can activate handler

PREPARE stmt1 FROM 'create table if not exists t1 (f1 int)';

EXECUTE stmt1;

DELIMITER |;

CREATE PROCEDURE p10_ps_with_warning ()
BEGIN
  DECLARE CONTINUE HANDLER FOR 1050 SELECT "a warn place";
  EXECUTE stmt1;
END|

DELIMITER ;|

CALL p10_ps_with_warning ();

DROP PROCEDURE p10_ps_with_warning;
DEALLOCATE PREPARE stmt1;
DROP TABLE t1;

--echo # show prepared statements still throw warnings OK
PREPARE stmt1 FROM 'create table if not exists t1 (f1 year)';
EXECUTE stmt1;
EXECUTE stmt1;
DEALLOCATE PREPARE stmt1;
DROP TABLE t1;



--echo
--echo # GET DIAGNOSTICS is not supported as prepared statement for now.
--echo # (It shouldn't be; Foundation 2007, 4.33.7)
--echo # This placeholder is intended to fail once that changes, so we
--echo # can add a proper test for it here.
SET @sql1='GET DIAGNOSTICS CONDITION 1 @err_no = MYSQL_ERRNO, @err_txt = MESSAGE_TEXT';
--error ER_UNSUPPORTED_PS
PREPARE stmt1 FROM @sql1;

--echo
--echo # Foundation 2007, 4.33.7 says diagnostics statements shouldn't be preparable!
--error ER_UNSUPPORTED_PS
PREPARE stmt2 FROM 'SHOW WARNINGS';
--error ER_UNSUPPORTED_PS
PREPARE stmt2 FROM 'SHOW ERRORS';
--error ER_UNSUPPORTED_PS
PREPARE stmt2 FROM 'SHOW COUNT(*) WARNINGS';
--error ER_UNSUPPORTED_PS
PREPARE stmt2 FROM 'SHOW COUNT(*) ERRORS';
--error ER_UNSUPPORTED_PS
PREPARE stmt2 FROM 'SELECT @@warning_count';
--error ER_UNSUPPORTED_PS
PREPARE stmt2 FROM 'SELECT @@error_count';



--echo
--echo # Bug#11745821: SELECT WITH NO TABLES BUT A DERIVED TABLE RESETS WARNING LIST, CONTRARY T
--echo #
# 1a)
# throw warning
SET @@max_heap_table_size= 1;
# SELECT without tables now clears it:
SELECT 1;
SHOW WARNINGS;
--echo
# 1b)
# throw warning
SET @@max_heap_table_size= 1;
# SELECT with sub-SELECT (but still without tables) still clears it:
SELECT 1 FROM (SELECT 1) t1;
SHOW WARNINGS;



--echo
--echo # additional tests derived from PeterG's mails.
--echo #

DELIMITER |;

CREATE PROCEDURE peter1 ()
BEGIN
  DECLARE v INTEGER DEFAULT 1234;

  DECLARE CONTINUE HANDLER FOR SQLWARNING
  BEGIN
    SHOW WARNINGS;
    SELECT "handler done: ",v;
  END;

  CREATE TABLE gg (smallint_column SMALLINT);
  CALL peter2(v);
END|

CREATE PROCEDURE peter2 (INOUT v INTEGER)
BEGIN
  INSERT INTO gg (smallint_column) VALUES (32769);
  GET DIAGNOSTICS v = ROW_COUNT;
END|

CREATE PROCEDURE peter3(a DECIMAL(2,2))
BEGIN
   DECLARE b DECIMAL(2,2) DEFAULT @var;
END|

DELIMITER ;|

CALL peter1();

DROP PROCEDURE peter2;
DROP PROCEDURE peter1;
DROP TABLE gg;

SET @var="foo";
CALL peter3("bar");
DROP PROCEDURE peter3;


--echo
--echo # additional adapted tests from sp
--echo #

DELIMITER |;

CREATE TABLE t3 (id INT NOT NULL)|
  
CREATE PROCEDURE bug15231_1()
BEGIN
  DECLARE xid INTEGER;
  DECLARE xdone INTEGER DEFAULT 0;
  DECLARE CONTINUE HANDLER FOR NOT FOUND SET xdone = 1;

  SET xid=NULL;
  CALL bug15231_2a(xid);
  SELECT "1,0", xid, xdone;

  SET xid=NULL;
  CALL bug15231_2b(xid);
  SELECT "NULL, 1", xid, xdone;
END|

# This no longer works; SELECT id ... throws a warning, then the
# subsequent statements clear the diagnostics area before exit,
# and the caller never sees the warning, and therefore doesn't
# run the NOT FOUND handler!
CREATE PROCEDURE bug15231_2a(INOUT ioid INTEGER)
BEGIN
  SELECT "Before NOT FOUND condition is triggered" AS '1';
  SELECT id INTO ioid FROM t3 WHERE id=ioid;
  SELECT "After NOT FOUND condtition is triggered" AS '2';

  IF ioid IS NULL THEN
    SET ioid=1;
  END IF;
END|

# This works!  The warning is thrown on the last procedure
# statement (END isn't, and therefore doesn't clear the
# diagnostics area).  Therefore, the caller sees the warning,
# and runs the NOT FOUND handler.
CREATE PROCEDURE bug15231_2b(INOUT ioid INTEGER)
BEGIN
  SELECT id INTO ioid FROM t3 WHERE id=ioid;
END|

CALL bug15231_1()|

DROP PROCEDURE bug15231_1|
DROP PROCEDURE bug15231_2a|
DROP PROCEDURE bug15231_2b|

--echo #

CREATE PROCEDURE bug15231_3()
BEGIN
  DECLARE EXIT HANDLER FOR SQLWARNING
    SELECT 'Caught it (correct)' AS 'Result';

  CALL bug15231_4();
END|

CREATE PROCEDURE bug15231_4()
BEGIN
  DECLARE x DECIMAL(2,1);

  SET x = 'zap';
  SHOW WARNINGS;
END|

CALL bug15231_3()|

--echo #

CREATE PROCEDURE bug15231_5()
BEGIN
  DECLARE EXIT HANDLER FOR SQLWARNING
    SELECT 'Caught it (wrong)' AS 'Result';

  CALL bug15231_6();
END|

CREATE PROCEDURE bug15231_6()
BEGIN
  DECLARE x DECIMAL(2,1);

  SET x = 'zap';
  SELECT id FROM t3;
END|

CALL bug15231_5()|

--echo #

--echo #

CREATE PROCEDURE bug15231_7()
BEGIN
  DECLARE EXIT HANDLER FOR SQLEXCEPTION
    SELECT 'Caught it (right)' AS 'Result';

  CALL bug15231_8();
END|

CREATE PROCEDURE bug15231_8()
BEGIN
  DROP TABLE no_such_table;
  SELECT 'Caught it (wrong)' AS 'Result';
END|

CALL bug15231_7()|

--echo #

DROP TABLE t3|

DROP PROCEDURE bug15231_3|
DROP PROCEDURE bug15231_4|
DROP PROCEDURE bug15231_5|
DROP PROCEDURE bug15231_6|
DROP PROCEDURE bug15231_7|
DROP PROCEDURE bug15231_8|

DELIMITER ;|

SET sql_mode = 'NO_ENGINE_SUBSTITUTION';

--echo

SET @@max_heap_table_size= @old_max_heap_table_size;

--echo
--echo #
--echo # Done WL#5928
--echo #

--echo
--echo # END 5.7 TESTS
--echo #
