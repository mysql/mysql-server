#### suite/funcs_1/storedproc/storedproc_08.inc
#
--source suite/funcs_1/storedproc/load_sp_tb.inc

# ==============================================================================
# (numbering from requirement document TP v1.0, Last updated: 25 Jan 2005 01:00)
#
# 3.1.8 SHOW statement checks:
#
#-  1. Ensure that the definition of a procedure is properly recorded and displayed when a SHOW CREATE PROCEDURE or SHOW CREATE PROCEDURE STATUS statement is executed.
#-  2. Ensure that the definition of a procedure is not displayed when a SHOW CREATE FUNCTION or SHOW CREATE FUNCTION STATUS statement is executed.
#-  3. Ensure that an attempt to execute SHOW CREATE PROCEDURE or SHOW CREATE PROCEDURE STATUS, with the name of a non-existent procedure, fails with an appropriate error message.
#-  4. Ensure that an attempt to execute SHOW CREATE PROCEDURE or SHOW CREATE PROCEDURE STATUS, with the name of a function, fails with an appropriate error message.
#-  5. Ensure that the definition of a function is properly recorded and displayed when a SHOW CREATE FUNCTION or SHOW CREATE FUNCTION STATUS statement is executed.
#-  6. Ensure that the definition of a function is not displayed when a SHOW CREATE PROCEDURE or SHOW CREATE PROCEDURE STATUS statement is executed.
#-  7. Ensure that an attempt to execute SHOW CREATE FUNCTION or SHOW CREATE FUNCTION STATUS, with the name of a non-existent function, fails with an appropriate error message.
#-  8. Ensure that an attempt to execute SHOW CREATE FUNCTION or SHOW CREATE FUNCTION STATUS, with the name of a procedure, fails with an appropriate error message.
##  9. Ensure that all stored procedure changes made with ALTER PROCEDURE or ALTER FUNCTION are properly recorded and displayed when a SHOW CREATE PROCEDURE or SHOW CREATE PROCEDURE STATUS statement, or a SHOW CREATE FUNCTION or SHOW CREATE FUNCTION STATUS statement (respectively) is executed.
#- 10. Ensure that, when a stored procedure is dropped, its definition no longer appears when a SHOW CREATE PROCEDURE or SHOW CREATE PROCEDURE STATUS statement, or a SHOW CREATE FUNCTION or SHOW CREATE FUNCTION STATUS statement (respectively) is executed.
#
# ==============================================================================
let $message= Section 3.1.8 - SHOW statement checks:;
--source include/show_msg80.inc


# ------------------------------------------------------------------------------
let $message= Testcase 3.1.8.9:;
--source include/show_msg.inc
let $message=
Ensure that all stored procedure changes made with ALTER PROCEDURE or ALTER
FUNCTION are properly recorded and displayed when a SHOW CREATE PROCEDURE or
SHOW CREATE PROCEDURE STATUS statement, or a SHOW CREATE FUNCTION or SHOW CREATE
FUNCTION STATUS statement (respectively) is executed.;
--source include/show_msg80.inc

--disable_warnings
DROP FUNCTION IF EXISTS fn_1;
DROP FUNCTION IF EXISTS fn_2;
DROP PROCEDURE IF EXISTS sp_1;
DROP PROCEDURE IF EXISTS sp_2;
--enable_warnings

delimiter //;
CREATE PROCEDURE sp_1 (i1 int)
BEGIN
    set @x=i1;
END//

CREATE PROCEDURE sp_2 (i1 int) SQL SECURITY INVOKER COMMENT 'created with INVOKER'
BEGIN
    set @x=i1;
END//

CREATE FUNCTION fn_1 (i1 longtext, i2 mediumint , i3 longblob, i4 year, i5 real) returns year
BEGIN
   set @x=i1;
   set @y=@x;
   return i4;
END//

CREATE FUNCTION fn_2 (i1 longtext, i2 mediumint , i3 longblob, i4 year, i5 real)
   RETURNS YEAR
   SQL SECURITY INVOKER
   COMMENT 'created with INVOKER'
BEGIN
   set @x=i1;
   set @y=@x;
   return i4;
END//
delimiter ;//

--source suite/funcs_1/storedproc/storedproc_08_show.inc

let $message= ... now change some stuff:;
--source include/show_msg.inc
# some of the changes might be 'strange' here, but they are used here ONLY to
# check that the changes are recorded / showd correct
ALTER PROCEDURE sp_1 SQL SECURITY INVOKER;
ALTER PROCEDURE sp_1 COMMENT 'new comment, SP changed to INVOKER';

ALTER PROCEDURE sp_2 SQL SECURITY DEFINER;
ALTER PROCEDURE sp_2 DROP COMMENT;
ALTER PROCEDURE sp_2 COMMENT 'SP changed to DEFINER';
ALTER PROCEDURE sp_2 READS SQL DATA;

ALTER FUNCTION  fn_1 SQL SECURITY INVOKER;
ALTER FUNCTION  fn_1 COMMENT 'new comment, FN changed to INVOKER';
ALTER FUNCTION  fn_1 NO SQL;

ALTER FUNCTION  fn_2 SQL SECURITY DEFINER;
ALTER FUNCTION  fn_2 COMMENT 'FN changed to DEFINER';
ALTER FUNCTION  fn_2 MODIFIES SQL DATA;

--source suite/funcs_1/storedproc/storedproc_08_show.inc


let $message= ... change back to default and check result:;
--source include/show_msg.inc
ALTER FUNCTION  fn_2 CONTAINS SQL;
--source suite/funcs_1/storedproc/storedproc_08_show.inc


# cleanup 3.1.8.9
let $message= ... cleanup;
--source include/show_msg.inc
DROP FUNCTION fn_1;
DROP FUNCTION fn_2;
DROP PROCEDURE sp_1;


# ==============================================================================
# USE the same .inc to cleanup before and after the test
--source suite/funcs_1/storedproc/cleanup_sp_tb.inc

# ==============================================================================
let $message= .                               +++ END OF SCRIPT +++;
--source include/show_msg80.inc
# ==============================================================================
