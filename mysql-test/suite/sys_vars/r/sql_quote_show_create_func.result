** Setup **

SET @default_sql_quote_show_create = @@sql_quote_show_create;
CREATE TEMPORARY TABLE t1(a varchar(20), b varchar(20));
'#-----------------------------FN_DYNVARS_163_01------------------------------------#'
SET SESSION sql_quote_show_create = TRUE;
SHOW CREATE DATABASE test;
Database	Create Database
test	CREATE DATABASE `test` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */
EXPECTING identifiers test TO BE quoted like 'test'
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TEMPORARY TABLE `t1` (
  `a` varchar(20) DEFAULT NULL,
  `b` varchar(20) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTING identifiers a, b, t1 TO BE quoted like 'a','b','t1'
'#-----------------------------FN_DYNVARS_163_02------------------------------------#'
SET SESSION sql_quote_show_create = FALSE;
SHOW CREATE DATABASE test;
Database	Create Database
test	CREATE DATABASE test /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */
EXPECTING identifiers test NOT TO BE quoted like 'test'
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TEMPORARY TABLE t1 (
  a varchar(20) DEFAULT NULL,
  b varchar(20) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTING identifiers a, b, t1 NOT TO BE quoted like 'a','b','t1'
'#----------------------------FN_DYNVARS_163_03--------------------------------------#'
** Connecting con_int1 using root **
** Connection con_int1 **
SELECT @@SESSION.sql_quote_show_create;
@@SESSION.sql_quote_show_create
1
1 / TRUE Expected
SET SESSION sql_quote_show_create = FALSE;
** Connecting con_int2 using root **
** Connection con_int2 **
SELECT @@SESSION.sql_quote_show_create;
@@SESSION.sql_quote_show_create
1
1 / TRUE Expected
SET SESSION sql_quote_show_create = TRUE;
** Connection con_int2 **
SELECT @@SESSION.sql_quote_show_create;
@@SESSION.sql_quote_show_create
1
1 / TRUE Expected
** Connection con_int1 **
SELECT @@SESSION.sql_quote_show_create;
@@SESSION.sql_quote_show_create
0
0 / FALSE Expected
** Connection default **
Disconnecting Connections con_int1, con_int2

Cleanup
SET @@sql_quote_show_create = @default_sql_quote_show_create;
DROP TABLE t1;
