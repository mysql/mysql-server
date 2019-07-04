'#--------------------FN_DYNVARS_011_01-------------------------#'
SET @global_collation_database = @@global.collation_database;
SET @session_collation_database = @@session.collation_database;
SET @session_collation_server = @@session.collation_server;
SET @@global.collation_database = latin1_danish_ci;
Warnings:
Warning	1681	Updating 'collation_database' is deprecated. It will be made read-only in a future release.
'connect (con1,localhost,root,,,,)'
'connection con1'
SELECT @@global.collation_database;
@@global.collation_database
latin1_danish_ci
SELECT @@session.collation_database;
@@session.collation_database
utf8mb4_0900_ai_ci
'#--------------------FN_DYNVARS_011_02-------------------------#'
'connection default'
DROP TABLE IF EXISTS t1,t2;
SELECT @@character_set_server,@@collation_server;
@@character_set_server	@@collation_server
utf8mb4	utf8mb4_0900_ai_ci
'--check if setting collation_database update character_set_database--'
SET @@session.collation_database = utf8_spanish_ci;
Warnings:
Warning	3778	'utf8_spanish_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
Warning	1681	Updating 'collation_database' is deprecated. It will be made read-only in a future release.
SELECT @@collation_database, @@character_set_database;
@@collation_database	@@character_set_database
utf8_spanish_ci	utf8
'--check if collation_database effects database/tables charset/collation'
SET @@session.collation_server = utf8_roman_ci;
Warnings:
Warning	3778	'utf8_roman_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
SET @@session.collation_database = latin2_croatian_ci;
Warnings:
Warning	1681	Updating 'collation_database' is deprecated. It will be made read-only in a future release.
CREATE DATABASE db1;
USE db1;
SHOW CREATE DATABASE db1;
Database	Create Database
db1	CREATE DATABASE `db1` /*!40100 DEFAULT CHARACTER SET utf8 COLLATE utf8_roman_ci */ /*!80016 DEFAULT ENCRYPTION='N' */
CREATE TABLE t1(a CHAR(20));
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` char(20) COLLATE utf8_roman_ci DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_roman_ci
'---check if updating collation_database effects new table/column---'
SET @@session.collation_database = latin7_general_cs;
Warnings:
Warning	1681	Updating 'collation_database' is deprecated. It will be made read-only in a future release.
CREATE TABLE t2(a CHAR(10));
SHOW CREATE TABLE t2;
Table	Create Table
t2	CREATE TABLE `t2` (
  `a` char(10) COLLATE utf8_roman_ci DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_roman_ci
DROP TABLE t1,t2;
'--check value of collation and character set when current db is dropped'
DROP DATABASE db1;
SELECT @@collation_database,@@collation_server,@@character_set_database,@@character_set_server;
@@collation_database	@@collation_server	@@character_set_database	@@character_set_server
utf8_roman_ci	utf8_roman_ci	utf8	utf8
USE test;
SELECT @@collation_database,@@collation_server,@@character_set_database,@@character_set_server;
@@collation_database	@@collation_server	@@character_set_database	@@character_set_server
utf8mb4_0900_ai_ci	utf8_roman_ci	utf8mb4	utf8
set names latin1;
'fill table with some test data';
CREATE TABLE t1(a CHAR(20))CHARACTER SET=latin1;
INSERT INTO t1 VALUES('Muffler'),('M�ller'),('MX Systems');
set names utf8mb4;
'---check if collation_database effects results sort order---'
SET @@session.collation_database = latin1_swedish_ci;
Warnings:
Warning	1681	Updating 'collation_database' is deprecated. It will be made read-only in a future release.
SELECT * FROM t1 ORDER BY a;
a
Muffler
MX Systems
Müller
SET @@session.collation_database = latin1_german1_ci;
Warnings:
Warning	1681	Updating 'collation_database' is deprecated. It will be made read-only in a future release.
SELECT * FROM t1 ORDER BY a;
a
Muffler
MX Systems
Müller
'explicit Collate clause should effects results sort order';
SELECT * FROM t1 ORDER BY a COLLATE latin1_swedish_ci;
a
Muffler
MX Systems
Müller
SELECT * FROM t1 ORDER BY a COLLATE latin1_german1_ci;
a
Muffler
Müller
MX Systems
'----check if indexing is effected by collation_database---'
SET @@session.collation_database = latin1_swedish_ci;
Warnings:
Warning	1681	Updating 'collation_database' is deprecated. It will be made read-only in a future release.
ALTER TABLE t1 ADD PRIMARY KEY (a);
SELECT * FROM t1 ORDER BY a;
a
Muffler
MX Systems
Müller
ALTER TABLE t1 DROP PRIMARY KEY;
SET @@session.collation_database = latin1_german1_ci;
Warnings:
Warning	1681	Updating 'collation_database' is deprecated. It will be made read-only in a future release.
ALTER TABLE t1 ADD PRIMARY KEY (a);
SELECT * FROM t1 ORDER BY a;
a
Muffler
MX Systems
Müller
DROP TABLE t1;
SET @@global.collation_database = @global_collation_database;
Warnings:
Warning	1681	Updating 'collation_database' is deprecated. It will be made read-only in a future release.
SET @@session.collation_database = @session_collation_database;
Warnings:
Warning	1681	Updating 'collation_database' is deprecated. It will be made read-only in a future release.
SET @@session.collation_server = @session_collation_server;
