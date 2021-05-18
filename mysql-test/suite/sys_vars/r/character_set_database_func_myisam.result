'#--------------------FN_DYNVARS_008_01-------------------------#'
SET @global_character_set_database = @@global.character_set_database;
SET @session_character_set_database = @@session.character_set_database;
SET @session_character_set_server = @@session.character_set_server;
SET @global_character_set_server = @@global.character_set_server;
SET @@global.character_set_database = utf8;
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
Warning	1681	Updating 'character_set_database' is deprecated. It will be made read-only in a future release.
'connect (con1,localhost,root,,,,)'
'connection con1'
SELECT @@global.character_set_database;
@@global.character_set_database
utf8mb3
SELECT @@session.character_set_database;
@@session.character_set_database
utf8mb4
'#--------------------FN_DYNVARS_008_02-------------------------#'
'connection default'
DROP TABLE IF EXISTS t1;
'------Check if load data uses character_set_database----------'
USE test;
CREATE TABLE t1 (a CHAR(4) CHARACTER SET utf8) ENGINE=MyISAM CHARACTER SET utf8;
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
'Verify with latin';
SET @@session.character_set_database = latin1;
Warnings:
Warning	1681	Updating 'character_set_database' is deprecated. It will be made read-only in a future release.
LOAD DATA INFILE 'MYSQLTEST_VARDIR/std_data/charset_utf8.txt' INTO TABLE t1;
SELECT count(*) FROM t1 WHERE CHAR_LENGTH(a)>1;
count(*)
2433
TRUNCATE TABLE t1;
'Verify with utf8';
SET @@session.character_set_database = utf8;
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
Warning	1681	Updating 'character_set_database' is deprecated. It will be made read-only in a future release.
LOAD DATA INFILE 'MYSQLTEST_VARDIR/std_data/charset_utf8.txt' INTO TABLE t1;
SELECT count(*) FROM t1 WHERE CHAR_LENGTH(a)>1;
count(*)
1
DROP TABLE IF EXISTS t1;
SET @@global.character_set_database = @global_character_set_database;
Warnings:
Warning	1681	Updating 'character_set_database' is deprecated. It will be made read-only in a future release.
SET @@session.character_set_database = @session_character_set_database;
Warnings:
Warning	1681	Updating 'character_set_database' is deprecated. It will be made read-only in a future release.
SET @@session.character_set_server = @session_character_set_server;
SET @@global.character_set_server = @global_character_set_server;
