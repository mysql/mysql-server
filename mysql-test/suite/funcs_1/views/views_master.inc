#### suite/funcs_1/views/views_master.test
#
# Last Change:
# 2007-10-05 mleich
#    1. Fix for Bug#31237 Test "ndb_views" fails because of differing order ...
#    2. Cleanup of test
# 2007-11-15 hhunger WL#4084: Review and fix all disabled tests ... 

let $message= ! Attention: The file with the expected results is not
              |            thoroughly checked.
              !            The server return codes are correct, but
              |            most result sets where the table tb2 is
              !            involved are not checked.;
--source include/show_msg80.inc

# As long as
# Bug#32285: mysqltest, --ps-protocol, strange output, float/double/real with zerofill
# is not fixed, we must switch the ps-protocol for some statements off (formerly bug#11589).
# If this bug is fixed, please
# 1. set the following variable to 0
# 2. check, if the test passes
# 3. remove the workarounds 
let $have_bug_32285= 1;
if ($have_bug_32285)
{
   let $message= There are some statements where the ps-protocol is switched off.
                 Bug#32285: mysqltest, --ps-protocol, strange output, float/double/real with zerofill;
   --source include/show_msg80.inc
}

# The sub testcases are nearly independend. That is the reason why
# we do not want to abort after the first error.
--disable_abort_on_error


# 3.3 Views
#     MySQL views are based on a subset of the view requirements described in
#     the following standard SQL document:
#
#     * ISO/IEC 9075-2:2003 Information technology -- Database languages --
#       SQL -- Part 2: Foundation (SQL/Foundation)
#
#     MySQL has also added some vendor-specific enhancements to the standard
#     SQL requirements.

# FIXME (mleich)
#   - Alter all object names so that they follow the v/t/..<number> scheme or
#     apply another method which prevents that customer data might be
#     accidently modified
#   - Remove any reference to the preloaded tables tb1 - tb4, if they could
#     be replaced without loss of value.
#     Example: failing CREATE VIEW statements
#     The goal is to split this script into two, where the first one does
#     not need the possibly huge tables.

# Load records needed within the testcases.
#     We load them here and not within the testcases itself, because the
#     removal of any unneeded testcase during bug analysis should not alter
#     result sets.
#     Testcase 3.3.1.1
insert into test.tb2 (f59,f60) values (76710,226546);
insert into test.tb2 (f59,f60) values(2760,985654);
insert into test.tb2 (f59,f60) values(569300,9114376);
insert into test.tb2 (f59,f60) values(660,876546);
insert into test.tb2 (f59,f60) values(250,87895654);
insert into test.tb2 (f59,f60) values(340,9984376);
insert into test.tb2 (f59,f60) values(3410,996546);
insert into test.tb2 (f59,f60) values(2550,775654);
insert into test.tb2 (f59,f60) values(3330,764376);
insert into test.tb2 (f59,f60) values(441,16546);
insert into test.tb2 (f59,f60) values(24,51654);
insert into test.tb2 (f59,f60) values(323,14376);
#     Testcase 3.3.1.45
insert into test.tb2 (f59,f60) values(34,41);
insert into test.tb2 (f59,f60) values(04,74);
insert into test.tb2 (f59,f60) values(15,87);
insert into test.tb2 (f59,f60) values(22,93);
#     Testcase 3.3.1.46
insert into test.tb2 (f59,f60) values(394,41);
insert into test.tb2 (f59,f60) values(094,74);
insert into test.tb2 (f59,f60) values(195,87);
insert into test.tb2 (f59,f60) values(292,93);
#     Testcase 3.3.1.47
insert into test.tb2 (f59,f60) values(0987,41) ;
insert into test.tb2 (f59,f60) values(7876,74) ;
#     Testcase 3.3.1.52
INSERT INTO tb2 (f59,f61) VALUES(321,765 );
INSERT INTO tb2 (f59,f61) VALUES(9112,8771);
#     Testcase 3.3.1.53
INSERT INTO tb2 (f59,f61) VALUES (500,900 ) ;
INSERT INTO tb2 (f59,f61) VALUES (500,900 ) ;
INSERT INTO tb2 (f59,f61) VALUES (500,900 ) ;
#     Testcase 3.3.1.A1
Insert into tb2 (f59,f60,f61) values (107,105,106) ;
Insert into tb2 (f59,f60,f61) values (109,108,104) ;
#     Testcase 3.3.1.A2
Insert into tb2 (f59,f60,f61) values (207,205,206) ;
Insert into tb2 (f59,f60,f61) values (209,208,204) ;
#     Testcase 3.3.1.A3
Insert into tb2 (f59,f60,f61) values (27,25,26) ;
Insert into tb2 (f59,f60,f61) values (29,28,24) ;
#     Testcase 3.3.1.63
Insert into tb2 (f59,f60,f61) values (17,15,16) ;
Insert into tb2 (f59,f60,f61) values (19,18,14) ;
insert into tb2 (f59,f60,f61) values (107,105,106);
insert into tb2 (f59,f60,f61) values (109,108,104);
#     Testcase 3.3.1.64
INSERT INTO tb2 (f59,f60) VALUES( 299,899 );
INSERT INTO tb2 (f59,f60) VALUES( 242,79  );
INSERT INTO tb2 (f59,f60) VALUES( 424,89  );
if ($have_bug_32285)
{
--disable_ps_protocol
}
SELECT * FROM tb2 ORDER BY f59, f60, f61;
--enable_ps_protocol
#
#
Use test;
#
# End of basic preparations.
#
##############################################################################



#==============================================================================
# 3.3.1 Syntax checks for CREATE VIEW, CREATE OR REPLACE VIEW, ALTER VIEW,
#       and DROP VIEW:
#==============================================================================

let $message= Testcase 3.3.1.1 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.1.1:  Ensure that all clauses that should be supported
#                    are supported.
###############################################################################
--disable_warnings
Drop table if exists t1;
--enable_warnings
Create table t1 (f59 INT, f60 INT) ;
Insert into t1  values (100,4234);
Insert into t1  values (990,6624);
Insert into t1  values (710,765);
Insert into t1  values (300,433334);
Insert into t1  values (800,9788);
Insert into t1  values (500,9866);

#(01)
    --disable_warnings
    Drop view if exists v1 ;
    --enable_warnings
    CREATE VIEW v1 AS select f59,f60,f61
    FROM test.tb2 where f59=250;
    select * FROM v1 order by f60,f61 limit 0,10;

#(02)
    Drop view if exists v1 ;
    CREATE VIEW v1 AS select f59,f60,f61
    FROM test.tb2 limit 100;
    select * FROM v1 order by f59,f60,f61 limit 0,10;

#(03)
    CREATE or REPLACE VIEW v1 AS select f59,f60,f61
    FROM test.tb2;
    select * FROM v1 order by f59,f60,f61 limit 4,3;

#(04)
    CREATE or REPLACE VIEW v1 AS select distinct f59
    FROM test.tb2;
    select * FROM v1 order by f59 limit 4,3;

#(05)
    ALTER VIEW v1 AS select f59
    FROM test.tb2;
    select * FROM v1 order by f59 limit 6,2;

#(06)
    CREATE or REPLACE VIEW v1 AS select f59
    from tb2 order by f59;
    select * FROM v1 order by f59 limit 0,10;

#(07)
    CREATE or REPLACE VIEW v1 AS select f59
    from tb2 order by f59 asc;
    select * FROM v1 limit 0,10;

#(08)
    CREATE or REPLACE VIEW v1 AS select f59
    from tb2 order by f59 desc;
    select * FROM v1 limit 0,10;

#(09)
    CREATE or REPLACE VIEW v1 AS select f59
    from tb2 group by f59;
    select * FROM v1 order by f59 limit 0,10;

#(10)
    CREATE or REPLACE VIEW v1 AS select f59
    from tb2 group by f59 asc;
    select * FROM v1 order by f59 limit 0,10;

#(11)
    CREATE or REPLACE VIEW v1 AS select f59
    from tb2 group by f59 desc;
    select * FROM v1 order by f59 limit 0,10;

#(12)
    CREATE or REPLACE VIEW v1 AS (select f59 from tb2)
    union (select f59 from t1);
    select * FROM v1 order by f59 limit 0,10;

#(13)
    CREATE or REPLACE VIEW v1 AS (select f59 FROM tb2)
    UNION DISTINCT(select f59 FROM t1) ;
    select * FROM v1 order by f59 limit 0,10;

#(14)
    CREATE or REPLACE VIEW v1 AS (select f59 FROM tb2)
    UNION ALL(select f59 FROM t1) ;
    select * FROM v1 order by f59 limit 0,10;

#(15)
if ($have_bug_32285)
{
--disable_ps_protocol
}
--vertical_results
    CREATE or REPLACE VIEW v1 AS select *
    FROM test.tb2 WITH LOCAL CHECK OPTION ;
    select * FROM v1 order by f59,f60,f61,f62,f63,f64 limit 0,50;

    #(16)
CREATE or REPLACE VIEW v1 AS select *
    FROM test.tb2 WITH CASCADED CHECK OPTION ;
    select * FROM v1 order by f59,f60,f61,f62,f63,f64 limit 0,10;
--horizontal_results
--enable_ps_protocol

#(17)
    CREATE OR REPLACE VIEW v1 AS SELECT F59, F60
    FROM test.tb2 WITH CASCADED CHECK OPTION;
    SELECT * FROM v1 order by f59,f60 limit 0,10;


#(18)
    CREATE or REPLACE VIEW v1 AS select f59, f60
    from test.tb2 where f59=3330 ;
    select * FROM v1 order by f60 limit 0,10;

    DROP VIEW  v1 ;
    DROP TABLE t1 ;


let $message= Testcase 3.3.1.2 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.1.2:  Ensure that all clauses that should not be supported are
#                    disallowed with an appropriate error message.
###############################################################################
--disable_warnings
DROP TABLE IF EXISTS t1 ;
DROP VIEW  IF EXISTS v1 ;
DROP VIEW  IF EXISTS v2 ;
--enable_warnings
CREATE TABLE t1 (f1 BIGINT) ;

# User variables and parameters are not supported in VIEWs -> 3.3.1.40

# SELECT INTO is illegal
SET @x=0;
--error ER_VIEW_SELECT_CLAUSE
CREATE or REPLACE VIEW v1 AS Select 1 INTO @x;
Select @x;

# Subquery in the FROM clause is illegal
--error ER_VIEW_SELECT_DERIVED
CREATE or REPLACE VIEW v1 AS Select 1
FROM (SELECT 1 FROM t1) my_table;

# Triggers cannot be associated with VIEWs
CREATE VIEW v1 AS SELECT f1 FROM t1;
# Show that 1. The trigger code basically works and the VIEW is updatable
#           2. The VIEW is updatable
#           3. Insert into view causes that the trigger is executed
CREATE TRIGGER tr1 BEFORE INSERT ON t1 FOR EACH ROW SET @a:=1 ;
SET @a:=0 ;
SELECT @a ;
INSERT INTO v1 VALUES (1) ;
SELECT @a ;
SELECT * FROM t1;
DROP TRIGGER tr1 ;
SET @a:=0 ;
--error ER_WRONG_OBJECT
CREATE TRIGGER tr1 BEFORE INSERT ON v1 FOR EACH ROW SET @a:=1 ;

RENAME TABLE v1 TO v2;
# RENAME VIEW is not available even when we try it via rename table.
--error ER_PARSE_ERROR
RENAME VIEW  v2 TO v1;
#--error ER_WRONG_OBJECT
ALTER TABLE v2 RENAME AS v1;
--error ER_PARSE_ERROR
ALTER VIEW  v1 RENAME AS v2;

# VIEWs cannot contain a PRIMARY KEY or have an Index.
--disable_warnings
DROP TABLE IF EXISTS t1, t2 ;
DROP VIEW  IF EXISTS v1 ;
DROP VIEW  IF EXISTS v2 ;
--enable_warnings
CREATE TABLE t1 ( f1 DATE, f2 BLOB, f3 DOUBLE );
CREATE VIEW  v1 AS SELECT f1, f2, f3 FROM t1;
ALTER  TABLE t1 ADD PRIMARY KEY(f1);
--error ER_WRONG_OBJECT
ALTER  TABLE v1 ADD PRIMARY KEY(f1);
--error ER_PARSE_ERROR
ALTER  VIEW  v1 ADD PRIMARY KEY(f1);
CREATE INDEX t1_idx ON t1(f3);
--error ER_WRONG_OBJECT
CREATE INDEX v1_idx ON v1(f3);
DROP TABLE t1;
DROP VIEW  v1;


let $message= Testcase 3.3.1.3 + 3.1.1.4 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.1.1.3:  Ensure that all supported clauses are supported only in
#                    the correct order.
# Testcase 3.1.1.4:  Ensure that an appropriate error message is returned if
#                    a clause is out-of-order in an SQL statement.
###############################################################################
--disable_warnings
DROP VIEW  IF EXISTS v1 ;
--enable_warnings
# REPLACE after VIEW name
--error ER_PARSE_ERROR
CREATE VIEW v1 or REPLACE AS Select * from tb2 my_table;
# CHECK OPTION before AS SELECT
--error ER_PARSE_ERROR
CREATE VIEW v1 WITH CASCADED CHECK OPTION AS Select *
from tb2 my_table limit 50;
# CHECK OPTION before AS SELECT
--error ER_PARSE_ERROR
CREATE VIEW v1 WITH LOCAL CHECK OPTION AS Select *
from tb2 my_table limit 50;
# CREATE after SELECT
--error ER_PARSE_ERROR
SELECT * FROM tb2 my_table CREATE VIEW As v1;
# AS forgotten
--error ER_PARSE_ERROR
CREATE or REPLACE VIEW v1 Select f59, f60
from test.tb2 my_table where f59 = 250 ;
# positive case
CREATE OR REPLACE VIEW v1 AS SELECT F59, F60
FROM test.tb2 my_table WITH CASCADED CHECK OPTION;
DROP VIEW v1;
# REPLACE OR CREATE instead of CREATE OR REPLACE
--error ER_PARSE_ERROR
REPLACE OR CREATE VIEW v1 AS SELECT F59, F60
FROM test.tb2 my_table WITH CASCADED CHECK OPTION;
# AS after SELECT
--error ER_PARSE_ERROR
CREATE OR REPLACE VIEW v1 SELECT AS F59, F60
FROM test.tb2 my_table WITH CASCADED CHECK OPTION;
--error ER_PARSE_ERROR
CREATE OR REPLACE VIEW v1 AS SELECT F59, F60
FROM test.tb2 my_table CASCADED WITH CHECK OPTION;
# OPTION CHECK instead of CHECK OPTION
--error ER_PARSE_ERROR
CREATE OR REPLACE VIEW v1 AS SELECT F59, F60
FROM test.tb2 my_table WITH CASCADED OPTION CHECK;
# CHECK OPTION before WITH
--error ER_PARSE_ERROR
CREATE OR REPLACE VIEW v1 AS SELECT F59, F60
FROM test.tb2 my_table CHECK OPTION WITH CASCADED;
# CHECK OPTION before AS SELECT
--error ER_PARSE_ERROR
CREATE OR REPLACE VIEW v1 WITH CASCADED CHECK OPTION
AS SELECT F59, F60 FROM test.tb2 my_table;
# VIEW <viewname> after AS SELECT
--error ER_PARSE_ERROR
CREATE OR REPLACE AS SELECT F59, F60
FROM test.tb2 my_table VIEW v1 WITH CASCADED CHECK OPTION;
# VIEW <viewname> after CHECK OPTION
--error ER_PARSE_ERROR
CREATE OR REPLACE AS SELECT F59, F60
FROM test.tb2 my_table WITH CASCADED CHECK OPTION VIEW v1;

# Variants with LOCAL CHECK OPTION
--error ER_PARSE_ERROR
REPLACE OR CREATE VIEW v1 AS SELECT F59, F60
FROM test.tb2 my_table WITH LOCAL CHECK OPTION;
--error ER_PARSE_ERROR
CREATE OR REPLACE VIEW v1 SELECT AS F59, F60
FROM test.tb2 my_table WITH LOCAL CHECK OPTION;
--error ER_PARSE_ERROR
CREATE OR REPLACE VIEW v1 AS SELECT F59, F60
FROM test.tb2 my_table LOCAL WITH CHECK OPTION;
--error ER_PARSE_ERROR
CREATE OR REPLACE VIEW v1 AS SELECT F59, F60
FROM test.tb2 my_table WITH LOCAL OPTION CHECK;
--error ER_PARSE_ERROR
CREATE OR REPLACE VIEW v1 AS SELECT F59, F60
FROM test.tb2 my_table CHECK OPTION WITH LOCAL;
--error ER_PARSE_ERROR
CREATE OR REPLACE VIEW v1 WITH CASCADED CHECK OPTION
AS SELECT F59, F60 FROM test.tb2 my_table;
--error ER_PARSE_ERROR
CREATE OR REPLACE AS SELECT F59, F60
FROM test.tb2 my_table VIEW v1 WITH LOCAL CHECK OPTION;
--error ER_PARSE_ERROR
CREATE OR REPLACE AS SELECT F59, F60
FROM test.tb2 my_table WITH LOCAL CHECK OPTION VIEW v1;

--disable_warnings
Drop table if exists t1 ;
--enable_warnings
CREATE table t1 (f1 int ,f2 int)  ;
INSERT INTO t1 values (235, 22);
INSERT INTO t1 values (554, 11);
# SELECTs of UNION in braces
--error ER_PARSE_ERROR
CREATE or REPLACE view v1 as (Select from f59 tb2)
Union ALL (Select from f1 t1);
# by before order
--error ER_PARSE_ERROR
CREATE or REPLACE view v1 as Select f59, f60
from tb2 by order f59;
# by before group
--error ER_PARSE_ERROR
CREATE or REPLACE view v1 as Select f59, f60
from tb2 by group f59  ;


let $message= Testcase 3.3.1.5 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.1.5:  Ensure that all clauses that are defined to be mandatory
#                    are indeed required to be mandatory by the MySQL server
#                    and tools.
###############################################################################
--disable_warnings
DROP VIEW  IF EXISTS v1 ;
--enable_warnings
--error ER_PARSE_ERROR
CREATE VIEW v1 SELECT * FROM tb2;
--error ER_PARSE_ERROR
CREATE v1 AS SELECT * FROM tb2;
--error ER_PARSE_ERROR
VIEW v1 AS SELECT * FROM tb2;
# positive case
CREATE VIEW v1 AS SELECT 1;
DROP VIEW v1;
--error ER_PARSE_ERROR
       VIEW v1 AS SELECT 1;
--error ER_PARSE_ERROR
CREATE      v1 AS SELECT 1;
--error ER_PARSE_ERROR
CREATE VIEW    AS SELECT 1;
--error ER_PARSE_ERROR
CREATE VIEW v1    SELECT 1;
--error ER_PARSE_ERROR
CREATE VIEW v1 AS         ;


let $message= Testcase 3.3.1.6 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.1.6: Ensure that any clauses that are defined to be optional
#                   are indeed treated as optional by the MySQL server
#                   and tools.
###############################################################################
# Note: The positive test in 3.3.1.5 shows, that ALGORITHM ..., CHECK OPTION
#       and any column_list after the VIEW name are optional.
#       Therefore check here:
#       - ALGORITHM = <all possible algorithms>
#       - all possible CHECK OPTIONs
#       - some incomplete or wrong stuff
--disable_warnings
DROP VIEW  IF EXISTS v1 ;
--enable_warnings
CREATE or REPLACE                       VIEW v1
as SELECT * from tb2;
CREATE or REPLACE ALGORITHM = UNDEFINED VIEW v1
as SELECT * from tb2;
CREATE or REPLACE ALGORITHM = MERGE     VIEW v1
as SELECT * from tb2;
CREATE or REPLACE ALGORITHM = TEMPTABLE VIEW v1
as SELECT * from tb2;
CREATE or REPLACE ALGORITHM = TEMPTABLE VIEW v1
as SELECT * from tb2;
# negative test cases
--error ER_PARSE_ERROR
CREATE or REPLACE           = TEMPTABLE VIEW v1
as SELECT * from tb2;
--error ER_PARSE_ERROR
CREATE or REPLACE ALGORITHM   TEMPTABLE VIEW v1
as SELECT * from tb2;
--error ER_PARSE_ERROR
CREATE or REPLACE ALGORITHM =           VIEW v1
as SELECT * from tb2;
--error ER_PARSE_ERROR
CREATE or REPLACE TEMPTABLE = ALGORITHM VIEW v1
as SELECT * from tb2;
--error ER_PARSE_ERROR
CREATE or REPLACE TEMPTABLE - ALGORITHM VIEW v1
as SELECT * from tb2;
--error ER_PARSE_ERROR
CREATE or REPLACE GARBAGE   = TEMPTABLE VIEW v1
as SELECT * from tb2;
--error ER_PARSE_ERROR
CREATE or REPLACE ALGORITHM = GARBAGE VIEW v1
as SELECT * from tb2;
Drop view if exists v1 ;

CREATE or REPLACE VIEW v1
AS SELECT * from tb2 where f59 < 1;
CREATE or REPLACE VIEW v1
AS SELECT * from tb2 where f59 < 1 WITH          CHECK OPTION;
CREATE or REPLACE VIEW v1
AS SELECT * from tb2 where f59 < 1 WITH CASCADED CHECK OPTION;
CREATE or REPLACE VIEW v1
AS SELECT * from tb2 where f59 < 1 WITH LOCAL    CHECK OPTION;
# negative test cases
--error ER_PARSE_ERROR
CREATE or REPLACE VIEW v1
AS SELECT * from tb2 where f59 < 1 WITH NO       CHECK OPTION;
--error ER_PARSE_ERROR
CREATE or REPLACE VIEW v1
AS SELECT * from tb2 where f59 < 1      CASCADED CHECK OPTION;
--error ER_PARSE_ERROR
CREATE or REPLACE VIEW v1
AS SELECT * from tb2 where f59 < 1 WITH CASCADED       OPTION;
--error ER_PARSE_ERROR
CREATE or REPLACE VIEW v1
AS SELECT * from tb2 where f59 < 1 WITH CASCADED CHECK       ;


let $message= Testcase 3.3.1.7 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.1.7: Ensure that all valid, fully-qualified, and non-qualified,
#                   view names are accepted, at creation time, alteration time,
#                   and drop time.
###############################################################################
# Note(mleich): non-qualified view name means a view name without preceeding
#               database name
--disable_warnings
DROP VIEW  IF EXISTS v1 ;
--enable_warnings
Create view test.v1 AS Select * from test.tb2;
Alter view test.v1 AS Select F59 from test. tb2 limit 100 ;
Drop view test.v1 ;
Create view v1 AS Select * from test.tb2 limit 100 ;
Alter view v1 AS Select F59 from test.tb2 limit 100 ;
Drop view v1 ;


let $message= Testcase 3.3.1.A0 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.1.A0: Ensure that view names are treated case sensitive.
###############################################################################
# Note(mleich): Maybe this test produces portability problems on Windows.
#               FIXME There should be a test outside this one checking the
#                     creation of objects with cases sensitive names.
#                     If we have this test the following sub testcase should
#                     be deleted.
--disable_warnings
DROP TABLE IF EXISTS t1 ;
DROP VIEW  IF EXISTS v1 ;
DROP VIEW  IF EXISTS V1 ;
--enable_warnings
eval CREATE TABLE t1 (f1 NUMERIC(4)) ENGINE = $engine_type;
INSERT INTO t1 VALUES(1111), (2222);
CREATE VIEW v1 AS SELECT * FROM t1 WHERE f1 = 1111;
# We get here the sql code
# - 0    on OS with    cases sensitive view names (Example: UNIX)
# - ER_TABLE_EXISTS_ERROR on OS without cases sensitive view names (Example: WINDOWS)
--error 0,ER_TABLE_EXISTS_ERROR
CREATE VIEW V1 AS SELECT * FROM t1 WHERE f1 = 2222;
SELECT * FROM v1;
# SELECT * FROM V1;
--disable_warnings
DROP TABLE IF EXISTS t1 ;
DROP VIEW  IF EXISTS v1 ;
DROP VIEW  IF EXISTS V1 ;
--enable_warnings


let $message= Testcase 3.3.1.8 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.1.8: Ensure that any invalid view name is never accepted, and
#                   that an appropriate error message is returned when the name
#                   is rejected.
###############################################################################
# Note(mleich): There could be more negative tests here, but I assume that the
#               server routines checking if a table or view name is acceptable
#               are heavily tested in tests checking the creation of tables.
--error ER_PARSE_ERROR
Create view select AS Select * from test.tb2 limit 100;
--error ER_PARSE_ERROR
Create view as AS Select * from test.tb2 limit 100;
--error ER_PARSE_ERROR
Create view where AS Select * from test.tb2 limit 100;
--error ER_PARSE_ERROR
Create view from AS Select * from test.tb2 limit 100;
--error ER_PARSE_ERROR
Create view while AS Select * from test.tb2 limit 100;
--error ER_PARSE_ERROR
Create view asdkj*(&*&&^ as Select * from test.tb2 limit 100 ;
--disable_warnings
Drop view if exists test.procedure ;
--enable_warnings
Create view test.procedure as Select * from test.tb2 limit 100 ;
Drop view if exists test.procedure ;


let $message= Testcase 3.3.1.9 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.1.9: Ensure that a reference to a non-existent view is rejected
#                   with an appropriate error message
###############################################################################
# Note(mleich): The SELECT statement syntax does not contain any functionality
#               to claim, that the object after FROM must be a VIEW. SHOW's will
#               be checked in
#                  3.3.11 Checks on SHOW, EXPLAIN, and DESCRIBE statements.
#               Let's check here a view based on a dropped view or table.
--disable_warnings
Drop TABLE IF EXISTS t1 ;
Drop VIEW  IF EXISTS v1;
Drop VIEW  IF EXISTS v2;
Drop VIEW  IF EXISTS v3;
--enable_warnings
CREATE TABLE t1 ( f1 char(5));
INSERT INTO t1 SET f1 = 'abcde';
CREATE VIEW v1 AS SELECT f1 FROM t1;
CREATE VIEW v2 AS SELECT *  FROM v1;

# Only negative cases, positive cases will be checked later:
DROP TABLE t1;
--error ER_VIEW_INVALID
SELECT * FROM v1;
--error ER_VIEW_INVALID
DELETE FROM v1;
--error ER_VIEW_INVALID
UPDATE v1 SET f1 = 'aaaaa';
--error ER_VIEW_INVALID
INSERT INTO v1 SET f1 = "fffff";
# v2 is based on v1, which is now invalid
--error ER_VIEW_INVALID
SELECT * FROM v2;
--error ER_VIEW_INVALID
DELETE FROM v2;
--error ER_VIEW_INVALID
UPDATE v2 SET f1 = 'aaaaa';
--error ER_VIEW_INVALID
INSERT INTO v2 SET f1 = "fffff";
DROP VIEW v1;
# v2 is based on v1, which is now dropped
--error ER_VIEW_INVALID
SELECT * FROM v2;
--error ER_VIEW_INVALID
DELETE FROM v2;
--error ER_VIEW_INVALID
UPDATE v2 SET f1 = 'aaaaa';
--error ER_VIEW_INVALID
INSERT INTO v2 SET f1 = "fffff";

DROP VIEW v2;

# A VIEW based on itself is non sense.
--disable_warnings
DROP TABLE IF EXISTS t1 ;
DROP VIEW  IF EXISTS v1 ;
--enable_warnings
CREATE TABLE t1 (f1 FLOAT);
# Create a new VIEW based on itself
--error ER_NO_SUCH_TABLE
CREATE VIEW v1 AS SELECT * FROM v1;
# Replace a valid VIEW with one new based on itself
CREATE VIEW  v1 AS SELECT * FROM t1;
--error ER_NO_SUCH_TABLE
CREATE or REPLACE VIEW  v1 AS SELECT * FROM v1;

DROP VIEW  v1;
DROP TABLE t1;

let $message= Testcase 3.3.1.10 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.1.10: Ensure that it is not possible to create two views with
#                    the same name in the same database.
###############################################################################
--disable_warnings
Drop view if exists test.v1 ;
--enable_warnings
Create view test.v1 AS Select * from test.tb2 ;
--error ER_TABLE_EXISTS_ERROR
Create view test.v1 AS Select F59 from test.tb2 ;
--error ER_TABLE_EXISTS_ERROR
Create view      v1 AS Select F59 from test.tb2 ;


let $message= Testcase 3.3.1.11 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.1.11: Ensure that it is not possible to create a view and a base
#                    table with the same name in the same database.
###############################################################################
# The VIEW should get the same name like an already existing TABLE.
--error ER_TABLE_EXISTS_ERROR
Create view test.tb2 AS Select f59,f60 from test.tb2 limit 100 ;
--error ER_TABLE_EXISTS_ERROR
Create view      tb2 AS Select f59,f60 from test.tb2 limit 100 ;
# The TABLE should get the same name like an already existing VIEW.
--disable_warnings
Drop view if exists test.v111 ;
--enable_warnings
Create view test.v111 as select * from tb2 limit 50;
--error ER_TABLE_EXISTS_ERROR
Create table test.v111(f1 int );
--error ER_TABLE_EXISTS_ERROR
Create table      v111(f1 int );
DROP VIEW test.v111;


let $message= Testcase 3.3.1.12 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.1.12: Ensure that it is possible to create two or more views and
#                    base tables with the same name, providing each resides in
#                    a different database.
###############################################################################
USE test;
--disable_warnings
Drop database if exists test2 ;
--enable_warnings
Create database test2 ;
# Plan of sub tests
# Object name  object type in  object type in
#              database test   database test2
# t1           TABLE           TABLE
# t2           TABLE           VIEW
# v1           VIEW            TABLE
# v2           VIEW            VIEW
--disable_warnings
DROP TABLE IF EXISTS test.t0, test.t1, test.t2;
DROP VIEW  IF EXISTS test.v1;
DROP VIEW  IF EXISTS test.v2;
--enable_warnings
CREATE TABLE  test.t1 ( f1 VARCHAR(20));
CREATE TABLE test2.t1 ( f1 VARCHAR(20));
CREATE TABLE  test.t2 ( f1 VARCHAR(20));
CREATE TABLE test2.v1 ( f1 VARCHAR(20));
# t0 is an auxiliary table needed for the VIEWs
CREATE TABLE  test.t0 ( f1 VARCHAR(20));
CREATE TABLE test2.t0 ( f1 VARCHAR(20));

CREATE VIEW  test2.t2 AS SELECT * FROM test2.t0;
CREATE VIEW   test.v1 AS SELECT * FROM  test.t0;
CREATE VIEW   test.v2 AS SELECT * FROM  test.t0;
CREATE VIEW  test2.v2 AS SELECT * FROM test2.t0;

# Some additional tests on the just created objects to show that they are
# accessable and do have the expected content.
# INSERTs with full qualified table
INSERT INTO  test.t1 VALUES('test.t1  - 1');
INSERT INTO test2.t1 VALUES('test2.t1 - 1');
INSERT INTO  test.t2 VALUES('test.t2  - 1');
INSERT INTO test2.v1 VALUES('test2.v1 - 1');
INSERT INTO  test.t0 VALUES('test.t0  - 1');
INSERT INTO test2.t0 VALUES('test2.t0 - 1');
# INSERTs with not full qualified table name.
USE test;
INSERT INTO  t1 VALUES('test.t1  - 2');
INSERT INTO  t2 VALUES('test.t2  - 2');
INSERT INTO  t0 VALUES('test.t0  - 2');
USE test2;
INSERT INTO  t1 VALUES('test2.t1 - 2');
INSERT INTO  v1 VALUES('test2.v1 - 2');
INSERT INTO  t0 VALUES('test2.t0 - 2');
# SELECTs with full qualified table
SELECT * FROM t1;
SELECT * FROM t2;
SELECT * FROM v1;
SELECT * FROM v2;
USE test;
SELECT * FROM t1;
SELECT * FROM t2;
SELECT * FROM v1;
SELECT * FROM v2;


let $message= Testcase 3.3.1.13 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.1.13: Ensure that, if the CREATE OR REPLACE VIEW statement is
#                    used to create a view using the name of an existing view,
#                    it first cleanly drops the existing view and then creates
#                    the new view.
###############################################################################
--disable_warnings
DROP TABLE IF EXISTS t1;
DROP VIEW  IF EXISTS v1;
--enable_warnings
CREATE TABLE t1 (f1 BIGINT);
INSERT INTO t1 VALUES(1);
CREATE VIEW test.v1 AS SELECT * FROM t1 limit 2;
SHOW CREATE VIEW test.v1;
--sorted_result
SELECT * FROM test.v1;
# Switch the algorithm
CREATE OR REPLACE ALGORITHM = TEMPTABLE VIEW test.v1
AS SELECT * FROM t1 limit 2;
SHOW CREATE VIEW test.v1;
--sorted_result
SELECT * FROM test.v1;
# Switch the base table
CREATE OR REPLACE VIEW test.v1 AS SELECT * FROM tb2 order by f59 limit 2;
SHOW CREATE VIEW test.v1;
if ($have_bug_11589)
{
--disable_ps_protocol
}
--vertical_results
SELECT * FROM test.v1 order by f59,f60,f61,f62,f63,f64,f65;
--horizontal_results
--enable_ps_protocol
# Switch the SELECT but not the base table
CREATE OR REPLACE VIEW test.v1 AS SELECT F59 FROM tb2;
SHOW CREATE VIEW test.v1;
SELECT * FROM test.v1 order by F59 limit 10,100;
Drop table test.t1 ;
Drop view  test.v1 ;


let $message= Testcase 3.3.1.14 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.1.14: Ensure that, if the CREATE OR REPLACE VIEW statement is
#                    used to create a view using the name of an existing base
#                    table, it fails with an appropriate error message.
###############################################################################
--error ER_WRONG_OBJECT
CREATE OR REPLACE VIEW test.tb2 AS SELECT * From tb2 LIMIT 2;
--error ER_WRONG_OBJECT
CREATE OR REPLACE VIEW tb2 AS SELECT * From tb2 LIMIT 2;


let $message= Testcase 3.3.1.15 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.1.15: Ensure that, if the CREATE OR REPLACE VIEW statement is
#                    used to create a view using a name that does not already
#                    belong to an existing view or base table, it cleanly
#                    creates the view.
###############################################################################
--disable_warnings
Drop table if exists test.v1 ;
--enable_warnings
CREATE OR REPLACE view test.v1 as select * from tb2;
if ($have_bug_32285)
{
--disable_ps_protocol
}
--sorted_result
SELECT * FROM test.v1;
--enable_ps_protocol
Drop view test.v1 ;


let $message= Testcase 3.3.1.16 + 3.3.1.17 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.1.16: Ensure that a view with a definition that does not include
#                    an explicit column-name list takes its column names from
#                    the underlying base table(s).
# Testcase 3.3.1.17: Ensure that a view with a definition that does include an
#                    explicit column-name list uses the explicit names and not
#                    the name of the columns from the underlying base tables(s)
###############################################################################
--disable_warnings
Drop table if exists test.v1 ;
--enable_warnings
CREATE OR REPLACE VIEW v1 AS SELECT * From tb2;
# Note(mleich): The empty result is intended, because I want to compare
#               column names only.
SELECT * FROM tb2 WHERE 1 = 2;
SELECT * FROM v1  WHERE 1 = 2;
Drop view v1;

--disable_warnings
DROP TABLE IF EXISTS t1;
DROP VIEW  IF EXISTS v1;
--enable_warnings
CREATE TABLE t1 (f1 NUMERIC(15,3));
INSERT INTO t1 VALUES(8.8);
# 1. no explicit column in VIEW definition or SELECT
CREATE VIEW v1 AS SELECT * FROM t1;
SHOW CREATE VIEW v1;
SELECT * FROM v1;
# 2. no explicit column in VIEW definition, but in SELECT column_list
CREATE OR REPLACE VIEW v1 AS SELECT f1 FROM t1;
SHOW CREATE VIEW v1;
SELECT * FROM v1;
# 3. no explicit column in VIEW definition, but alias from SELECT column_list
CREATE OR REPLACE VIEW v1 AS SELECT f1 As my_column FROM t1;
SHOW CREATE VIEW v1;
SELECT * FROM v1;
# 4. Finally the requirement: explicit column_list in VIEW definition
CREATE OR REPLACE VIEW v1(column1,column2)
AS SELECT f1 As my_column, f1 FROM t1;
SHOW CREATE VIEW v1;
SELECT * FROM v1;
CREATE OR REPLACE VIEW test.v1(column1,column2)
AS SELECT f1 As my_column, f1 FROM test.t1;
SHOW CREATE VIEW v1;
SELECT * FROM v1;


let $message= Testcase 3.3.1.18 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.1.18: Ensure that a reference to a view with a definition that
#                    includes an explicit column-name fails, with an appropriate
#                    error message, if the reference includes columns names
#                    from the underlying base table(s) rather than the view
#                    column names.
###############################################################################
# Note(mleich): The goal is to check the merge algorithm.
--disable_warnings
Drop view if exists v1 ;
Drop view if exists v1_1 ;
--enable_warnings
Create view v1
as Select test.tb2.f59 as NewNameF1, test.tb2.f60
from test.tb2 limit 0,100 ;
Create view v1_1
as Select test.tb2.f59 as NewNameF1, test.tb2.f60 as NewNameF2
from tb2 limit 0,100 ;
--error ER_BAD_FIELD_ERROR
SELECT NewNameF1,f60             FROM test.v1_1 ;
--error ER_BAD_FIELD_ERROR
SELECT NewNameF1, v1_1.f60 FROM test.v1_1 ;
--error ER_BAD_FIELD_ERROR
SELECT f59, f60 FROM test.v1 ;
Use test ;
--error ER_BAD_FIELD_ERROR
SELECT F59 FROM v1 ;


let $message= Testcase 3.3.1.19 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.1.19: Ensure that every column of a view must have a
#                    distinct name
###############################################################################
--disable_warnings
DROP TABLE IF EXISTS t1, t2;
DROP VIEW  IF EXISTS v1;
--enable_warnings
CREATE TABLE t1( f1 BIGINT, f2 DECIMAL(5,2));
INSERT INTO t1 VALUES(7, 7.7);
CREATE TABLE t2( f1 BIGINT, f2 DECIMAL(5,2));
INSERT INTO t2 VALUES(6, 6.6);
# positive testcases
CREATE VIEW v1 AS SELECT * FROM t1;
SELECT * FROM v1;
CREATE OR REPLACE VIEW v1 AS SELECT f1, f2 FROM t1;
SELECT * FROM v1;
CREATE OR REPLACE VIEW v1 AS SELECT f1 AS my_f1, f2 AS my_f2 FROM t1;
SELECT * FROM v1;
CREATE OR REPLACE VIEW v1 (my_f1, my_f2) AS SELECT f1, f2 FROM t1;
SELECT * FROM v1;
CREATE OR REPLACE VIEW v1 (my_f1, my_f2) AS SELECT t1.f1, t2.f2 FROM t1, t2;
SELECT * FROM v1;
# negative testcases (sometimes including the underlying SELECT)
# duplicate via alias in SELECT
SELECT f1, f2 AS f1 FROM t1;
--error ER_DUP_FIELDNAME
CREATE OR REPLACE VIEW v1 AS SELECT f1, f2 AS f1 FROM t1;
# duplicate via JOIN SELECT
SELECT t1.f1, t2.f1 AS f1 FROM t1, t2;
--error ER_DUP_FIELDNAME
CREATE OR REPLACE VIEW v1 AS SELECT t1.f1, t2.f1 AS f1 FROM t1, t2;
# duplicate via VIEW definition
--error ER_DUP_FIELDNAME
CREATE OR REPLACE VIEW v1 (my_col, my_col) AS SELECT * FROM t1;


let $message= Testcase 3.3.1.20 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.1.20: Ensure that, if a column-name list is provided for a
#                    view definition, the list contains a name for every column
#                    in the view
###############################################################################
--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings
CREATE TABLE t1( f1 BIGINT, f2 DECIMAL(5,2));
# positive case
CREATE OR REPLACE VIEW v1 (my_f1, my_f2) AS SELECT *      FROM t1;
CREATE OR REPLACE VIEW v1 (my_f1, my_f2) AS SELECT f1, f2 FROM t1;
# negative cases, where we assign a wrong number of column names
--error ER_VIEW_WRONG_LIST
CREATE OR REPLACE VIEW v1 (my_f1       ) AS SELECT *      FROM t1;
--error ER_VIEW_WRONG_LIST
CREATE OR REPLACE VIEW v1 (my_f1       ) AS SELECT f1, f2 FROM t1;
--error ER_VIEW_WRONG_LIST
CREATE OR REPLACE VIEW v1 (my_f1, my_f2, my_f3) AS SELECT *      FROM t1;
--error ER_VIEW_WRONG_LIST
CREATE OR REPLACE VIEW v1 (my_f1, my_f2, my_f3) AS SELECT f1, f2 FROM t1;


let $message= Testcase 3.3.1.21 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.1.21:  Ensure that a view column can be a direct copy of a
#                     column from an underlying table.
###############################################################################
--disable_warnings
DROP VIEW  IF EXISTS v1;
--enable_warnings
CREATE VIEW test.v1( F59, F60 ) AS SELECT F59, F60 From tb2;
SELECT * FROM test.v1 order by F59, F60 desc LIMIT 2;
Drop view if exists test.v1 ;


let $message= Testcase 3.3.1.22 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.1.22: Ensure that a view column can be based on any valid
#                    expression, whether or not the expression includes a
#                    reference of the column of an underlying table.
###############################################################################
--disable_warnings
DROP VIEW  IF EXISTS v1;
--enable_warnings
CREATE VIEW test.v1( product ) AS SELECT f59*f60 From tb2 WHERE f59 < 3;
--sorted_result
SELECT * FROM test.v1;
CREATE OR REPLACE VIEW test.v1( product ) AS SELECT 1*2;
--sorted_result
SELECT * FROM test.v1;
CREATE OR REPLACE VIEW test.v1( product ) AS SELECT USER();
--sorted_result
SELECT * FROM test.v1;
Drop view if exists test.v1 ;


let $message= Testcase 3.3.1.23 + 3.3.1.24 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.1.23: Ensure that a view definition that includes a reference to
#                    a non-existent table fails, with an appropriate error
#                    message, at creation time.
# Testcase 3.3.1.24: Ensure that a view definition that includes a reference to
#                    a non-existent view fails, with an appropriate error
#                    message, at creation time.
###############################################################################
# Note(mleich): The SELECT statement syntax does not contain any functionality
#               to claim, that the object after FROM must be a VIEW.
#               Testcase 3.3.1.24 should be deleted.
USE test;
--disable_warnings
DROP TABLE IF EXISTS t1;
DROP VIEW  IF EXISTS v1;
DROP VIEW  IF EXISTS v2;
--enable_warnings
--error ER_NO_SUCH_TABLE
CREATE VIEW test.v2 AS SELECT * FROM test.t1;
--error ER_NO_SUCH_TABLE
CREATE VIEW      v2 AS Select * from test.v1;
DROP VIEW IF EXISTS v2;


let $message= Testcase 3.3.1.25 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.1.25: Ensure that a view cannot be based on one or more
#                    temporary tables.
###############################################################################
# Note(mleich): A temporary table hides permanent tables which have the same
#               name. So do not forget to drop the temporary table.
--disable_warnings
DROP TABLE IF EXISTS t1_temp;
DROP TABLE IF EXISTS t2_temp;
DROP VIEW  IF EXISTS v1;
--enable_warnings
Create table t1_temp(f59 char(10),f60 int) ;
Create temporary table t1_temp(f59 char(10),f60 int) ;
Insert into t1_temp values('FER',90);
Insert into t1_temp values('CAR',27);
--error ER_VIEW_SELECT_TMPTABLE
Create view v1 as select * from t1_temp ;

Create temporary table t2_temp(f59 char(10),f60 int) ;
Insert into t2_temp values('AAA',11);
Insert into t2_temp values('BBB',22);
--error ER_VIEW_SELECT_TMPTABLE
Create or replace view v1
as select t1_temp.f59,t2_temp.f59 from t1_temp,t2_temp ;
DROP temporary table t1_temp;
DROP table t1_temp;
DROP temporary table t2_temp;

--disable_warnings
DROP TABLE IF EXISTS t1;
DROP VIEW  IF EXISTS v1;
--enable_warnings
CREATE           TABLE t1 (f1 char(10));
CREATE TEMPORARY TABLE t2 (f2 char(10));
INSERT INTO t1 VALUES('t1');
INSERT INTO t1 VALUES('A');
INSERT INTO t2 VALUES('t2');
INSERT INTO t2 VALUES('B');
# simple SELECT
--error ER_VIEW_SELECT_TMPTABLE
CREATE OR REPLACE VIEW v1 AS SELECT f2 FROM t2;
# JOIN - temporary table first
--error ER_VIEW_SELECT_TMPTABLE
CREATE OR REPLACE VIEW v1 AS SELECT *      FROM t2, t1;
--error ER_VIEW_SELECT_TMPTABLE
CREATE OR REPLACE VIEW v1 AS SELECT f2, f1 FROM t2, t1;
# JOIN - temporary table last
--error ER_VIEW_SELECT_TMPTABLE
CREATE OR REPLACE VIEW v1 AS SELECT *      FROM t1, t2;
--error ER_VIEW_SELECT_TMPTABLE
CREATE OR REPLACE VIEW v1 AS SELECT f1, f2 FROM t1, t2;
# UNION - temporary table first
--error ER_VIEW_SELECT_TMPTABLE
CREATE OR REPLACE VIEW v1 AS SELECT *  FROM t2 UNION SELECT *  FROM t1;
--error ER_VIEW_SELECT_TMPTABLE
CREATE OR REPLACE VIEW v1 AS SELECT f2 FROM t2 UNION SELECT f1 FROM t1;
# UNION - temporary table last
--error ER_VIEW_SELECT_TMPTABLE
CREATE OR REPLACE VIEW v1 AS SELECT *  FROM t1 UNION SELECT *  FROM t2;
--error ER_VIEW_SELECT_TMPTABLE
CREATE OR REPLACE VIEW v1 AS SELECT f1 FROM t1 UNION SELECT f2 FROM t2;
# SUBQUERY - temporary table first
--error ER_VIEW_SELECT_TMPTABLE
CREATE OR REPLACE VIEW v1 AS SELECT 1 FROM t2
WHERE f2 = ( SELECT f1 FROM t1 );
# SUBQUERY - temporary table last
--error ER_VIEW_SELECT_TMPTABLE
CREATE OR REPLACE VIEW v1 AS SELECT 1 FROM t1
WHERE f1 = ( SELECT f2 FROM t2 );
DROP TABLE t1;
DROP TEMPORARY TABLE t2;


let $message= Testcase 3.3.1.26 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.1.26: Ensure that a view can be based on an underlying table
#                    within the same database
###############################################################################
--disable_warnings
DROP VIEW  IF EXISTS v1;
--enable_warnings
Create view test.v1 AS Select * from test.tb2;
if ($have_bug_11589)
{
--disable_ps_protocol
}
--sorted_result
Select * from test.v1;
--enable_ps_protocol
Drop view test.v1 ;


let $message= Testcase 3.3.1.27 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.1.27: Ensure that a view can be based on an underlying view
#                    within the same database.
###############################################################################
--disable_warnings
DROP VIEW  IF EXISTS test.v1;
Drop VIEW  IF EXISTS test.v1_1 ;
--enable_warnings
Create view test.v1 AS Select * from test.tb2;
Create view test.v1_1 AS Select F59 from test.v1 ;
Select * from test.v1_1 order by F59 limit 2;
Drop view test.v1 ;
Drop view test.v1_1 ;


let $message= Testcase 3.3.1.28 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.1.28:  Ensure that a view can be based on an underlying table
#                     from another database.
###############################################################################
--disable_warnings
Drop database if exists test2 ;
--enable_warnings
create database test2 ;
Create view test2.v2 AS Select * from test.tb2 limit 50,50;
use test2 ;
Create view v1 AS Select * from test.tb2 limit 50 ;
if ($have_bug_32285)
{
--disable_ps_protocol
}
--vertical_results
Select * from v1 order by f59,f60,f61,f62,f63,f64,f65;
--horizontal_results
--enable_ps_protocol
--sorted_result
Select * from test2.v2 ;
Drop view if exists test2.v1 ;
Drop view if exists test2.v2 ;
Drop database test2 ;


let $message= Testcase 3.3.1.29 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.1.29: Ensure that a view can be based on an underlying view from
#                    another database.
###############################################################################
--disable_warnings
Drop database if exists test2 ;
Drop view if exists test.v1 ;
--enable_warnings
create database test2 ;

use test2;
Create view test.v1 AS Select * from test.tb2 limit 50 ;
Create view test2.v2 AS Select F59 from test.v1 ;
Drop view if exists test.v1 ;
Drop view if exists test2.v2 ;

# Note(mleich): Testcase 3.3.1.30 (identical requirements like 3.3.1.26)
#               --> omitted

let $message= Testcase 3.3.1.31 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.1.31: Ensure that a view can be based on a join of multiple
#                    tables within the same database.
###############################################################################
--disable_warnings
Drop table if exists test.t1 ;
--enable_warnings
CREATE TABLE test.t1 ( f59 int, f60 int );
INSERT INTO test.t1 VALUES( 34, 654 );
INSERT INTO test.t1 VALUES( 906, 434 );
INSERT INTO test.t1 VALUES( 445, 765 );
Create or replace view test.v1
AS SELECT test.t1.F59, test.tb2.F60
FROM test.tb2 JOIN test.t1 ON test.tb2.F59 = test.t1.F59     ;
--sorted_result
Select * from test.v1;
Drop view test.v1 ;


let $message= Testcase 3.3.1.32 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.1.32: Ensure that a view can be based on a join of multiple
#                    tables from another database.
###############################################################################
--disable_warnings
Drop table    if exists test.t1 ;
Drop database if exists test2 ;
Drop view     if exists test.v1 ;
--enable_warnings
create database test2 ;
use test2 ;
CREATE TABLE t1 ( f59 int, f60 int );
INSERT INTO t1 VALUES( 34, 654 );
INSERT INTO t1 VALUES( 906, 434 );
INSERT INTO t1 VALUES( 445, 765 );
CREATE VIEW test2.v1
AS SELECT test.tb2.F59, test.tb2.F60
FROM test.tb2 INNER JOIN test2.t1 ON tb2.f59 = t1.f59;
--sorted_result
Select * from test2.v1;

Use test;


let $message= Testcase 3.3.1.33 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.1.33: Ensure that a view can be based on a join of multiple
#                    views within the same database.
###############################################################################
--disable_warnings
Drop view if exists test.v1_firstview  ;
Drop view if exists test.v1_secondview ;
Drop view if exists test.v1 ;
--enable_warnings
CREATE VIEW test.v1_firstview AS SELECT * FROM test.tb2;
CREATE VIEW test.v1_secondview AS SELECT * FROM test.tb2;
CREATE VIEW test.v1
AS SELECT test.v1_firstview.f59, test.v1_firstview.f60
FROM test.v1_firstview INNER JOIN test.v1_secondview
ON test.v1_firstview.f59 = test.v1_secondview.f59 ;
SELECT * FROM test.v1 order by f59,f60 limit 0,10;
Drop view if exists test.v1_firstview  ;
Drop view if exists test.v1_secondview ;
Drop view if exists test.v1 ;


let $message= Testcase 3.3.1.34 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.1.34: Ensure that a view can be based on a join of multiple
#                    views from another database.
###############################################################################
--disable_warnings
Drop database if exists test2 ;
Drop view if exists test.v1_firstview  ;
Drop view if exists test.v1_secondview ;
--enable_warnings

create database test2 ;
use test2 ;
CREATE VIEW test.v1_firstview AS SELECT * FROM test.tb2 ;
CREATE VIEW test.v1_secondview AS SELECT * FROM test.tb2 ;

CREATE VIEW v1
AS SELECT test.v1_firstview.F59, test.v1_firstview.F60
FROM test.v1_firstview INNER JOIN test.v1_secondview
ON test.v1_firstview.f59 = test.v1_secondview.f59 ;
SELECT * FROM v1 order by f59,f60 limit 0,10;
Drop view v1 ;
Drop view test.v1_firstview ;
Drop view test.v1_secondview ;


let $message= Testcase 3.3.1.35 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.1.35: Ensure that a view can be based on a join of multiple
#                    tables and/or views within the same database.
###############################################################################
use test;

--disable_warnings
Drop view if exists test.v1;
Drop view if exists test.v1_firstview;
--enable_warnings

CREATE VIEW test.v1_firstview AS SELECT * FROM test.tb2;

CREATE VIEW test.v1
AS SELECT test.v1_firstview.f59, test.v1_firstview.f60
FROM test.v1_firstview INNER JOIN test.tb2
ON test.v1_firstview.f59 = test.tb2.f59;
SELECT * FROM test.v1 order by f59,f60 limit 0,10;
Drop view test.v1 ;
Drop view test.v1_firstview;


let $message= Testcase 3.3.1.36 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.1.36: Ensure that a view can be based on a join of multiple
#                    tables and/or views from another database.
###############################################################################
--disable_warnings
Drop database if exists test2 ;
--enable_warnings
create database test2 ;
use test2 ;

CREATE VIEW v1_firstview AS SELECT * FROM test.tb2 ;
CREATE VIEW v1
AS SELECT v1_firstview.f59, v1_firstview.f60
FROM v1_firstview INNER JOIN test.tb2 ON v1_firstview.f59 = test.tb2.f59 ;
SELECT * FROM v1 order by f59,f60 limit 0,10;

Drop database test2 ;


let $message= Testcase 3.3.1.37 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.1.37: Ensure that a view can be based on a join of multiple
#                    tables and/or views, some of which reside in the same
#                    database and some of which reside in one other database.
###############################################################################
use test;
--disable_warnings
Drop table if exists t1;
Drop view  if exists test.v1 ;
Drop view  if exists test.v1_1 ;
Drop view  if exists test.v1_1 ;
Drop view  if exists test.v1_main ;
--enable_warnings
Create view test.v1 as Select f59, f60 FROM test.tb2;
Select * from test.v1 order by f59,f60 limit 0,10;

Create table t1(f59 int, f60 int);
Insert into t1 values (90,507) ;

Create view v1_1 as Select f59,f60 from t1 ;
Select * from v1_1 ;

Create view v1_main
as SELECT test.tb2.f59 FROM test.tb2 JOIN test.v1
ON test.tb2.f59 = test.v1.f59;
Select * from v1_main order by f59 limit 0,10;

Drop table t1;
Drop view  test.v1 ;
Drop view  test.v1_1 ;
Drop view  test.v1_main ;


let $message= Testcase 3.3.1.31 - 3.3.1.37 New Implementation ;
--source include/show_msg80.inc
###############################################################################
# mleich: The testcases 3.3.1.31 - 3.3.1.37 should be tested more systematic.
#                       Ensure that a view can be based on a join of multiple
# Testcase 3.3.1.31:        tables              within the same database
# Testcase 3.3.1.32:        tables              from another    database.
# Testcase 3.3.1.33:        views               within the same database
# Testcase 3.3.1.34:        views               from another    database
# Testcase 3.3.1.35:        tables and/or views within the same database
# Testcase 3.3.1.36:        tables and/or views from another database
# Testcase 3.3.1.37:        tables and/or views, some of which reside in
#                             the same database and some of which reside in
#                             one other database.
###############################################################################
USE test;
--disable_warnings
DROP DATABASE IF EXISTS test2;
DROP TABLE IF EXISTS t0,t1;
DROP VIEW  IF EXISTS t3,t4;
--enable_warnings
CREATE DATABASE test2;

--disable_warnings
CREATE TABLE test1.t0 (f1 VARCHAR(20));
CREATE TABLE test1.t1 (f1 VARCHAR(20));
--enable_warnings
CREATE TABLE test2.t0 (f1 VARCHAR(20));
CREATE TABLE test2.t1 (f1 VARCHAR(20));
--disable_warnings
CREATE VIEW  test1.t2 AS SELECT * FROM test1.t0;
CREATE VIEW  test1.t3 AS SELECT * FROM test2.t0;
--enable_warnings
CREATE VIEW  test2.t2 AS SELECT * FROM test2.t0;
CREATE VIEW  test2.t3 AS SELECT * FROM test1.t0;
INSERT INTO  test1.t0 VALUES('test1.t0');
INSERT INTO  test1.t1 VALUES('test1.t1');
INSERT INTO  test2.t0 VALUES('test2.t0');
INSERT INTO  test2.t1 VALUES('test2.t1');

# The extreme simple standard JOIN VIEW is:
# CREATE OR REPLACE VIEW <database>.v1
# AS SELECT * FROM <table or view 1>,<table or view 2>
let $view= test.v1;
let $tab1= test.t0;
let $tab2= test.t1;
# eval CREATE OR REPLACE VIEW $view AS SELECT * FROM $tab1, $tab2;
# Produce at least all testcases via simple combinatorics, because it is better
# to check some useless combinations than to forget an important one.
let $view= test.v1;
let $num_tab1= 3;
while ($num_tab1)
{
   let $num_tab2= 3;
   while ($num_tab2)
   {
      let $num_db1= 2;
      while ($num_db1)
      {
         let $num_db2= 2;
         while ($num_db2)
         {
            # Maybe somebody needs to check the generated values
            # --disable_query_log
            # eval SELECT '$num_db1.$num_tab1,$num_db2.$num_tab2';
            # --enable_query_log
            eval CREATE OR REPLACE VIEW $view AS
                 SELECT ta.f1 AS col1,
                        tb.f1 AS col2
                 FROM test$num_db1.t$num_tab1 ta, test$num_db2.t$num_tab2 tb;
            eval SELECT * FROM $view;

            dec $num_db2;
         }

         dec $num_db1;
      }

      dec $num_tab2;
   }

   dec $num_tab1;
}


let $message= Testcase 3.3.1.38 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.1.38: Ensure that a view can be based on a join of multiple
#                    tables and/or views, some of which reside in the same
#                    database and some of which reside two or more other
#                    databases.
###############################################################################
--disable_warnings
Drop table if exists test1.t1 ;
Drop view  if exists test.v1 ;
Drop view  if exists test.v1_main;
Drop view  if exists test1.v1_1 ;
Drop database if exists test3 ;
--enable_warnings
Create view test.v1 as Select f59, f60 FROM test.tb2;
Select * from test.v1 order by f59,f60 limit 20;

Create table test1.t1 (f59 int,f60 int)  ;
Insert into test1.t1 values (199,507) ;
Create view test1.v1_1 as Select f59,f60 from test1.t1 ;
Select * from test1.v1_1 ;

--disable_warnings
--enable_warnings
Create database test3 ;

Create table test3.t1(f59 int,f60 int) ;
Insert into test3.t1 values (1023,7670) ;
Create view test3.v1_2 as Select f59,f60 from test3.t1 ;
Select * from test3.v1_2 ;
use test ;

# mleich: FIXME The SELECT should deliver at least one row.
Create view v1_main
as SELECT test.tb2.f59 as f1, test1.v1_1.f59 as f2,
          test3.v1_2.f59 as f3
FROM (test.tb2,test1.v1_1,test.v1) JOIN test3.v1_2
ON (test.v1.f59 = test1.v1_1.f59) ;
Select * from v1_main ;

DROP VIEW test.v1 ;
DROP VIEW test1.v1_1 ;
DROP VIEW test.v1_main ;
DROP DATABASE test3;


let $message= Testcase 3.3.1.39 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.1.39: Ensure that a view definition that includes a subquery in
#                    a FROM clause is rejected with an appropriate error
#                    message at create time.
###############################################################################
--disable_warnings
Drop view if exists test.v1 ;
--enable_warnings
--error ER_VIEW_SELECT_DERIVED
CREATE VIEW test.v1
AS Select f59 from (Select * FROM tb2 limit 20) tx ;
--error ER_NO_SUCH_TABLE
SELECT * FROM test.v1 order by f59 ;
--disable_warnings
Drop view if exists test.v1 ;
--enable_warnings


let $message= Testcase 3.3.1.40 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.1.40: Ensure that a view definition that includes references to
#                    one or more user variables is rejected with an appropriate
#                    error message at create time.
###############################################################################
--disable_warnings
Drop view if exists test.v1 ;
--enable_warnings
Set @var1 = 'ABC' ;
Set @var2 = 'XYZ' ;
--error ER_VIEW_SELECT_VARIABLE
CREATE VIEW test.v1 AS SELECT @var1, @var2 ;
# System variables (name starts with '@@') are also not allowed
--error ER_VIEW_SELECT_VARIABLE
CREATE VIEW test.v1 AS SELECT @@global.sort_buffer_size;
--disable_warnings
Drop view if exists test.v1 ;
--enable_warnings


let $message= Testcase 3.3.1.41 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.1.41: Ensure that a view definition within a stored procedure
#                    definition cannot include references to any of the stored
#                    procedures parameters.
###############################################################################
--disable_warnings
Drop view if exists test.v1 ;
Drop procedure if exists sp1 ;
--enable_warnings

delimiter //;
Create procedure sp1() DETERMINISTIC
      Begin
              DECLARE x char;
              Set x = 200 ;
              Create view test.v1  as SELECT * FROM tb2 WHERE f59 = x ;
      End   //
delimiter ;//
--error ER_SP_DOES_NOT_EXIST
Call sp1() ;
Drop view if exists test.v1 ;
Drop procedure sp1 ;


let $message= Testcase 3.3.1.42 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.1.42: Ensure that a view definition that attempts to create a
#                    temporary view (e.g. CREATE TEMPORARY VIEW or CREATE OR
#                    REPLACE TEMPORARY VIEW) fails, with an appropriate
#                    error message.
###############################################################################
#(01)
--disable_warnings
Drop VIEW if exists test.v1 ;
--enable_warnings
--error ER_PARSE_ERROR
CREATE TEMPORARY VIEW test.v1 AS
SELECT * FROM test.tb2 limit 2 ;
#(02)
--error ER_PARSE_ERROR
CREATE OR REPLACE TEMPORARY VIEW test.v1 AS
SELECT * FROM test.tb2 limit 2 ;

--disable_warnings
Drop view if exists test.v1 ;
--enable_warnings

Use test;


let $message= Testcase 3.3.1.43 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.1.43: Ensure that all valid changes (i.e. INSERT, UPDATE, DELETE
#                    statements) to a view are shown in the view and are
#                    accepted as changes by the underlying table(s).
###############################################################################
--disable_warnings
Drop view if exists test.v1 ;
--enable_warnings

CREATE VIEW test.v1 AS SELECT f59,f60 FROM test.tb2;

INSERT INTO test.v1 values(122,432);

if ($have_bug_32285)
{
--disable_ps_protocol
}
--vertical_results
SELECT * FROM test.tb2 where f59 = 122 and f60 = 432 limit 0,20;
--horizontal_results
--enable_ps_protocol

UPDATE test.v1 SET f59 = 3000 WHERE test.v1.f59 = 122 ;

if ($have_bug_32285)
{
--disable_ps_protocol
}
--vertical_results
SELECT * FROM test.tb2  where f59 = 3000 limit 0,20;
--horizontal_results
--enable_ps_protocol

DELETE FROM test.v1
where test.v1.f59 = 3000 and test.v1.f60 = 432;

SELECT * FROM test.tb2 where f59 = 3000 and f60 = 432;

drop view test.v1 ;


let $message= Testcase 3.3.1.44 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.1.44: Ensure that all invalid changes to a view are rejected
#                    with an appropriate error message and do not affect the
#                    data in the underlying tables(s).
###############################################################################
# mleich: Maybe we need some more tests here.
--disable_warnings
Drop view if exists test.v1 ;
--enable_warnings

# Note(mleich): The modification will fail, because the VIEW contains 'limit'
CREATE VIEW test.v1 AS SELECT f59,f60 FROM test.tb2 limit 100;

--error ER_NON_INSERTABLE_TABLE
INSERT INTO test.v1 values(31, 32, 33) ;

Drop view test.v1 ;


let $message= Testcase 3.3.1.45 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.1.45: Ensure that, for a view with a definition that does not
#                    include WITH CHECK OPTION, all changes to the view which
#                    violate the view definition do not show in the view but
#                    are accepted as changes by the underlying table(s) unless
#                    a constraint on an underlying table also makes the change
#                    invalid.
###############################################################################
--disable_warnings
Drop view if exists test.v1 ;
--enable_warnings
CREATE VIEW test.v1 AS SELECT * FROM test.tb2 where f59 = 04;

--enable_info
UPDATE test.v1 SET f59 = 30 where F59 = 04 ;
--disable_info
SELECT * FROM test.v1   where f59 = 30 order by f59;
if ($have_bug_32285)
{
--disable_ps_protocol
}
--vertical_results
SELECT * FROM test.tb2        where f59 = 30 ;
--horizontal_results
--enable_ps_protocol

--enable_info
UPDATE tb2 SET f59 = 100 where f59 = 30 ;
--disable_info
if ($have_bug_32285)
{
--disable_ps_protocol
}
--vertical_results
SELECT * FROM tb2 where f59 = 100 ;
--horizontal_results
--enable_ps_protocol
SELECT * FROM test.v1 order by f59 ;

drop view if exists test.v1 ;

--disable_warnings
Drop TABLE IF EXISTS test.t1 ;
Drop VIEW  IF EXISTS test.v1 ;
--enable_warnings
eval CREATE TABLE t1 (f1 BIGINT, f2 VARCHAR(20), PRIMARY KEY(f1))
     ENGINE = $engine_type;
INSERT INTO t1 VALUES(1,'one');
INSERT INTO t1 VALUES(2,'two');
INSERT INTO t1 VALUES(3,'three');
INSERT INTO t1 VALUES(5,'five');
CREATE VIEW v1 AS SELECT * FROM t1 WHERE f1 BETWEEN 2 AND 4;

### SELECTs
# 1.   Searched record is within the scope of the view
# 1.1  + exists within the base table
SELECT COUNT(*) FROM v1 WHERE f1 = 2;
# 1.2  + does not exists within the base table
SELECT COUNT(*) FROM v1 WHERE f1 = 4;
# 2.   Searched record is outside of the scope of the view
# 2.1  + exists within the base table
SELECT COUNT(*) FROM v1 WHERE f1 = 5;
# 2.2  + does not exists within the base table
SELECT COUNT(*) FROM v1 WHERE f1 = 10;

INSERT INTO t1 VALUES(4,'four');

### DELETEs
--enable_info
# 1.   Searched record is within the scope of the view
#      + exists within the base table
DELETE FROM v1 WHERE f1 = 3;
# 2.   Searched record is outside of the scope of the view
#      + exists within the base table
DELETE FROM v1 WHERE f1 = 5;
--disable_info
SELECT * FROM t1 ORDER BY f1;
SELECT * FROM v1 ORDER BY f1;

### INSERTs
--enable_info
# 1.   The record to be inserted will be within the scope of the view.
#      But there is already a record with the PRIMARY KEY f1 = 2 .
# OBN change for 5.1.21 --error ER_DUP_ENTRY_WITH_KEY_NAME
--error ER_DUP_ENTRY
INSERT INTO v1 VALUES(2,'two');
# 2.   The record to be inserted will be within the scope of the view.
#      There is no already existing record with the PRIMARY KEY f1 = 3 .
INSERT INTO v1 VALUES(3,'three');
# 3.   The record to be inserted will be outside of the scope of the view.
#      There is no already existing record with the PRIMARY KEY f1 = 6 .
INSERT INTO v1 VALUES(6,'six');
--disable_info
SELECT * FROM t1 ORDER BY f1;
SELECT * FROM v1 ORDER BY f1;

### UPDATEs
--enable_info
# 1.   The record to be updated is within the scope of the view
#      and will stay inside the scope.
#      But there is already a record with the PRIMARY KEY f1 = 2 .
# OBN change for 5.1.21 --error ER_DUP_ENTRY_WITH_KEY_NAME
--error ER_DUP_ENTRY
UPDATE v1 SET f1 = 2 WHERE f1 = 3;
# 2.   The record to be updated is within the scope of the view
#      and will stay inside the scope.
UPDATE v1 SET f2 = 'number' WHERE f1 = 3;
# 3.   The record to be updated is within the scope of the view
#      and will leave the scope.
UPDATE v1 SET f1 = 10 WHERE f1 = 3;
# 4.   The record to be updated is outside of the scope of the view.
UPDATE v1 SET f2 = 'number' WHERE f1 = 1;
--disable_info


let $message= Testcase 3.3.1.46 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.1.46: Ensure that, for a view with a definition that does
#                    include WITH CHECK OPTION, all changes to the view which
#                    violate the view definition are rejected with an
#                    appropriate error message and are not accepted as changes
#                    by the underlying table(s).
###############################################################################
--disable_warnings
Drop view if exists test.v1 ;
--enable_warnings
CREATE VIEW test.v1 AS SELECT f59,f60
FROM test.tb2 where f59 = 195 WITH CHECK OPTION ;

--error ER_VIEW_CHECK_FAILED
UPDATE test.v1 SET f59 = 198 where f59=195 ;
SELECT * FROM test.v1 order by f59 ;

drop view if exists test.v1 ;


let $message= Testcase 3.3.1.47 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.1.47: Ensure that, for a view with a definition that does
#                    include WITH LOCAL CHECK OPTION, all changes to the view
#                    which violate the view definition are rejected with an
#                    appropriate error message and are not accepted as changes
#                    by the underlying table(s).
###############################################################################
--disable_warnings
Drop view if exists test.v1 ;
Drop view if exists test.v2 ;
--enable_warnings
CREATE VIEW test.v1 AS SELECT f59,f60
FROM test.tb2 where F59 = 0987 WITH LOCAL CHECK OPTION ;
CREATE VIEW test.v2 as SELECT * FROM test.v1 ;

# This UPDATE violates the definition of VIEW test.v1.
--error ER_VIEW_CHECK_FAILED
UPDATE test.v1 SET F59 = 919 where f59 = 0987 ;
SELECT * FROM test.v1 order by f59 ;

# mleich: This UPDATE violates the definition of VIEW test.v1, but this
#         does not count, because the UPDATE runs on test.v2, which
#         is defined without any CHECK OPTION.
#         FIXME Does this testcase fit to 3.3.1.47 ?
UPDATE test.v2 SET F59 = 9879 where f59 = 919 ;
SELECT * FROM tb2 where f59 = 9879 ;

drop view if exists v1 ;
drop view if exists v2 ;


let $message= Testcase 3.3.1.48 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.1.48: Ensure that, for a view with a definition that does
#                    include WITH CASCADED CHECK OPTION, all changes to the
#                    view which violate the view definition are rejected with
#                    an appropriate error message and are not accepted as
#                    changes by the underlying table(s).
###############################################################################
--disable_warnings
DROP TABLE IF EXISTS test.t1;
DROP VIEW  IF EXISTS test.v1;
--enable_warnings
eval CREATE TABLE t1 (f1 ENUM('A', 'B', 'C') NOT NULL, f2 INTEGER)
     ENGINE = $engine_type;
INSERT INTO t1 VALUES ('A', 1);
SELECT * FROM t1 order by f1, f2;

CREATE VIEW  v1 AS SELECT * FROM t1 WHERE f2 BETWEEN 1 AND 2
WITH CASCADED CHECK OPTION ;
SELECT * FROM v1 order by f1, f2;
--enable_info
# positive cases
UPDATE v1 SET f2 = 2 WHERE f2 = 1;
INSERT INTO v1 VALUES('B',2);
--disable_info
# Bug#11771: View over InnoDB table, wrong result SELECT on VIEW,
#            field->query_id wrong
SELECT * FROM v1 order by f1, f2;
# negative cases
--enable_info
--error ER_VIEW_CHECK_FAILED
UPDATE v1 SET f2 = 4;
--error ER_VIEW_CHECK_FAILED
INSERT INTO v1 VALUES('B',3);
--disable_info
# Bug#11771: View over InnoDB table, wrong result SELECT on VIEW,
#            field->query_id wrong
SELECT * FROM v1 order by f1, f2;


let $message= Testcase 3.3.1.49 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.1.49: Ensure that the WITH [LOCAL | CASCADED] CHECK OPTION
#                    constraint is always correctly performed within the
#                    correct scope, including in cases where a view is based
#                    upon multiple other views whose definitions include every
#                    possible combination of the WITH CHECK OPTION variants.
###############################################################################
--disable_warnings
Drop table if exists test.t1 ;
Drop view  if exists test.v1 ;
Drop view  if exists test.v2 ;
Drop view  if exists test.v3 ;
--enable_warnings
Create table test.t1 (f59 INT, f60 INT) ;

Insert into test.t1  values (100,4234);
Insert into test.t1  values (290,6624);
Insert into test.t1  values (410,765);
Insert into test.t1  values (300,433334);
Insert into test.t1  values (800,9788);
Insert into test.t1  values (501,9866);

Create view test.v1 as select f59
FROM test.t1 where f59<500 with check option ;

Create view test.v2 as select *
from test.v1 where f59>0 with local check option ;

--disable_warnings
--enable_warnings

Create view test.v3 as select *
from test.v1 where f59>0 with cascaded check option ;

Insert into test.v2 values(23) ;
Insert into test.v3 values(24) ;

drop view if exists test.v1 ;
drop view if exists test.v2 ;
drop view if exists test.v3 ;

let $message= Testcase 3.3.1.49A ;
--source include/show_msg80.inc
# Testplan:
# -----------------------------------------------------------
# VIEW v1 is based on table t1 (*)
# VIEW v2 is based on view  v1 (*)
# VIEW v3 is based on view  v2 (*)
#
# (*) All variants like
# - without check option
# - WITH CASCADED CHECK OPTION
# - WITH          CHECK OPTION (default = CASCADED)
# - WITH LOCAL    CHECK OPTION
#
# The rules for updating and inserting column values:
# 1. Top VIEW   WITH CASCADED CHECK OPTION
#    --> The WHERE qualifications of all nested VIEWs have to be fulfilled.
#        The CHECK OPTIONS of underlying VIEWs have no effect.
# 2. Top VIEW   WITH LOCAL CHECK OPTION
#    --> Only the WHERE qualification of this VIEW has to be fulfilled.
#        The CHECK OPTIONS of underlying VIEWs have no effect.
# 3. Top VIEW   without any CHECK OPTION
#    --> The WHERE qualifications of all nested VIEWs need not to be fulfilled.
#        The CHECK OPTIONS of underlying VIEWs have no effect.
#
# v3        | v2        | v1        | Qualifications to be checked
# ------------------------------------------------------------------------
# CASCADED  | <any>     | <any>     | qual_v3 + qual_v2 + qual_v3
# <default> | <any>     | <any>     | qual_v3 + qual_v2 + qual_v3
# LOCAL     | <any>     | <any>     | qual_v3
# <without> | <any>     | <any>     |
#
# Note: The CHECK OPTION does not influence the retrieval of rows
#       (SELECT/DELETE/UPDATE). All WHERE qualifications will be applied
#       for the retrieval of rows.
#
#       The annoying redundant
#       eval INSERT INTO t1_results VALUES (@v3_to_v1_options,@statement,
#                                           @v3_to_v1_violation,$mysql_errno);
#       could not be put into a file to be sourced because of the closed 
#       Bug#10267 mysqltest, wrong number of loops when a script is sourced
#                 within a loop
# To be implemented later.

USE test;
--disable_warnings
DROP TABLE IF EXISTS test.t1 ;
DROP TABLE IF EXISTS test.t1_results ;
DROP VIEW  IF EXISTS test.v1;
DROP VIEW  IF EXISTS test.v2;
DROP VIEW  IF EXISTS test.v3;
--enable_warnings
CREATE TABLE t1 (f1 INTEGER, f2 CHAR(20));
CREATE TABLE t1_results (v3_to_v1_options VARCHAR(100), statement VARCHAR(10),
                         v3_to_v1_violation VARCHAR(20), errno CHAR(10));
--disable_query_log
SET @part1= '';
SET @part2= 'WITH          CHECK OPTION';
SET @part3= 'WITH CASCADED CHECK OPTION';
SET @part4= 'WITH LOCAL    CHECK OPTION';
--enable_query_log

let $num1= 4;
while ($num1)
{
  --disable_query_log
  eval SET @v1_part= @part$num1;
  let $aux= `SELECT CONCAT('CREATE VIEW v1 AS SELECT f1, f2
  FROM t1 WHERE f1 BETWEEN 0 AND 10 ', @v1_part)` ;
  --enable_query_log
  eval $aux ;

  let $num2= 4;
  while ($num2)
  {
    --disable_query_log
    eval SET @v2_part= @part$num2;
    let $aux= `SELECT CONCAT('CREATE VIEW v2 AS SELECT f1 AS col1, f2 AS col2
    FROM v1 WHERE f1 BETWEEN 6 AND 16 ', @v2_part)` ;
    --enable_query_log
    eval $aux ;

    let $num3= 4;
    while ($num3)
    {
      --disable_query_log
      eval SET @v3_part= @part$num3;
      let $aux= `SELECT CONCAT('CREATE VIEW v3 (my_col1,my_col2) AS SELECT *
      FROM v2 WHERE col1 MOD 2 = 0 ', @v3_part)` ;
      eval $aux ;
      --vertical_results
      SELECT CONCAT(IF(@v3_part = '','        <nothing>         ',
                                     @v3_part), ' - ',
                    IF(@v2_part = '','        <nothing>         ',
                                     @v2_part), ' - ',
                    IF(@v1_part = '','        <nothing>         ',
                                     @v1_part))
             AS "option_variant"
      UNION SELECT RPAD('', 80, '-');
      SET @v3_to_v1_options = CONCAT(IF(@v3_part = '','        <nothing>         ',
                                     @v3_part), ' - ',
                    IF(@v2_part = '','        <nothing>         ',
                                     @v2_part), ' - ',
                    IF(@v1_part = '','        <nothing>         ',
                                     @v1_part));
      --horizontal_results
      --enable_query_log
    # 1. Visibility of records of t1 via SELECT on the VIEWs
      # Outside v1 (0 to 10)
      INSERT INTO t1 VALUES(16, 'sixteen');
      # Inside v1 (0 to 10), Outside v2 ((0 to 10) AND (6 to 16) -> (6 to 10))
      INSERT INTO t1 VALUES(0, 'zero');
      # Inside v1 (0 to 10), Inside v2 ((0 to 10) AND (6 to 16) -> (6 to 10))
      # Outside v3 ( value MOD 2 = 0 )
      INSERT INTO t1 VALUES(7, 'seven');
      # Inside v1 (0 to 10), Inside v2 ((0 to 10) AND (6 to 16) -> (6 to 10))
      # Inside v3 ( value MOD 2 = 0 )
      INSERT INTO t1 VALUES(8, 'eight');
      SELECT * FROM v1;
      SELECT * FROM v2;
      SELECT * FROM v3;
      SELECT * FROM t1;
      DELETE FROM t1;
    # 2. DELETEs within v3
      # Outside v1 (0 to 10)
      INSERT INTO t1 VALUES(16, 'sixteen');
      # Inside v1 (0 to 10), Outside v2 ((0 to 10) AND (6 to 16) -> (6 to 10))
      INSERT INTO t1 VALUES(0, 'zero');
      # Inside v1 (0 to 10), Inside v2 ((0 to 10) AND (6 to 16) -> (6 to 10))
      # Outside v3 ( value MOD 2 = 0 )
      INSERT INTO t1 VALUES(7, 'seven');
      # Inside v1 (0 to 10), Inside v2 ((0 to 10) AND (6 to 16) -> (6 to 10))
      # Inside v3 ( value MOD 2 = 0 )
      INSERT INTO t1 VALUES(8, 'eight');
      --enable_info
      # Outside v1 (0 to 10)
      DELETE FROM v3 WHERE my_col1 = 16;
      # Inside v1 (0 to 10), Outside v2 ((0 to 10) AND (6 to 16) -> (6 to 10))
      DELETE FROM v3 WHERE my_col1 = 0;
      # Inside v1 (0 to 10), Inside v2 ((0 to 10) AND (6 to 16) -> (6 to 10))
      # Outside v3 ( value MOD 2 = 0 )
      DELETE FROM v3 WHERE my_col1 = 7;
      # Inside v1 (0 to 10), Inside v2 ((0 to 10) AND (6 to 16) -> (6 to 10))
      # Inside v3 ( value MOD 2 = 0 )
      DELETE FROM v3 WHERE my_col1 = 8;
      --disable_info
      SELECT * FROM t1;
      DELETE FROM t1;
    # 3. UPDATEs within v3 (modify my_col2, which is not part of any
    #    WHERE qualification)
    #    The behaviour should be similar to 3. DELETE.
      # Outside v1 (0 to 10)
      INSERT INTO t1 VALUES(16, 'sixteen');
      # Inside v1 (0 to 10), Outside v2 ((0 to 10) AND (6 to 16) -> (6 to 10))
      INSERT INTO t1 VALUES(0, 'zero');
      # Inside v1 (0 to 10), Inside v2 ((0 to 10) AND (6 to 16) -> (6 to 10))
      # Outside v3 ( value MOD 2 = 0 )
      INSERT INTO t1 VALUES(7, 'seven');
      # Inside v1 (0 to 10), Inside v2 ((0 to 10) AND (6 to 16) -> (6 to 10))
      # Inside v3 ( value MOD 2 = 0 )
      INSERT INTO t1 VALUES(8, 'eight');
      --enable_info
      # Outside v1 (0 to 10)
      UPDATE v3 SET my_col2 = 'whatever' WHERE my_col1 = 16;
      # Inside v1 (0 to 10), Outside v2 ((0 to 10) AND (6 to 16) -> (6 to 10))
      UPDATE v3 SET my_col2 = 'whatever' WHERE my_col1 = 0;
      # Inside v1 (0 to 10), Inside v2 ((0 to 10) AND (6 to 16) -> (6 to 10))
      # Outside v3 ( value MOD 2 = 0 )
      UPDATE v3 SET my_col2 = 'whatever' WHERE my_col1 = 7;
      # Inside v1 (0 to 10), Inside v2 ((0 to 10) AND (6 to 16) -> (6 to 10))
      # Inside v3 ( value MOD 2 = 0 )
      UPDATE v3 SET my_col2 = 'whatever' WHERE my_col1 = 8;
      --disable_info
      SELECT * FROM t1;
      DELETE FROM t1;
    # 4. UPDATEs within v3 (modify my_col1 to values inside and outside
    #    of the WHERE qualifications)
      --disable_query_log
      SET @statement = 'UPDATE';
      --enable_query_log
      INSERT INTO t1 VALUES(8, 'eight');
      # Alter to value outside of v3
      --disable_query_log
      SET @v3_to_v1_violation = 'v3_  _  ';
      --enable_query_log
      --enable_info
      UPDATE v3 SET my_col1 = 7 WHERE my_col1 = 8;
      --disable_info
      --disable_query_log
      eval INSERT INTO t1_results VALUES (@v3_to_v1_options,@statement,
                                          @v3_to_v1_violation,$mysql_errno);
      --enable_query_log
      SELECT * FROM t1;
      DELETE FROM t1;
      INSERT INTO t1 VALUES(8, 'eight');
      # Alter to value outside of v2
      --disable_query_log
      SET @v3_to_v1_violation = '  _v2_  ';
      --enable_query_log
      --enable_info
      UPDATE v3 SET my_col1 = 0 WHERE my_col1 = 8;
      --disable_info
      --disable_query_log
      eval INSERT INTO t1_results VALUES (@v3_to_v1_options,@statement,
                                          @v3_to_v1_violation,$mysql_errno);
      --enable_query_log
      SELECT * FROM t1;
      DELETE FROM t1;
      INSERT INTO t1 VALUES(8, 'eight');
      # Alter to value outside of v1
      --disable_query_log
      SET @v3_to_v1_violation = '  _  _v1';
      --enable_query_log
      --enable_info
      UPDATE v3 SET my_col1 = 16 WHERE my_col1 = 8;
      --disable_info
      --disable_query_log
      eval INSERT INTO t1_results VALUES (@v3_to_v1_options,@statement,
                                          @v3_to_v1_violation,$mysql_errno);
      --enable_query_log
      SELECT * FROM t1;
      DELETE FROM t1;
      INSERT INTO t1 VALUES(8, 'eight');
      # Alter to value inside of v1
      --disable_query_log
      SET @v3_to_v1_violation = '  _  _  ';
      --enable_query_log
      --enable_info
      UPDATE v3 SET my_col1 = 10 WHERE my_col1 = 8;
      --disable_info
      --disable_query_log
      eval INSERT INTO t1_results VALUES (@v3_to_v1_options,@statement,
                                          @v3_to_v1_violation,$mysql_errno);
      --enable_query_log
      SELECT * FROM t1;
      DELETE FROM t1;
    # 5. INSERTs into v3
      --disable_query_log
      SET @statement = 'INSERT';
      --enable_query_log
      # Outside v1 (0 to 10)
      --disable_query_log
      SET @v3_to_v1_violation = '  _  _v1';
      --enable_query_log
      --enable_info
      INSERT INTO v3 VALUES(16, 'sixteen');
      --disable_info
      --disable_query_log
      eval INSERT INTO t1_results VALUES (@v3_to_v1_options,@statement,
                                          @v3_to_v1_violation,$mysql_errno);
      --enable_query_log
      # Inside v1 (0 to 10), Outside v2 ((0 to 10) AND (6 to 16) -> (6 to 10))
      --disable_query_log
      SET @v3_to_v1_violation = '  _v2_  ';
      --enable_query_log
      --enable_info
      INSERT INTO v3 VALUES(0, 'zero');
      --disable_info
      --disable_query_log
      eval INSERT INTO t1_results VALUES (@v3_to_v1_options,@statement,
                                          @v3_to_v1_violation,$mysql_errno);
      --enable_query_log
      # Inside v1 (0 to 10), Inside v2 ((0 to 10) AND (6 to 16) -> (6 to 10))
      # Outside v3 ( value MOD 2 = 0 )
      --disable_query_log
      SET @v3_to_v1_violation = 'v3_  _  ';
      --enable_query_log
      --enable_info
      INSERT INTO v3 VALUES(7, 'seven');
      --disable_info
      # Inside v1 (0 to 10), Inside v2 ((0 to 10) AND (6 to 16) -> (6 to 10))
      # Inside v3 ( value MOD 2 = 0 )
      --disable_query_log
      SET @v3_to_v1_violation = '  _  _  ';
      --enable_query_log
      --enable_info
      INSERT INTO v3 VALUES(8, 'eight');
      --disable_info
      --disable_query_log
      eval INSERT INTO t1_results VALUES (@v3_to_v1_options,@statement,
                                          @v3_to_v1_violation,$mysql_errno);
      --enable_query_log
      SELECT * FROM t1;
      DELETE FROM t1;

      DROP VIEW v3;
      dec $num3;
    }

    DROP VIEW v2;
    dec $num2;
  }

  DROP VIEW v1;
  dec $num1;
}

SELECT * FROM t1_results ORDER BY v3_to_v1_options;

let $message=
Plausibility checks for INSERTs and UPDATEs ( 4. and 5. above).
All following SELECTs must give ROW NOT FOUND ;
--source include/show_msg80.inc

# Plausibility checks for INSERTs and UPDATEs ( 4. and 5. above):
# 1. There must be NO denied INSERT/UPDATE, when no WHERE qualification
#    is violated. Expect ROW NOT FUND
SELECT * FROM t1_results
WHERE v3_to_v1_violation = '  _  _  ' AND errno <> 0
ORDER BY v3_to_v1_options;
# 2. There must be NO denied INSERT/UPDATE, when the toplevel VIEW v3 is
#    defined without any CHECK OPTION. Expect ROW NOT FUND
SELECT * FROM t1_results
WHERE v3_to_v1_options LIKE '  %' AND errno <> 0
ORDER BY v3_to_v1_options;
# 3. There must be NO successful INSERT/UPDATE, when the toplevel VIEW v3 is
#    defined with any CHECK OPTION and the WHERE qualification of this VIEW is
#    violated. Expect ROW NOT FUND
SELECT * FROM t1_results
WHERE v3_to_v1_options LIKE 'WITH %'
  AND v3_to_v1_violation LIKE 'v3_%' AND errno = 0
ORDER BY v3_to_v1_options;
# 4. There must be NO successful INSERT/UPDATE, when the toplevel VIEW v3 is
#    defined with any CHECK OPTION and the CHECK OPTION does not contain LOCAL
#    and the WHERE qualification of any VIEW is violated. Expect ROW NOT FUND
SELECT * FROM t1_results
WHERE v3_to_v1_options LIKE 'WITH %' AND v3_to_v1_options NOT LIKE 'WITH LOCAL %'
  AND v3_to_v1_violation NOT LIKE '  _  _  ' AND errno = 0
ORDER BY v3_to_v1_options;
# 5. There must be NO failing INSERT/UPDATE getting a
#    sql_errno <> 1369 (ER_VIEW_CHECK_FAILED).
SELECT * FROM t1_results
WHERE errno <> 0 AND errno <> 1369
ORDER BY v3_to_v1_options;
let $message= End of plausibility checks;
--source include/show_msg80.inc

DROP TABLE t1_results;


let $message= Testcase 3.3.1.50 - 3.3.1.53;
--source include/show_msg80.inc
--disable_warnings
DROP VIEW  IF EXISTS test.v1;
--enable_warnings
###############################################################################
# Testcase 3.3.1.50: Ensure that a view that is a subset of every column and
#                    every row of a single underlying table, contains the
#                    correct row-and-column data; such a view has a definition
#                    that is semantically equivalent to CREATE VIEW <view name>
#                    AS SELECT * FROM <table name>.
###############################################################################
CREATE VIEW test.v1 AS SELECT * FROM test.tb2;
if ($have_bug_32285)
{
--disable_ps_protocol
}
--vertical_results
SELECT * FROM test.v1 order by f59,f60,f61 ;
--horizontal_results
--enable_ps_protocol
drop view test.v1 ;
###############################################################################
# Testcase 3.3.1.51: Ensure that a view that is a subset of only some columns
#                    and every row of a single underlying table, contains the
#                    correct row-and-column data; such a view has a definition
#                    that is semantically equivalent to CREATE VIEW <view name>
#                    AS SELECT col1, col3 FROM <table name>.
###############################################################################
CREATE VIEW test.v1 AS SELECT F59,F61 FROM test.tb2;
SELECT * FROM test.v1 order by F59, F61 limit 50;
drop view test.v1 ;
###############################################################################
# Testcase 3.3.1.52: Ensure that a view that is a subset of every column and
#                    some rows of a single underlying table, contains the
#                    correct row-and-column data; such a view has a definition
#                    that is semantically equivalent to CREATE VIEW <view name>
#                    AS SELECT * FROM <table name> WHERE ....
###############################################################################
CREATE VIEW test.v1 AS SELECT * FROM test.tb2 order by f59, f60, f61;
if ($have_bug_11589)
{
--disable_ps_protocol
}
--vertical_results
SELECT * FROM test.v1 order by f59,f60,f61 ;
--horizontal_results
--enable_ps_protocol
drop view test.v1 ;
###############################################################################
# Testcase 3.3.1.53: Ensure that a view that is a subset of only some columns
#                    and some rows of a single underlying table, contains
#                    the correct row-and-column data; such a view has a
#                    definition that is semantically equivalent to CREATE VIEW
#                    <view name> AS SELECT col1, col3 FROM <table name> WHERE ..
###############################################################################
CREATE VIEW test.v1 AS SELECT F59,f61 FROM test.tb2;
SELECT * FROM test.v1 order by f59,f61 desc limit 20;
drop view test.v1 ;


let $message= Testcase 3.3.1.54 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.1.54: Ensure that a view that is a subset of some or all columns
#                    and/or column expressions and some or all rows of a single
#                    underlying table contains the correct row-and-column data.
###############################################################################
USE test;
--disable_warnings
drop table if exists  test.t1 ;
drop table if exists  test.t2 ;
drop view  if exists  test.v1 ;
--enable_warnings
Create table t1 (f59 int, f60 int) ;
Create table t2 (f59 int, f60 int) ;

Insert into t1 values (1,10)   ;
Insert into t1 values (2,20)   ;
Insert into t1 values (47,80)  ;
Insert into t2 values (1,1000) ;
Insert into t2 values (2,2000) ;
Insert into t2 values (31,97)  ;
Create view test.v1 as select t1.f59, t1.f60
from t1,t2 where t1.f59=t2.f59 ;
Select * from test.v1 order by f59 limit 50 ;

drop table test.t1 ;
drop table test.t2 ;
drop view  test.v1 ;


# FIXME(mleich): Implement an automatic check for 3.3.1.50 - 3.3.1.54
#                CREATE VIEW ... AS <SELECT ... FROM tb2 ...>
#                CREATE TEMPORARY TABLE ... AS <SELECT ... FROM tb2 ...>
#                Comparison of the VIEW with the temporary table

let $message= Testcase 3.3.1.50 - 3.3.1.54 additional implementation;
--source include/show_msg80.inc
--disable_warnings
DROP TABLE IF EXISTS t1 ;
DROP VIEW  IF EXISTS v1 ;
--enable_warnings
#
#
# Testplan
# ------------------------
#
#  Testcase  |  all columns  |  all rows  |  column expressions
#  ---------------------------------------------------
#  3.3.1.50  |  yes          |  yes       |  no
#  3.3.1.51  |  no           |  yes       |  no
#  3.3.1.52  |  yes          |  no        |  no
#  3.3.1.53  |  no           |  no        |  no
#  3.3.1.54  |  no           |  no        |  yes
CREATE TABLE t1 ( f1 BIGINT, f2 char(10), f3 DECIMAL(10,5) );
INSERT INTO t1 VALUES(1, 'one',   1.1);
INSERT INTO t1 VALUES(2, 'two',   2.2);
INSERT INTO t1 VALUES(3, 'three', 3.3);
# 3.3.1.50
CREATE OR REPLACE VIEW v1 AS SELECT * FROM t1;
SELECT * FROM v1;
# 3.3.1.51
CREATE OR REPLACE VIEW v1 AS SELECT f2 FROM t1;
SELECT * FROM v1;
# 3.3.1.52
CREATE OR REPLACE VIEW v1 AS SELECT * FROM t1 WHERE f3 = 2.2;
SELECT * FROM v1;
# 3.3.1.53
CREATE OR REPLACE VIEW v1 AS SELECT f2 FROM t1 WHERE f3 = 2.2;
SELECT * FROM v1;
# 3.3.1.54
--vertical_results
SET sql_mode = 'traditional,ansi';
# due to bug#32496 "no trailing blanks in identifier".
CREATE OR REPLACE VIEW v1 AS
SELECT f3 AS "pure column f3:", f1 + f3 AS "sum of columns f1 + f3 =",
       3 * (- 0.11111E+1) AS "product of constants 3 * (- 0.11111E+1):",
       '->' || CAST(f3 AS CHAR) || '<-'
         AS "expression with '||'=CONCAT and CAST(DECIMAL column AS CHAR):"
FROM t1 WHERE f1 = 2;
# This error is not conformant with ansi (see bug#32496). hhunger
--error ER_WRONG_COLUMN_NAME
CREATE OR REPLACE VIEW v1 AS
SELECT f3 AS "pure column f3: ", f1 + f3 AS "sum of columns f1 + f3 = ",
       3 * (- 0.11111E+1) AS "product of constants 3 * (- 0.11111E+1): ",
       '->' || CAST(f3 AS CHAR) || '<-'
         AS "expression with '||'=CONCAT and CAST(DECIMAL column AS CHAR): "
FROM t1 WHERE f1 = 2;
SELECT * FROM v1;
SET sql_mode = '';
--horizontal_results


let $message= Testcases 3.3.1.55 - 3.3.1.62 ;
--source include/show_msg80.inc
###############################################################################
# Testcase: Ensure that a view that is a subset of some or all columns and
#           some or all rows of multiple tables joined with an
# 3.3.1.55  INNER JOIN
# 3.3.1.56  CROSS JOIN
# 3.3.1.57  STRAIGHT JOIN
# 3.3.1.58  NATURAL JOIN
# 3.3.1.59  LEFT OUTER JOIN
# 3.3.1.60  NATURAL LEFT OUTER JOIN
# 3.3.1.61  RIGHT OUTER
# 3.3.1.62  NATURAL RIGHT OUTER
#           condition contains the correct row-and-column data.
###############################################################################
--disable_warnings
Drop table if exists t1, t2 ;
Drop view  if exists v1 ;
--enable_warnings
Create table t1 (f59 int, f60 char(10), f61 int, a char(1)) ;
Insert into t1 values (1, 'single',    3, '1')  ;
Insert into t1 values (2, 'double',    6, '2')  ;
Insert into t1 values (3, 'single-f3', 4, '3')  ;

Create table t2 (f59 int, f60 char(10), f61 int, b char(1)) ;
Insert into t2 values (2, 'double',    6, '2')  ;
Insert into t2 values (3, 'single-f3', 6, '3')  ;
Insert into t2 values (4, 'single',    4, '4')  ;

# Testcase 3.3.1.55 ;
create or replace view test.v1 as
Select t1.f59 t1_f59, t2.f59 t2_f59, t1.f60 t1_f60, t2.f60 t2_f60,
       t1.f61 t1_f61, t2.f61 t2_f61
from t1 inner join t2 where t1.f59 = t2.f59 ;
select * from test.v1 order by t1_f59 ;
Select t1.f59 t1_f59, t2.f59 t2_f59, t1.f60 t1_f60, t2.f60 t2_f60,
       t1.f61 t1_f61, t2.f61 t2_f61
from t1 inner join t2 where t1.f59 = t2.f59;

# Testcase 3.3.1.56 ;
Create or replace view test.v1 as
Select t1.f59 AS t1_f59, t2.f59 AS t2_f59
FROM t2 cross join t1;
Select * from v1 order by t1_f59,t2_f59;
Select t1.f59 AS t1_f59, t2.f59 AS t2_f59
FROM t2 cross join t1;

# Testcase 3.3.1.57 ;
Create or replace view test.v1 as
Select straight_join t1.f59 AS t1_f59, t2.f59 AS t2_f59
FROM t2,t1;
Select * from v1 order by t1_f59,t2_f59;
Select straight_join t1.f59 AS t1_f59, t2.f59 AS t2_f59
FROM t2,t1;

# Testcase 3.3.1.58 ;
Create or replace view test.v1 as
Select f59, f60, f61, a, b
FROM t2 natural join t1;
Select * from v1 order by f59;
Select f59, f60, f61, a, b
FROM t2 natural join t1;

# Testcase 3.3.1.59 ;
Create or replace view test.v1 as
Select t1.f59 t1_f59, t2.f59 t2_f59, t1.f60 t1_f60, t2.f60 t2_f60,
       t1.f61 t1_f61, t2.f61 t2_f61
FROM t2 left outer join t1 on t2.f59=t1.f59;
Select * from v1 order by t1_f59;
Select t1.f59 t1_f59, t2.f59 t2_f59, t1.f60 t1_f60, t2.f60 t2_f60,
       t1.f61 t1_f61, t2.f61 t2_f61
FROM t2 left outer join t1 on t2.f59=t1.f59;

# Testcase 3.3.1.60 ;
Create or replace view test.v1 as
Select f59, f60, f61, t1.a, t2.b
FROM t2 natural left outer join t1;
Select * from v1 order by f59;
Select f59, f60, f61, t1.a, t2.b
FROM t2 natural left outer join t1;

# Testcase 3.3.1.61 ;
Create or replace view test.v1 as
Select t1.f59 t1_f59, t2.f59 t2_f59, t1.f60 t1_f60, t2.f60 t2_f60,
       t1.f61 t1_f61, t2.f61 t2_f61
FROM t2 right outer join t1 on t2.f59=t1.f59;
Select * from v1 order by t1_f59;
Select t1.f59 t1_f59, t2.f59 t2_f59, t1.f60 t1_f60, t2.f60 t2_f60,
       t1.f61 t1_f61, t2.f61 t2_f61
FROM t2 right outer join t1 on t2.f59=t1.f59;

# Testcase 3.3.1.62 ;
Create or replace view test.v1 as
Select f59, f60, a, b
FROM t2 natural right outer join t1;
Select * from v1 order by f59 desc;
Select f59, f60, a, b
FROM t2 natural right outer join t1;

drop table t1, t2;
drop view  v1 ;

Use test;


let $message= Testcase 3.3.1.A1 - 3.3.1.A3 ;
--source include/show_msg80.inc
###############################################################################
# Testcase: Ensure that a view that is a subset of some or all columns and/or
#           column expressions and some or all rows of multiple tables joined
#           with the combination of
# 3.3.1.A1  LEFT  JOIN
# 3.3.1.A2  INNER JOIN
# 3.3.1.A3  CROSS JOIN
#           condition contains the correct row-and-column data
###############################################################################
# Testcase 3.3.1.A1 ;
--disable_warnings
Drop table if exists t1 ;
Drop view  if exists v1;
--enable_warnings
Create table t1 (f59 int, f60 int, f61 int) ;
Insert into t1 values (101,201,301)  ;
Insert into t1 values (107,501,601)  ;
Insert into t1 values (901,801,401)  ;

Create or replace view test.v1 as
Select tb2.f59 FROM tb2 LEFT JOIN t1 on tb2.f59 = t1.f59 ;
Select * from test.v1 order by f59 limit 0,10;
Drop view if exists test.v1 ;

# Testcase 3.3.1.A2 ;
--disable_warnings
Drop table if exists t1 ;
Drop view  if exists v1;
--enable_warnings
Create table t1 (f59 int, f60 int, f61 int) ;
Insert into t1 values (201,201,201)  ;
Insert into t1 values (207,201,201)  ;
Insert into t1 values (201,201,201)  ;

Create or replace view test.v1
as Select tb2.f59 FROM tb2 INNER JOIN t1 on tb2.f59 = t1.f59 ;
Select * from test.v1 order by f59 limit 0,10;
Drop view if exists test.v1 ;

# Testcase 3.3.1.A3 ;
--disable_warnings
Drop table if exists t1 ;
Drop view  if exists v1;
--enable_warnings
Create table t1 (f59 int, f60 int, f61 int) ;
Insert into t1 values (21,21,21)  ;
Insert into t1 values (27,21,21)  ;
Insert into t1 values (21,21,21)  ;

Create or replace view test.v1
as Select tb2.f59 FROM tb2 CROSS JOIN t1 on tb2.f59 = t1.f59 ;
Select * from test.v1 order by f59 limit 0,10;

Drop view test.v1 ;


let $message= Testcase 3.3.1.63 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.1.63: Ensure that a view that is a subset of some or all columns
#                    and/or column expressions and some or all rows of multiple
#                    tables joined with every possible combination of JOIN
#                    conditions, UNION, UNION ALL and UNION DISTINCT, nested at
#                    multiple levels, contains the correct row-and-column data.
###############################################################################
--disable_warnings
Drop table if exists t1 ;
Drop view if exists test.v1 ;
--enable_warnings

Create table t1 (f59 int, f60 int, f61 int) ;
Insert into t1 values (11,21,31)  ;
Insert into t1 values (17,51,61)  ;
Insert into t1 values (91,81,41)  ;

Create or replace view test.v1 as  (Select f59 FROM tb2 where f59=17 )
Union  ALL (Select f59 from t1 where f59=17 );
Select * from test.v1 order by f59 limit 0,10;

Create or replace view test.v1 as  (Select f59 FROM tb2 where f59=17 )
Union  (Select f59 from t1 where f59=17 );
Select * from test.v1 order by f59 limit 0,10;

Create or replace view test.v1 as  (Select f59 FROM tb2 where f59=17 )
Union Distinct (Select f59 from t1 where f60=17 );
Select * from test.v1 order by f59 limit 0,10;

Drop view test.v1 ;

--disable_warnings
drop table if exists t1;
drop view  if exists test.v1;
--enable_warnings
create table t1 (f59 int, f60 int, f61 int);

insert into t1 values (101,201,301);
insert into t1 values (107,501,601);
insert into t1 values (901,801,401);

create or replace view test.v1 as
select tb2.f59 from tb2  join t1 on tb2.f59 = t1.f59;
select * from test.v1 order by f59 limit 0,10;

create or replace view test.v1 as
(select f59 from tb2 where f59=107 )
union all
(select f59 from t1 where f59=107 );
select * from test.v1 order by f59 limit 0,10;

create or replace view test.v1 as
(select f59 from tb2 where f59=107 )
union
(select f59 from t1 where f59=107 );
select * from test.v1 order by f59 limit 0,10;

create or replace view test.v1 as
(select f59 from tb2 where f59=107 )
union distinct
(select f59 from t1 where f59=107 );
select * from test.v1 order by f59 limit 0,10;

drop view if exists test.v1 ;
drop table t1;


let $message= Testcase 3.3.1.64 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.1.64: Ensure that all changes to a view definition, executed by
#                    the ALTER VIEW statement, are correctly recorded and have
#                    the correct effect on the data shown by the view.
###############################################################################
--disable_warnings
Drop view if exists test.v1 ;
--enable_warnings
CREATE VIEW test.v1 AS SELECT F59
FROM test.tb2 where test.tb2.F59 = 109;

SELECT * FROM test.v1 order by f59 limit 0,10;

ALTER VIEW test.v1 AS  SELECT *
FROM test.tb2 WHERE test.tb2.f59 = 242 ;
if ($have_bug_32285)
{
--disable_ps_protocol
}
--vertical_results
SELECT * FROM test.v1 order by f59 limit 0,10;
--horizontal_results
--enable_ps_protocol

Drop view test.v1 ;


let $message= Testcase 3.3.1.65, 3.3.1.A4, 3.3.1.66, 3.3.1.67 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.1.65: Ensure that the DROP VIEW statement cleanly drops its
#                    target view.
# Testcase 3.3.1.A4: Ensure that the DROP VIEW IF EXISTS statement cleanly
#                    drops its target view.
# Testcase 3.3.1.66: Ensure that DROP VIEW <view name> fails, with an appro-
#                    priate error message, if the view named does not exist.
# Testcase 3.3.1.67: Ensure that DROP VIEW IF EXISTS <view name> does not fail,
#                    but merely returns an appropriate warning, if the view
#                    named does not exist.
###############################################################################
--disable_warnings
DROP TABLE IF EXISTS t1;
DROP VIEW  IF EXISTS test.v1 ;
--enable_warnings
eval CREATE TABLE t1 ( f1 VARCHAR(1000) ) ENGINE = $engine_type ;
CREATE VIEW  v1 AS SELECT f1 FROM t1;

# DROP VIEW
DROP VIEW v1;
--error ER_BAD_TABLE_ERROR
DROP VIEW v1;

CREATE VIEW  v1 AS SELECT f1 FROM t1;
# DROP VIEW IF EXISTS
DROP VIEW IF EXISTS v1;
DROP VIEW IF EXISTS v1;


let $message= Testcase 3.3.1.68 ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.1.68: Ensure that DROP VIEW <view name>, DROP VIEW <view name>
#                    RESTRICT, and DROP VIEW <view name> CASCADE all take
#                    exactly the same action, until such time as the RESTRICT
#                    and CASCADE keyword actions are implemented by MySQL.
###############################################################################
--disable_warnings
DROP TABLE IF EXISTS t1;
DROP VIEW  IF EXISTS v1_base ;
DROP VIEW  IF EXISTS v1_top ;
--enable_warnings
CREATE TABLE t1 ( f1 DOUBLE);

--disable_query_log
SET @part1= '';
SET @part2= 'RESTRICT';
SET @part3= 'CASCADE';
--enable_query_log

let $num1= 3;
while ($num1)
{

  CREATE VIEW  v1_base AS SELECT * FROM t1;
  CREATE VIEW  v1_top AS SELECT * FROM v1_base;
  --disable_query_log
  let $aux1= `SELECT CONCAT('DROP VIEW v1_top ', @v1_part)` ;
  let $aux2= `SELECT CONCAT('DROP VIEW v1_base ', @v1_part)` ;
  eval SET @v1_part= @part$num1;
  --enable_query_log

  # 1. more non important sub testcase, where the view (v1_top) is not the base of
  #    another object
  # DROP VIEW v1_top < |RESTRICD|CASCADE> must be successful.
  eval $aux1 ;
  # Check, that v1_top really no more exists + cleanup for the second sub test
  --error ER_BAD_TABLE_ERROR
  DROP VIEW v1_top;

  CREATE VIEW  v1_top AS SELECT * FROM v1_base;
  # 2. more important sub testcase, where the view (v1_base) is the base of
  #    another object (v1_top)
  # DROP VIEW v1_base < |RESTRICT|CASCADE>
  # If the RESTRICT and CASCADE keyword actions are implemented by MySQL,
  # CASCADE  will remove v1_base and the dependend view v1_top and
  # RESTRICT will fail, because there exists the dependend view v1_top
  eval $aux2 ;
  # Check, if v1_base and v1_top exist + cleanup for next loop
  DROP VIEW v1_base;
  DROP VIEW v1_top;

  dec $num1;
}


let $message= Testcase 3.3.1.69, 3.3.1.70, 3.3.1.A5 ;
--source include/show_msg80.inc
###############################################################################
# Testcases : Ensure that, when a view is dropped, its definition no longer
#             appears when a
# 3.3.1.69    SHOW CREATE VIEW, SHOW CREATE TABLE, SHOW TABLE STATUS,
#             SHOW TABLE
# 3.3.1.70    CHECK TABLE statement is executed
# 3.3.1.A5    SHOW COLUMNS, SHOW FIELDS, DESCRIBE, EXPLAIN
#             statement is executed
###############################################################################
# Note(mleich): There will be no non failing sub testcases with SHOW here.
#               They will be done in 3.3.11 ff.
--disable_warnings
DROP TABLE IF EXISTS t1 ;
DROP VIEW  IF EXISTS v1 ;
--enable_warnings
eval CREATE TABLE t1 (f59 INT, f60 INT, f61 INT) ENGINE = $engine_type;
CREATE VIEW v1 AS SELECT * FROM t1;
DROP VIEW v1 ;

# The negative tests:
#     SELECT
--error ER_NO_SUCH_TABLE
SELECT * FROM v1 ;
#
--error ER_NO_SUCH_TABLE
SHOW CREATE VIEW v1 ;
--error ER_NO_SUCH_TABLE
SHOW CREATE TABLE v1 ;
# Attention: Like is a filter. So we will get an empty result set here.
SHOW TABLE STATUS like 'v1' ;
SHOW TABLES LIKE 'v1';
--error ER_NO_SUCH_TABLE
SHOW COLUMNS FROM v1;
--error ER_NO_SUCH_TABLE
SHOW FIELDS  FROM v1;
CHECK TABLE v1;
--error ER_NO_SUCH_TABLE
DESCRIBE v1;
--error ER_NO_SUCH_TABLE
EXPLAIN SELECT * FROM v1;

Use test;

let $message= Testcase 3.3.1.A6 ;
--source include/show_msg80.inc
###############################################################################
# Testcases 3.3.1.A6 : Ensure that nested views up to level @max_level work.
###############################################################################
# 1. Simple nested VIEWs
#       Configurable parameter @max_level = nesting level
#       128 must be good enough, it is already a pathologic value.
#       We currently set it to 32, because of performance issues.
--disable_query_log
SET @max_level= 32;
--enable_query_log
--disable_warnings
DROP DATABASE IF EXISTS test3;
--enable_warnings
CREATE DATABASE test3;
eval CREATE TABLE test3.t1 (f1 DECIMAL(5,3)) ENGINE = $engine_type;
INSERT INTO test3.t1 SET f1 = 1.0;
CREATE VIEW test3.v0 AS SELECT * FROM test3.t1;

let $level= 1;
let $run= 1;
while ($run)
{
   --disable_query_log
   eval SET @aux = $level - 1;
   --enable_query_log
   let $sublevel= `SELECT @aux`;

   eval CREATE VIEW test3.v$level AS SELECT * FROM test3.v$sublevel;

   # DEBUG Please set $debug to 1, if the statements on the toplevel VIEW
   #       (direct after the while loop) show suspicious results.
   let $debug= 0;
   if ($debug)
   {
      eval SHOW CREATE VIEW test3.v$level;
      eval SELECT * FROM test3.v$level;
      eval EXPLAIN SELECT * FROM test3.v$level;
   }

   --disable_query_log
   eval SET @aux = @max_level > $level;
   --enable_query_log
   inc $level;
   # DEBUG
   # eval SELECT @aux AS "@aux", $level AS "next level";

   let $run= `SELECT @aux`;
}
#--------------------------------------------------------------------------
# Attention: If the following statements get suspicious/unexpected results
#           and you assume that something with the non toplevel VIEWs might
#           be wrong, please edit the while loop above and set $debug to 1.
#--------------------------------------------------------------------------
# 1.1 Check of top level VIEW
let $toplevel= `SELECT @max_level`;
eval SHOW CREATE VIEW test3.v$toplevel;
eval SELECT * FROM test3.v$toplevel;
eval EXPLAIN SELECT * FROM test3.v$toplevel;

# 1.2 Check the top level view when a base VIEW is dropped
DROP VIEW test3.v0;
eval SHOW CREATE VIEW test3.v$toplevel;
--error ER_VIEW_INVALID
eval SELECT * FROM test3.v$toplevel;
--error ER_VIEW_INVALID
eval EXPLAIN SELECT * FROM test3.v$toplevel;


# 2. Complicated nested VIEWs
#       parameter @max_level = nesting level
#       There is a limit(@join_limit = 61) for the number of tables which
#       could be joined. This limit will be reached, when we set
#            @max_level = @join_limit - 1  .
--disable_query_log
#++++++++++++++++++++++++++++++++++++++++++++++
# OBN - Reduced the value of join limit to 30
#       Above seems to hang - FIXME
# mleich  - Reason unclear why it hangs for OBN on innodb and memory.
#           Hypothesis: Maybe the consumption of virtual memory is high
#                       and OBN's box performs excessive paging.
#                       (RAM: OBN ~384MB RAM, mleich 1 GB)
#++++++++++++++++++++++++++++++++++++++++++++++
let $message= FIXME - Setting join_limit to 28 - hangs for higher values;
--source include/show_msg.inc
# OBN - Reduced from 30 in 5.1.21 to avoid hitting the ndbcluster limit
#       of "ERROR HY000: Got temporary error 4006 'Connect failure
#           - out of connection objects (increase MaxNoOfConcurrentTransactions)'
#           from NDBCLUSTER " to early;
#SET @join_limit = 61;
SET @join_limit = 28; # OBN - see above
SET @max_level = @join_limit - 1;
--enable_query_log

--disable_warnings
DROP DATABASE IF EXISTS test3;
DROP TABLE IF EXISTS test1.t1;
DROP TABLE IF EXISTS test2.t1;
let $level= `SELECT @max_level + 1`;
while ($level)
{
   dec $level;

   eval DROP VIEW IF EXISTS test1.v$level;
}
--enable_warnings
CREATE DATABASE test3;

# Testplan for the content of the tables:
# ---------------------------------------------------------
# Records                        test1.t1 test2.t1 test3.t1
# NULL, 'numeric column is NULL'      yes      yes      yes
# 0   , NULL                          yes      yes      yes
# 5   , 'five'                        yes      yes      yes
# 1   , 'one'                         yes      yes       no
# 2   , 'two'                         yes       no      yes
# 3   , 'three'                        no      yes      yes

USE test1;
eval CREATE TABLE t1 (f1 BIGINT, f2 CHAR(50)) ENGINE = $engine_type ;
INSERT INTO t1 VALUES (NULL, 'numeric column is NULL');
INSERT INTO t1 VALUES (0, NULL);
INSERT INTO t1 VALUES (5, 'five');

INSERT INTO t1 VALUES (1, 'one');
INSERT INTO t1 VALUES (2, 'two');

USE test2;
eval CREATE TABLE t1 (f1 DECIMAL(64,30), f2 VARCHAR(50)) ENGINE = $engine_type;
INSERT INTO  t1 VALUES (NULL, 'numeric column is NULL');
INSERT INTO t1 VALUES (0.000000000000000000000000000000, NULL);
INSERT INTO t1 VALUES (5.000000000000000000000000000000, 'five');

INSERT INTO t1 VALUES (+1.000000000000000000000000000000, 'one');
INSERT INTO t1 VALUES (3.000000000000000, 'three');

USE test3;
eval CREATE TABLE t1 (f1 DOUBLE, f2 VARBINARY(50)) ENGINE = $engine_type;
INSERT INTO t1 VALUES (NULL, 'numeric column is NULL');
INSERT INTO t1 VALUES (+0.0E-35, NULL);
INSERT INTO t1 VALUES (+0.5E+1, 'five');

INSERT INTO t1 VALUES (20.0E-1, 'two');
INSERT INTO t1 VALUES (0.0300E2, 'three');

USE test;

CREATE OR REPLACE VIEW test1.v0 AS SELECT * FROM test2.t1;

--disable_query_log
SET @max_level = IFNULL(@limit1,@max_level);
--enable_query_log
let $level= 1;
let $run= 1;
while ($run)
{
   --disable_query_log
   eval SET @aux = $level - 1;
   let $sublevel= `SELECT @aux`;
   eval SET @AUX = $level MOD 3 + 1;
   let $dbnum= `SELECT @AUX`;
   --enable_query_log

   eval CREATE OR REPLACE VIEW test1.v$level AS SELECT f1, f2
               FROM test$dbnum.t1 tab1 NATURAL JOIN test1.v$sublevel tab2;

   # DEBUG Please set $debug to 1, if the statements on the toplevel VIEW
   #       (direct after the while loop) show suspicious results.
   let $debug= 0;
   if ($debug)
   {
      eval SHOW CREATE VIEW test1.v$level;
      eval SELECT * FROM test1.v$level;
      eval SELECT f1, f2
           FROM test$dbnum.t1 tab1 NATURAL JOIN test1.v$sublevel tab2;
      eval SELECT CAST(f1 AS SIGNED INTEGER) AS f1,
                  CAST(f2 AS CHAR) AS f2 FROM test1.v$level;
      eval EXPLAIN SELECT CAST(f1 AS SIGNED INTEGER) AS f1,
                 CAST(f2 AS CHAR) AS f2 FROM test1.v$level;
   }

   --disable_query_log
   eval SET @aux = @max_level > $level;
   --enable_query_log
   inc $level;
   # DEBUG
   # eval SELECT @aux AS "@aux", $level AS "next level";

   let $run= `SELECT @aux`;
}

#--------------------------------------------------------------------------
# Atention: If the following statements get suspicious/unexpected results
#           and you assume that something with the non toplevel VIEWs might
#           be wrong, please edit the while loop above and set $debug to 1.
#--------------------------------------------------------------------------
# 2.1 Check of top level VIEW
let $toplevel= `SELECT @max_level`;
# Show should be easy
eval SHOW CREATE VIEW test1.v$toplevel;
# SELECT is much more complicated
eval SELECT CAST(f1 AS SIGNED INTEGER) AS f1,
            CAST(f2 AS CHAR) AS f2 FROM test1.v$toplevel;
let $message= The output of following EXPLAIN is deactivated, because the result
              differs on some platforms
              FIXME Is this a bug ? ;
--source include/show_msg80.inc
if (1)
{
--disable_result_log
}
# EXPLAIN might be the hell
eval EXPLAIN SELECT CAST(f1 AS SIGNED INTEGER) AS f1,
            CAST(f2 AS CHAR) AS f2 FROM test1.v$toplevel;
if (1)
{
--enable_result_log
}

# 2.2 Check of top level VIEW when join limit is exceeded
# Exceed the limit for the number of tables which could be joined.
let $level= `SELECT @max_level + 1`;
let $sublevel= `SELECT @max_level`;
eval CREATE VIEW test1.v$level AS SELECT f1, f2
            FROM test3.t1 tab1 NATURAL JOIN test1.v$sublevel tab2;
eval SHOW CREATE VIEW test1.v$level;
# the following line as written as '--eror ER_TOO_MANY_TABLES' and the command
# is successful so assuming no expected error was intended
# --error ER_TOO_MANY_TABLES
eval SELECT CAST(f1 AS SIGNED INTEGER) AS f1,
            CAST(f2 AS CHAR) AS f2 FROM test1.v$level;
let $message= The output of following EXPLAIN is deactivated, because the result
              differs on some platforms
              FIXME Is this a bug ? ;
--source include/show_msg80.inc
if (1)
{
--disable_result_log
}
# the following line as written as '--eror ER_TOO_MANY_TABLES' and the command
# is successful so assuming no expected error was intended
# --error ER_TOO_MANY_TABLES
eval EXPLAIN SELECT CAST(f1 AS SIGNED INTEGER) AS f1,
                CAST(f2 AS CHAR) AS f2 FROM test1.v$level;
if (1)
{
--enable_result_log
}
eval DROP VIEW IF EXISTS test1.v$level;

# 2.3 Create a logical wrong (data type "garbage") base for the upper views
#     and check the behaviour of the top level view.
# 2.3.1 Exchange numeric and string column
--disable_result_log
CREATE OR REPLACE VIEW test1.v0 AS
SELECT f1 as f2, f2 as f1 FROM test2.t1;
# 2.3.2 DATE instead of numeric
CREATE OR REPLACE VIEW test2.v0 AS
SELECT CAST('0001-01-01' AS DATE) as f1, f2 FROM test3.t1;
eval SHOW CREATE VIEW test1.v$toplevel;
eval SELECT CAST(f1 AS SIGNED INTEGER) AS f1,
            CAST(f2 AS CHAR) AS f2 FROM test1.v$toplevel;
eval EXPLAIN SELECT CAST(f1 AS SIGNED INTEGER) AS f1,
                   CAST(f2 AS CHAR) AS f2 FROM test1.v$toplevel;
# 2.3.3 UCS2 string instead of common string
CREATE OR REPLACE VIEW test3.v0 AS
SELECT f1 , CONVERT('ßÄäÖöÜü§' USING UCS2) as f2 FROM test1.t1;
eval SHOW CREATE VIEW test1.v$toplevel;
eval SELECT CAST(f1 AS SIGNED INTEGER) AS f1,
            CAST(f2 AS CHAR) AS f2 FROM test1.v$toplevel;
eval EXPLAIN SELECT CAST(f1 AS SIGNED INTEGER) AS f1,
                   CAST(f2 AS CHAR) AS f2 FROM test1.v$toplevel;

# 2.3.4 UCS2 string instead of numeric
CREATE OR REPLACE VIEW test3.v0 AS
SELECT CONVERT('ßÄäÖöÜü§' USING UCS2) as f1, f2 FROM test1.t1;
eval SHOW CREATE VIEW test1.v$toplevel;
eval SELECT CAST(f1 AS SIGNED INTEGER) AS f1,
            CAST(f2 AS CHAR) AS f2 FROM test1.v$toplevel;
eval EXPLAIN SELECT CAST(f1 AS SIGNED INTEGER) AS f1,
                   CAST(f2 AS CHAR) AS f2 FROM test1.v$toplevel;
--enable_result_log

# Cleanup
let $level= `SELECT @max_level + 1`;
while ($level)
{
   dec $level;

eval DROP VIEW IF EXISTS test1.v$level;

}
DROP DATABASE test3;
DROP TABLE test1.t1;
DROP TABLE test2.t1;

#==============================================================================
# 3.3.2 Updatable and Insertable-into views:
#==============================================================================
Use test;

let $message= Testcase 3.3.2.1;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.2.1: Ensure that every view which may theoretically accept new
#                   rows via the INSERT statement does, in fact, do so.
###############################################################################
--disable_warnings
Drop view if exists test.v1 ;
--enable_warnings

Create View test.v1 AS SELECT f59,f60 FROM tb2 where f59 = 1995 ;
--enable_info
INSERT INTO test.v1 (f59,f60) values (879,700) ;
--disable_info
SELECT f59,f60 FROM test.v1 where f59 = 879 and f60 = 700 ;
DELETE FROM tb2 where f59 = 879 and f60 = 700 ;

Drop view test.v1 ;


let $message= Testcase 3.3.2.2;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.2.2:  Ensure that, for every row inserted into a view,
#                    the correct new data also appears in every relevant
#                    underlying table.
###############################################################################
--disable_warnings
Drop view if exists test.v1 ;
--enable_warnings

Create view test.v1 AS SELECT f59,f60,f61 FROM tb2  ;
--enable_info
INSERT INTO test.v1 ( f59 , f60 ) values (2005,0101) ;
--disable_info

if ($have_bug_32285)
{
--disable_ps_protocol
}
--vertical_results
SELECT * FROM tb2 where f59 = 2005 and f60 = 0101 ;
--horizontal_results
--enable_ps_protocol
SELECT f59,f60 FROM test.v1 where f59 = 2005 and f60 = 0101 ;
DELETE FROM tb2 where f59 = 2005 and f60 = 0101 ;

Drop view test.v1 ;


let $message= Testcase 3.3.2.3;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.2.3: Ensure that every view which may theoretically accept data
#                   changes via the UPDATE statement does, in fact, do so.
###############################################################################
Insert into tb2 (f59,f60,f61) values (780,105,106) ;

--disable_warnings
Drop view if exists test.v1 ;
--enable_warnings

CREATE VIEW test.v1 AS SELECT f59,f60,f61 FROM tb2  ;
--enable_info
UPDATE test.v1 SET f59 = 8 WHERE f59 = 780 and f60 = 105;
--disable_info

if ($have_bug_32285)
{
--disable_ps_protocol
}
--vertical_results
SELECT * FROM tb2 where f59 = 8 and f60 = 105;
--horizontal_results
--enable_ps_protocol
SELECT f59,f60 FROM test.v1 where f59 = 8 and f60 = 105 ;

Drop view test.v1 ;


let $message= Testcase 3.3.2.4;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.2.4:  Ensure that, for data values updated within a view, the
#                    correct new data also appears in every relevant
#                    underlying table.
###############################################################################
Insert into tb2 (f59,f60,f61) values (781,105,106) ;

--disable_warnings
Drop view if exists test.v1 ;
--enable_warnings
CREATE VIEW test.v1 AS SELECT f59,f60,f61 FROM tb2 ;
--enable_info
UPDATE test.v1 SET f59 = 891 WHERE f60 = 105 ;
--disable_info

if ($have_bug_32285)
{
--disable_ps_protocol
}
--vertical_results
SELECT * FROM tb2 where f59 = 891 and f60 = 105;
--horizontal_results
--enable_ps_protocol
SELECT f59,f60 FROM test.v1 where f59 = 891 and f60 = 105 ;

Drop view test.v1 ;


let $message= Testcase 3.3.2.5;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.2.5:  Ensure that every view which may theoretically accept data
#                    deletions via the DELETE statement does, in fact, do so.
###############################################################################
Insert into tb2 (f59,f60,f61) values (789,105,106) ;

--disable_warnings
Drop view if exists test.v1 ;
--enable_warnings
CREATE VIEW test.v1 AS SELECT f59,f60,f61 FROM tb2 where f59 = 789 ;
--enable_info
DELETE FROM test.v1 where f59 = 789 ;
--disable_info
SELECT * FROM tb2 where f59 = 789 ;
SELECT f59,f60 FROM test.v1 where f59 = 789 order by f60 ;

Drop view test.v1 ;


let $message= Testcase 3.3.2.6;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.2.6: Ensure that, for data rows deleted from a view, the correct
#                   rows have also been deleted from every relevant
#                   underlying table.
###############################################################################
Insert into tb2 (f59,f60,f61) values (711,105,106) ;

--disable_warnings
Drop view if exists test.v1 ;
--enable_warnings
CREATE VIEW test.v1 AS SELECT f59,f60,f61 FROM tb2 where f59 = 711  ;
--enable_info
DELETE FROM test.v1 where f59 = 711 ;
--disable_info

SELECT * FROM tb2 where f59 = 711 ;
SELECT f59,f60 FROM test.v1 where f59 = 711 order by f60 ;

Drop view test.v1 ;

let $message= Testcase 3.3.2.1 - 3.3.2.6 alternative implementation;
--source include/show_msg80.inc

--disable_warnings
DROP TABLE IF EXISTS t1;
DROP VIEW  IF EXISTS v1;
--enable_warnings

CREATE TABLE t1 ( f1 BIGINT, f2 CHAR(20), f3 NUMERIC(7,4),
                  f4 CHAR, PRIMARY KEY(f1));

# VIEW including the base table PRIMARY KEY, but not all base table columns (f4)
#      no additional columns
CREATE VIEW v1 AS SELECT f1, f2, f3 FROM t1;

# Incomplete INSERT 1
# - f2 missing
# - PRIMARY KEY f1 included
#           f2 gets the default NULL
INSERT INTO v1 SET f1 = 1;
SELECT * from t1;
DELETE FROM t1;

# Incomplete INSERT 2
# - f2 included
# - PRIMARY KEY f1 missing
#           f1 gets the default 0, because we are in the native sql_mode
INSERT INTO v1 SET f2 = 'ABC';
#           f1 gets the default 0, but this value is already exists
# OBN change for 5.1.21 --error ER_DUP_ENTRY_WITH_KEY_NAME
--error ER_DUP_ENTRY
INSERT INTO v1 SET f2 = 'ABC';
SELECT * from t1;
DELETE FROM t1;

# Testplan for DELETE:
#
# Column within WHERE qualification
# f1 (PK)
# f2 (non PK)
# none
#
INSERT INTO t1 VALUES(1, 'ABC', -1.2E-3, 'X');
# DELETE f1
DELETE FROM v1 WHERE f1 = 1;
SELECT * from t1;
INSERT INTO t1 VALUES(1, 'ABC', -1.2E-3, 'X');
# DELETE f2
DELETE FROM v1 WHERE f2 = 'ABC';
SELECT * from t1;
INSERT INTO t1 VALUES(1, 'ABC', -1.2E-3, 'X');
# DELETE none
DELETE FROM v1;
SELECT * from t1;

# Testplan for UPDATE:
# Column to modify         Column within WHERE qualification
# f1 (PK)                  f1(PK + same column to modify)
# f1 (PK)                  f2
# f1 (PK)                  none
# f2 (non PK)              f1(PK)
# f2 (non PK)              f2(non PK + same column to modify)
# f2 (non PK)              f3(non PK)
# f2 (non PK)              none
# f1,f2                    f1,f2
#
INSERT INTO t1 VALUES(1, 'ABC', -1.2E-3, 'X');
# UPDATE f1 - f1
UPDATE v1 SET f1 = 2 WHERE f1 = 1;
SELECT * from t1;
DELETE FROM t1;
INSERT INTO t1 VALUES(1, 'ABC', -1.2E-3, 'X');
# UPDATE f1 - f2
UPDATE v1 SET f1 = 2 WHERE f2 = 'ABC';
SELECT * from t1;
DELETE FROM t1;
INSERT INTO t1 VALUES(1, 'ABC', -1.2E-3, 'X');
# UPDATE f1 - none
UPDATE v1 SET f1 = 2;
SELECT * from t1;
DELETE FROM t1;
INSERT INTO t1 VALUES(1, 'ABC', -1.2E-3, 'X');
# UPDATE f2 - f1
UPDATE v1 SET f2 = 'NNN' WHERE f1 = 1;
SELECT * from t1;
DELETE FROM t1;
INSERT INTO t1 VALUES(1, 'ABC', -1.2E-3, 'X');
# UPDATE f2 - f2
UPDATE v1 SET f2 = 'NNN' WHERE f2 = 'ABC';
SELECT * from t1;
DELETE FROM t1;
INSERT INTO t1 VALUES(1, 'ABC', -1.2E-3, 'X');
# UPDATE f2 - f3
UPDATE v1 SET f2 = 'NNN' WHERE f3 = -1.2E-3;
SELECT * from t1;
DELETE FROM t1;
INSERT INTO t1 VALUES(1, 'ABC', -1.2E-3, 'X');
# UPDATE f2 - none
UPDATE v1 SET f2 = 'NNN';
SELECT * from t1;
DELETE FROM t1;
INSERT INTO t1 VALUES(1, 'ABC', -1.2E-3, 'X');
# UPDATE f1,f2 - f1,f2
UPDATE v1 SET f1 = 2, f2 = 'NNN' WHERE f1 = 1 AND f2 = 'ABC';
SELECT * from t1;
DELETE FROM t1;


DROP VIEW v1;
# VIEW without the PRIMARY KEY f1 of the base table
#      no additional columns
CREATE VIEW v1 AS SELECT f2, f3 FROM t1;

# INSERT
# - PRIMARY KEY f1 missing in VIEW definition
#           f1 gets the default 0, because we are in the native sql_mode
INSERT INTO v1 SET f2 = 'ABC';
#           f1 gets the default 0 and this value is already exists
# OBN change for 5.1.21 --error ER_DUP_ENTRY_WITH_KEY_NAME
--error ER_DUP_ENTRY
INSERT INTO v1 SET f2 = 'ABC';
SELECT * from t1;
DELETE FROM t1;

# Testplan for DELETE:
#
# Column within WHERE qualification
# f2 (non PK)
# none
#
INSERT INTO t1 VALUES(1, 'ABC', -1.2E-3, 'X');
# DELETE f2
DELETE FROM v1 WHERE f2 = 'ABC';
SELECT * from t1;
INSERT INTO t1 VALUES(1, 'ABC', -1.2E-3, 'X');
# DELETE none
DELETE FROM v1;
SELECT * from t1;

# Testplan for UPDATE:
#
# Column to modify         Column within WHERE qualification
# f2 (non PK)              f2(non PK + same column to modify)
# f2 (non PK)              f3(non PK)
# f2 (non PK)              none
INSERT INTO t1 VALUES(1, 'ABC', -1.2E-3, 'X');
# UPDATE f2 - f2
UPDATE v1 SET f2 = 'NNN' WHERE f2 = 'ABC';
SELECT * from t1;
DELETE FROM t1;
INSERT INTO t1 VALUES(1, 'ABC', -1.2E-3, 'X');
# UPDATE f2 - f3
UPDATE v1 SET f2 = 'NNN' WHERE f3 = -1.2E-3;
SELECT * from t1;
DELETE FROM t1;
INSERT INTO t1 VALUES(1, 'ABC', -1.2E-3, 'X');
# UPDATE f2 - none
UPDATE v1 SET f2 = 'NNN';
SELECT * from t1;
DELETE FROM t1;


DROP VIEW v1;
# VIEW with the PRIMARY KEY f1 of the base table
#      but additional constant column
CREATE VIEW v1 AS SELECT f1, f2, f3, 'HELLO' AS my_greeting FROM t1;

# Maybe the SQL standard allows the following INSERT.
# But it would be a very sophisticated DBMS.
--error ER_NON_INSERTABLE_TABLE
INSERT INTO v1 SET f1 = 1;
SELECT * from t1;
DELETE FROM t1;
# The next INSERTs should never work, because my_greeting is a constant.
--error ER_NON_INSERTABLE_TABLE
INSERT INTO v1 SET f1 = 1, my_greeting = 'HELLO';
SELECT * from t1;
DELETE FROM t1;

# Testplan for DELETE:
#
# Column within WHERE qualification
# f1 (PK)
# f2 (non PK)
# my_greeting(non base table column)
# none
#
INSERT INTO t1 VALUES(1, 'ABC', -1.2E-3, 'X');
# DELETE f1
DELETE FROM v1 WHERE f1 = 1;
SELECT * from t1;
INSERT INTO t1 VALUES(1, 'ABC', -1.2E-3, 'X');
# DELETE f2
DELETE FROM v1 WHERE f2 = 'ABC';
SELECT * from t1;
INSERT INTO t1 VALUES(1, 'ABC', -1.2E-3, 'X');
# DELETE my_greeting
DELETE FROM v1 WHERE my_greeting = 'HELLO';
SELECT * from t1;
INSERT INTO t1 VALUES(1, 'ABC', -1.2E-3, 'X');
# DELETE none
DELETE FROM v1;
SELECT * from t1;


# Testplan for UPDATE:
#
# Column to modify                    Column within WHERE qualification
# f1 (PK)                             f1(PK + same column to modify)
# f1 (PK)                             f2
# f1 (PK)                             my_greeting(non base table column)
# f1 (PK)                             none
# f2 (non PK)                         f1(PK)
# f2 (non PK)                         f2(non PK + same column to modify)
# f2 (non PK)                         f3(non PK)
# f2 (non PK)                         my_greeting(non base table column)
# f2 (non PK)                         none
# my_greeting(non base table column)  f1(PK)
# my_greeting(non base table column)  f2(non PK)
# my_greeting(non base table column)  my_greeting(same non base table column)
# my_greeting(non base table column)  none
# f1,f2                               f1,f2
#
INSERT INTO t1 VALUES(1, 'ABC', -1.2E-3, 'X');
# UPDATE f1 - f1
UPDATE v1 SET f1 = 2 WHERE f1 = 1;
SELECT * from t1;
DELETE FROM t1;
INSERT INTO t1 VALUES(1, 'ABC', -1.2E-3, 'X');
# UPDATE f1 - f2
UPDATE v1 SET f1 = 2 WHERE f2 = 'ABC';
SELECT * from t1;
DELETE FROM t1;
INSERT INTO t1 VALUES(1, 'ABC', -1.2E-3, 'X');
# UPDATE f1 - my_greeting
UPDATE v1 SET f1 = 2 WHERE my_greeting = 'HELLO';
SELECT * from t1;
DELETE FROM t1;
INSERT INTO t1 VALUES(1, 'ABC', -1.2E-3, 'X');
# UPDATE f1 - none
UPDATE v1 SET f1 = 2;
SELECT * from t1;
DELETE FROM t1;
#------------------------------------------------
INSERT INTO t1 VALUES(1, 'ABC', -1.2E-3, 'X');
# UPDATE f2 - f1
UPDATE v1 SET f2 = 'NNN' WHERE f1 = 1;
SELECT * from t1;
DELETE FROM t1;
INSERT INTO t1 VALUES(1, 'ABC', -1.2E-3, 'X');
# UPDATE f2 - f2
UPDATE v1 SET f2 = 'NNN' WHERE f2 = 'ABC';
SELECT * from t1;
DELETE FROM t1;
INSERT INTO t1 VALUES(1, 'ABC', -1.2E-3, 'X');
# UPDATE f2 - f3
UPDATE v1 SET f2 = 'NNN' WHERE f3 = -1.2E-3;
SELECT * from t1;
DELETE FROM t1;
INSERT INTO t1 VALUES(1, 'ABC', -1.2E-3, 'X');
# UPDATE f2 - my_greeting
UPDATE v1 SET f2 = 'NNN' WHERE my_greeting = 'HELLO';
SELECT * from t1;
DELETE FROM t1;
INSERT INTO t1 VALUES(1, 'ABC', -1.2E-3, 'X');
# UPDATE f2 - none
UPDATE v1 SET f2 = 'NNN';
SELECT * from t1;
DELETE FROM t1;
#------------------------------------------------
INSERT INTO t1 VALUES(1, 'ABC', -1.2E-3, 'X');
# UPDATE my_greeting - f1
--error ER_NONUPDATEABLE_COLUMN
UPDATE v1 SET my_greeting = 'Hej' WHERE f1 = 1;
SELECT * from t1;
DELETE FROM t1;
INSERT INTO t1 VALUES(1, 'ABC', -1.2E-3, 'X');
# UPDATE my_greeting - f2
--error ER_NONUPDATEABLE_COLUMN
UPDATE v1 SET my_greeting = 'Hej' WHERE f2 = 'ABC';
SELECT * from t1;
DELETE FROM t1;
INSERT INTO t1 VALUES(1, 'ABC', -1.2E-3, 'X');
# UPDATE my_greeting - my_greeting
--error ER_NONUPDATEABLE_COLUMN
UPDATE v1 SET my_greeting = 'Hej' WHERE my_greeting = 'HELLO';
SELECT * from t1;
DELETE FROM t1;
INSERT INTO t1 VALUES(1, 'ABC', -1.2E-3, 'X');
# UPDATE my_greeting - none
--error ER_NONUPDATEABLE_COLUMN
UPDATE v1 SET my_greeting = 'Hej';
SELECT * from t1;
DELETE FROM t1;
#------------------------------------------------
INSERT INTO t1 VALUES(1, 'ABC', -1.2E-3, 'X');
# UPDATE f1, f2 - f1, f2
UPDATE v1 SET f1 = 2, f2 = 'NNN' WHERE f1 = 1 AND f2 = 'ABC';
SELECT * from t1;
DELETE FROM t1;


DROP TABLE t1;
SET sql_mode = 'traditional';
CREATE TABLE t1 ( f1 BIGINT, f2 CHAR(20), f3 NUMERIC(7,4) NOT NULL,
                  f4 CHAR, PRIMARY KEY(f1));
# VIEW including the base table PRIMARY KEY, but not the NOT NULL
#      base table column (f3)
#      no additional columns
DROP VIEW v1;
CREATE VIEW v1 AS SELECT f1, f2, f4 FROM t1;

# This INSERT must fail
--error ER_NO_DEFAULT_FOR_VIEW_FIELD
INSERT INTO v1 SET f1 = 1;
SELECT * from t1;
DELETE FROM t1;

INSERT INTO t1 VALUES(1, 'ABC', -1.2E-3, 'X');
# DELETE f1
DELETE FROM v1 WHERE f1 = 1;

INSERT INTO t1 VALUES(1, 'ABC', -1.2E-3, 'X');
# UPDATE f1 - f2
UPDATE v1 SET f4 = 'Y' WHERE f2 = 'ABC';
SELECT * from t1;
DELETE FROM t1;

# Switch back to the native SQL mode
SET sql_mode = '';



let $message= Testcases 3.3.2.7 - 3.3.2.9,
              3.3.2.10 - 3.3.2.11 omitted because of missing
              features EXCEPT and INTERSECT ;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.2.7: Ensure that a view with a definition that includes
#                               UNION
#                   rejects all INSERT, UPDATE, or DELETE attempts
#                   with an appropriate error message.
# Testcase 3.3.2.8: Ensure that a view with a definition that includes
#                               UNION DISTINCT
#                   rejects all INSERT, UPDATE, or DELETE attempts
#                   with an appropriate error message.
# Testcase 3.3.2.9: Ensure that a view with a definition that includes
#                               UNION ALL
#                   rejects all INSERT, UPDATE, or DELETE attempts
#                   with an appropriate error message.
# Testcase 3.3.2.10: Ensure that a view with a definition that includes
#                               EXCEPT
#                    rejects all INSERT, UPDATE, or DELETE attempts
#                    with an appropriate error message.
#                    (Note: MySQL does not support EXCEPT at this time;
#                    this test is for the future.)
# Testcase 3.3.2.11: Ensure that a view with a definition that includes
#                               INTERSECT
#                     rejects all INSERT, UPDATE, or DELETE attempts
#                     with an appropriate error message.
#                     (Note: MySQL does not support INTERSECT at this time;
#                     this test is for the future.)
#
# Summary of 3.3.2.7 - 3.3.2.11
#        Ensure that a view with a definition that includes
#        UNION or UNION DISTINCT or UNION ALL or EXCEPT or INTERSECT
#        rejects any INSERT or UPDATE or DELETE statement with an
#        appropriate error message
#
# mleich: I assume the type of the storage engine does not play any role.
###############################################################################
INSERT INTO tb2 (f59,f60,f61) VALUES (77,185,126) ;
INSERT INTO tb2 (f59,f60,f61) VALUES (59,58,54) ;

--disable_warnings
DROP TABLE IF EXISTS t1 ;
DROP VIEW  IF EXISTS v1 ;
--enable_warnings
CREATE TABLE t1 (f59 INT, f60 INT, f61 INT) ;
INSERT INTO t1 VALUES (19,41,32)  ;
INSERT INTO t1 VALUES (59,54,71)  ;
INSERT INTO t1 VALUES (21,91,99)  ;

SET @variant1 = 'UNION ';
SET @variant2 = 'UNION ALL ';
SET @variant3 = 'UNION DISTINCT ';
SET @variant4 = 'EXCEPT ';
SET @variant5 = 'INTERSECT ';

# Attention: Set $num to 5, when EXCEPT and INTERSECT is supported
let $num= 3;
while ($num)
{
   --disable_query_log
   eval SET @variant= @variant$num;
   let $aux= `SELECT CONCAT('CREATE VIEW v1 AS ',
                     'SELECT f61 FROM tb2 WHERE f59=59 ',
                     @variant,
                     'SELECT f61 FROM t1 WHERE f59=19')`;
   --enable_query_log
   # $aux contains the CREATE VIEW statement
   eval $aux;
   --error ER_NON_INSERTABLE_TABLE
   INSERT INTO v1 VALUES (3000);
   --error ER_NON_UPDATABLE_TABLE
   UPDATE v1 SET f61 = 100 WHERE f61 = 32;
   --error ER_NON_UPDATABLE_TABLE
   DELETE FROM v1;
   DROP VIEW v1 ;

   dec $num;
}


let $message= Testcases 3.3.2.12 - 3.3.2.20;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.2.12: Ensure that a view with a definition that includes
#                    DISTINCT
#                    rejects all INSERT, UPDATE, or DELETE attempts
#                    with an appropriate error message.
# Testcase 3.3.2.13: Ensure that a view with a definition that includes
#                    DISTINCTROW
#                    rejects all INSERT, UPDATE, or DELETE attempts
#                    with an appropriate error message.
# Testcase 3.3.2.14: Ensure that a view with a definition that includes
#                    a set function
#                    rejects all INSERT, UPDATE, or DELETE attempts
#                    with an appropriate error message.
# Testcase 3.3.2.15: Ensure that a view with a definition that includes
#                    GROUP BY
#                    rejects all INSERT, UPDATE, or DELETE attempts
#                    with an appropriate error message.
# Testcase 3.3.2.16: Ensure that a view with a definition that includes
#                    HAVING
#                    rejects all INSERT, UPDATE, or DELETE attempts
#                    with an appropriate error message.
# Testcase 3.3.2.17: Ensure that a view with a definition that includes
#                    a subquery in the select list
#                    rejects all INSERT, UPDATE, or DELETE attempts
#                    with an appropriate error message.
# Testcase 3.3.2.18: Ensure that a view with a definition that includes
#                    a reference to a non-updatable view
#                    rejects all INSERT, UPDATE, or DELETE attempts
#                    with an appropriate error message.
# Testcase 3.3.2.19: Ensure that a view with a definition that includes
#                    a WHERE clause subquery that refers to a table also
#                    referenced in a FROM clause
#                    rejects all INSERT, UPDATE, or DELETE attempts
#                    with an appropriate error message.
# Testcase 3.3.2.20: Ensure that a view with a definition that includes
#                    ALGORITHM = TEMPTABLE
#                    rejects all INSERT, UPDATE, or DELETE attempts
#                    with an appropriate error message.
#
# Summary of 3.3.2.12 - 3.3.2.20:
# Ensure that a view with a definition that includes
#        DISTINCT                       3.3.2.12
#        DISTINCTROW                    3.3.2.13
#        SET                            3.3.2.14
#        GROUP BY                       3.3.2.15
#        HAVING                         3.3.2.16
#        a sub query in the select list 3.3.2.17
#        a reference to a non-updateable view 3.3.2.18
#        a WHERE clause sub query that refers to a table also referenced in a
#                FROM clause            3.3.2.19
#        ALGORITHM = TEMPTABLE          3.3.2.20
# rejects
#        any INSERT or UPDATE or DELETE statement
# with an appropriate error message.
#
###############################################################################
--disable_warnings
DROP TABLE IF EXISTS t1, t2 ;
DROP VIEW  IF EXISTS test.v1 ;
Drop view if exists v2 ;
--enable_warnings

CREATE TABLE t1 (f59 int, f60 int, f61 int) ;
INSERT INTO t1 VALUES (19,41,32)  ;
INSERT INTO t1 VALUES (59,54,71)  ;
INSERT INTO t1 VALUES (21,91,99)  ;
CREATE TABLE t2 (f59 int, f60 int, f61 int) ;
INSERT INTO t2 VALUES (19,41,32)  ;
INSERT INTO t2 VALUES (59,54,71)  ;
INSERT INTO t2 VALUES (21,91,99)  ;
CREATE VIEW v2 AS SELECT f59, f60, f61 FROM t2 LIMIT 5;

# For DISTINCT                       3.3.2.12
SET @variant1= 'CREATE VIEW v1 AS SELECT DISTINCT(f61)   FROM t1';
# For DISTINCTROW                    3.3.2.13
SET @variant2= 'CREATE VIEW v1 AS SELECT DISTINCTROW(f61) FROM t1';
# For SET                            3.3.2.14
SET @variant3= 'CREATE VIEW v1 AS SELECT SUM(f59) AS f61 FROM t1';
# For GROUP BY                       3.3.2.15
SET @variant4= 'CREATE VIEW v1 AS SELECT f61 FROM t1 GROUP BY f61';
# For HAVING                         3.3.2.16
SET @variant5= 'CREATE VIEW v1 AS SELECT f61 FROM t1 HAVING f61 > 0';
# For a sub query in the select list 3.3.2.17
SET @variant6= 'CREATE VIEW v1 AS SELECT (SELECT f60 FROM t2 WHERE f59=19) AS f61 FROM t1';
# For a WHERE clause sub query that refers to a table also referenced in a
# FROM clause 3.3.2.18
SET @variant7= 'CREATE VIEW v1 AS SELECT f61 FROM v2';
SET @variant8= 'CREATE VIEW v1 AS SELECT f59 AS f61 FROM t1 WHERE f60 IN (SELECT f59 FROM t1)';
# For ALGORITHM = TEMPTABLE          3.3.2.20
SET @variant9= 'CREATE ALGORITHM = TEMPTABLE VIEW v1 (f61) AS select f60 from t1';

let $num= 9;
while ($num)
{
   --disable_abort_on_error
   --disable_query_log
   eval SET @variant= @variant$num;
   let $aux= `SELECT @variant`;
   --enable_query_log

   # CREATE VIEW v1 ...
   eval $aux;

   --error ER_NON_INSERTABLE_TABLE
   INSERT INTO v1 VALUES (1002);
   # --error ER_NON_UPDATABLE_TABLE, ER_UPDATE_TABLE_USED
   --error ER_NON_UPDATABLE_TABLE
   UPDATE v1 SET f61=1007;
   --error ER_NON_UPDATABLE_TABLE
   DELETE FROM v1;
   DROP VIEW v1;
   dec $num;
}

Drop TABLE t1, t2 ;
Drop VIEW  v2 ;


let $message= Testcases 3.3.A1;
--source include/show_msg80.inc
###############################################################################
# Testcase 3.3.A1: Check the effects of base table modifications on an already
#                  existing VIEW
#
# Attention: Many modifications are logical non sense.
#            The consequence is in many cases a "garbage in garbage out" effect.
#
#            There is no specification of the intended behaviour within
#            the MySQL manual. That means I assume the observed effects are
#            no bug as long we do not get a crash or obviously non
#            reasonable results.
###############################################################################
--disable_warnings
DROP TABLE IF EXISTS t1;
DROP TABLE IF EXISTS t2;
DROP VIEW  IF EXISTS v1;
DROP VIEW  IF EXISTS v2;
--enable_warnings

eval CREATE TABLE t1 (f1 BIGINT, f2 DATE DEFAULT NULL, f4 CHAR(5),
                      report char(10)) ENGINE = $engine_type;
CREATE VIEW v1 AS SELECT * FROM t1;
INSERT INTO t1 SET f1 = -1, f4 = 'ABC', report = 't1 0';
INSERT INTO v1 SET f1 = -1, f4 = 'ABC', report = 'v1 0';

# 0. Initial state
DESCRIBE t1;
DESCRIBE v1;
SELECT * FROM t1 order by f1, report;
SELECT * FROM v1 order by f1, report;
#
# 1. Name of one base table column is altered
ALTER TABLE t1 CHANGE COLUMN f4 f4x CHAR(5);
INSERT INTO t1 SET f1 = 0, f4x = 'ABC', report = 't1 1';
--error ER_VIEW_INVALID
INSERT INTO v1 SET f1 = 0, f4  = 'ABC', report = 'v1 1';
--error ER_BAD_FIELD_ERROR
INSERT INTO v1 SET f1 = 0, f4x = 'ABC', report = 'v1 1a';
--error ER_VIEW_INVALID
INSERT INTO v1 SET f1 = 0, report = 'v1 1b';
DESCRIBE t1;
# Bug#12533 crash on DESCRIBE <view> after renaming base table column;
--error ER_VIEW_INVALID
DESCRIBE v1;
SELECT * FROM t1 order by f1, report;
--error ER_VIEW_INVALID
SELECT * FROM v1 order by f1, report;
ALTER TABLE t1 CHANGE COLUMN f4x f4 CHAR(5);
#
# 2. Length of one base table column is increased
ALTER TABLE t1 CHANGE COLUMN f4 f4 CHAR(10);
INSERT INTO t1 SET f1 = 2, f4 = '<-- 10 -->', report = 't1 2';
INSERT INTO v1 SET f1 = 2, f4 = '<-- 10 -->', report = 'v1 2';
DESCRIBE t1;
DESCRIBE v1;
SELECT * FROM t1 order by f1, report;
SELECT * FROM v1 order by f1, report;
#
# 3. Length of one base table column is reduced
ALTER TABLE t1 CHANGE COLUMN f4 f4 CHAR(8);
INSERT INTO t1 SET f1 = 3, f4 = '<-- 10 -->', report = 't1 3';
INSERT INTO v1 SET f1 = 3, f4 = '<-- 10 -->', report = 'v1 3';
DESCRIBE t1;
DESCRIBE v1;
SELECT * FROM t1 order by f1, report;
SELECT * FROM v1 order by f1, report;
#
# 4. Type of one base table column is altered   string -> string
ALTER TABLE t1 CHANGE COLUMN f4 f4 VARCHAR(20);
INSERT INTO t1 SET f1 = 4, f4 = '<------ 20 -------->', report = 't1 4';
INSERT INTO v1 SET f1 = 4, f4 = '<------ 20 -------->', report = 'v1 4';
DESCRIBE t1;
DESCRIBE v1;
SELECT * FROM t1 order by f1, report;
SELECT * FROM v1 order by f1, report;
#
# 5. Type of one base table column altered   numeric -> string
ALTER TABLE t1 CHANGE COLUMN f1 f1 VARCHAR(30);
INSERT INTO t1 SET f1 = '<------------- 30 ----------->',
                   f4 = '<------ 20 -------->', report = 't1 5';
INSERT INTO v1 SET f1 = '<------------- 30 ----------->',
                   f4 = '<------ 20 -------->', report = 'v1 5';
DESCRIBE t1;
DESCRIBE v1;
SELECT * FROM t1 order by f1, report;
SELECT * FROM v1 order by f1, report;
#
# 6. DROP of one base table column
ALTER TABLE t1 DROP COLUMN f2;
INSERT INTO t1 SET f1 = 'ABC', f4 = '<------ 20 -------->', report = 't1 6';
--error ER_VIEW_INVALID
INSERT INTO v1 SET f1 = 'ABC', f4 = '<------ 20 -------->', report = 'v1 6';
DESCRIBE t1;
--error ER_VIEW_INVALID
DESCRIBE v1;
SELECT * FROM t1 order by f1, report;
--error ER_VIEW_INVALID
SELECT * FROM v1 order by f1, report;
#
# 7. Recreation of dropped base table column with the same data type like before
ALTER TABLE t1 ADD COLUMN f2 DATE DEFAULT NULL;
INSERT INTO t1 SET f1 = 'ABC', f2 = '1500-12-04',
                   f4 = '<------ 20 -------->', report = 't1 7';
INSERT INTO v1 SET f1 = 'ABC', f2 = '1500-12-04',
                   f4 = '<------ 20 -------->', report = 'v1 7';
DESCRIBE t1;
DESCRIBE v1;
SELECT * FROM t1 order by f1, report;
SELECT * FROM v1 order by f1, report;
#
# 8. Recreation of dropped base table column with a different data type
#    like before
ALTER TABLE t1 DROP COLUMN f2;
ALTER TABLE t1 ADD COLUMN f2 FLOAT;
INSERT INTO t1 SET f1 = 'ABC', f2 = -3.3E-4,
                   f4 = '<------ 20 -------->', report = 't1 8';
INSERT INTO v1 SET f1 = 'ABC', f2 = -3.3E-4,
                   f4 = '<------ 20 -------->', report = 'v1 8';
DESCRIBE t1;
DESCRIBE v1;
SELECT * FROM t1 order by f1, report;
SELECT * FROM v1 order by f1, report;
#
# 9. Add a column to the base table
ALTER TABLE t1 ADD COLUMN f3 NUMERIC(7,2);
INSERT INTO t1 SET f1 = 'ABC', f2 = -3.3E-4,
                   f3 = -2.2, f4 = '<------ 20 -------->', report = 't1 9';
--error ER_BAD_FIELD_ERROR
INSERT INTO v1 SET f1 = 'ABC', f2 = -3.3E-4,
                   f3 = -2.2, f4 = '<------ 20 -------->', report = 'v1 9';
INSERT INTO v1 SET f1 = 'ABC', f2 = -3.3E-4,
                   f4 = '<------ 20 -------->', report = 'v1 9a';
DESCRIBE t1;
DESCRIBE v1;
SELECT * FROM t1 order by f1, report;
SELECT * FROM v1 order by f1, report;
#
# 10. VIEW with numeric function is "victim" of data type change
DROP TABLE t1;
DROP VIEW  v1;
eval CREATE TABLE t1 (f1 CHAR(10), f2 BIGINT) ENGINE = $engine_type;
INSERT INTO t1   SET f1 = 'ABC', f2 = 3;
CREATE VIEW v1 AS SELECT f1, SQRT(f2) my_sqrt FROM t1;
DESCRIBE t1;
DESCRIBE v1;
SELECT * FROM t1 order by f1, f2;
SELECT * FROM v1 order by 2;
ALTER TABLE t1 CHANGE COLUMN f2 f2 VARCHAR(30);
INSERT INTO t1   SET f1 = 'ABC', f2 = 'DEF';
DESCRIBE t1;
DESCRIBE v1;
SELECT * FROM t1 order by f1, f2;
SELECT * FROM v1 order by 2;
# Some statements for comparison
# - the ugly SQRT('DEF) as constant
SELECT SQRT('DEF');
# - Will a VIEW based on the same definition show the same result ?
CREATE VIEW v2 AS SELECT SQRT('DEF');
SELECT * FROM v2 order by 1;
# - Will a VIEW v2 created after the base table column recreation show the same
#   result set like v1 ?
CREATE OR REPLACE VIEW v2 AS SELECT f1, SQRT(f2) my_sqrt FROM t1;
DESCRIBE v2;
SELECT * FROM v2 order by 2;
# - What will be the content of base table created with AS SELECT ?
CREATE TABLE t2 AS SELECT f1, SQRT(f2) my_sqrt FROM t1;
if ($have_bug_32285)
{
--disable_ps_protocol
}
--vertical_results
SELECT * FROM t2 order by 2;
--horizontal_results
--enable_ps_protocol
DROP TABLE t2;
CREATE TABLE t2 AS SELECT * FROM v1;
if ($have_bug_32285)
{
--disable_ps_protocol
}
--vertical_results
SELECT * FROM t2 order by 2;
--horizontal_results
--enable_ps_protocol
DROP TABLE t2;
CREATE TABLE t2 AS SELECT * FROM v2;
if ($have_bug_32285)
{
--disable_ps_protocol
}
--vertical_results
SELECT * FROM t2 order by 2;
--horizontal_results
--enable_ps_protocol
#
DROP TABLE t1;
DROP TABLE t2;
DROP VIEW  v1;
DROP VIEW  v2;



# Clean up
--disable_warnings
DROP TABLE IF EXISTS t1;
DROP TABLE IF EXISTS t2;
DROP VIEW  IF EXISTS v1;
DROP VIEW  IF EXISTS v1_1;
DROP VIEW  IF EXISTS v1_2;
DROP VIEW  IF EXISTS v1_firstview;
DROP VIEW  IF EXISTS v1_secondview;
DROP VIEW  IF EXISTS v2;
DROP DATABASE IF EXISTS test2;
DROP DATABASE IF EXISTS test3;
--enable_warnings

# FIXME sub testcases, which might be included, if they fit good into
#       the requirements and the completeness of the tests is increased
# Bug#10970     Views: dependence on temporary table allowed
# Bug#4663      constant function in WHERE clause evaluated in view definition
# Bug#6808      Views: CREATE VIEW v ... FROM t AS v fails
# Bug#10977     Views: no warning if column name is truncated
# Bug#9505: Views: privilege needed on underlying function

#  --source suite/funcs_1/Views/Views_403x406.test
# --source suite/funcs_1/Views/Views_407.test
# --source suite/funcs_1/Views/Views_408x411.test



