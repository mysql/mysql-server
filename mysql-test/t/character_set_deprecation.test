--echo #
--echo # WL10778: Parser: output deprecation warnings on utf8mb3 references, where
--echo # utf8mb3 is an alias of utf8mb3.
--echo #

--echo # Character set introducers.
SELECT _utf8mb3'abc';
SELECT n'abc';

--echo # convert().
SELECT CONVERT ( 'abc' USING utf8mb3 );
SELECT CAST( 'abc' AS NATIONAL CHAR );
SELECT CAST( 'abc' AS NCHAR );

--echo # cast().
SELECT CAST('test' AS CHAR CHARACTER SET utf8mb3);

--echo # Column definitions.
CREATE TABLE t1 ( a CHAR(1) ) CHARACTER SET utf8mb3;
CREATE TABLE t2 ( a CHAR(1) ) CHARACTER SET "utf8mb3";
CREATE TABLE t3 ( a CHAR(1) ) CHARACTER SET 'utf8mb3';
CREATE TABLE t4 ( a CHAR(1) ) CHARACTER SET `utf8mb3`;
CREATE TABLE t5 ( a NATIONAL CHAR(1) );
CREATE TABLE t6 ( a NCHAR(1) );
CREATE TABLE t7 ( a NCHAR );
CREATE TABLE t8 ( a NVARCHAR(1) );

DROP TABLE t1, t2, t3, t4, t5, t6, t7, t8;

--echo # Function definitions.
CREATE FUNCTION f1 ( a CHAR(1) CHARACTER SET utf8mb3 ) RETURNS INT RETURN 1;
CREATE FUNCTION f2 ( a CHAR(1) CHARACTER SET "utf8mb3" ) RETURNS INT RETURN 1;
CREATE FUNCTION f3 ( a CHAR(1) CHARACTER SET 'utf8mb3' ) RETURNS INT RETURN 1;
CREATE FUNCTION f4 ( a CHAR(1) CHARACTER SET `utf8mb3` ) RETURNS INT RETURN 1;
CREATE FUNCTION f5 ( a NATIONAL CHAR(1) ) RETURNS INT RETURN 1;
CREATE FUNCTION f6 ( a NCHAR(1) ) RETURNS INT RETURN 1;
CREATE FUNCTION f7 ( a NCHAR ) RETURNS INT RETURN 1;
CREATE FUNCTION f8 ( a NVARCHAR(1) ) RETURNS INT RETURN 1;

DROP FUNCTION f1;
DROP FUNCTION f2;
DROP FUNCTION f3;
DROP FUNCTION f4;
DROP FUNCTION f5;
DROP FUNCTION f6;
DROP FUNCTION f7;
DROP FUNCTION f8;

--echo # Columns clause in JSON table functions.
SELECT * FROM json_table('[]', '$[*]'
  COLUMNS (p CHAR(1) CHARACTER SET utf8mb3 PATH '$.a')) AS t;
SELECT * FROM json_table('[]', '$[*]'
  COLUMNS (p CHAR(1) CHARACTER SET "utf8mb3" PATH '$.a')) AS t;
SELECT * FROM json_table('[]', '$[*]'
  COLUMNS (p CHAR(1) CHARACTER SET 'utf8mb3' PATH '$.a')) AS t;
SELECT * FROM json_table('[]', '$[*]'
  COLUMNS (p CHAR(1) CHARACTER SET `utf8mb3` PATH '$.a')) AS t;
SELECT * FROM json_table('[]', '$[*]'
  COLUMNS (p NATIONAL CHAR(1) PATH '$.a')) AS t;
SELECT * FROM json_table('[]', '$[*]' COLUMNS (p NCHAR(1) PATH '$.a')) AS t;
SELECT * FROM json_table('[]', '$[*]' COLUMNS (p NCHAR PATH '$.a')) AS t;
SELECT * FROM json_table('[]', '$[*]' COLUMNS (p NVARCHAR(1) PATH '$.a')) AS t;

#
# WL#13594 Deprecate character set ucs2 and collations
#
--echo # Character set introducers.
SELECT HEX(_ucs2'abc');

--echo # convert().
SELECT CONVERT ( 'abc' USING ucs2 );

--echo # cast().
SELECT CAST('test' AS CHAR CHARACTER SET ucs2);

--echo # Column definitions.
CREATE TABLE t1 ( a CHAR(1) ) CHARACTER SET ucs2 COLLATE ucs2_persian_ci;
CREATE TABLE t2 ( a CHAR(1) ) CHARACTER SET "ucs2" COLLATE ucs2_persian_ci;
CREATE TABLE t3 ( a CHAR(1) ) CHARACTER SET 'ucs2' COLLATE ucs2_persian_ci;
CREATE TABLE t4 ( a CHAR(1) ) CHARACTER SET `ucs2`COLLATE ucs2_persian_ci;
DROP TABLE t1, t2, t3, t4;

--echo # Function definitions.
CREATE FUNCTION f1 ( a CHAR(1) CHARACTER SET ucs2 COLLATE ucs2_persian_ci ) RETURNS INT RETURN 1;
DROP FUNCTION f1;

--echo # Columns clause in JSON table functions.
SELECT * FROM json_table('[]', '$[*]'
  COLUMNS (p CHAR(1) CHARACTER SET ucs2 COLLATE ucs2_persian_ci PATH '$.a')) AS t;

--let $restart_parameters= "restart: --character-set-server=ucs2 --collation-server=ucs2_persian_ci"
--source include/restart_mysqld.inc
--let SEARCH_FILE=$MYSQLTEST_VARDIR/log/mysqld.1.err
--let SEARCH_PATTERN=The character set ucs2 is deprecated and will be removed in a future release. Please consider using utf8mb4 instead.
--source include/search_pattern.inc
--let SEARCH_PATTERN=is a collation of the deprecated character set ucs2. Please consider using utf8mb4 with an appropriate collation instead.
--source include/search_pattern.inc

--let $restart_parameters= "restart: "
--source include/restart_mysqld.inc

#
# WL#13595 Deprecate Classic Mac OS character sets and collations
#
--echo Deprecate macroman
--echo # Character set introducers.
SELECT HEX(_macroman'abc');

--echo # convert().
SELECT CONVERT ( 'abc' USING macroman );

--echo # cast().
SELECT CAST('test' AS CHAR CHARACTER SET macroman);

--echo # Column definitions.
CREATE TABLE t1 ( a CHAR(1) ) CHARACTER SET macroman COLLATE macroman_general_ci;
CREATE TABLE t2 ( a CHAR(1) ) CHARACTER SET "macroman" COLLATE macroman_general_ci;
CREATE TABLE t3 ( a CHAR(1) ) CHARACTER SET 'macroman' COLLATE macroman_general_ci;
CREATE TABLE t4 ( a CHAR(1) ) CHARACTER SET `macroman`COLLATE macroman_general_ci;
DROP TABLE t1, t2, t3, t4;

--echo # Function definitions.
CREATE FUNCTION f1 ( a CHAR(1) CHARACTER SET macroman COLLATE macroman_general_ci ) RETURNS INT RETURN 1;
DROP FUNCTION f1;

--echo # Columns clause in JSON table functions.
SELECT * FROM json_table('[]', '$[*]'
  COLUMNS (p CHAR(1) CHARACTER SET macroman COLLATE macroman_general_ci PATH '$.a')) AS t;

--let $restart_parameters= "restart: --character-set-server=macroman --collation-server=macroman_general_ci"
--source include/restart_mysqld.inc
--let SEARCH_FILE=$MYSQLTEST_VARDIR/log/mysqld.1.err
--let SEARCH_PATTERN=The character set macroman is deprecated and will be removed in a future release. Please consider using utf8mb4 instead.
--source include/search_pattern.inc
--let SEARCH_PATTERN=is a collation of the deprecated character set macroman. Please consider using utf8mb4 with an appropriate collation instead.
--source include/search_pattern.inc

--let $restart_parameters= "restart: "
--source include/restart_mysqld.inc

--echo Deprecate macce
--echo # Character set introducers.
SELECT HEX(_macce'abc');

--echo # convert().
SELECT CONVERT ( 'abc' USING macce );

--echo # cast().
SELECT CAST('test' AS CHAR CHARACTER SET macce);

--echo # Column definitions.
CREATE TABLE t1 ( a CHAR(1) ) CHARACTER SET macce COLLATE macce_general_ci;
CREATE TABLE t2 ( a CHAR(1) ) CHARACTER SET "macce" COLLATE macce_general_ci;
CREATE TABLE t3 ( a CHAR(1) ) CHARACTER SET 'macce' COLLATE macce_general_ci;
CREATE TABLE t4 ( a CHAR(1) ) CHARACTER SET `macce`COLLATE macce_general_ci;
DROP TABLE t1, t2, t3, t4;

--echo # Function definitions.
CREATE FUNCTION f1 ( a CHAR(1) CHARACTER SET macce COLLATE macce_general_ci ) RETURNS INT RETURN 1;
DROP FUNCTION f1;

--echo # Columns clause in JSON table functions.
SELECT * FROM json_table('[]', '$[*]'
  COLUMNS (p CHAR(1) CHARACTER SET macce COLLATE macce_general_ci PATH '$.a')) AS t;

--echo Use of old aliases
SET SESSION character_set_results=mac_latin2;
SELECT @@character_set_results;
SET SESSION character_set_results=macce_latin2;
SELECT @@character_set_results;
SET SESSION character_set_results=default;
SELECT @@character_set_results;

--let $restart_parameters= "restart: --character-set-server=macce --collation-server=macce_general_ci"
--source include/restart_mysqld.inc
--let SEARCH_FILE=$MYSQLTEST_VARDIR/log/mysqld.1.err
--let SEARCH_PATTERN=The character set macce is deprecated and will be removed in a future release. Please consider using utf8mb4 instead.
--source include/search_pattern.inc
--let SEARCH_PATTERN=is a collation of the deprecated character set macce. Please consider using utf8mb4 with an appropriate collation instead.
--source include/search_pattern.inc

--let $restart_parameters= "restart: "
--source include/restart_mysqld.inc

#
# WL#13596 Deprecate vendor specific DEC character set and collations
#
--echo Deprecate dec8
--echo # Character set introducers.
SELECT HEX(_dec8'abc');

--echo # convert().
SELECT CONVERT ( 'abc' USING dec8 );

--echo # cast().
SELECT CAST('test' AS CHAR CHARACTER SET dec8);

--echo # Column definitions.
CREATE TABLE t1 ( a CHAR(1) ) CHARACTER SET  dec8  COLLATE dec8_swedish_ci;
CREATE TABLE t2 ( a CHAR(1) ) CHARACTER SET "dec8" COLLATE dec8_swedish_ci;
CREATE TABLE t3 ( a CHAR(1) ) CHARACTER SET 'dec8' COLLATE dec8_swedish_ci;
CREATE TABLE t4 ( a CHAR(1) ) CHARACTER SET `dec8` COLLATE dec8_swedish_ci;
DROP TABLE t1, t2, t3, t4;

--echo # Function definitions.
CREATE FUNCTION f1 ( a CHAR(1) CHARACTER SET dec8 COLLATE dec8_swedish_ci ) RETURNS INT RETURN 1;
DROP FUNCTION f1;

--echo # Columns clause in JSON table functions.
SELECT * FROM json_table('[]', '$[*]'
  COLUMNS (p CHAR(1) CHARACTER SET dec8 COLLATE dec8_swedish_ci PATH '$.a')) AS t;

--let $restart_parameters= "restart: --character-set-server=dec8 --collation-server=dec8_swedish_ci"
--source include/restart_mysqld.inc
--let SEARCH_FILE=$MYSQLTEST_VARDIR/log/mysqld.1.err
--let SEARCH_PATTERN=The character set dec8 is deprecated and will be removed in a future release. Please consider using utf8mb4 instead.
--source include/search_pattern.inc
--let SEARCH_PATTERN='dec8_swedish_ci' is a collation of the deprecated character set dec8. Please consider using utf8mb4 with an appropriate collation instead.
--source include/search_pattern.inc

--let $restart_parameters= "restart: "
--source include/restart_mysqld.inc

#
# WL#13597 Deprecate vendor specific HP character set and collations
#
--echo Deprecate hp8
--echo # Character set introducers.
SELECT HEX(_hp8'abc');

--echo # convert().
SELECT CONVERT ( 'abc' USING hp8 );

--echo # cast().
SELECT CAST('test' AS CHAR CHARACTER SET hp8);

--echo # Column definitions.
CREATE TABLE t1 ( a CHAR(1) ) CHARACTER SET  hp8  COLLATE hp8_english_ci;
CREATE TABLE t2 ( a CHAR(1) ) CHARACTER SET "hp8" COLLATE hp8_english_ci;
CREATE TABLE t3 ( a CHAR(1) ) CHARACTER SET 'hp8' COLLATE hp8_english_ci;
CREATE TABLE t4 ( a CHAR(1) ) CHARACTER SET `hp8` COLLATE hp8_english_ci;
DROP TABLE t1, t2, t3, t4;

--echo # Function definitions.
CREATE FUNCTION f1 ( a CHAR(1) CHARACTER SET hp8 COLLATE hp8_english_ci ) RETURNS INT RETURN 1;
DROP FUNCTION f1;

--echo # Columns clause in JSON table functions.
SELECT * FROM json_table('[]', '$[*]'
  COLUMNS (p CHAR(1) CHARACTER SET hp8 COLLATE hp8_english_ci PATH '$.a')) AS t;

--let $restart_parameters= "restart: --character-set-server=hp8 --collation-server=hp8_english_ci"
--source include/restart_mysqld.inc
--let SEARCH_FILE=$MYSQLTEST_VARDIR/log/mysqld.1.err
--let SEARCH_PATTERN=The character set hp8 is deprecated and will be removed in a future release. Please consider using utf8mb4 instead.
--source include/search_pattern.inc
--let SEARCH_PATTERN='hp8_english_ci' is a collation of the deprecated character set hp8. Please consider using utf8mb4 with an appropriate collation instead.
--source include/search_pattern.inc

--let $restart_parameters= "restart: "
--source include/restart_mysqld.inc

