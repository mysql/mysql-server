create table t1 (a int) engine=myisam;
create view v1 as select * from t1;
test.t1                                            OK
test.t1                                            Table is already up to date
test.t1                                            OK
test.t1                                            Table is already up to date
drop view v1;
drop table t1;
create table `t``1`(a int) engine=myisam;
create table `t 1`(a int) engine=myisam;
test.t 1                                           OK
test.t`1                                           OK
drop table `t``1`, `t 1`;
create database d_bug25347;
use d_bug25347;
create table t_bug25347 (a int) engine=myisam;
create view v_bug25347 as select * from t_bug25347;
insert into t_bug25347 values (1),(2),(3);
flush tables;
removing and creating
d_bug25347.t_bug25347
Error    : Incorrect file format 't_bug25347'
error    : Corrupt
insert into t_bug25347 values (4),(5),(6);
ERROR HY000: Incorrect file format 't_bug25347'
d_bug25347.t_bug25347
warning  : Number of rows changed from 0 to 3
status   : OK
insert into t_bug25347 values (7),(8),(9);
select * from t_bug25347;
a
1
2
3
7
8
9
select * from v_bug25347;
a
1
2
3
7
8
9
drop view v_bug25347;
drop table t_bug25347;
drop database d_bug25347;
use test;
#
# Bug#20868496: MYSQL_UPGRADE IN 5.7.7+ REPAIR LOOKS USER TABLES
#               IN TEST WHEN LOAD FROM 50/51/55
#
CREATE DATABASE db1;
CREATE DATABASE db2;
CREATE TABLE db1.t1 (a INT) ENGINE=MYISAM;
CREATE TABLE db2.t2 (a INT);
db1.t1                                             To be repaired, cause follows:
Server issued Error    : Incorrect file format 't1'
error    : Corrupt
db2.t2                                             OK

Repairing tables
db1.t1
Error    : Incorrect file format 't1'
error    : Corrupt
DROP DATABASE db1;
DROP DATABASE db2;
#
# Bug#13926 --order-by-primary fails if PKEY contains quote character
#
DROP TABLE IF EXISTS `t1`;
CREATE TABLE `t1` (
`a b` INT,
`c"d` INT,
`e``f` INT,
PRIMARY KEY (`a b`, `c"d`, `e``f`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;
insert into t1 values (0815, 4711, 2006);
/*!40103 SET @OLD_TIME_ZONE=@@TIME_ZONE */;
/*!40103 SET TIME_ZONE='+00:00' */;
/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO,ANSI' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;
DROP TABLE IF EXISTS "t1";
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE "t1" (
  "a b" int NOT NULL,
  "c""d" int NOT NULL,
  "e`f" int NOT NULL,
  PRIMARY KEY ("a b","c""d","e`f")
);
/*!40101 SET character_set_client = @saved_cs_client */;

LOCK TABLES "t1" WRITE;
/*!40000 ALTER TABLE "t1" DISABLE KEYS */;
INSERT INTO "t1" VALUES (815,4711,2006);
/*!40000 ALTER TABLE "t1" ENABLE KEYS */;
UNLOCK TABLES;
/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;


/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!50503 SET NAMES utf8mb4 */;
/*!40103 SET @OLD_TIME_ZONE=@@TIME_ZONE */;
/*!40103 SET TIME_ZONE='+00:00' */;
/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;
DROP TABLE IF EXISTS `t1`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `t1` (
  `a b` int NOT NULL,
  `c"d` int NOT NULL,
  `e``f` int NOT NULL,
  PRIMARY KEY (`a b`,`c"d`,`e``f`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

LOCK TABLES `t1` WRITE;
/*!40000 ALTER TABLE `t1` DISABLE KEYS */;
INSERT INTO `t1` VALUES (815,4711,2006);
/*!40000 ALTER TABLE `t1` ENABLE KEYS */;
UNLOCK TABLES;
/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

DROP TABLE `t1`;
#
# Test of fix to Bug#12597 mysqldump dumps triggers wrongly
#
DROP TABLE IF EXISTS `test1`;
Warnings:
Note	1051	Unknown table 'test.test1'
CREATE TABLE `test1` (
`a1` int(11) default NULL
) ENGINE=MyISAM DEFAULT CHARSET=latin1;
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
DROP TABLE IF EXISTS `test2`;
Warnings:
Note	1051	Unknown table 'test.test2'
CREATE TABLE `test2` (
`a2` int(11) default NULL
) ENGINE=MyISAM DEFAULT CHARSET=latin1;
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
CREATE TRIGGER `testref` BEFORE INSERT ON `test1` FOR EACH ROW BEGIN
INSERT INTO test2 SET a2 = NEW.a1; END  //
INSERT INTO `test1` VALUES (1);
SELECT * FROM `test2`;
a2
1
SHOW TRIGGERS;
Trigger	Event	Table	Statement	Timing	Created	sql_mode	Definer	character_set_client	collation_connection	Database Collation
testref	INSERT	test1	BEGIN
INSERT INTO test2 SET a2 = NEW.a1; END	BEFORE	#	ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION	root@localhost	utf8mb4	utf8mb4_0900_ai_ci	utf8mb4_0900_ai_ci
SELECT * FROM `test1`;
a1
1
SELECT * FROM `test2`;
a2
1
DROP TRIGGER testref;
DROP TABLE test1;
DROP TABLE test2;
#
# Test of fix to Bug#13146 ansi quotes break loading of triggers
#
DROP TABLE IF EXISTS `t1 test`;
DROP TABLE IF EXISTS `t2 test`;
CREATE TABLE `t1 test` (
`a1` int(11) default NULL
) ENGINE=MyISAM DEFAULT CHARSET=latin1;
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
CREATE TABLE `t2 test` (
`a2` int(11) default NULL
) ENGINE=MyISAM DEFAULT CHARSET=latin1;
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
CREATE TRIGGER `test trig` BEFORE INSERT ON `t1 test` FOR EACH ROW BEGIN
INSERT INTO `t2 test` SET a2 = NEW.a1; END  //
INSERT INTO `t1 test` VALUES (1);
INSERT INTO `t1 test` VALUES (2);
INSERT INTO `t1 test` VALUES (3);
SELECT * FROM `t2 test`;
a2
1
2
3
/*!40103 SET @OLD_TIME_ZONE=@@TIME_ZONE */;
/*!40103 SET TIME_ZONE='+00:00' */;
/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO,ANSI' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;
DROP TABLE IF EXISTS "t1 test";
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE "t1 test" (
  "a1" int DEFAULT NULL
);
/*!40101 SET character_set_client = @saved_cs_client */;

LOCK TABLES "t1 test" WRITE;
/*!40000 ALTER TABLE "t1 test" DISABLE KEYS */;
INSERT INTO "t1 test" VALUES (1),(2),(3);
/*!40000 ALTER TABLE "t1 test" ENABLE KEYS */;
UNLOCK TABLES;
/*!50003 SET @saved_cs_client      = @@character_set_client */ ;
/*!50003 SET @saved_cs_results     = @@character_set_results */ ;
/*!50003 SET @saved_col_connection = @@collation_connection */ ;
/*!50003 SET character_set_client  = utf8mb4 */ ;
/*!50003 SET character_set_results = utf8mb4 */ ;
/*!50003 SET collation_connection  = utf8mb4_0900_ai_ci */ ;
/*!50003 SET @saved_sql_mode       = @@sql_mode */ ;
/*!50003 SET sql_mode              = 'ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION' */ ;
DELIMITER ;;
/*!50003 CREATE*/ /*!50017 DEFINER="root"@"localhost"*/ /*!50003 TRIGGER "test trig" BEFORE INSERT ON "t1 test" FOR EACH ROW BEGIN
INSERT INTO `t2 test` SET a2 = NEW.a1; END */;;
DELIMITER ;
/*!50003 SET sql_mode              = @saved_sql_mode */ ;
/*!50003 SET character_set_client  = @saved_cs_client */ ;
/*!50003 SET character_set_results = @saved_cs_results */ ;
/*!50003 SET collation_connection  = @saved_col_connection */ ;
DROP TABLE IF EXISTS "t2 test";
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE "t2 test" (
  "a2" int DEFAULT NULL
);
/*!40101 SET character_set_client = @saved_cs_client */;

LOCK TABLES "t2 test" WRITE;
/*!40000 ALTER TABLE "t2 test" DISABLE KEYS */;
INSERT INTO "t2 test" VALUES (1),(2),(3);
/*!40000 ALTER TABLE "t2 test" ENABLE KEYS */;
UNLOCK TABLES;
/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

DROP TRIGGER `test trig`;
DROP TABLE `t1 test`;
DROP TABLE `t2 test`;
CREATE TABLE t2 (a INT) ENGINE=MYISAM;
CREATE TABLE t3 (a INT) ENGINE=MYISAM;
CREATE TABLE t1 (a INT) ENGINE=merge UNION=(t2, t3);

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!50503 SET NAMES utf8mb4 */;
/*!40103 SET @OLD_TIME_ZONE=@@TIME_ZONE */;
/*!40103 SET TIME_ZONE='+00:00' */;
/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;
DROP TABLE IF EXISTS `t1`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `t1` (
  `a` int DEFAULT NULL
) ENGINE=MRG_MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci UNION=(`t2`,`t3`);
/*!40101 SET character_set_client = @saved_cs_client */;
DROP TABLE IF EXISTS `t2`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `t2` (
  `a` int DEFAULT NULL
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

LOCK TABLES `t2` WRITE;
/*!40000 ALTER TABLE `t2` DISABLE KEYS */;
/*!40000 ALTER TABLE `t2` ENABLE KEYS */;
UNLOCK TABLES;
DROP TABLE IF EXISTS `t3`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `t3` (
  `a` int DEFAULT NULL
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

LOCK TABLES `t3` WRITE;
/*!40000 ALTER TABLE `t3` DISABLE KEYS */;
/*!40000 ALTER TABLE `t3` ENABLE KEYS */;
UNLOCK TABLES;
/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

DROP TABLE t1, t2, t3;
#
# BUG#11766310 : 59398: MYSQLDUMP 5.1 CAN'T HANDLE A DASH ("-") IN
#                DATABASE NAMES IN ALTER DATABASE
#
CREATE DATABASE `test-database`;
USE `test-database`;
CREATE TABLE `test` (`c1` VARCHAR(10)) ENGINE=MYISAM DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci;
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
Warning	3778	'utf8mb3_unicode_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
CREATE TRIGGER `trig` BEFORE INSERT ON `test` FOR EACH ROW BEGIN
END |
ALTER DATABASE `test-database` CHARACTER SET latin1 COLLATE latin1_swedish_ci;
ALTER DATABASE `test-database` CHARACTER SET utf8 COLLATE utf8_unicode_ci ;
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
Warning	3778	'utf8mb3_unicode_ci' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `test` (
  `c1` varchar(10) COLLATE utf8mb3_unicode_ci DEFAULT NULL
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb3 COLLATE=utf8mb3_unicode_ci;
/*!40101 SET character_set_client = @saved_cs_client */;
ALTER DATABASE `test-database` CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci ;
/*!50003 SET @saved_cs_client      = @@character_set_client */ ;
/*!50003 SET @saved_cs_results     = @@character_set_results */ ;
/*!50003 SET @saved_col_connection = @@collation_connection */ ;
/*!50003 SET character_set_client  = utf8mb4 */ ;
/*!50003 SET character_set_results = utf8mb4 */ ;
/*!50003 SET collation_connection  = utf8mb4_0900_ai_ci */ ;
/*!50003 SET @saved_sql_mode       = @@sql_mode */ ;
/*!50003 SET sql_mode              = 'ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION' */ ;
DELIMITER ;;
/*!50003 CREATE*/ /*!50017 DEFINER=`root`@`localhost`*/ /*!50003 TRIGGER `trig` BEFORE INSERT ON `test` FOR EACH ROW BEGIN
END */;;
DELIMITER ;
/*!50003 SET sql_mode              = @saved_sql_mode */ ;
/*!50003 SET character_set_client  = @saved_cs_client */ ;
/*!50003 SET character_set_results = @saved_cs_results */ ;
/*!50003 SET collation_connection  = @saved_col_connection */ ;
ALTER DATABASE `test-database` CHARACTER SET utf8mb3 COLLATE utf8mb3_unicode_ci ;
DROP DATABASE `test-database`;
USE test;
#
# BUG#11760384 : 52792: mysqldump in XML mode does not dump routines.
#
CREATE DATABASE BUG52792;
USE BUG52792;
CREATE TABLE t1 (c1 INT, c2 VARCHAR(20)) ENGINE=MyISAM;
CREATE TABLE t2 (c1 INT) ENGINE=MyISAM;
INSERT INTO t1 VALUES (1, 'aaa'), (2, 'bbb'), (3, 'ccc');
INSERT INTO t2 VALUES (1),(2),(3);
# Stored Procedures.
CREATE PROCEDURE simpleproc1 (OUT param1 INT)
BEGIN
SELECT COUNT(*) INTO param1 FROM t1;
END//
CREATE PROCEDURE simpleproc2 (OUT param1 INT)
BEGIN
SELECT COUNT(*) INTO param1 FROM t2;
END//
# Events.
SET GLOBAL EVENT_SCHEDULER = OFF;
CREATE EVENT e1 ON SCHEDULE EVERY 1 SECOND DO DROP DATABASE BUG52792;
CREATE EVENT e2 ON SCHEDULE EVERY 1 SECOND DO DROP DATABASE BUG52792;
# Functions.
CREATE FUNCTION `hello1` (s CHAR(20))
RETURNS CHAR(50) DETERMINISTIC
RETURN CONCAT('Hello, ' ,s ,'!');
CREATE FUNCTION `hello2` (s CHAR(20))
RETURNS CHAR(50) DETERMINISTIC
RETURN CONCAT(']]>, ' , s ,'!');
# Triggers.
CREATE TRIGGER trig1 BEFORE INSERT ON t2
FOR EACH ROW BEGIN
INSERT INTO t2 VALUES(1);
END;
|
CREATE TRIGGER trig2 AFTER INSERT ON t2
FOR EACH ROW BEGIN
INSERT INTO t2 VALUES(1, ']]>');
INSERT INTO t2 VALUES(2, '<![CDATA]]>');
INSERT INTO t2 VALUES(3, '<![CDATA[');
INSERT INTO t2 VALUES(4, '< > & \ " _');
END;
|
# Views
CREATE VIEW v1 AS SELECT * FROM t1;
CREATE VIEW v2 AS SELECT * FROM t2;

# Dumping BUG52792 database in xml format.

# Running 'replace_regex on timestamp'
<?xml version="1.0"?>
<mysqldump xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
<database name="BUG52792">
	<table_structure name="t1">
		<field Field="c1" Type="int" Null="YES" Key="" Extra="" Comment="" />
		<field Field="c2" Type="varchar(20)" Null="YES" Key="" Extra="" Comment="" />
		<options Name="t1" Engine="MyISAM" Version="10" Row_format="Dynamic" Rows="3" Avg_row_length="20" Data_length="60" Max_data_length="281474976710655" Index_length="1024" Data_free="0" Auto_increment="1" Create_time="--TIME--" Update_time="--TIME--" Collation="utf8mb4_0900_ai_ci" Create_options="" Comment="" />
	</table_structure>
	<table_data name="t1">
	<row>
		<field name="c1">1</field>
		<field name="c2">aaa</field>
	</row>
	<row>
		<field name="c1">2</field>
		<field name="c2">bbb</field>
	</row>
	<row>
		<field name="c1">3</field>
		<field name="c2">ccc</field>
	</row>
	</table_data>
	<table_structure name="t2">
		<field Field="c1" Type="int" Null="YES" Key="" Extra="" Comment="" />
		<options Name="t2" Engine="MyISAM" Version="10" Row_format="Fixed" Rows="3" Avg_row_length="7" Data_length="21" Max_data_length="1970324836974591" Index_length="1024" Data_free="0" Auto_increment="1" Create_time="--TIME--" Update_time="--TIME--" Collation="utf8mb4_0900_ai_ci" Create_options="" Comment="" />
	</table_structure>
	<table_data name="t2">
	<row>
		<field name="c1">1</field>
	</row>
	<row>
		<field name="c1">2</field>
	</row>
	<row>
		<field name="c1">3</field>
	</row>
	</table_data>
	<triggers name="t2">
		<trigger Trigger="trig1" sql_mode="ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION" character_set_client="utf8mb4" collation_connection="utf8mb4_0900_ai_ci" Database_Collation="utf8mb4_0900_ai_ci" Created="--TIMESTAMP--">
<![CDATA[
CREATE DEFINER=`root`@`localhost` TRIGGER `trig1` BEFORE INSERT ON `t2` FOR EACH ROW BEGIN
INSERT INTO t2 VALUES(1);
END
]]>
		</trigger>
		<trigger Trigger="trig2" sql_mode="ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION" character_set_client="utf8mb4" collation_connection="utf8mb4_0900_ai_ci" Database_Collation="utf8mb4_0900_ai_ci" Created="--TIMESTAMP--">
<![CDATA[
CREATE DEFINER=`root`@`localhost` TRIGGER `trig2` AFTER INSERT ON `t2` FOR EACH ROW BEGIN
INSERT INTO t2 VALUES(1, ']]]]><![CDATA[>');
INSERT INTO t2 VALUES(2, '<![CDATA]]]]><![CDATA[>');
INSERT INTO t2 VALUES(3, '<![CDATA[');
INSERT INTO t2 VALUES(4, '< > & \ " _');
END
]]>
		</trigger>
	</triggers>
	<table_structure name="v1">
		<field Field="c1" Type="int" Null="YES" Key="" Extra="" Comment="" />
		<field Field="c2" Type="varchar(20)" Null="YES" Key="" Extra="" Comment="" />
		<options Name="v1" Create_time="--TIME--" Comment="VIEW" />
	</table_structure>
	<table_structure name="v2">
		<field Field="c1" Type="int" Null="YES" Key="" Extra="" Comment="" />
		<options Name="v2" Create_time="--TIME--" Comment="VIEW" />
	</table_structure>
	<events>
		<event Event="e1" sql_mode="ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION" time_zone="SYSTEM" character_set_client="utf8mb4" collation_connection="utf8mb4_0900_ai_ci" Database_Collation="utf8mb4_0900_ai_ci">
<![CDATA[
CREATE DEFINER=`root`@`localhost` EVENT `e1` ON SCHEDULE EVERY 1 SECOND STARTS '--TIME--' ON COMPLETION NOT PRESERVE ENABLE DO DROP DATABASE BUG52792
]]>
		</event>
		<event Event="e2" sql_mode="ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION" time_zone="SYSTEM" character_set_client="utf8mb4" collation_connection="utf8mb4_0900_ai_ci" Database_Collation="utf8mb4_0900_ai_ci">
<![CDATA[
CREATE DEFINER=`root`@`localhost` EVENT `e2` ON SCHEDULE EVERY 1 SECOND STARTS '--TIME--' ON COMPLETION NOT PRESERVE ENABLE DO DROP DATABASE BUG52792
]]>
		</event>
	</events>
	<routines>
		<routine Function="hello1" sql_mode="ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION" character_set_client="utf8mb4" collation_connection="utf8mb4_0900_ai_ci" Database_Collation="utf8mb4_0900_ai_ci">
<![CDATA[
CREATE DEFINER=`root`@`localhost` FUNCTION `hello1`(s CHAR(20)) RETURNS char(50) CHARSET utf8mb4
    DETERMINISTIC
RETURN CONCAT('Hello, ' ,s ,'!')
]]>
		</routine>
		<routine Function="hello2" sql_mode="ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION" character_set_client="utf8mb4" collation_connection="utf8mb4_0900_ai_ci" Database_Collation="utf8mb4_0900_ai_ci">
<![CDATA[
CREATE DEFINER=`root`@`localhost` FUNCTION `hello2`(s CHAR(20)) RETURNS char(50) CHARSET utf8mb4
    DETERMINISTIC
RETURN CONCAT(']]]]><![CDATA[>, ' , s ,'!')
]]>
		</routine>
		<routine Procedure="simpleproc1" sql_mode="ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION" character_set_client="utf8mb4" collation_connection="utf8mb4_0900_ai_ci" Database_Collation="utf8mb4_0900_ai_ci">
<![CDATA[
CREATE DEFINER=`root`@`localhost` PROCEDURE `simpleproc1`(OUT param1 INT)
BEGIN
SELECT COUNT(*) INTO param1 FROM t1;
END
]]>
		</routine>
		<routine Procedure="simpleproc2" sql_mode="ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION" character_set_client="utf8mb4" collation_connection="utf8mb4_0900_ai_ci" Database_Collation="utf8mb4_0900_ai_ci">
<![CDATA[
CREATE DEFINER=`root`@`localhost` PROCEDURE `simpleproc2`(OUT param1 INT)
BEGIN
SELECT COUNT(*) INTO param1 FROM t2;
END
]]>
		</routine>
	</routines>
</database>
</mysqldump>

# Dumping BUG52792 database in xml format with comments.

# Running 'replace_regex on timestamp'
<?xml version="1.0"?>
<mysqldump xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
<database name="BUG52792">
<!-- 
-
- Table structure for table `t1`
-

 -->
	<table_structure name="t1">
		<field Field="c1" Type="int" Null="YES" Key="" Extra="" Comment="" />
		<field Field="c2" Type="varchar(20)" Null="YES" Key="" Extra="" Comment="" />
		<options Name="t1" Engine="MyISAM" Version="10" Row_format="Dynamic" Rows="3" Avg_row_length="20" Data_length="60" Max_data_length="281474976710655" Index_length="1024" Data_free="0" Auto_increment="1" Create_time="--TIME--" Update_time="--TIME--" Collation="utf8mb4_0900_ai_ci" Create_options="" Comment="" />
	</table_structure>
<!-- 
-
- Dumping data for table `t1`
-
 -->
	<table_data name="t1">
	<row>
		<field name="c1">1</field>
		<field name="c2">aaa</field>
	</row>
	<row>
		<field name="c1">2</field>
		<field name="c2">bbb</field>
	</row>
	<row>
		<field name="c1">3</field>
		<field name="c2">ccc</field>
	</row>
	</table_data>
<!-- 
-
- Table structure for table `t2`
-

 -->
	<table_structure name="t2">
		<field Field="c1" Type="int" Null="YES" Key="" Extra="" Comment="" />
		<options Name="t2" Engine="MyISAM" Version="10" Row_format="Fixed" Rows="3" Avg_row_length="7" Data_length="21" Max_data_length="1970324836974591" Index_length="1024" Data_free="0" Auto_increment="1" Create_time="--TIME--" Update_time="--TIME--" Collation="utf8mb4_0900_ai_ci" Create_options="" Comment="" />
	</table_structure>
<!-- 
-
- Dumping data for table `t2`
-
 -->
	<table_data name="t2">
	<row>
		<field name="c1">1</field>
	</row>
	<row>
		<field name="c1">2</field>
	</row>
	<row>
		<field name="c1">3</field>
	</row>
	</table_data>
	<triggers name="t2">
		<trigger Trigger="trig1" sql_mode="ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION" character_set_client="utf8mb4" collation_connection="utf8mb4_0900_ai_ci" Database_Collation="utf8mb4_0900_ai_ci" Created="--TIMESTAMP--">
<![CDATA[
CREATE DEFINER=`root`@`localhost` TRIGGER `trig1` BEFORE INSERT ON `t2` FOR EACH ROW BEGIN
INSERT INTO t2 VALUES(1);
END
]]>
		</trigger>
		<trigger Trigger="trig2" sql_mode="ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION" character_set_client="utf8mb4" collation_connection="utf8mb4_0900_ai_ci" Database_Collation="utf8mb4_0900_ai_ci" Created="--TIMESTAMP--">
<![CDATA[
CREATE DEFINER=`root`@`localhost` TRIGGER `trig2` AFTER INSERT ON `t2` FOR EACH ROW BEGIN
INSERT INTO t2 VALUES(1, ']]]]><![CDATA[>');
INSERT INTO t2 VALUES(2, '<![CDATA]]]]><![CDATA[>');
INSERT INTO t2 VALUES(3, '<![CDATA[');
INSERT INTO t2 VALUES(4, '< > & \ " _');
END
]]>
		</trigger>
	</triggers>
<!-- 
-
- Table structure for table `v1`
-

 -->
	<table_structure name="v1">
		<field Field="c1" Type="int" Null="YES" Key="" Extra="" Comment="" />
		<field Field="c2" Type="varchar(20)" Null="YES" Key="" Extra="" Comment="" />
		<options Name="v1" Create_time="--TIME--" Comment="VIEW" />
	</table_structure>
<!-- 
-
- Table structure for table `v2`
-

 -->
	<table_structure name="v2">
		<field Field="c1" Type="int" Null="YES" Key="" Extra="" Comment="" />
		<options Name="v2" Create_time="--TIME--" Comment="VIEW" />
	</table_structure>
<!-- 
-
- Dumping events for database 'BUG52792'
-
 -->
	<events>
		<event Event="e1" sql_mode="ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION" time_zone="SYSTEM" character_set_client="utf8mb4" collation_connection="utf8mb4_0900_ai_ci" Database_Collation="utf8mb4_0900_ai_ci">
<![CDATA[
CREATE DEFINER=`root`@`localhost` EVENT `e1` ON SCHEDULE EVERY 1 SECOND STARTS '--TIME--' ON COMPLETION NOT PRESERVE ENABLE DO DROP DATABASE BUG52792
]]>
		</event>
		<event Event="e2" sql_mode="ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION" time_zone="SYSTEM" character_set_client="utf8mb4" collation_connection="utf8mb4_0900_ai_ci" Database_Collation="utf8mb4_0900_ai_ci">
<![CDATA[
CREATE DEFINER=`root`@`localhost` EVENT `e2` ON SCHEDULE EVERY 1 SECOND STARTS '--TIME--' ON COMPLETION NOT PRESERVE ENABLE DO DROP DATABASE BUG52792
]]>
		</event>
	</events>
<!-- 
-
- Dumping routines for database 'BUG52792'
-
 -->
	<routines>
		<routine Function="hello1" sql_mode="ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION" character_set_client="utf8mb4" collation_connection="utf8mb4_0900_ai_ci" Database_Collation="utf8mb4_0900_ai_ci">
<![CDATA[
CREATE DEFINER=`root`@`localhost` FUNCTION `hello1`(s CHAR(20)) RETURNS char(50) CHARSET utf8mb4
    DETERMINISTIC
RETURN CONCAT('Hello, ' ,s ,'!')
]]>
		</routine>
		<routine Function="hello2" sql_mode="ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION" character_set_client="utf8mb4" collation_connection="utf8mb4_0900_ai_ci" Database_Collation="utf8mb4_0900_ai_ci">
<![CDATA[
CREATE DEFINER=`root`@`localhost` FUNCTION `hello2`(s CHAR(20)) RETURNS char(50) CHARSET utf8mb4
    DETERMINISTIC
RETURN CONCAT(']]]]><![CDATA[>, ' , s ,'!')
]]>
		</routine>
		<routine Procedure="simpleproc1" sql_mode="ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION" character_set_client="utf8mb4" collation_connection="utf8mb4_0900_ai_ci" Database_Collation="utf8mb4_0900_ai_ci">
<![CDATA[
CREATE DEFINER=`root`@`localhost` PROCEDURE `simpleproc1`(OUT param1 INT)
BEGIN
SELECT COUNT(*) INTO param1 FROM t1;
END
]]>
		</routine>
		<routine Procedure="simpleproc2" sql_mode="ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION" character_set_client="utf8mb4" collation_connection="utf8mb4_0900_ai_ci" Database_Collation="utf8mb4_0900_ai_ci">
<![CDATA[
CREATE DEFINER=`root`@`localhost` PROCEDURE `simpleproc2`(OUT param1 INT)
BEGIN
SELECT COUNT(*) INTO param1 FROM t2;
END
]]>
		</routine>
	</routines>
</database>
</mysqldump>

# Test to check 'Insufficient privileges' error.

CREATE USER user1;
GRANT ALL PRIVILEGES ON BUG52792.* TO user1;
# Running 'replace_regex on timestamp'
<?xml version="1.0"?>
<mysqldump xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
<database name="BUG52792">
	<table_structure name="t1">
		<field Field="c1" Type="int" Null="YES" Key="" Extra="" Comment="" />
		<field Field="c2" Type="varchar(20)" Null="YES" Key="" Extra="" Comment="" />
		<options Name="t1" Engine="MyISAM" Version="10" Row_format="Dynamic" Rows="3" Avg_row_length="20" Data_length="60" Max_data_length="281474976710655" Index_length="1024" Data_free="0" Auto_increment="1" Create_time="--TIME--" Update_time="--TIME--" Collation="utf8mb4_0900_ai_ci" Create_options="" Comment="" />
	</table_structure>
	<table_data name="t1">
	<row>
		<field name="c1">1</field>
		<field name="c2">aaa</field>
	</row>
	<row>
		<field name="c1">2</field>
		<field name="c2">bbb</field>
	</row>
	<row>
		<field name="c1">3</field>
		<field name="c2">ccc</field>
	</row>
	</table_data>
	<table_structure name="t2">
		<field Field="c1" Type="int" Null="YES" Key="" Extra="" Comment="" />
		<options Name="t2" Engine="MyISAM" Version="10" Row_format="Fixed" Rows="3" Avg_row_length="7" Data_length="21" Max_data_length="1970324836974591" Index_length="1024" Data_free="0" Auto_increment="1" Create_time="--TIME--" Update_time="--TIME--" Collation="utf8mb4_0900_ai_ci" Create_options="" Comment="" />
	</table_structure>
	<table_data name="t2">
	<row>
		<field name="c1">1</field>
	</row>
	<row>
		<field name="c1">2</field>
	</row>
	<row>
		<field name="c1">3</field>
	</row>
	</table_data>
	<triggers name="t2">
		<trigger Trigger="trig1" sql_mode="ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION" character_set_client="utf8mb4" collation_connection="utf8mb4_0900_ai_ci" Database_Collation="utf8mb4_0900_ai_ci" Created="--TIMESTAMP--">
<![CDATA[
CREATE DEFINER=`root`@`localhost` TRIGGER `trig1` BEFORE INSERT ON `t2` FOR EACH ROW BEGIN
INSERT INTO t2 VALUES(1);
END
]]>
		</trigger>
		<trigger Trigger="trig2" sql_mode="ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION" character_set_client="utf8mb4" collation_connection="utf8mb4_0900_ai_ci" Database_Collation="utf8mb4_0900_ai_ci" Created="--TIMESTAMP--">
<![CDATA[
CREATE DEFINER=`root`@`localhost` TRIGGER `trig2` AFTER INSERT ON `t2` FOR EACH ROW BEGIN
INSERT INTO t2 VALUES(1, ']]]]><![CDATA[>');
INSERT INTO t2 VALUES(2, '<![CDATA]]]]><![CDATA[>');
INSERT INTO t2 VALUES(3, '<![CDATA[');
INSERT INTO t2 VALUES(4, '< > & \ " _');
END
]]>
		</trigger>
	</triggers>
	<table_structure name="v1">
		<field Field="c1" Type="int" Null="YES" Key="" Extra="" Comment="" />
		<field Field="c2" Type="varchar(20)" Null="YES" Key="" Extra="" Comment="" />
		<options Name="v1" Create_time="--TIME--" Comment="VIEW" />
	</table_structure>
	<table_structure name="v2">
		<field Field="c1" Type="int" Null="YES" Key="" Extra="" Comment="" />
		<options Name="v2" Create_time="--TIME--" Comment="VIEW" />
	</table_structure>
	<events>
		<event Event="e1" sql_mode="ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION" time_zone="SYSTEM" character_set_client="utf8mb4" collation_connection="utf8mb4_0900_ai_ci" Database_Collation="utf8mb4_0900_ai_ci">
<![CDATA[
CREATE DEFINER=`root`@`localhost` EVENT `e1` ON SCHEDULE EVERY 1 SECOND STARTS '--TIME--' ON COMPLETION NOT PRESERVE ENABLE DO DROP DATABASE BUG52792
]]>
		</event>
		<event Event="e2" sql_mode="ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION" time_zone="SYSTEM" character_set_client="utf8mb4" collation_connection="utf8mb4_0900_ai_ci" Database_Collation="utf8mb4_0900_ai_ci">
<![CDATA[
CREATE DEFINER=`root`@`localhost` EVENT `e2` ON SCHEDULE EVERY 1 SECOND STARTS '--TIME--' ON COMPLETION NOT PRESERVE ENABLE DO DROP DATABASE BUG52792
]]>
		</event>
	</events>
	<routines>
<!-- 
- insufficient privileges to SHOW CREATE FUNCTION `hello1`
 -->
<!-- - does user1 have permissions on mysql.proc?

 -->
DROP USER user1;
DROP DATABASE BUG52792;
SET NAMES default;
USE test;
SET GLOBAL EVENT_SCHEDULER = ON;
