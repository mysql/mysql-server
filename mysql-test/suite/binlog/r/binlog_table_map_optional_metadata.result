RESET MASTER;
#
# Temporal types can be printed correctly
#
CREATE TABLE t1(c_year YEAR, c_date DATE, c_time TIME, c_time_f TIME(3),
c_datetime DATETIME, c_datetime_f DATETIME(3),
c_timestamp TIMESTAMP, c_timestamp_f TIMESTAMP(3) DEFAULT "2017-1-1 10:10:10");
INSERT INTO t1(c_year) VALUES(2017);
Matching lines are:
# Columns(YEAR,
#         DATE,
#         TIME,
#         TIME(3),
#         DATETIME,
#         DATETIME(3),
#         TIMESTAMP,
#         TIMESTAMP(3))
Occurrences of '# (?:Columns\(| {8})' in the input file: 8
DROP TABLE t1;
RESET MASTER;
#
# Json types can be printed correctly
#
CREATE TABLE t1 (c_json JSON, c_char CHAR(100));
INSERT INTO t1(c_char) VALUES("abc");
Matching lines are:
# Columns(JSON,
#         CHAR(100) CHARSET utf8mb4 COLLATE utf8mb4_0900_ai_ci)
Occurrences of '# (?:Columns\(| {8})' in the input file: 2
DROP TABLE t1;
RESET MASTER;
#
# Geometry types can be printed correctly
#
CREATE TABLE t1 (c_geo GEOMETRY, c_point POINT, c_linestring LINESTRING,
c_polygon POLYGON, c_multi_point MULTIPOINT,
c_multi_linestring MULTILINESTRING, c_multi_polygon MULTIPOLYGON,
c_geometrycollection GEOMETRYCOLLECTION, c_char CHAR(100));
INSERT INTO t1(c_point) VALUES(ST_PointFromText('POINT(10 10)'));
Matching lines are:
# Columns(GEOMETRY,
#         POINT,
#         LINESTRING,
#         POLYGON,
#         MULTIPOINT,
#         MULTILINESTRING,
#         MULTIPOLYGON,
#         GEOMCOLLECTION,
#         CHAR(100) CHARSET utf8mb4 COLLATE utf8mb4_0900_ai_ci)
Occurrences of '# (?:Columns\(| {8})' in the input file: 9
RESET MASTER;
SET GLOBAL binlog_row_metadata = FULL;
INSERT INTO t1(c_point) VALUES(ST_PointFromText('POINT(10 10)'));
Matching lines are:
# Columns(`c_geo` GEOMETRY,
#         `c_point` POINT,
#         `c_linestring` LINESTRING,
#         `c_polygon` POLYGON,
#         `c_multi_point` MULTIPOINT,
#         `c_multi_linestring` MULTILINESTRING,
#         `c_multi_polygon` MULTIPOLYGON,
#         `c_geometrycollection` GEOMCOLLECTION,
#         `c_char` CHAR(100) CHARSET utf8mb4 COLLATE utf8mb4_0900_ai_ci)
Occurrences of '# (?:Columns\(| {8})' in the input file: 9
DROP TABLE t1;
RESET MASTER;
#
# Numeric types can be printed correctly
#
CREATE TABLE t1(c_bit BIT(10), c_bool BOOL, c_smallint SMALLINT,
c_mediumint MEDIUMINT, c_int INT UNSIGNED, c_bigint BIGINT,
c_float FLOAT UNSIGNED, c_double DOUBLE, c_decimal DECIMAL(10, 2));
Warnings:
Warning	1681	UNSIGNED for decimal and floating point data types is deprecated and support for it will be removed in a future release.
SET GLOBAL binlog_row_metadata = MINIMAL;
INSERT INTO t1(c_bool) VALUES(1);
# UNSIGNED flag should be printed
Matching lines are:
# Columns(BIT(10),
#         TINYINT,
#         SMALLINT,
#         MEDIUMINT,
#         INT UNSIGNED,
#         BIGINT,
#         FLOAT UNSIGNED,
#         DOUBLE,
#         DECIMAL(10,2))
Occurrences of '# (?:Columns\(| {8})' in the input file: 9
RESET MASTER;
SET GLOBAL binlog_row_metadata = FULL;
INSERT INTO t1(c_bool) VALUES(1);
Matching lines are:
# Columns(`c_bit` BIT(10),
#         `c_bool` TINYINT,
#         `c_smallint` SMALLINT,
#         `c_mediumint` MEDIUMINT,
#         `c_int` INT UNSIGNED,
#         `c_bigint` BIGINT,
#         `c_float` FLOAT UNSIGNED,
#         `c_double` DOUBLE,
#         `c_decimal` DECIMAL(10,2))
Occurrences of '# (?:Columns\(| {8})' in the input file: 9
DROP TABLE t1;
RESET MASTER;
#
# Character types can be printed correctly
#
CREATE TABLE t1(c_char CHAR(10), c_varchar VARCHAR(500),
c_tinytext TINYTEXT, c_text TEXT,
c_mediumtext MEDIUMTEXT, c_longtext LONGTEXT CHARSET utf8);
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
SET GLOBAL binlog_row_metadata = MINIMAL;
INSERT INTO t1(c_char) VALUES("1");
Matching lines are:
# Columns(CHAR(10) CHARSET utf8mb4 COLLATE utf8mb4_0900_ai_ci,
#         VARCHAR(500) CHARSET utf8mb4 COLLATE utf8mb4_0900_ai_ci,
#         TINYTEXT CHARSET utf8mb4 COLLATE utf8mb4_0900_ai_ci,
#         TEXT CHARSET utf8mb4 COLLATE utf8mb4_0900_ai_ci,
#         MEDIUMTEXT CHARSET utf8mb4 COLLATE utf8mb4_0900_ai_ci,
#         LONGTEXT CHARSET utf8mb3 COLLATE utf8mb3_general_ci)
Occurrences of '# (?:Columns\(| {8})' in the input file: 6
RESET MASTER;
SET GLOBAL binlog_row_metadata = FULL;
INSERT INTO t1(c_char) VALUES("1");
Matching lines are:
# Columns(`c_char` CHAR(10) CHARSET utf8mb4 COLLATE utf8mb4_0900_ai_ci,
#         `c_varchar` VARCHAR(500) CHARSET utf8mb4 COLLATE utf8mb4_0900_ai_ci,
#         `c_tinytext` TINYTEXT CHARSET utf8mb4 COLLATE utf8mb4_0900_ai_ci,
#         `c_text` TEXT CHARSET utf8mb4 COLLATE utf8mb4_0900_ai_ci,
#         `c_mediumtext` MEDIUMTEXT CHARSET utf8mb4 COLLATE utf8mb4_0900_ai_ci,
#         `c_longtext` LONGTEXT CHARSET utf8mb3 COLLATE utf8mb3_general_ci)
Occurrences of '# (?:Columns\(| {8})' in the input file: 6
DROP TABLE t1;
RESET MASTER;
#
# Column names with non-ascii characters and escape characters can be printed correctly
#
CREATE TABLE t1(`åäö表\a'``"` INT);
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `åäö表\a'``"` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
INSERT INTO t1 VALUES(1);
Matching lines are:
# Columns(`åäö表\\a\'``"` INT)
Occurrences of '# (?:Columns\(| {8})' in the input file: 1
DROP TABLE t1;
RESET MASTER;
#
# Charsets can be printed correctly
#
CREATE TABLE t1(c_char_utf8 CHAR(10) CHARSET utf8,
c_varchar_utf8 VARCHAR(10) CHARSET utf8,
c_text_utf8 TEXT CHARSET utf8);
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
INSERT INTO t1 VALUES("1", "2", "3");
Matching lines are:
# Columns(`c_char_utf8` CHAR(10) CHARSET utf8mb3 COLLATE utf8mb3_general_ci,
#         `c_varchar_utf8` VARCHAR(10) CHARSET utf8mb3 COLLATE utf8mb3_general_ci,
#         `c_text_utf8` TEXT CHARSET utf8mb3 COLLATE utf8mb3_general_ci)
Occurrences of '# (?:Columns\(| {8})' in the input file: 3
DROP TABLE t1;
RESET MASTER;
CREATE TABLE t1(c_utf8mb4_520 CHAR(10) CHARSET utf8mb4 COLLATE utf8mb4_unicode_520_ci,
c_utf8mb4_0900 VARCHAR(10) CHARSET utf8mb4 COLLATE utf8mb4_pl_0900_as_cs,
c_utf8mb4_def TEXT CHARSET utf8mb4);
INSERT INTO t1 VALUES("1", "2", "3");
Matching lines are:
# Columns(`c_utf8mb4_520` CHAR(10) CHARSET utf8mb4 COLLATE utf8mb4_unicode_520_ci,
#         `c_utf8mb4_0900` VARCHAR(10) CHARSET utf8mb4 COLLATE utf8mb4_pl_0900_as_cs,
#         `c_utf8mb4_def` TEXT CHARSET utf8mb4 COLLATE utf8mb4_0900_ai_ci)
Occurrences of '# (?:Columns\(| {8})' in the input file: 3
DROP TABLE t1;
RESET MASTER;
#
# Blob and binary columns can be printed correctly
#
CREATE TABLE t1(c_binary BINARY(10), c_varbinary VARBINARY(10),
c_tinyblob TINYBLOB, c_blob BLOB,
c_mediumblob MEDIUMBLOB, c_longblob LONGBLOB);
INSERT INTO t1 VALUES("1", "2", "3", "4", "5", "6");
Matching lines are:
# Columns(`c_binary` BINARY(10),
#         `c_varbinary` VARBINARY(10),
#         `c_tinyblob` TINYBLOB,
#         `c_blob` BLOB,
#         `c_mediumblob` MEDIUMBLOB,
#         `c_longblob` LONGBLOB)
Occurrences of '# (?:Columns\(| {8})' in the input file: 6
DROP TABLE t1;
RESET MASTER;
#
# Verify that SET string values and character sets can be printed correctly
#
CREATE TABLE t1(
c_set_1 SET("set1_v1_å", "set1_v2_ä", "set1_v3_ö"),
c_set_2 SET("set2_v1_å", "set2_v2_ä", "set2_v3_ö") CHARACTER SET binary,
c_set_3 SET("set3_v1_å", "set3_v2_ä", "set3_v3_ö") CHARACTER SET latin1,
c_set_4 SET("set4_v1_å", "set4_v2_ä", "set4_v3_ö") CHARACTER SET swe7 COLLATE swe7_bin,
c_set_5 SET("set5_v1_å", "set5_v2_ä", "set5_v3_ö") CHARACTER SET ucs2,
c_set_6 SET("set6_v1_å", "set6_v2_ä", "set6_v3_ö") CHARACTER SET utf32
);
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
SET GLOBAL binlog_row_metadata = MINIMAL;
INSERT INTO t1 VALUES("set1_v1_å", "set2_v2_ä", "set3_v3_ö", "set4_v1_å", "set5_v2_ä", "set6_v3_ö");
Matching lines are:
# Columns(SET,
#         SET,
#         SET,
#         SET,
#         SET,
#         SET)
Occurrences of '# (?:Columns\(| {8})' in the input file: 6
RESET MASTER;
SET GLOBAL binlog_row_metadata = FULL;
INSERT INTO t1 VALUES("set1_v1_å", "set2_v2_ä", "set3_v3_ö", "set4_v1_å", "set5_v2_ä", "set6_v3_ö");
Matching lines are:
# Columns(`c_set_1` SET('set1_v1_å', 'set1_v2_ä', 'set1_v3_ö') CHARSET utf8mb4 COLLATE utf8mb4_0900_ai_ci,
#         `c_set_2` SET('set2_v1_å', 'set2_v2_ä', 'set2_v3_ö') CHARSET binary COLLATE binary,
#         `c_set_3` SET('set3_v1_�', 'set3_v2_�', 'set3_v3_�') CHARSET latin1 COLLATE latin1_swedish_ci,
#         `c_set_4` SET('set4_v1_}', 'set4_v2_{', 'set4_v3_|') CHARSET swe7 COLLATE swe7_bin,
#         `c_set_5` SET('\0s\0e\0t\05\0_\0v\01\0_\0�', '\0s\0e\0t\05\0_\0v\02\0_\0�', '\0s\0e\0t\05\0_\0v\03\0_\0�') CHARSET ucs2 COLLATE ucs2_general_ci,
#         `c_set_6` SET('\0\0\0s\0\0\0e\0\0\0t\0\0\06\0\0\0_\0\0\0v\0\0\01\0\0\0_\0\0\0�', '\0\0\0s\0\0\0e\0\0\0t\0\0\06\0\0\0_\0\0\0v\0\0\02\0\0\0_\0\0\0�', '\0\0\0s\0\0\0e\0\0\0t\0\0\06\0\0\0_\0\0\0v\0\0\03\0\0\0_\0\0\0�') CHARSET utf32 COLLATE utf32_general_ci)
Occurrences of '# (?:Columns\(| {8})' in the input file: 6
DROP TABLE t1;
RESET MASTER;
#
# Verify that ENUM string values and character sets can be printed correctly
#
CREATE TABLE t1(
c_enum_1 ENUM("enum1_v1_å", "enum1_v2_ä", "enum1_v3_ö"),
c_enum_2 ENUM("enum2_v1_å", "enum2_v2_ä", "enum2_v3_ö") CHARACTER SET binary,
c_enum_3 ENUM("enum3_v1_å", "enum3_v2_ä", "enum3_v3_ö") CHARACTER SET latin1,
c_enum_4 ENUM("enum4_v1_å", "enum4_v2_ä", "enum4_v3_ö") CHARACTER SET swe7 COLLATE swe7_bin,
c_enum_5 ENUM("enum5_v1_å", "enum5_v2_ä", "enum5_v3_ö") CHARACTER SET ucs2,
c_enum_6 ENUM("enum6_v1_å", "enum6_v2_ä", "enum6_v3_ö") CHARACTER SET utf32
);
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
SET GLOBAL binlog_row_metadata = MINIMAL;
INSERT INTO t1 VALUES("enum1_v1_å", "enum2_v2_ä", "enum3_v3_ö", "enum4_v1_å", "enum5_v2_ä", "enum6_v3_ö");
Matching lines are:
# Columns(ENUM,
#         ENUM,
#         ENUM,
#         ENUM,
#         ENUM,
#         ENUM)
Occurrences of '# (?:Columns\(| {8})' in the input file: 6
RESET MASTER;
SET GLOBAL binlog_row_metadata = FULL;
INSERT INTO t1 VALUES("enum1_v1_å", "enum2_v2_ä", "enum3_v3_ö", "enum4_v1_å", "enum5_v2_ä", "enum6_v3_ö");
Matching lines are:
# Columns(`c_enum_1` ENUM('enum1_v1_å', 'enum1_v2_ä', 'enum1_v3_ö') CHARSET utf8mb4 COLLATE utf8mb4_0900_ai_ci,
#         `c_enum_2` ENUM('enum2_v1_å', 'enum2_v2_ä', 'enum2_v3_ö') CHARSET binary COLLATE binary,
#         `c_enum_3` ENUM('enum3_v1_�', 'enum3_v2_�', 'enum3_v3_�') CHARSET latin1 COLLATE latin1_swedish_ci,
#         `c_enum_4` ENUM('enum4_v1_}', 'enum4_v2_{', 'enum4_v3_|') CHARSET swe7 COLLATE swe7_bin,
#         `c_enum_5` ENUM('\0e\0n\0u\0m\05\0_\0v\01\0_\0�', '\0e\0n\0u\0m\05\0_\0v\02\0_\0�', '\0e\0n\0u\0m\05\0_\0v\03\0_\0�') CHARSET ucs2 COLLATE ucs2_general_ci,
#         `c_enum_6` ENUM('\0\0\0e\0\0\0n\0\0\0u\0\0\0m\0\0\06\0\0\0_\0\0\0v\0\0\01\0\0\0_\0\0\0�', '\0\0\0e\0\0\0n\0\0\0u\0\0\0m\0\0\06\0\0\0_\0\0\0v\0\0\02\0\0\0_\0\0\0�', '\0\0\0e\0\0\0n\0\0\0u\0\0\0m\0\0\06\0\0\0_\0\0\0v\0\0\03\0\0\0_\0\0\0�') CHARSET utf32 COLLATE utf32_general_ci)
Occurrences of '# (?:Columns\(| {8})' in the input file: 6
DROP TABLE t1;
RESET MASTER;
#
# Verify that explicit NOT NULL can be printed correctly
#
CREATE TABLE t1(c_not_null1 INT NOT NULL, c_null1 INT, c_not_null2 INT NOT NULL,
c_null2 INT);
INSERT INTO t1 VALUES(1, 2, 3, 4);
Matching lines are:
# Columns(`c_not_null1` INT NOT NULL,
#         `c_null1` INT,
#         `c_not_null2` INT NOT NULL,
#         `c_null2` INT)
Occurrences of '# (?:Columns\(| {8})' in the input file: 4
DROP TABLE t1;
RESET MASTER;
#
# Verify that primary key can be printed correctly
#
CREATE TABLE t1(c_key1 INT, c_key3 INT, c_not_key INT, c_key2 INT,
PRIMARY KEY(c_key1, c_key2, c_key3));
INSERT INTO t1 VALUES(1, 2, 3, 4);
Matching lines are:
# Columns(`c_key1` INT NOT NULL,
#         `c_key3` INT NOT NULL,
#         `c_not_key` INT,
#         `c_key2` INT NOT NULL)
Occurrences of '# (?:Columns\(| {8})' in the input file: 4
Matching lines are:
# Primary Key(c_key1, c_key2, c_key3)
Occurrences of '# Primary Key' in the input file: 1
DROP TABLE t1;
RESET MASTER;
CREATE TABLE t1(c_key1 CHAR(100), c_key3 CHAR(100), c_not_key INT, c_key2 CHAR(10),
PRIMARY KEY(c_key1(5), c_key2, c_key3(10)));
INSERT INTO t1 VALUES("1", "2", 3, "4");
Matching lines are:
# Columns(`c_key1` CHAR(100) NOT NULL CHARSET utf8mb4 COLLATE utf8mb4_0900_ai_ci,
#         `c_key3` CHAR(100) NOT NULL CHARSET utf8mb4 COLLATE utf8mb4_0900_ai_ci,
#         `c_not_key` INT,
#         `c_key2` CHAR(10) NOT NULL CHARSET utf8mb4 COLLATE utf8mb4_0900_ai_ci)
Occurrences of '# (?:Columns\(| {8})' in the input file: 4
Matching lines are:
# Primary Key(c_key1(5), c_key2, c_key3(10))
Occurrences of '# Primary Key' in the input file: 1
RESET MASTER;
SET GLOBAL binlog_row_metadata = MINIMAL;
INSERT INTO t1 VALUES("2", "2", 3, "4");
Matching lines are:
# Columns(CHAR(100) NOT NULL CHARSET utf8mb4 COLLATE utf8mb4_0900_ai_ci,
#         CHAR(100) NOT NULL CHARSET utf8mb4 COLLATE utf8mb4_0900_ai_ci,
#         INT,
#         CHAR(10) NOT NULL CHARSET utf8mb4 COLLATE utf8mb4_0900_ai_ci)
Occurrences of '# (?:Columns\(| {8})' in the input file: 4
Matching lines are:
None
Occurrences of '# Primary Key' in the input file: 0
RESET MASTER;
#
# Coverage test: Print column index instead of column name if column name
#                is not binlogged.
#
SET GLOBAL binlog_row_metadata = FULL;
SET SESSION debug = 'd, dont_log_column_name';
INSERT INTO t1 VALUES("3", "2", 3, "4");
Matching lines are:
# Columns(CHAR(100) NOT NULL CHARSET utf8mb4 COLLATE utf8mb4_0900_ai_ci,
#         CHAR(100) NOT NULL CHARSET utf8mb4 COLLATE utf8mb4_0900_ai_ci,
#         INT,
#         CHAR(10) NOT NULL CHARSET utf8mb4 COLLATE utf8mb4_0900_ai_ci)
Occurrences of '# (?:Columns\(| {8})' in the input file: 4
Matching lines are:
# Primary Key(0(5), 3, 1(10))
Occurrences of '# Primary Key' in the input file: 1
DROP TABLE t1;
RESET MASTER;
#
# Coverage test: Inject an invalid column type
#
CREATE TABLE t1(c1 int, c2 BLOB);
SET SESSION debug = 'd,inject_invalid_column_type';
INSERT INTO t1 VALUES(1, "a");
Matching lines are:
# Columns(`c1` INT,
#         `c2` INVALID_TYPE(230))
Occurrences of '# (?:Columns\(| {8})' in the input file: 2
RESET MASTER;
#
# Coverage test: Inject an invalid BLOB metadata
#
SET SESSION debug = 'd,inject_invalid_blob_size';
INSERT INTO t1 VALUES(2, "b");
Matching lines are:
# Columns(`c1` INT,
#         `c2` INVALID_BLOB(5))
Occurrences of '# (?:Columns\(| {8})' in the input file: 2
#
# Coverage test: Inject an invalid Geometry type
#
DROP TABLE t1;
CREATE TABLE t1(c_geometry GEOMETRY, c_point POINT, c_multilinestring MULTILINESTRING);
RESET MASTER;
SET SESSION debug = 'd,inject_invalid_geometry_type';
INSERT INTO t1(c_point) VALUES(ST_PointFromText('POINT(10 10)'));
Matching lines are:
# Columns(`c_geometry` INVALID_GEOMETRY_TYPE(100),
#         `c_point` INVALID_GEOMETRY_TYPE(100),
#         `c_multilinestring` INVALID_GEOMETRY_TYPE(100))
Occurrences of '# (?:Columns\(| {8})' in the input file: 3
DROP TABLE t1;
RESET MASTER;
#
# Comptibility Test: Verify mysqlbinlog can print OLD table_map_log_event
# without any optional metadata
#
CREATE TABLE t1(c_int INT, c_tiny_int_unsigned TINYINT UNSIGNED,
c_binary BINARY(10), c_text TEXT, c_point POINT);
SET session debug='d,simulate_no_optional_metadata';
INSERT INTO t1(c_int) VALUES(1);
Matching lines are:
# Columns(INT,
#         TINYINT,
#         BINARY(10),
#         BLOB,
#         GEOMETRY)
Occurrences of '# (?:Columns\(| {8})' in the input file: 5
DROP TABLE t1;
RESET MASTER;
#
# Simulate error on initializing charset and primary key metadata
#
CREATE TABLE t1(c1 char(10) PRIMARY KEY);
SET session debug='d,simulate_init_charset_field_error';
INSERT INTO t1 VALUES("a");
SET GLOBAL binlog_row_metadata = FULL;
SET session debug='d,simulate_init_primary_key_field_error';
INSERT INTO t1 VALUES("b");
Matching lines are:
# Columns(BINARY(40) NOT NULL)
# Columns(BINARY(40) NOT NULL)
Occurrences of '# (?:Columns\(| {8})' in the input file: 2
Matching lines are:
None
Occurrences of '# Primary Key' in the input file: 0
SET SESSION debug = '';
SET GLOBAL binlog_row_metadata = MINIMAL;
DROP TABLE t1;
RESET MASTER;
#
# WL#10905 - Support for INVISIBLE columns.
#
# Test case to verify that INVISIBLE clause can be printed correctly.
CREATE TABLE t1 (c1 INT,
c2 INT UNSIGNED INVISIBLE DEFAULT(10),
c3 INT UNSIGNED NOT NULL INVISIBLE DEFAULT(90),
c4 INT UNSIGNED PRIMARY KEY INVISIBLE DEFAULT(5),
c5 VARCHAR(100) CHARSET ucs2 INVISIBLE,
c6 GEOMETRY INVISIBLE,
c7 ENUM("enum_v1", "enum_v2", "enum_v3") CHARACTER SET swe7 COLLATE swe7_bin INVISIBLE,
c8 SET("set_v1", "set_v2", "set_v3") CHARACTER SET swe7 COLLATE swe7_bin INVISIBLE);
Warnings:
Warning	1287	'ucs2' is deprecated and will be removed in a future release. Please use utf8mb4 instead
SET GLOBAL binlog_row_metadata = FULL;
INSERT INTO t1 VALUES(1);
Matching lines are:
# Columns(`c1` INT,
#         `c2` INT UNSIGNED INVISIBLE,
#         `c3` INT UNSIGNED NOT NULL INVISIBLE,
#         `c4` INT UNSIGNED NOT NULL INVISIBLE,
#         `c5` VARCHAR(100) CHARSET ucs2 COLLATE ucs2_general_ci INVISIBLE,
#         `c6` GEOMETRY INVISIBLE,
#         `c7` ENUM('enum_v1', 'enum_v2', 'enum_v3') CHARSET swe7 COLLATE swe7_bin INVISIBLE,
#         `c8` SET('set_v1', 'set_v2', 'set_v3') CHARSET swe7 COLLATE swe7_bin INVISIBLE)
Occurrences of '# (?:Columns\(| {8})' in the input file: 8
Matching lines are:
# Primary Key(c4)
Occurrences of '# Primary Key' in the input file: 1
UPDATE t1 SET c1=4 WHERE c4=5;
Matching lines are:
# Columns(`c1` INT,
#         `c2` INT UNSIGNED INVISIBLE,
#         `c3` INT UNSIGNED NOT NULL INVISIBLE,
#         `c4` INT UNSIGNED NOT NULL INVISIBLE,
#         `c5` VARCHAR(100) CHARSET ucs2 COLLATE ucs2_general_ci INVISIBLE,
#         `c6` GEOMETRY INVISIBLE,
#         `c7` ENUM('enum_v1', 'enum_v2', 'enum_v3') CHARSET swe7 COLLATE swe7_bin INVISIBLE,
#         `c8` SET('set_v1', 'set_v2', 'set_v3') CHARSET swe7 COLLATE swe7_bin INVISIBLE)
Occurrences of '# (?:Columns\(| {8})' in the input file: 8
Matching lines are:
# Primary Key(c4)
Occurrences of '# Primary Key' in the input file: 1
RESET MASTER;
# INVISIBLE clause is included with only binlog_row_metadata = FULL.
SET GLOBAL binlog_row_metadata = MINIMAL;
INSERT INTO t1(c1, c4) VALUES(3, 6);
Matching lines are:
# Columns(INT,
#         INT UNSIGNED,
#         INT UNSIGNED NOT NULL,
#         INT UNSIGNED NOT NULL,
#         VARCHAR(100) CHARSET ucs2 COLLATE ucs2_general_ci,
#         GEOMETRY,
#         ENUM,
#         SET)
Occurrences of '# (?:Columns\(| {8})' in the input file: 8
Matching lines are:
None
Occurrences of '# Primary Key' in the input file: 0
UPDATE t1 SET c1=4 WHERE c4=6;
Matching lines are:
# Columns(INT,
#         INT UNSIGNED,
#         INT UNSIGNED NOT NULL,
#         INT UNSIGNED NOT NULL,
#         VARCHAR(100) CHARSET ucs2 COLLATE ucs2_general_ci,
#         GEOMETRY,
#         ENUM,
#         SET)
# Columns(INT,
#         INT UNSIGNED,
#         INT UNSIGNED NOT NULL,
#         INT UNSIGNED NOT NULL,
#         VARCHAR(100) CHARSET ucs2 COLLATE ucs2_general_ci,
#         GEOMETRY,
#         ENUM,
#         SET)
Occurrences of '# (?:Columns\(| {8})' in the input file: 16
Matching lines are:
None
Occurrences of '# Primary Key' in the input file: 0
DROP TABLE t1;
RESET MASTER;
