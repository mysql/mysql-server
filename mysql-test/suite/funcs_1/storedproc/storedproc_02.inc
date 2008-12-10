#### suite/funcs_1/storedproc/storedproc_02.inc
#
--source suite/funcs_1/storedproc/load_sp_tb.inc

# ==============================================================================
# (numbering from requirement document TP v1.0, Last updated: 25 Jan 2005 01:00)
#
# 3.1.2 Syntax checks for the stored procedure-specific programming statements
#       BEGIN/END, DECLARE, SET, SELECT/INTO, OPEN, FETCH, CLOSE:
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
#-  7. Ensure that every BEGIN statement is coupled with a terminating
#      END statement.
##  8. Ensure that the scope of each BEGIN/END compound statement within a
#      stored procedure definition is properly applied.
#-  9. Ensure that the labels enclosing each BEGIN/END compound statement
#      must match.
#- 10. Ensure that it is possible to put a beginning label at the start of
#      a BEGIN/END compound statement without also requiring an ending label
#      at the end of the same statement.
#- 11. Ensure that it is not possible to put an ending label at the end of
#      a BEGIN/END compound statement without also requiring a matching
#      beginning label at the start of the same statement.
#- 12. Ensure that every beginning label must end with a colon (:).
#- 13. Ensure that every beginning label with the same scope must be unique.
#- 14. Ensure that the variables, cursors, conditions, and handlers declared
#      for a stored procedure (with the DECLARE statement) may only be
#      properly defined.
#- 15. Ensure that the variables, cursors, conditions, and handlers declared for
#      a stored procedure (with the DECLARE statement) may only be defined in
#      the correct order.
#- 16. Ensure that every possible type of variable -- utilizing every data type
#      definition supported by the MySQL server in combination with both no
#      DEFAULT subclause and with DEFAULT subclauses that set the variables
#      default value to a range of appropriate values -- may be declared for
#      a stored procedure.
#- 17. Ensure that the DECLARE statement can declare multiple variables both
#      separately and all at once from a variable list.
#- 18. Ensure that invalid variable declarations are rejected, with an
#      appropriate error message.
#- 19. Ensure that every possible type of cursor may be declared for a
#      stored procedure.
#- 20. Ensure that invalid cursor declarations are rejected, with an appropriate
#      error message.
#- 21. Ensure that every possible type of condition may be declared for
#      a stored procedure.
# -22. Ensure that invalid condition declarations are rejected, with an
#      appropriate error message.
#- 23. Ensure that every possible type of handler may be declared for a
#      stored procedure.
#- 24. Ensure that invalid handler declarations are rejected, with an
#      appropriate error message.
#- 25. Ensure that the scope of every variable, cursor, condition, and handler
#      declared for a stored procedure (with the DECLARE statement) is
#      properly applied.
## 26. Ensure that the initial value of every variable declared for a stored
#      procedure is either NULL or its DEFAULT value, as appropriate.
#- 27. Ensure that the SET statement can assign a value to every local variable
#      declared within a stored procedures definition, as well as to every
#      appropriate global server variable.
#- 28. Ensure that the SET statement can assign values to variables either
#      separately or to multiple variables in a list.
#- 29. Ensure that the SET statement may assign only those values to a variable
#      that are appropriate for that variables data type definition.
## 30. Ensure that, when a stored procedure is called/executed, every variable
#      always uses the correct value: either the value with which it is
#      initialized or the value to which it is subsequently SET or otherwise
#      assigned, as appropriate.
## 31. Ensure that the SELECT ... INTO statement properly assigns values to the
#      variables in its variable list.
## 32. Ensure that a SELECT ... INTO statement that retrieves multiple rows is
#      rejected, with an appropriate error message.
## 33. Ensure that a SELECT ... INTO statement that retrieves too many columns
#      for the number of variables in its variable list is rejected, with an
#      appropriate error message.
## 34. Ensure that a SELECT ... INTO statement that retrieves too few columns
#      for the number of variables in its variable list is rejected, with an
#      appropriate error message.
#- 35. Ensure that a SELECT ... INTO statement that retrieves column values
#      with inappropriate data types for the matching variables in its variable
#      list is rejected, with an appropriate error message.
#- 36. Ensure that the DECLARE ... CONDITION FOR statement can declare a
#      properly-named condition for every possible SQLSTATE and MySQL-specific
#      error code.
#- 37. Ensure that no two conditions declared with the same scope may have the
#      same condition name.
## 38. Ensure that the scope of every condition declared is properly applied.
#- 39. Ensure that every SQLSTATE value declared with a DECLARE ... CONDITION
#      FOR statement is a character string that is 5 characters long.
#- 40. Ensure that the DECLARE ... CONDITION FOR statement cannot declare a
#      condition for an invalid SQLSTATE.
#- 41. Ensure that the DECLARE ... CONDITION FOR statement cannot declare a
#      condition for the successful completion SQLSTATE: 00000.
#- 42. Ensure that the DECLARE ... HANDLER FOR statement can declare a CONTINUE,
#      EXIT, and UNDO handler for every condition declared (with a DECLARE ...
#      CONDITION FOR statement), within the scope of the handler, for a stored
#      procedure, as well as for every possible SQLSTATE and MySQL-specific
#      error code, as well as for the predefined conditions SQLWARNING,
#      NOT FOUND, and SQLEXCEPTION.
## 43. Ensure that the DECLARE ... HANDLER FOR statement can not declare any
#      handler for a condition declared outside of the scope of the handler.
## 44. Ensure that the DECLARE ... HANDLER FOR statement cannot declare a
#      handler for any invalid, or undeclared, condition.
## 45. Ensure that the scope of every handler declared is properly applied.
#- 46. Ensure that, within the same scope, no two handlers may be declared for
#      the same condition.
#- 47. Ensure that every SQLSTATE value declared with a DECLARE ... HANDLER FOR
#      statement is a character string that is 5 characters long.
#- 48. Ensure that the DECLARE ... HANDLER FOR statement cannot declare a
#      condition for an invalid SQLSTATE.
#- 49. Ensure that the DECLARE ... HANDLER FOR statement cannot declare a
#      condition for the successful completion SQLSTATE: 00000.
## 50. Ensure that a CONTINUE handler allows the execution of the stored
#      procedure to continue once the handler statement has completed its
#      own execution (that is, once the handler action statement has been
#      executed).
## 51. Ensure that an EXIT handler causes the execution of the stored procedure
#      to terminate, within its scope, once the handler action statement has
#      been executed.
## 52. Ensure that an EXIT handler does not cause the execution of the stored
#      procedure to terminate outside of its scope.
#- 53. Ensure that a handler condition of SQLWARNING takes the same action as
#      a handler condition defined with an SQLSTATE that begins with 01.
## 54. Ensure that a handler with a condition defined with an SQLSTATE that
#      begins with 01 is always exactly equivalent in action to a
#      handler with an SQLWARNING condition.
#- 55. Ensure that a handler condition of NOT FOUND takes the same action as a
#      handler condition defined with an SQLSTATE that begins with 02.
## 56. Ensure that a handler with a condition defined with an SQLSTATE that
#      begins with 02 is always exactly equivalent in action to a
#      handler with a NOT FOUND condition.
#- 57. Ensure that a handler condition of SQLEXCEPTION takes the same action
#      as a handler condition defined with an SQLSTATE that begins with
#      anything other that 01 or 02.
## 58. Ensure that a handler with a condition defined with an SQLSTATE that
#      begins with anything other that 01 or 02 is always
#      exactly equivalent in action to a handler with an SQLEXCEPTION condition.
#- 59. Ensure that no two cursors in a stored procedure can have the same name.
#- 60. Ensure that a cursor declaration may not include a SELECT ... INTO
#      statement.
#- 61. Ensure that a cursor declaration that includes an ORDER BY clause may
#      not be an updatable cursor.
#- 62. Ensure that OPEN <cursor name> fails unless a cursor with the same name
#      has already been declared.
#- 63. Ensure that OPEN <cursor name> fails if the same cursor is currently
#      already open.
#- 64. Ensure that FETCH <cursor name> fails unless a cursor with the same name
#      is already open.
## 65. Ensure that FETCH <cursor name> returns the first row of the cursors
#      result set the first time FETCH is executed, that it returns each
#      subsequent row of the cursors result set each of the subsequent
#      times FETCH is executed, and that it returns a NOT FOUND warning if it
#      is executed after the last row of the cursors result set has already
#      been fetched.
#- 66. Ensure that FETCH <cursor name> fails with an appropriate error message
#      if it is executed before the cursor has been opened.
#- 67. Ensure that FETCH <cursor name> fails with an appropriate error message
#      if it is executed after the cursor has been closed.
## 68. Ensure that FETCH <cursor name> fails with an appropriate error message
#      if the number of columns to be fetched does not match the number of
#      variables specified by the FETCH statement.
#- 69. Ensure that FETCH <cursor name> fails with an appropriate error message
#      if the data type of the column values being fetched are not appropriate
#      for the matching FETCH variables to which the data is being assigned.
#- 70. Ensure that CLOSE <cursor name> fails unless a cursor with the same name
#      is already open.
#- 71. Ensure that all cursors are closed when a transaction terminates with
#      a COMMIT statement.
#- 72. Ensure that all cursors are closed when a transaction terminates with
#      a ROLLBACK statement.
#- 73. Ensure that the result set of a cursor that has been closed is not
#      longer available to the FETCH statement.
#- 74. Ensure that every cursor declared within a compound statement is closed
#      when that compound statement ends.
## 75. Ensure that, for nested compound statements, a cursor that was declared
#      and opened during an outer level of the statement is not closed when an
#      inner level of a compound statement ends.
## 76. Ensure that all cursors operate asensitively, so that there is no
#      concurrency conflict between cursors operating on the same, or similar,
#      sets of results during execution of one or more stored procedures.
# 77.  Ensure that multiple cursors, nested within multiple compound statements
#      within a stored procedure, always act correctly and return the
#      expected result.
#
# ==============================================================================
let $message= Section 3.1.2 - Syntax checks for the stored procedure-specific
programming statements BEGIN/END, DECLARE, SET, SELECT/INTO, OPEN, FETCH, CLOSE:;
--source include/show_msg80.inc


# ------------------------------------------------------------------------------
let $message= Testcase 3.1.2.8:;
--source include/show_msg.inc
let $message=
Ensure that the scope of each BEGIN/END compound statement within a stored
procedure definition is properly applied;
--source include/show_msg80.inc

--disable_warnings
DROP PROCEDURE IF EXISTS sp1;
--enable_warnings

delimiter //;
CREATE PROCEDURE sp1( )
begin_label: BEGIN
   declare x char DEFAULT 'x';
   declare y char DEFAULT 'y';
   set x = '1';
   set y = '2';
   label1: BEGIN
      declare x char DEFAULT 'X';
      declare y char DEFAULT 'Y';
      SELECT f1, f2 into x, y from t2 limit 1;
      SELECT '1.1', x, y;
      label2: BEGIN
         declare x char default 'a';
         declare y char default 'b';
         label3: BEGIN
            declare x char default 'c';
            declare y char default 'd';
            label4: BEGIN
               declare x char default 'e';
               declare y char default 'f';
               label5: BEGIN
                  declare x char default 'g';
                  declare y char default 'h';
                  SELECT 5, x, y;
               END label5;
               SELECT 4, x, y;
            END label4;
            SELECT 3, x, y;
         END label3;
         SELECT 2, x, y;
      END label2;
   END label1;
   set @v1 = x;
   set @v2 = y;
   SELECT '1.2', @v1, @v2;
END begin_label//
delimiter ;//

CALL sp1();

#cleanup
DROP PROCEDURE IF EXISTS sp1;


# ------------------------------------------------------------------------------
let $message= Testcase 3.1.2.26:;
--source include/show_msg.inc
let $message=
Ensure that the initial value of every variable declared for a stored procedure
is either NULL or its DEFAULT value, as appropriate.;
--source include/show_msg80.inc

--disable_warnings
DROP PROCEDURE IF EXISTS sp1;
--enable_warnings

set @v1=0;
set @v2=0;

delimiter //;
CREATE PROCEDURE sp1( )
BEGIN
   declare x1 char default 'x';
   declare y1 char;
   declare x2 tinytext default 'tinytext';
   declare y2 tinytext;
   declare x3 datetime default '2005-10-03 12:13:14';
   declare y3 datetime;
   declare x4 float default 1.2;
   declare y4 float;
   declare x5 blob default 'b';
   declare y5 blob;
   declare x6 smallint default 127;
   declare y6 smallint;
   SELECT x1, x2, x3, x4, x5, x6, y1, y2, y3, y4, y5, y6;
END//
delimiter ;//

CALL sp1();

# cleanup
DROP PROCEDURE sp1;


# ------------------------------------------------------------------------------
let $message= Testcase 3.1.2.30:;
--source include/show_msg.inc
let $message=
Ensure that, when a stored procedure is called/executed, every variable always
uses the correct value: either the value with which it is initialized or the
value to which it is subsequently SET or otherwise assigned, as appropriate.;
--source include/show_msg80.inc

--disable_warnings
DROP PROCEDURE IF EXISTS sp1;
--enable_warnings

delimiter //;
CREATE PROCEDURE sp1( IN invar INT, OUT outvar INT )
BEGIN
    declare x integer;
    declare y integer default 1;
    set @x = x;
    set @y = y;
    set @z = 234;
    SELECT f1, f2 into @x, @y from t2 where f1='a`' and f2='a`' limit 1;
    SELECT @x, @y, @z, invar;
    BEGIN
      set @x = 2;
      SELECT @x, @y, @z;
      SET outvar = @x * invar + @z * @f;
      SET invar = outvar;
      BEGIN
        set @y = null, @z = 'abcd';
        SELECT @x, @y, @z;
      END;
    END;
END//
delimiter ;//

SET @invar  = 100;
SET @outvar = @invar;
SET @f      = 10;

SELECT @x, @y, @z, @invar, @outvar;

CALL sp1( @invar, @outvar );

SELECT @x, @y, @z, @invar, @outvar;

# cleanup
DROP PROCEDURE sp1;


# ------------------------------------------------------------------------------
let $message= Testcase 3.1.2.31:;
--source include/show_msg.inc
let $message=
Ensure that the SELECT ... INTO statement properly assigns values to the
variables in its variable list.;
--source include/show_msg80.inc
# also tested in a lot of other testcases

--disable_warnings
DROP PROCEDURE IF EXISTS sp1;
--enable_warnings

delimiter //;
CREATE PROCEDURE sp1( )
BEGIN
   declare x integer; declare y integer;
   set @x=x;
   set @y=y;
   SELECT f4, f3 into @x, @y from t2 where f4=-5000 and f3='1000-01-01' limit 1;
   SELECT @x, @y;
END//
delimiter ;//

CALL sp1();

# cleanup 3.1.2.31
DROP PROCEDURE sp1;


# ------------------------------------------------------------------------------
let $message= Testcase 3.1.2.32:;
--source include/show_msg.inc
let $message=
Ensure that a SELECT ... INTO statement that retrieves multiple rows is
rejected, with an appropriate error message.;
--source include/show_msg80.inc

--disable_warnings
DROP PROCEDURE IF EXISTS sp1;
--enable_warnings

delimiter //;
CREATE PROCEDURE sp1( )
BEGIN
   declare x integer; declare y integer;
   set @x=x;
   set @y=y;
   SELECT f4, f3 into @x, @y from t2;
END//
delimiter ;//

# Error: SQLSTATE: 42000 (ER_TOO_MANY_ROWS)
#        Message: Result consisted of more than one row
--error ER_TOO_MANY_ROWS
CALL sp1();

# cleanup 3.1.2.32
DROP PROCEDURE sp1;


# ------------------------------------------------------------------------------
let $message= Testcase 3.1.2.33:;
--source include/show_msg.inc
let $message=
Ensure that a SELECT ... INTO statement that retrieves too many columns for the
number of variables in its variable list is rejected, with an appropriate error
message.;
--source include/show_msg80.inc

--disable_warnings
DROP PROCEDURE IF EXISTS sp1;
--enable_warnings

delimiter //;
CREATE PROCEDURE sp1( )
BEGIN
    declare x integer; declare y integer;
    set @x=x;
    set @y=y;
    SELECT f4, f3, f2, f1 into @x, @y from t2;
END//
delimiter ;//

--error ER_WRONG_NUMBER_OF_COLUMNS_IN_SELECT
CALL sp1();

# cleanup 3.1.2.33
DROP PROCEDURE sp1;


# ------------------------------------------------------------------------------
let $message= Testcase 3.1.2.34:;
--source include/show_msg.inc
let $message=
Ensure that a SELECT ... INTO statement that retrieves too few columns for the
number of variables in its variable list is rejected, with an appropriate error
message.;
--source include/show_msg80.inc

--disable_warnings
DROP PROCEDURE IF EXISTS sp1;
--enable_warnings

delimiter //;
CREATE PROCEDURE sp1( )
BEGIN
    declare x integer; declare y integer; declare z integer;
    set @x=x;
    set @y=y;
    set @z=z;
    SELECT f4 into @x, @y, @z from t2;
END//
delimiter ;//

--error ER_WRONG_NUMBER_OF_COLUMNS_IN_SELECT
CALL sp1();

# cleanup 3.1.2.34
DROP PROCEDURE sp1;


# ------------------------------------------------------------------------------
let $message= Testcase 3.1.2.38:;
--source include/show_msg.inc
let $message=
Ensure that the scope of every condition declared is properly applied.;
--source include/show_msg80.inc

--disable_warnings
DROP PROCEDURE IF EXISTS h1;
DROP TABLE IF EXISTS res_t1;
--enable_warnings

create table res_t1(w char unique, x char);

insert into res_t1 values('a', 'b');

# Error: SQLSTATE: 20000 (ER_SP_CASE_NOT_FOUND)
#        Message: Case not found for CASE statement
# Error: SQLSTATE: 23000 (ER_DUP_KEY)
#        Message: Can't write; duplicate key in table '%s'

delimiter //;
CREATE PROCEDURE h1 ()
BEGIN
   declare x1, x2, x3, x4, x5, x6 int default 0;
   SELECT '-1-', x1, x2, x3, x4, x5, x6;
   BEGIN
      declare condname condition for sqlstate '23000';
      declare continue handler for condname set x5 = 1;
      set x6 = 0;
      insert into res_t1 values ('a', 'b');
      set x6 = 1;
      SELECT '-2-', x1, x2, x3, x4, x5, x6;
   END;
   begin1_label: BEGIN
      BEGIN
         declare condname condition for sqlstate '20000';
         declare continue handler for condname set x1 = 1;
         set x2 = 0;
         case x2
            when 1 then set x2=10;
            when 2 then set x2=11;
         END case;
         set x2 = 1;
         SELECT '-3-', x1, x2, x3, x4, x5, x6;
         begin2_label: BEGIN
            BEGIN
               declare condname condition for sqlstate '23000';
               declare exit handler for condname set x3 = 1;
               set x4= 1;
               SELECT '-4a', x1, x2, x3, x4, x5, x6;
               insert into res_t1 values ('a', 'b');
               set x4= 2;
               SELECT '-4b', x1, x2, x3, x4, x5, x6;
            END;
            SELECT '-5-', x1, x2, x3, x4, x5, x6;
         END begin2_label;
         SELECT '-6-', x1, x2, x3, x4, x5, x6;
      END;
      SELECT '-7-', x1, x2, x3, x4, x5, x6;
   END begin1_label;
   SELECT 'END', x1, x2, x3, x4, x5, x6;
END//
delimiter ;//

CALL h1();

# and a 2nd test
--disable_warnings
DROP TABLE IF EXISTS tnull;
DROP PROCEDURE IF EXISTS sp1;
--enable_warnings

CREATE TABLE tnull(f1 int);

delimiter //;
CREATE PROCEDURE sp1()
BEGIN
    declare cond1 condition for sqlstate '42S02';
    declare continue handler for cond1 set @var2 = 1;
    BEGIN
      declare cond1 condition for sqlstate '23000';
      declare continue handler for cond1 set @var2 = 1;
    END;
          insert into tnull values(1);
END//
delimiter ;//

CALL sp1();

# cleanup 3.1.2.38
DROP PROCEDURE h1;
drop table res_t1;
DROP PROCEDURE sp1;
DROP TABLE tnull;


# ------------------------------------------------------------------------------
let $message= Testcase 3.1.2.43:;
--source include/show_msg.inc
let $message=
Ensure that the DECLARE ... HANDLER FOR statement can not declare any handler
for a condition declared outside of the scope of the handler.;
--source include/show_msg80.inc

--disable_warnings
DROP PROCEDURE IF EXISTS h1;
DROP PROCEDURE IF EXISTS h2;
drop table IF EXISTS res_t1;
--enable_warnings

create table res_t1(w char unique, x char);
insert into res_t1 values ('a', 'b');

delimiter //;
--error ER_SP_COND_MISMATCH
CREATE PROCEDURE h1 ()
BEGIN
   declare x1, x2, x3, x4, x5, x6 int default 0;
   BEGIN
      declare cond_1 condition for sqlstate '23000';
      declare continue handler for cond_1 set x5 = 1;
      BEGIN
         declare cond_2 condition for sqlstate '20000';
         declare continue handler for cond_1 set x1 = 1;
         BEGIN
            declare continue handler for cond_2 set x3 = 1;
            set x2 = 1;
         END;
         set x6 = 0;
      END;
      BEGIN
         declare continue handler for cond_1 set x1 = 1;
         BEGIN
            declare continue handler for cond_2 set x3 = 1;
            set x2 = 1;
         END;
         set x6 = 0;
      END;
   END;
   SELECT x1, x2, x3, x4, x5, x6;
END//

CREATE PROCEDURE h2 ()
BEGIN
   declare x1, x2, x3, x4, x5, x6 int default 0;
   BEGIN
      declare condname condition for sqlstate '23000';
      declare continue handler for condname set x5 = 1;
      BEGIN
         declare condname condition for sqlstate '20000';
         declare continue handler for condname set x1 = 1;
         BEGIN
            declare condname condition for sqlstate '42000';
            declare continue handler for condname set x3 = 1;
            set x6 = 0;
            insert into res_t1 values ('a', 'b');
            set x6 = 1;
            set x4= 0;
            CALL sp1();
            set x4= 1;
            set x2 = 0;
            case x2
               when 1 then set x2=10;
               when 2 then set x2=11;
            END case;
            set x2 = 1;
         END;
         set x2 = 0;
         case x2
            when 1 then set x2=10;
            when 2 then set x2=11;
         END case;
         set x2 = 1;
         set x6 = 0;
         insert into res_t1 values ('a', 'b');
         set x6 = 1;
      END;
   END;
   SELECT x1, x2, x3, x4, x5, x6;
END//
delimiter ;//

CALL h2();
SELECT * FROM res_t1;

# cleanup 3.1.2.43
DROP PROCEDURE h2;
drop table res_t1;


# ------------------------------------------------------------------------------
let $message= Testcase 3.1.2.44:;
--source include/show_msg.inc
let $message=
Ensure that the DECLARE ... HANDLER FOR statement cannot declare a handler for
any invalid, or undeclared, condition.;
--source include/show_msg80.inc

--disable_warnings
DROP PROCEDURE IF EXISTS h1;
--enable_warnings

delimiter //;
# Error: SQLSTATE: 42000 (ER_SP_COND_MISMATCH)
#        Message: Undefined CONDITION: %s
--error ER_SP_COND_MISMATCH
CREATE PROCEDURE h1 ()
BEGIN
   declare x1, x2, x3, x4, x5, x6 int default 0;
   BEGIN
      declare condname1 condition for sqlstate '23000';
      BEGIN
         declare condname2 condition for sqlstate '20000';
         declare continue handler for condname1 set x3 = 1;
         declare continue handler for condname2 set x1 = 1;
      END;
   END;
   BEGIN
      declare condname3 condition for sqlstate '42000';
      declare continue handler for condname1 set x3 = 1;
      declare continue handler for condname2 set x5 = 1;
      declare continue handler for condname3 set x1 = 1;
   END;
END//

# Error: SQLSTATE: 42000 (ER_PARSE_ERROR)
#        Message: %s near '%s' at line %d
--error ER_PARSE_ERROR
CREATE PROCEDURE h1 ()
BEGIN
   DECLARE x1 INT DEFAULT 0;
   BEGIN
      DECLARE condname1 CONDITION CHECK SQLSTATE '23000';
   END;
   DECLARE CONTINUE HANDLER FOR condname1 SET x1 = 1;
END//

# Error: SQLSTATE: 42000 (ER_SP_BAD_SQLSTATE)
#        Message: Bad SQLSTATE: '%s'
--error ER_SP_BAD_SQLSTATE
CREATE PROCEDURE h1 ()
BEGIN
   DECLARE x1 INT DEFAULT 0;
   BEGIN
      DECLARE condname1 CONDITION FOR SQLSTATE 'qwert';
   END;
   DECLARE CONTINUE HANDLER FOR condname1 SET x1 = 1;
END//
delimiter ;//

# cleanup 3.1.2.44
#DROP PROCEDURE h1;


# ------------------------------------------------------------------------------
let $message= Testcase 3.1.2.45 + 3.1.2.50:;
--source include/show_msg.inc
let $message=
45. Ensure that the scope of every handler declared is properly applied.
50. Ensure that a CONTINUE handler allows the execution of the stored procedure
.   to continue once the handler statement has completed its own execution (that
.   is, once the handler action statement has been executed).;
--source include/show_msg80.inc

# RefMan: For an EXIT handler, execution of the current BEGIN...END compound
#         statement is terminated.

--disable_warnings
DROP PROCEDURE IF EXISTS p1;
DROP PROCEDURE IF EXISTS p1undo;
DROP PROCEDURE IF EXISTS h1;
DROP PROCEDURE IF EXISTS sp1;
drop table IF EXISTS res_t1;
--enable_warnings

--echo ==> 'UNDO' is still not supported.
delimiter //;
--error ER_PARSE_ERROR
create procedure p1undo ()
begin
   declare undo handler for sqlexception select '1';
   select * from tqq;
   SELECT 'end of 1';
end;//

create procedure p1 ()
begin
   declare exit handler for sqlexception select 'exit handler 1';
   begin
      declare exit handler for sqlexception select 'exit handler 2';
      begin
         declare continue handler for sqlexception select 'continue handler 3';
         drop table if exists tqq;
         select * from tqq;
         SELECT 'end of BEGIN/END 3';
      end;
      drop table if exists tqq;
      select * from tqq;
      SELECT 'end of BEGIN/END 2';
   end;
   select * from tqq;
   SELECT 'end of BEGIN/END 1';
end;//

call p1()//
delimiter ;//

create table res_t1(w char unique, x char);
insert into res_t1 values ('a', 'b');

delimiter //;
CREATE PROCEDURE h1 ()
BEGIN
   declare x1, x2, x3, x4, x5, x6 int default 0;
   BEGIN
      declare continue handler for sqlstate '23000' set x5 = 1;
      insert into res_t1 values ('a', 'b');
      set x6 = 1;
   END;
   begin1_label: BEGIN
      BEGIN
         declare continue handler for sqlstate '23000' set x1 = 1;
         insert into res_t1 values ('a', 'b');
         set x2 = 1;
         begin2_label: BEGIN
            BEGIN
               declare exit handler for sqlstate '23000' set x3 = 1;
               set x4= 1;
               insert into res_t1 values ('a', 'b');
               set x4= 0;
            END;
         END begin2_label;
      END;
   END begin1_label;
   SELECT x1, x2, x3, x4, x5, x6;
END//
delimiter ;//

CALL h1();

--echo This will fail, SQLSTATE 00000 is not allowed
--ERROR ER_SP_BAD_SQLSTATE
delimiter //;
CREATE PROCEDURE sp1()
   begin1_label:BEGIN
      declare exit handler for sqlstate '00000' set @var1 = 5;
      set @var2 = 6;
      begin2_label:BEGIN
         declare continue handler for sqlstate '00000' set @var3 = 7;
         set @var4 = 8;
         SELECT @var3, @var4;
      END begin2_label;
      SELECT @var1, @var2;
   END begin1_label//
delimiter ;//

--echo Verify SP wasn't created
--ERROR ER_SP_DOES_NOT_EXIST
CALL sp1();

# cleanup 3.1.2.45+50
DROP PROCEDURE p1;
DROP PROCEDURE h1;
--disable_warnings
DROP PROCEDURE IF EXISTS sp1;
--enable_warnings
DROP TABLE res_t1;


# ------------------------------------------------------------------------------
let $message= Testcase 3.1.2.50:;
--source include/show_msg.inc

# Testcase: Ensure that a continue handler allows the execution of the stored procedure
#            to continue once the handler statement has completed its own execution
#            (that is, once the handler action statement has been executed).


--disable_warnings
DROP PROCEDURE IF EXISTS sp1;
DROP PROCEDURE IF EXISTS sp2;
--enable_warnings

delimiter //;
CREATE PROCEDURE sp1 (x int, y int)
BEGIN
    set @y=0;
END//
delimiter ;//

delimiter //;
CREATE PROCEDURE sp2 ()
BEGIN
   declare continue handler for sqlstate '42000' set @x2 = 1;
   set @x=1;
   SELECT @x2;
   CALL sp1(1);
   set @x=2;
   SELECT @x2, @x;
END//
delimiter ;//

CALL sp2();

# cleanup
DROP PROCEDURE sp1;
DROP PROCEDURE sp2;


# ------------------------------------------------------------------------------
let $message= Testcase 3.2.2.51:;
--source include/show_msg.inc
let $message=
Ensure that an EXIT handler causes the execution of the stored procedure to
terminate, within its scope, once the handler action statement has been
executed.;
--source include/show_msg80.inc
# also tested in 3.1.2.45

--disable_warnings
DROP PROCEDURE IF EXISTS sp1;
DROP PROCEDURE IF EXISTS sp2;
--enable_warnings

delimiter //;
CREATE PROCEDURE sp1 (x int, y int)
BEGIN
    set @x=0;
END//
delimiter ;//

delimiter //;
CREATE PROCEDURE sp2 ()
BEGIN
   declare exit handler for sqlstate '42000' set @x2 = 1;
   set @x2=0;
   set @x=1;
   SELECT '-1-', @x2, @x;
   CALL sp1(1);
   SELECT '-2-', @x2, @x;
   set @x=2;
END//
delimiter ;//

# Error: SQLSTATE: 42000 (ER_SP_WRONG_NO_OF_ARGS)
#        Message: Incorrect number of arguments for %s %s; expected %u, got %u
--error ER_SP_WRONG_NO_OF_ARGS
CALL sp1(1);
CALL sp2();
SELECT '-3-', @x2, @x;

# cleanup 3.1.2.51
DROP PROCEDURE sp1;
DROP PROCEDURE sp2;


# ------------------------------------------------------------------------------
let $message= Testcase 3.1.2.52:;
--source include/show_msg.inc
let $message=
Ensure that an EXIT handler does not cause the execution of the stored procedure
to terminate outside of its scope.;
--source include/show_msg80.inc
# tested also above in

--disable_warnings
DROP PROCEDURE IF EXISTS sp1;
DROP PROCEDURE IF EXISTS sp2;
--enable_warnings

delimiter //;
CREATE PROCEDURE sp1 (x int, y int)
BEGIN
    set @x=0;
END//
delimiter ;//

delimiter //;
CREATE PROCEDURE sp2()
BEGIN
   declare continue handler for sqlstate '42000' set @x2 = 2;
   set @x2 = 1;
   set @x =20;
   SELECT '-1-', @x2, @x;
   BEGIN
      declare exit handler for sqlstate '42000' set @x2 = 11;
      SELECT '-2-', @x2, @x;
      CALL sp1(1);
      SELECT '-3a', @x2, @x;
      set @x=21;
      SELECT '-3b', @x2, @x;
   END;
   set @x=22;
   SELECT '-4-', @x2, @x;
END//
delimiter ;//

CALL sp2();

# cleanup 3.1.2.52
DROP PROCEDURE sp1;
DROP PROCEDURE sp2;


# ------------------------------------------------------------------------------
let $message= Testcase 3.1.2.54:;
--source include/show_msg.inc
let $message=
Ensure that a handler with a condition defined with an SQLSTATE that begins with
01 is always exactly equivalent in action to a handler with an SQLWARNING
condition.;
--source include/show_msg80.inc

--disable_warnings
DROP PROCEDURE IF EXISTS sp0;
DROP PROCEDURE IF EXISTS sp1;
DROP PROCEDURE IF EXISTS sp2;
DROP PROCEDURE IF EXISTS sp3;
DROP PROCEDURE IF EXISTS sp4;
DROP TABLE IF EXISTS temp;
--enable_warnings

CREATE TABLE temp( f1 CHAR, f2 CHAR);

delimiter //;
# 0 - without handler
CREATE PROCEDURE sp0()
BEGIN
   set @done=0;
   set @x=0;
   insert into temp values('xxx', 'yy');
   set @x=1;
END//

# 1st one with SQLSTATE + CONTINUE
CREATE PROCEDURE sp1()
BEGIN
   declare continue handler for sqlstate '01000' set @done = 1;
   set @done=0;
   set @x=0;
   insert into temp values('xxx', 'yy');
   set @x=1;
END//

# 2nd one with SQLWARNING + CONTINUE
CREATE PROCEDURE sp2()
BEGIN
   declare continue handler for sqlwarning set @done = 1;
   set @done=0;
   set @x=0;
   insert into temp values('xxx', 'yy');
   set @x=1;
END//

# 3 with SQLSTATE + EXIT
CREATE PROCEDURE sp3()
BEGIN
   declare exit handler for sqlstate '01000' set @done = 1;
   set @done=0;
   set @x=0;
   insert into temp values('xxx', 'yy');
   set @x=1;
END//

# 4 with SQLWARNING + EXIT
CREATE PROCEDURE sp4()
BEGIN
   declare exit handler for sqlwarning set @done = 1;
   set @done=0;
   set @x=0;
   insert into temp values('xxx', 'yy');
   set @x=1;
END//
delimiter ;//

INSERT INTO temp VALUES('0', NULL);
CALL sp0();
SELECT @done, @x;

INSERT INTO temp VALUES('1', NULL);
CALL sp1();
SELECT @done, @x;

INSERT INTO temp VALUES('2', NULL);
CALL sp2();
SELECT @done, @x;

INSERT INTO temp VALUES('3', NULL);
CALL sp3();
SELECT @done, @x;

INSERT INTO temp VALUES('4', NULL);
CALL sp4();
SELECT @done, @x;

SELECT * FROM temp;

# cleanup 3.1.2.54
DROP PROCEDURE sp1;
DROP PROCEDURE sp2;
DROP PROCEDURE sp3;
DROP PROCEDURE sp4;
DROP TABLE temp;


# ------------------------------------------------------------------------------
let $message= Testcase 3.1.2.56:;
--source include/show_msg.inc
let $message=
Ensure that a handler with a condition defined with an SQLSTATE that begins with
02 is always exactly equivalent in action to a handler with a NOT FOUND
condition.;
--source include/show_msg80.inc

--disable_warnings
DROP PROCEDURE IF EXISTS sp0;
DROP PROCEDURE IF EXISTS sp1;
DROP PROCEDURE IF EXISTS sp2;
DROP PROCEDURE IF EXISTS sp3;
DROP PROCEDURE IF EXISTS sp4;
--enable_warnings

delimiter //;
# 0 - wihtout handler
CREATE PROCEDURE sp0()
BEGIN
   DECLARE f1_value CHAR(20);
   DECLARE cur1 CURSOR FOR SELECT f1 FROM t2 LIMIT 1;
   SET @done = 0;
   SET @x = 0;
   OPEN cur1;
   FETCH cur1 INTO f1_value;
   SET @x = 1;
   FETCH cur1 INTO f1_value;
   SET @x = 2;
   CLOSE cur1;
END//

# 1st one with SQLSTATE + CONTINUE
CREATE PROCEDURE sp1()
BEGIN
   DECLARE f1_value CHAR(20);
   DECLARE cur1 CURSOR FOR SELECT f1 FROM t2 LIMIT 1;
   declare continue handler for sqlstate '02000' set @done = 1;
   SET @done = 0;
   SET @x = 0;
   OPEN cur1;
   FETCH cur1 INTO f1_value;
   SET @x = 1;
   FETCH cur1 INTO f1_value;
   SET @x = 2;
   CLOSE cur1;
END//

# 2nd one with NOT FOUND + CONTINUE
CREATE PROCEDURE sp2()
BEGIN
   DECLARE f1_value CHAR(20);
   DECLARE cur1 CURSOR FOR SELECT f1 FROM t2 LIMIT 1;
   declare continue handler for not found set @done = 1;
   SET @done = 0;
   SET @x = 0;
   OPEN cur1;
   FETCH cur1 INTO f1_value;
   SET @x = 1;
   FETCH cur1 INTO f1_value;
   SET @x = 2;
   CLOSE cur1;
END//

# 3 with SQLSTATE + EXIT
CREATE PROCEDURE sp3()
BEGIN
   DECLARE f1_value CHAR(20);
   DECLARE cur1 CURSOR FOR SELECT f1 FROM t2 LIMIT 1;
   declare exit handler for sqlstate '02000' set @done = 1;
   SET @done = 0;
   SET @x = 0;
   OPEN cur1;
   FETCH cur1 INTO f1_value;
   SET @x = 1;
   FETCH cur1 INTO f1_value;
   SET @x = 2;
   CLOSE cur1;
END//

# 4 with NOT FOUND + EXIT
CREATE PROCEDURE sp4()
BEGIN
   DECLARE f1_value CHAR(20);
   DECLARE cur1 CURSOR FOR SELECT f1 FROM t2 LIMIT 1;
   declare exit handler for not found set @done = 1;
   SET @done = 0;
   SET @x = 0;
   OPEN cur1;
   FETCH cur1 INTO f1_value;
   SET @x = 1;
   FETCH cur1 INTO f1_value;
   SET @x = 2;
   CLOSE cur1;
END//
delimiter ;//

--error ER_SP_FETCH_NO_DATA
CALL sp0();
SELECT @done, @x;

CALL sp1();
SELECT @done, @x;

CALL sp2();
SELECT @done, @x;

CALL sp3();
SELECT @done, @x;

CALL sp4();
SELECT @done, @x;

# cleanup 3.1.2.56
DROP PROCEDURE sp0;
DROP PROCEDURE sp1;
DROP PROCEDURE sp2;
DROP PROCEDURE sp3;
DROP PROCEDURE sp4;


# ------------------------------------------------------------------------------
let $message= Testcase 3.1.2.58:;
--source include/show_msg.inc
let $message=
Ensure that a handler with a condition defined with an SQLSTATE that begins with
anything other that 01 or 02 is always exactly equivalent in action to a
handler with an SQLEXCEPTION condition.;
--source include/show_msg80.inc

# Error: SQLSTATE: 20000 (ER_SP_CASE_NOT_FOUND)
#        Message: Case not found for CASE statement
# Error: SQLSTATE: 21000 (ER_WRONG_NUMBER_OF_COLUMNS_IN_SELECT)
#        Message: The used SELECT statements have a different number of columns
# Error: SQLSTATE: 24000 (ER_SP_CURSOR_NOT_OPEN)
#        Message: Cursor is not open

--disable_warnings
DROP PROCEDURE IF EXISTS sp0;
DROP PROCEDURE IF EXISTS sp1;
DROP PROCEDURE IF EXISTS sp2;
DROP PROCEDURE IF EXISTS sp3;
DROP PROCEDURE IF EXISTS sp4;
--enable_warnings

delimiter //;
# 0 - without handler
CREATE PROCEDURE sp0()
BEGIN
   DECLARE f1_value CHAR(20);
   DECLARE cv INT DEFAULT 0;
   DECLARE cur1 CURSOR FOR SELECT f1 FROM t2 LIMIT 1;
   SET @x = 1;
   CASE cv
      WHEN 2 THEN SET @x = 2;
      WHEN 3 THEN SET @x = 3;
   END case;
   SET @x = 4;
   SELECT f1, f2 FROM t2
   UNION
   SELECT f1, f2,3 FROM t2;
   SET @x = 5;
   FETCH cur1 INTO f1_value;
   SET @x = 6;
END//

# 1 - SQLSTATEs - CONTINUE
CREATE PROCEDURE sp1()
BEGIN
   DECLARE f1_value CHAR(20);
   DECLARE cv INT DEFAULT 0;
   DECLARE cur1 CURSOR FOR SELECT f1 FROM t2 LIMIT 1;
   DECLARE continue HANDLER FOR SQLSTATE '20000' SELECT '20000' AS 'SQLSTATE';
   DECLARE continue HANDLER FOR SQLSTATE '21000' SELECT '21000' AS 'SQLSTATE';
   DECLARE continue HANDLER FOR SQLSTATE '24000' SELECT '24000' AS 'SQLSTATE';
   SET @x = 1;
   CASE cv
      WHEN 2 THEN SET @x = 2;
      WHEN 3 THEN SET @x = 3;
   END case;
   SET @x = 4;
   SELECT f1, f2 FROM t2
   UNION
   SELECT f1, f2,3 FROM t2;
   SET @x = 5;
   FETCH cur1 INTO f1_value;
   SET @x = 6;
END//

# 2 - SQLEXCEPTION matches 2 of 3 conditions - CONTINUE
CREATE PROCEDURE sp2()
BEGIN
   DECLARE f1_value CHAR(20);
   DECLARE cv INT DEFAULT 0;
   DECLARE cur1 CURSOR FOR SELECT f1 FROM t2 LIMIT 1;
   DECLARE continue HANDLER FOR SQLEXCEPTION SELECT 'SQLEXCEPTION' AS 'SQLSTATE';
   DECLARE continue HANDLER FOR SQLSTATE '24000' SELECT '24000' AS 'SQLSTATE';
   SET @x = 1;
   CASE cv
      WHEN 2 THEN SET @x = 2;
      WHEN 3 THEN SET @x = 3;
   END case;
   SET @x = 4;
   SELECT f1, f2 FROM t2
   UNION
   SELECT f1, f2,3 FROM t2;
   SET @x = 5;
   FETCH cur1 INTO f1_value;
   SET @x = 6;
END//

# 3 - SQLSTATEs - EXIT
CREATE PROCEDURE sp3()
BEGIN
   DECLARE f1_value CHAR(20);
   DECLARE cv INT DEFAULT 0;
   DECLARE cur1 CURSOR FOR SELECT f1 FROM t2 LIMIT 1;
   DECLARE EXIT HANDLER FOR SQLSTATE '20000' SELECT '20000' AS 'SQLSTATE';
   DECLARE EXIT HANDLER FOR SQLSTATE '21000' SELECT '21000' AS 'SQLSTATE';
   DECLARE EXIT HANDLER FOR SQLSTATE '24000' SELECT '24000' AS 'SQLSTATE';
   SET @x = 1;
   CASE cv
      WHEN 2 THEN SET @x = 2;
      WHEN 3 THEN SET @x = 3;
   END case;
   SET @x = 4;
   SELECT f1, f2 FROM t2
   UNION
   SELECT f1, f2,3 FROM t2;
   SET @x = 5;
   FETCH cur1 INTO f1_value;
   SET @x = 6;
END//

# 4 - SQLEXCEPTION matches 2 of 3 conditions - EXIT
CREATE PROCEDURE sp4()
BEGIN
   DECLARE f1_value CHAR(20);
   DECLARE cv INT DEFAULT 0;
   DECLARE cur1 CURSOR FOR SELECT f1 FROM t2 LIMIT 1;
   DECLARE EXIT HANDLER FOR SQLEXCEPTION SELECT 'SQLEXCEPTION' AS 'SQLSTATE';
   DECLARE EXIT HANDLER FOR SQLSTATE '24000' SELECT '24000' AS 'SQLSTATE';
   SET @x = 1;
   CASE cv
      WHEN 2 THEN SET @x = 2;
      WHEN 3 THEN SET @x = 3;
   END case;
   SET @x = 4;
   SELECT f1, f2 FROM t2
   UNION
   SELECT f1, f2,3 FROM t2;
   SET @x = 5;
   FETCH cur1 INTO f1_value;
   SET @x = 6;
   CLOSE cur1;
END//
delimiter ;//

CALL sp0();
SELECT '-0-', @x;

CALL sp1();
SELECT '-1-', @x;

CALL sp2();
SELECT '-2-', @x;

CALL sp3();
SELECT '-3-', @x;

CALL sp4();
SELECT '-4-', @x;

# cleanup 3.1.2.58
DROP PROCEDURE sp0;
DROP PROCEDURE sp1;
DROP PROCEDURE sp2;
DROP PROCEDURE sp3;
DROP PROCEDURE sp4;


# ------------------------------------------------------------------------------
let $message= Testcase 3.1.2.65:;
--source include/show_msg.inc
let $message=
Ensure that FETCH <cursor name> returns the first row of the cursor_s result set
the first time FETCH is executed, that it returns each subsequent row of the
cursor_s result set each of the subsequent times FETCH is executed, and that it
returns a NOT FOUND warning if it is executed after the last row of the cursor_s
result set has already been fetched.;
--source include/show_msg80.inc

--disable_warnings
DROP PROCEDURE IF EXISTS sp1;
DROP TABLE IF EXISTS temp;
--enable_warnings

CREATE TABLE temp(
   cnt INT,
   f1 CHAR(20),
   f2 CHAR(20),
   f3 INT,
   f4 CHAR(20),
   f5 INT);

INSERT INTO temp VALUES(0, 'onip', 'abc', 8760, 'xyz', 10);

# NOT used: declare continue handler for sqlstate '02000' set proceed=0;
# --> warning is shown when procedure is executed.
delimiter //;
CREATE PROCEDURE sp1( )
BEGIN
   declare proceed int default 1;
   declare count integer default 1;
   declare f1_value char(20);
   declare f2_value char(20);
   declare f5_value char(20);
   declare f4_value integer;
   declare f6_value integer;
   declare cur1 cursor for SELECT f1, f2, f4, f5, f6 from t2
                where f4 >=-5000 order by f4 limit 3;
   open cur1;
   while proceed do
      SELECT count AS 'loop';
      fetch cur1 into f1_value, f2_value, f4_value, f5_value, f6_value;
      insert into temp values (count, f1_value, f2_value, f4_value, f5_value, f6_value);
      set count = count + 1;
   END while;
END//
delimiter ;//

--error ER_SP_FETCH_NO_DATA
CALL sp1();

SELECT * FROM temp;

# cleanup 3.1.2.65
DROP TABLE temp;
DROP PROCEDURE sp1;


# ------------------------------------------------------------------------------
let $message= Testcase 3.1.2.68:;
--source include/show_msg.inc
let $message=
Ensure that FETCH <cursor name> fails with an appropriate error message if the
number of columns to be fetched does not match the number of variables specified
by the FETCH statement.;
--source include/show_msg80.inc

--disable_warnings
DROP PROCEDURE IF EXISTS sp1;
DROP PROCEDURE IF EXISTS sp2;
--enable_warnings

delimiter //;
--echo --> not enough columns in FETCH statement
CREATE PROCEDURE sp1( )
BEGIN
   declare newf1 char(20);
   declare cur1 cursor for SELECT f1, f2 from t2 limit 10;
   declare continue handler for sqlstate '02000' SELECT 'HANDLER executed.' AS '';
   BEGIN
      open cur1;
      fetch cur1 into newf1;
      SELECT newf1;
      close cur1;
   END;
END//

--echo --> too many columns in FETCH statement
CREATE PROCEDURE sp2( )
BEGIN
   declare newf1 char(20);
   declare newf2 char(20);
   declare cur1 cursor for SELECT f1 from t2 limit 10;
   declare continue handler for sqlstate '02000' SELECT 'HANDLER executed.' AS '';
   BEGIN
      open cur1;
      fetch cur1 into newf1, newf2;
      SELECT newf1, newf2;
      close cur1;
   END;
END//
delimiter ;//

--echo --> not enough columns in FETCH statement
--error ER_SP_WRONG_NO_OF_FETCH_ARGS
CALL sp1();

--echo --> too many columns in FETCH statement
--error ER_SP_WRONG_NO_OF_FETCH_ARGS
CALL sp2();

# cleanup 3.1.2.68
DROP PROCEDURE sp1;
DROP PROCEDURE sp2;


# ------------------------------------------------------------------------------
let $message= Testcase 3.1.2.75:;
--source include/show_msg.inc
let $message=
Ensure that, for nested compound statements, a cursor that was declared and
opened during an outer level of the statement is not closed when an inner level
of a compound statement ends.;
--source include/show_msg80.inc

--disable_warnings
DROP TABLE IF EXISTS temp1;
DROP PROCEDURE IF EXISTS sp1;
--enable_warnings

create table temp1( f0 char(20), f1 char(20), f2 char(20), f3 int, f4 char(20) );

# Error: SQLSTATE: 02000 (ER_SP_FETCH_NO_DATA)
#        Message: No data to FETCH

SELECT f1, f2, f4, f5 from t2 order by f4;

delimiter //;
CREATE PROCEDURE sp1( )
BEGIN
   declare count integer;
   declare from0 char(20);
   declare newf1 char(20);
   declare newf2 char(20);
   declare newf5 char(20);
   declare newf4 integer;
   declare cur1 cursor for SELECT f1, f2, f4, f5 from t2 where f4 >= -5000 order by f4 limit 5;
   declare cur2 cursor for SELECT f1, f2, f4, f5 from t2 where f4 >= -5000 order by f4 limit 5;
   open cur1;
   open cur2;
   BEGIN
      declare continue handler for sqlstate '02000' set count = 1;
      fetch cur1 into newf1, newf2, newf4, newf5;
      SELECT '-1-', count, newf1, newf2, newf4, newf5;
      insert into temp1 values ('cur1_out', newf1, newf2, newf4, newf5);
      set count = 4;
      BEGIN
         while count > 0 do
            fetch cur1 into newf1, newf2, newf4, newf5;
            SELECT '-2-', count, newf1, newf2, newf4, newf5;
            set count = count - 1;
         END while;
         SELECT '-3-', count, newf1, newf2, newf4, newf4;
      END;
      BEGIN
         fetch cur1 into newf1, newf2, newf4, newf5;
         SELECT '-4-', newf1, newf2, newf4, newf5;
         insert into temp1 values ('cur1_in', newf1, newf2, newf4, newf5);
      END;
      fetch cur2 into newf1, newf2, newf4, newf5;
      SELECT '-5-', newf1, newf2, newf4, newf5;
      insert into temp1 values ('cur2', newf1, newf2, newf4, newf5);
      close cur1;
   END;
   fetch cur2 into newf1, newf2, newf4, newf5;
   SELECT '-6-', newf1, newf2, newf4, newf5;
   close cur2;
END//
delimiter ;//

CALL sp1();

SELECT * from temp1;

# cleanup 3.1.2.75
DROP PROCEDURE sp1;
drop table temp1;


# ------------------------------------------------------------------------------
let $message= Testcase 3.1.2.76:;
--source include/show_msg.inc
let $message=
Ensure that all cursors operate asensitively, so that there is no concurrency
conflict between cursors operating on the same, or similar, sets of results
during execution of one or more stored procedures.;
--source include/show_msg80.inc

--disable_warnings
DROP PROCEDURE IF EXISTS sp1;
drop table IF EXISTS temp1;
drop table IF EXISTS temp2;
--enable_warnings

create table temp1( f0 char(10), cnt int, f1 char(20), f2 char(20), f3 date, f4 integer );
create table temp2( f0 char(10), cnt int, f1 char(20), f2 char(20), f3 date, f4 integer );

delimiter //;
CREATE PROCEDURE sp_inner( )
BEGIN
   declare proceed int default 1;
   declare i_count integer default 20;
   declare i_newf1 char(20);
   declare i_newf2 char(20);
   declare i_newf3 date;
   declare i_newf4 integer;
   declare i_newf11 char(20);
   declare i_newf12 char(20);
   declare i_newf13 date;
   declare i_newf14 integer;
   declare cur1 cursor for SELECT f1, f2, f3, f4 from t2
                where f4>=-5000 order by f4 limit 4;
   declare cur2 cursor for SELECT f1, f2, f3, f4 from t2
                where f4>=-5000 order by f4 limit 3;
   declare continue handler for sqlstate '02000' set proceed=0;
   open cur1;
   open cur2;
   set i_count = 10;
   while proceed do
      fetch cur1 into i_newf1, i_newf2, i_newf3, i_newf4;
      IF proceed THEN
         insert into temp1 values ('sp_inner', i_count, i_newf1, i_newf2, i_newf3, i_newf4);
         fetch cur2 into i_newf11, i_newf12, i_newf13, i_newf14;
         IF proceed THEN
            insert into temp2 values ('sp_inner', i_count, i_newf11, i_newf12, i_newf13, i_newf14);
         END IF;
      END IF;
      set i_count = i_count - 1;
   END while;
   close cur1;
   close cur2;
END//

CREATE PROCEDURE sp_outer( )
BEGIN
   DECLARE proceed INT DEFAULT 1;
   DECLARE o_count INTEGER DEFAULT 20;
   DECLARE o_newf1 CHAR(20);
   DECLARE o_newf2 CHAR(20);
   DECLARE o_newf3 DATE;
   DECLARE o_newf4 INTEGER;
   DECLARE o_newf11 CHAR(20);
   DECLARE o_newf12 CHAR(20);
   DECLARE o_newf13 DATE;
   DECLARE o_newf14 INTEGER;
   DECLARE cur1 CURSOR FOR SELECT f1, f2, f3, f4 FROM t2
                WHERE f4>=-5000 ORDER BY f4 LIMIT 5;
   DECLARE cur2 CURSOR FOR SELECT f1, f2, f3, f4 FROM t2
                WHERE f4>=-5000 ORDER BY f4 LIMIT 5;
   DECLARE CONTINUE HANDLER FOR SQLSTATE '02000' SET proceed=0;
   OPEN cur1;
   OPEN cur2;
   SET o_count = 1;
   WHILE proceed DO
      FETCH cur1 INTO o_newf1, o_newf2, o_newf3, o_newf4;
      IF proceed THEN
         INSERT INTO temp1 VALUES ('_sp_out_', o_count, o_newf1, o_newf2, o_newf3, o_newf4);
         CALL sp_inner();
         FETCH cur2 INTO o_newf11, o_newf12, o_newf13, o_newf14;
         IF proceed THEN
            INSERT INTO temp2 VALUES ('_sp_out_', o_count, o_newf11, o_newf12, o_newf13, o_newf14);
         END IF;
      END IF;
      SET o_count = o_count + 1;
   END WHILE;
   CLOSE cur1;
   CLOSE cur2;
END//
delimiter ;//

CALL sp_outer();

SELECT * FROM temp1;
SELECT * FROM temp2;

# cleanup 3.1.2.75
DROP PROCEDURE sp_outer;
DROP PROCEDURE sp_inner;
DROP TABLE temp1;
DROP TABLE temp2;


# ==============================================================================
# USE the same .inc to cleanup before and after the test
--source suite/funcs_1/storedproc/cleanup_sp_tb.inc

# ==============================================================================
--echo
--echo .                               +++ END OF SCRIPT +++
--echo --------------------------------------------------------------------------------
# ==============================================================================
