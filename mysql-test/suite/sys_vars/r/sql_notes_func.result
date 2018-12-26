** Setup **

SET @default_sql_notes = @@sql_notes;
'#--------------------FN_DYNVARS_162_01-------------------------#'
SET SESSION sql_notes = ON;
creating a temporary table
CREATE TEMPORARY TABLE t1(a varchar(20), b varchar(20));
conditionally dropping the table twice so that a note generates
DROP TABLE IF EXISTS t1;
SELECT @@warning_count;
@@warning_count
0
0 Expected
DROP TABLE IF EXISTS t1;
Warnings:
Note	1051	Unknown table 'test.t1'
SELECT @@warning_count;
@@warning_count
1
1 Expected
'#--------------------FN_DYNVARS_162_02-------------------------#'
SET SESSION sql_notes = OFF;
creating a temporary table
CREATE TEMPORARY TABLE t1(a varchar(20), b varchar(20));
conditionally dropping the table twice so that a note generates
DROP TABLE IF EXISTS t1;
SELECT @@warning_count;
@@warning_count
0
0 Expected
DROP TABLE IF EXISTS t1;
SELECT @@warning_count;
@@warning_count
0
0 Expected
'#------------------FN_DYNVARS_162_03---------------------------#'
** Connecting con_int1 using root **
** Connection con_int1 **
SELECT @@SESSION.sql_notes;
@@SESSION.sql_notes
1
1 / TRUE Expected
SET SESSION sql_notes = FALSE;
** Connecting con_int2 using root **
** Connection con_int2 **
SELECT @@SESSION.sql_notes;
@@SESSION.sql_notes
1
1 / TRUE Expected
SET SESSION sql_notes = TRUE;
** Connection con_int2 **
SELECT @@SESSION.sql_notes;
@@SESSION.sql_notes
1
1 / TRUE Expected
** Connection con_int1 **
SELECT @@SESSION.sql_notes;
@@SESSION.sql_notes
0
0 / FALSE Expected
** Connection default **
Disconnecting Connections con_int1, con_int2
SET SESSION sql_notes = @default_sql_notes;
DROP TABLE IF EXISTS t1;
