#### suite/funcs_1/storedproc/storedproc_03.inc
#
--source suite/funcs_1/storedproc/load_sp_tb.inc

# ==============================================================================
# (numbering from requirement document TP v1.0, Last updated: 25 Jan 2005 01:00)
#
# 3.1.3 Syntax checks for the stored procedure-specific flow control statements
#       IF, CASE, LOOP, LEAVE, ITERATE, REPEAT, WHILE:
#
#-  1. Ensure that all subclauses that should be supported are supported.
#-  2. Ensure that all subclauses that should not be supported are disallowed
#      with an appropriate error message.
#-  3. Ensure that all supported subclauses are supported only in the
#      correct order.
#-  4. Ensure that an appropriate error message is returned if a subclause is
#      out-of-order in a stored procedure definition.
#-  5. Ensure that all subclauses that are defined to be mandatory are indeed
#      required to be mandatory by the MySQL server and tools.
#-  6. Ensure that any subclauses that are defined to be optional are indeed
#      treated as optional by the MySQL server and tools.
##  7. Ensure that the IF statement acts correctly for all variants, including
#      cases where statements are nested.
##  8. Ensure that the CASE statement acts correctly for all variants,
#      including cases where statements are nested.
##  9. Ensure that the LOOP statement acts correctly for all variants,
#      including cases where statements are nested.
#- 10. Ensure that the labels enclosing each LOOP statement must match.
#- 11. Ensure that it is possible to put a beginning label at the start of
#      a LOOP statement without also requiring an ending label at the end of
#      the same statement.
#- 12. Ensure that it is not possible to put an ending label at the end of
#      a LOOP statement without also requiring a matching beginning label
#      at the start of the same statement.
#- 13. Ensure that every beginning label must end with a colon (:).
#- 14. Ensure that every beginning label with the same scope must be unique.
## 15. Ensure that the LEAVE statement acts correctly for all variants,
#      including cases where statements are nested.
## 16. Ensure that the ITERATE statement acts correctly for all variants,
#      including cases where statements are nested.
#- 17. Ensure that the ITERATE statement fails, with an appropriate error
#      message, if it appears in any context other than within LOOP, REPEAT,
#      or WHILE statements.
## 18. Ensure that the REPEAT statement acts correctly for all variants,
#      including cases where statements are nested.
#- 19. Ensure that the labels enclosing each REPEAT statement must match.
#- 20. Ensure that it is possible to put a beginning label at the start of
#      a REPEAT statement without also requiring an ending label at the end
#      of the same statement.
#- 21. Ensure that it is not possible to put an ending label at the end of
#      a REPEAT statement without also requiring a matching beginning label
#      at the start of the same statement.
#- 22. Ensure that every beginning label must end with a colon (:).
#- 23. Ensure that every beginning label with the same scope must be unique.
## 24. Ensure that the WHILE statement acts correctly for all variants,
#      including cases where statements are nested.
#- 25. Ensure that the labels enclosing each WHILE statement must match.
#- 26. Ensure that it is possible to put a beginning label at the start of
#      a WHILE statement without also requiring an ending label at the end
#      of the same statement.
#- 27. Ensure that it is not possible to put an ending label at the end of
#      a WHILE statement without also requiring a matching beginning label
#      at the start of the same statement.
#- 28. Ensure that every beginning label must end with a colon (:).
#- 29. Ensure that every beginning label with the same scope must be unique.
## 30. Ensure that multiple cases of all possible combinations of the control
#      flow statements, nested within multiple compound statements within
#      a stored procedure, always act correctly and return the expected result.
#
# ==============================================================================
let $message= Section 3.1.3 - Syntax checks for the stored procedure-specific flow
control statements IF, CASE, LOOP, LEAVE, ITERATE, REPEAT, WHILE:;
--source include/show_msg80.inc

#FIXME # 3.1.3: enhance syntax checks with very complicated checks

# ------------------------------------------------------------------------------
let $message= Testcase 3.1.3.7:;
--source include/show_msg.inc
let $message=
Ensure that the IF statement acts correctly for all variants, including cases
where statements are nested.;
--source include/show_msg80.inc

--disable_warnings
DROP TABLE IF EXISTS res_t3_itisalongname_1381742_itsaverylongname_1381742;
DROP PROCEDURE IF EXISTS sp9;
--enable_warnings

CREATE TABLE res_t3_itisalongname_1381742_itsaverylongname_1381742( f1 CHAR(20), f2 VARCHAR(20), f3 SMALLINT);

delimiter //;
CREATE PROCEDURE sp9( action char(20), subaction char(20) )
BEGIN
   if action = 'action' then
      if subaction = 'subaction' then
         insert into res_t3_itisalongname_1381742_itsaverylongname_1381742 values( 'action', 'subaction' , 1);
      else
         insert into res_t3_itisalongname_1381742_itsaverylongname_1381742 values( 'action', 'none' , 2);
      END if;
   else
      if subaction = 'subaction'
      then
         insert into res_t3_itisalongname_1381742_itsaverylongname_1381742 values( 'none', 'subaction' , 3);
      elseif subaction = 'subaction1'
      then
         BEGIN
            insert into res_t3_itisalongname_1381742_itsaverylongname_1381742 values ('none', 'subaction1', 4);
         END;
      else
         insert into res_t3_itisalongname_1381742_itsaverylongname_1381742 values( 'none', 'none' , 5);
      END if;
   END if;
END//
delimiter ;//

CALL sp9( 'action', 'subaction' );
SELECT * from res_t3_itisalongname_1381742_itsaverylongname_1381742 where f3=1;

CALL sp9( 'temp', 'subaction' );
SELECT * from res_t3_itisalongname_1381742_itsaverylongname_1381742 where f3=3;

CALL sp9( 'temp', 'subaction1' );
SELECT * from res_t3_itisalongname_1381742_itsaverylongname_1381742 where f3=4;

CALL sp9( 'action', 'temp' );
SELECT * from res_t3_itisalongname_1381742_itsaverylongname_1381742 where f3=2;

CALL sp9( 'temp', 'temp' );
SELECT * from res_t3_itisalongname_1381742_itsaverylongname_1381742 where f3=5;

# cleanup 3.1.3.7
DROP PROCEDURE sp9;
DROP TABLE res_t3_itisalongname_1381742_itsaverylongname_1381742;


# ------------------------------------------------------------------------------
let $message= Testcase 3.1.3.8.:;
--source include/show_msg.inc
let $message=
Ensure that the CASE statement acts correctly for all variants, including cases
where statements are nested.;
--source include/show_msg80.inc

--disable_warnings
drop table IF EXISTS res_t3_itisalongname_1381742_itsaverylongname_1381742;
DROP PROCEDURE IF EXISTS sp10;
--enable_warnings

create table res_t3_itisalongname_1381742_itsaverylongname_1381742( f1 char(20), f2 varchar(20), f3 smallint);

delimiter //;
CREATE PROCEDURE sp10( action char(20), subaction char(20) )
BEGIN
   case action
   when 'action' then
      case
      when subaction = 'subaction_1' then
         insert into res_t3_itisalongname_1381742_itsaverylongname_1381742 values( 'action', 'subaction_2' , 1);
      when subaction = 'subaction_2' then
         insert into res_t3_itisalongname_1381742_itsaverylongname_1381742 values( 'action', 'subaction_2' , 2);
      else
         insert into res_t3_itisalongname_1381742_itsaverylongname_1381742 values( 'action', 'none' , 3);
      END case;
    else
      case
      when subaction = 'subaction_1' then
         insert into res_t3_itisalongname_1381742_itsaverylongname_1381742 values( 'none', 'subaction_1' , 4);
      when subaction = 'subaction_2' then
         insert into res_t3_itisalongname_1381742_itsaverylongname_1381742 values( 'none', 'subaction_2' , 5);
      else
         insert into res_t3_itisalongname_1381742_itsaverylongname_1381742 values( 'none', 'none' , 6);
      END case;
   END case;
END//
delimiter ;//

CALL sp10( 'action', 'subaction_1' );
SELECT * from res_t3_itisalongname_1381742_itsaverylongname_1381742;
delete from res_t3_itisalongname_1381742_itsaverylongname_1381742;

CALL sp10( 'action', 'subaction_2' );
SELECT * from res_t3_itisalongname_1381742_itsaverylongname_1381742;
delete from res_t3_itisalongname_1381742_itsaverylongname_1381742;

CALL sp10( 'temp', 'subaction_1' );
SELECT * from res_t3_itisalongname_1381742_itsaverylongname_1381742;
delete from res_t3_itisalongname_1381742_itsaverylongname_1381742;

CALL sp10( 'temp', 'subaction_2' );
SELECT * from res_t3_itisalongname_1381742_itsaverylongname_1381742;
delete from res_t3_itisalongname_1381742_itsaverylongname_1381742;

CALL sp10( 'action', 'temp' );
SELECT * from res_t3_itisalongname_1381742_itsaverylongname_1381742;
delete from res_t3_itisalongname_1381742_itsaverylongname_1381742;

CALL sp10( 'temp', 'temp' );
SELECT * from res_t3_itisalongname_1381742_itsaverylongname_1381742;

# cleanup 3.1.3.8
DROP PROCEDURE sp10;
DROP TABLE res_t3_itisalongname_1381742_itsaverylongname_1381742;


# ------------------------------------------------------------------------------
let $message= Testcase 3.1.3.9 + 3.1.3.15:;
--source include/show_msg.inc
let $message=
09. Ensure that the LOOP statement acts correctly for all variants, including
.   cases where statements are nested.
15. Ensure that the LEAVE statement acts correctly for all variants, including
.   cases where statements are nested.;
--source include/show_msg80.inc

--disable_warnings
DROP TABLE IF EXISTS res_t3_itisalongname_1381742_itsaverylongname_1381742;
DROP PROCEDURE IF EXISTS sp11;
--enable_warnings

CREATE TABLE res_t3_itisalongname_1381742_itsaverylongname_1381742( f1 CHAR(20), f2 VARCHAR(20), f3 SMALLINT);

delimiter //;
CREATE PROCEDURE sp11( )
BEGIN
   declare count1 integer default 1;
   declare count2 integer default 1;
   label1: loop
      if count2 > 3 then leave label1;
      END if;
      set count1 = 1;
      label2: loop
         if count1 > 4 then leave label2;
         END if;
         insert into res_t3_itisalongname_1381742_itsaverylongname_1381742 values( 'xyz' , 'pqr', count1);
         set count1 = count1 + 1;
         iterate label2;
      END loop label2;
      set count2 = count2 + 1;
      iterate label1;
   END loop label1;
END//
delimiter ;//

CALL sp11();
SELECT * from res_t3_itisalongname_1381742_itsaverylongname_1381742;

# cleanup 3.1.3.9
DROP PROCEDURE sp11;
DROP TABLE res_t3_itisalongname_1381742_itsaverylongname_1381742;


# ------------------------------------------------------------------------------
let $message= Testcase 3.1.3.16:;
--source include/show_msg.inc
let $message=
Ensure that the ITERATE statement acts correctly for all variants, including
cases where statements are nested.
(tests for this testcase are also included in other testcases);
--source include/show_msg80.inc

--disable_warnings
DROP PROCEDURE IF EXISTS sp31316;
--enable_warnings

delimiter //;

# wrong label at iterate
# Error: SQLSTATE: 42000 (ER_SP_LILABEL_MISMATCH)
#        Message: %s with no matching label: %s
--error ER_SP_LILABEL_MISMATCH
CREATE PROCEDURE sp31316( )
BEGIN
   declare count1 integer default 1;
   declare count2 integer default 1;
   label1: loop
      if count2 > 3 then leave label1;
      END if;
      set count1 = 1;
      label2: loop
         if count1 > 4 then leave label2;
         END if;
         insert into temp values( count1, count2);
         set count1 = count1 + 1;
         iterate label3;
      END loop label2;
      set count2 = count2 + 1;
      iterate label1;
   END loop label1;
END//
delimiter ;//

# cleanup 3.1.3.16
#DROP PROCEDURE sp31316;


# ------------------------------------------------------------------------------
let $message= Testcase 3.1.3.18:;
--source include/show_msg.inc
let $message=
Ensure that the REPEAT statement acts correctly for all variants, including
cases where statements are nested.;
--source include/show_msg80.inc

--disable_warnings
DROP PROCEDURE IF EXISTS sp17;
DROP TABLE IF EXISTS res_t3_itisalongname_1381742_itsaverylongname_1381742;
--enable_warnings

CREATE TABLE res_t3_itisalongname_1381742_itsaverylongname_1381742( f1 CHAR(20), f2 VARCHAR(20), f3 SMALLINT);

delimiter //;
CREATE PROCEDURE sp17( )
BEGIN
   declare count1 integer default 1;
   declare count2 integer default 1;
   repeat
      set count1 = count1 + 1;
      set count2 = 1;
      label1: repeat
         set count2 = count2 + 1;
         insert into res_t3_itisalongname_1381742_itsaverylongname_1381742 values( 'xyz' , 'pqr', count1);
         until count2 > 3
      END repeat label1;
      until count1 > 3
   END repeat;
END//
delimiter ;//

CALL sp17();
SELECT * from res_t3_itisalongname_1381742_itsaverylongname_1381742;

# cleanup 3.1.3.18
DROP PROCEDURE sp17;
DROP TABLE res_t3_itisalongname_1381742_itsaverylongname_1381742;



# ------------------------------------------------------------------------------
let $message= Testcase 3.1.3.24:;
--source include/show_msg.inc
let $message=
Ensure that the WHILE statement acts correctly for all variants, including cases
where statements are nested.;
--source include/show_msg80.inc

--disable_warnings
drop table IF EXISTS res_t21;
DROP PROCEDURE IF EXISTS sp21;
--enable_warnings

create table res_t21(name text(10), surname blob(20), age_averylongfieldname_averylongname_1234569 smallint);
insert into res_t21 values('ashwin', 'mokadam', 25);

delimiter //;
CREATE PROCEDURE sp21( )
BEGIN
   declare count1 integer default 0;
   declare count2 integer default 0;
   while count1 < 3 do
      BEGIN
         declare ithisissamevariablename int default 100;
         SELECT ithisissamevariablename;
         BEGIN
            declare ithisissamevariablename int default 200;
            SELECT ithisissamevariablename;
         END;
         set count2 = 0;
         label1: while count2 < 3 do
            BEGIN
               declare count1 integer default 7;
               set count2 = count2 + 1;
               insert into res_t21 values( 'xyz' , 'pqr', count2);
               label2: while count1 < 10 do
                  set count1 = count1 + 1;
                  insert into res_t21 values( 'xyz' , 'pqr', count1);
               END while label2;
            END;
         END while label1;
         set count1 = count1 + 1;
      END;
   END while;
END//
delimiter ;//

CALL sp21();
SELECT * from res_t21;

# cleanup 3.1.3.
DROP PROCEDURE sp21;
drop table res_t21;


# ------------------------------------------------------------------------------
let $message= Testcase 3.1.3.30:;
--source include/show_msg.inc
let $message=
Ensure that multiple cases of all possible combinations of the control flow
statements, nested within multiple compound statements within a stored
procedure, always act correctly and return the expected result.;
--source include/show_msg80.inc

--disable_warnings
DROP TABLE IF EXISTS res_tbl;
DROP PROCEDURE IF EXISTS sp31330;
--enable_warnings

create table res_tbl (f1 int, f2 text, f3 blob, f4 date,
      f5 set('one', 'two', 'three', 'four', 'five') default 'one');

delimiter //;
#FIXME: can be enhanced more and more ...
CREATE PROCEDURE sp31330 (path int)
BEGIN
   declare count int default 1;
   declare var1 text;
   declare var2 blob;
   declare var3 date;
   declare var4 set('one', 'two', 'three', 'four', 'five') DEFAULT 'five';
   case
      when path=1 then
         set var3 = '2000-11-09';
         set var1 = 'flowing through case 1';
         label1: loop
            if count > 5 then
               if var4=1000 then
                  set var2 = 'exiting out of case 1 - invalid SET';
               END if;
               if var4='two' then
                  set var2 = 'exiting out of case 1';
               END if;
               insert into res_tbl values (1, var1, var2, var3, (count-2));
               leave label1;
            elseif count = 5 then
               set count= count + 2;
               set var4='two';
               iterate label1;
            else
               set count= count + 1;
            END if;
            set var4='one';
         END loop label1;
      when path=2 then
         set var3 = '1989-11-09';
         set var1 = 'flowing through case 2';
         set @count3=0;
         label2: repeat
            set count=count + 1;
            set @count2=1;
            while @count2 <= 5 do
               set @count2 = @count2 + 1;
            END while;
            SELECT @count2;
            set @count3=@count3 + @count2;
            until count > 5
         END repeat label2;
         set var2 = 'exiting out of case 2';
         set var4 = count-3;
         SELECT @count3;
         insert into res_tbl values (2, var1, var2, var3, var4);
      ELSE BEGIN
         set @error_opt='undefined path specified';
         SELECT @error_opt;
      END;
   END case;
END//
delimiter ;//

# Error: SQLSTATE: 42000 (ER_SP_WRONG_NO_OF_ARGS)
#        Message: Incorrect number of arguments for %s %s; expected %u, got %u
--error ER_SP_WRONG_NO_OF_ARGS
CALL sp31330();

CALL sp31330(1);
SELECT * from res_tbl;

CALL sp31330(2);
SELECT * from res_tbl;

CALL sp31330(4);

# cleanup 3.1.3.30
DROP PROCEDURE sp31330;
drop table res_tbl;



# ==============================================================================
# USE the same .inc to cleanup before and after the test
--source suite/funcs_1/storedproc/cleanup_sp_tb.inc

# ==============================================================================
--echo
--echo .                               +++ END OF SCRIPT +++
--echo --------------------------------------------------------------------------------
# ==============================================================================
