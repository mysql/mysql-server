# mysqlbinlog_row.test
#
# Show that mysqlbinlog displays human readable comments to
# row-based log events.
#
# Procedure:
#   Create a table that represents all-known types in 5.1.
#   Write rows that contain the mins, maxes, and NULL for each type.
#   Write a random or "problematic" value (i.e. one that might require
#     escaping if it's represented as a string-y type) for each type.
#   Update each of these rows.
#   Delete each of these rows.
#   Inspect the binlog.
#
# Bug#31455 - mysqlbinlog don't print user readable info about RBR events
#

--source include/have_log_bin.inc

SET NAMES 'utf8';
#SHOW VARIABLES LIKE 'character_set%';


--echo #
--echo # Preparatory cleanup.
--echo #
--disable_warnings
DROP TABLE IF EXISTS t1, t2, t3;
--enable_warnings

--echo #
--echo # We need a fixed timestamp to avoid varying results.
--echo #
SET timestamp=1000000000;

--echo #
--echo # ===================================================
--echo # Test #1 - Insert/update/delete with all data types.
--echo # ===================================================
--echo #
--echo # Delete all existing binary logs.
--echo #
RESET MASTER;

--echo #
--echo # Create a test table with all data types.
--echo #
eval CREATE TABLE t1 (
  c01 BIT,
  c02 BIT(64),
  c03 TINYINT,
  c04 TINYINT UNSIGNED,
  c05 TINYINT ZEROFILL,
  c06 BOOL,
  c07 SMALLINT,
  c08 SMALLINT UNSIGNED,
  c09 SMALLINT ZEROFILL,
  c10 MEDIUMINT,
  c11 MEDIUMINT UNSIGNED,
  c12 MEDIUMINT ZEROFILL,
  c13 INT,
  c14 INT UNSIGNED,
  c15 INT ZEROFILL,
  c16 BIGINT,
  c17 BIGINT UNSIGNED,
  c18 BIGINT ZEROFILL,
  c19 FLOAT,
  c20 FLOAT UNSIGNED,
  c21 FLOAT ZEROFILL,
  c22 DOUBLE,
  c23 DOUBLE UNSIGNED,
  c24 DOUBLE ZEROFILL,
  c25 DECIMAL,
  c26 DECIMAL UNSIGNED,
  c27 DECIMAL ZEROFILL,
  #
  c28 DATE,
  c29 DATETIME,
  c30 TIMESTAMP,
  c31 TIME,
  c32 YEAR,
  #
  c33 CHAR,
  c34 CHAR(0),
  c35 CHAR(1),
  c36 CHAR(255),
  c37 NATIONAL CHAR,
  c38 NATIONAL CHAR(0),
  c39 NATIONAL CHAR(1),
  c40 NATIONAL CHAR(255),
  c41 CHAR CHARACTER SET UCS2,
  c42 CHAR(0) CHARACTER SET UCS2,
  c43 CHAR(1) CHARACTER SET UCS2,
  c44 CHAR(255) CHARACTER SET UCS2,
  #
  c45 VARCHAR(0),
  c46 VARCHAR(1),
  c47 VARCHAR(255),
  c48 VARCHAR(261),
  c49 NATIONAL VARCHAR(0),
  c50 NATIONAL VARCHAR(1),
  c51 NATIONAL VARCHAR(255),
  c52 NATIONAL VARCHAR(261),
  c53 VARCHAR(0) CHARACTER SET UCS2,
  c54 VARCHAR(1) CHARACTER SET UCS2,
  c55 VARCHAR(255) CHARACTER SET UCS2,
  c56 VARCHAR(261) CHARACTER SET UCS2,
  #
  c57 BINARY,
  c58 BINARY(0),
  c59 BINARY(1),
  c60 BINARY(255),
  #
  c61 VARBINARY(0),
  c62 VARBINARY(1),
  c63 VARBINARY(255),
  c64 VARBINARY(261),
  #
  c65 TINYBLOB,
  c66 TINYTEXT,
  c67 TINYTEXT CHARACTER SET UCS2,
  c68 BLOB,
  c69 TEXT,
  c70 TEXT CHARACTER SET UCS2,
  c71 MEDIUMBLOB,
  c72 MEDIUMTEXT,
  c73 MEDIUMTEXT CHARACTER SET UCS2,
  c74 LONGBLOB,
  c75 LONGTEXT,
  c76 LONGTEXT CHARACTER SET UCS2,
  #
  c77 ENUM('a','b','c'),
  c78 SET('a','b','c'),
  #
  crn INT -- row number
  ) ENGINE=$engine_type DEFAULT CHARSET latin1;

--echo #
--echo # Insert minimum values.
--echo #
INSERT INTO t1 VALUES (
  b'0',                         -- c01
  b'0000000000000000000000000000000000000000000000000000000000000000',  -- c02
  -128,                         -- c03
  0,                            -- c04
  000,                          -- c05
  false,                        -- c06
  -32768,                       -- c07
  0,                            -- c08
  00000,                        -- c09
  -8388608,                     -- c10
  0,                            -- c11
  00000000,                     -- c12
  -2147483648,                  -- c13
  0,                            -- c14
  0000000000,                   -- c15
  -9223372036854775808,         -- c16
  0,                            -- c17
  00000000000000000000,         -- c18
  -3.402823466E+38,             -- c19
  1.175494351E-38,              -- c20
  000000000000,                 -- c21
  -1.7976931348623E+308,        -- c22 three digits cut for ps-protocol
  2.2250738585072E-308,         -- c23 three digits cut for ps-protocol
  0000000000000000000000,       -- c24
  -9999999999,                  -- c25
  0,                            -- c26
  0000000000,                   -- c27
  #
  '1000-01-01',                 -- c28
  '1000-01-01 00:00:00',        -- c29
  '1970-01-02 00:00:01',        -- c30 one day later due to timezone issues
  '-838:59:59',                 -- c31
  '1901',                       -- c32
  #
  '',                           -- c33
  '',                           -- c34
  '',                           -- c35
  '',                           -- c36
  '',                           -- c37
  '',                           -- c38
  '',                           -- c39
  '',                           -- c40
  '',                           -- c41
  '',                           -- c42
  '',                           -- c43
  '',                           -- c44
  #
  '',                           -- c45
  '',                           -- c46
  '',                           -- c47
  '',                           -- c48
  '',                           -- c49
  '',                           -- c50
  '',                           -- c51
  '',                           -- c52
  '',                           -- c53
  '',                           -- c54
  '',                           -- c55
  '',                           -- c56
  #
  '',                           -- c57
  '',                           -- c58
  '',                           -- c59
  '',                           -- c60
  #
  '',                           -- c61
  '',                           -- c62
  '',                           -- c63
  '',                           -- c64
  #
  '',                           -- c65
  '',                           -- c66
  '',                           -- c67
  '',                           -- c68
  '',                           -- c69
  '',                           -- c70
  '',                           -- c71
  '',                           -- c72
  '',                           -- c73
  '',                           -- c74
  '',                           -- c75
  '',                           -- c76
  #
  'a',                          -- c77
  '',                           -- c78
  #
  1                             -- crn -- row number
  );

--echo #
--echo # Insert maximum values.
--echo #
INSERT INTO t1 VALUES (
  b'1',                         -- c01
  b'1111111111111111111111111111111111111111111111111111111111111111',  -- c02
  127,                          -- c03
  255,                          -- c04
  255,                          -- c05
  true,                         -- c06
  32767,                        -- c07
  65535,                        -- c08
  65535,                        -- c09
  8388607,                      -- c10
  16777215,                     -- c11
  16777215,                     -- c12
  2147483647,                   -- c13
  4294967295,                   -- c14
  4294967295,                   -- c15
  9223372036854775807,          -- c16
  18446744073709551615,         -- c17
  18446744073709551615,         -- c18
  3.402823466E+38,              -- c19
  3.402823466E+38,              -- c20
  3.402823466E+38,              -- c21
  1.7976931348623E+308,         -- c22 three digits cut for ps-protocol
  1.7976931348623E+308,         -- c23 three digits cut for ps-protocol
  1.7976931348623E+308,         -- c24 three digits cut for ps-protocol
  9999999999,                   -- c25
  9999999999,                   -- c26
  9999999999,                   -- c27
  #
  '9999-12-31',                 -- c28
  '9999-12-31 23:59:59',        -- c29
  '2038-01-08 03:14:07',        -- c30 one day earlier due to timezone issues
  '838:59:59',                  -- c31
  '2155',                       -- c32
  #
  x'ff',                        -- c33
  '',                           -- c34
  x'ff',                        -- c35
  REPEAT(x'ff',255),            -- c36
  _utf8 x'efbfbf',              -- c37
  '',                           -- c38
  _utf8 x'efbfbf',              -- c39
  REPEAT(_utf8 x'efbfbf',255),  -- c40
  _ucs2 x'ffff',                -- c41
  '',                           -- c42
  _ucs2 x'ffff',                -- c43
  REPEAT(_ucs2 x'ffff',255),    -- c44
  #
  '',                           -- c45
  x'ff',                        -- c46
  REPEAT(x'ff',255),            -- c47
  REPEAT(x'ff',261),            -- c48
  '',                           -- c49
  _utf8 x'efbfbf',              -- c50
  REPEAT(_utf8 x'efbfbf',255),  -- c51
  REPEAT(_utf8 x'efbfbf',261),  -- c52
  '',                           -- c53
  _ucs2 x'ffff',                -- c54
  REPEAT(_ucs2 x'ffff',255),    -- c55
  REPEAT(_ucs2 x'ffff',261),    -- c56
  #
  x'ff',                        -- c57
  '',                           -- c58
  x'ff',                        -- c59
  REPEAT(x'ff',255),            -- c60
  #
  '',                           -- c61
  x'ff',                        -- c62
  REPEAT(x'ff',255),            -- c63
  REPEAT(x'ff',261),            -- c64
  #
  'tinyblob',                   -- c65 not using maximum value here
  'tinytext',                   -- c66 not using maximum value here
  'tinytext-ucs2',              -- c67 not using maximum value here
  'blob',                       -- c68 not using maximum value here
  'text',                       -- c69 not using maximum value here
  'text-ucs2',                  -- c70 not using maximum value here
  'mediumblob',                 -- c71 not using maximum value here
  'mediumtext',                 -- c72 not using maximum value here
  'mediumtext-ucs2',            -- c73 not using maximum value here
  'longblob',                   -- c74 not using maximum value here
  'longtext',                   -- c75 not using maximum value here
  'longtext-ucs2',              -- c76 not using maximum value here
  #
  'c',                          -- c77
  'a,b,c',                      -- c78
  #
  2                             -- crn -- row number
  );

--echo #
--echo # Insert a row with NULL values and one with arbitrary values.
--echo #
INSERT INTO t1 VALUES (
  NULL,                         -- c01
  NULL,                         -- c02
  NULL,                         -- c03
  NULL,                         -- c04
  NULL,                         -- c05
  NULL,                         -- c06
  NULL,                         -- c07
  NULL,                         -- c08
  NULL,                         -- c09
  NULL,                         -- c10
  NULL,                         -- c11
  NULL,                         -- c12
  NULL,                         -- c13
  NULL,                         -- c14
  NULL,                         -- c15
  NULL,                         -- c16
  NULL,                         -- c17
  NULL,                         -- c18
  NULL,                         -- c19
  NULL,                         -- c20
  NULL,                         -- c21
  NULL,                         -- c22
  NULL,                         -- c23
  NULL,                         -- c24
  NULL,                         -- c25
  NULL,                         -- c26
  NULL,                         -- c27
  #
  NULL,                         -- c28
  NULL,                         -- c29
  NULL,                         -- c30
  NULL,                         -- c31
  NULL,                         -- c32
  #
  NULL,                         -- c33
  NULL,                         -- c34
  NULL,                         -- c35
  NULL,                         -- c36
  NULL,                         -- c37
  NULL,                         -- c38
  NULL,                         -- c39
  NULL,                         -- c40
  NULL,                         -- c41
  NULL,                         -- c42
  NULL,                         -- c43
  NULL,                         -- c44
  #
  NULL,                         -- c45
  NULL,                         -- c46
  NULL,                         -- c47
  NULL,                         -- c48
  NULL,                         -- c49
  NULL,                         -- c50
  NULL,                         -- c51
  NULL,                         -- c52
  NULL,                         -- c53
  NULL,                         -- c54
  NULL,                         -- c55
  NULL,                         -- c56
  #
  NULL,                         -- c57
  NULL,                         -- c58
  NULL,                         -- c59
  NULL,                         -- c60
  #
  NULL,                         -- c61
  NULL,                         -- c62
  NULL,                         -- c63
  NULL,                         -- c64
  #
  NULL,                         -- c65
  NULL,                         -- c66
  NULL,                         -- c67
  NULL,                         -- c68
  NULL,                         -- c69
  NULL,                         -- c70
  NULL,                         -- c71
  NULL,                         -- c72
  NULL,                         -- c73
  NULL,                         -- c74
  NULL,                         -- c75
  NULL,                         -- c76
  #
  NULL,                         -- c77
  NULL,                         -- c78
  #
  3                             -- crn -- row number
  ), (
  b'1',                         -- c01
  b'1111111111111111111111111111111111111111111111111111111111111111',  -- c02
  127,                          -- c03
  0,                            -- c04
  001,                          -- c05
  true,                         -- c06
  32767,                        -- c07
  0,                            -- c08
  00001,                        -- c09
  8388607,                      -- c10
  0,                            -- c11
  00000001,                     -- c12
  2147483647,                   -- c13
  0,                            -- c14
  0000000001,                   -- c15
  9223372036854775807,          -- c16
  0,                            -- c17
  00000000000000000001,         -- c18
  -1.175494351E-38,             -- c19
  1.175494351E-38,              -- c20
  000000000000001,              -- c21
  -2.2250738585072E-308,        -- c22
  2.2250738585072E-308,         -- c23
  00000000000000000000001,      -- c24
  -9999999999,                  -- c25
  9999999999,                   -- c26
  0000000001,                   -- c27
  #
  '2008-08-04',                 -- c28
  '2008-08-04 16:18:06',        -- c29
  '2008-08-04 16:18:24',        -- c30
  '16:18:47',                   -- c31
  '2008',                       -- c32
  #
  'a',                          -- c33
  '',                           -- c34
  'e',                          -- c35
  REPEAT('i',255),              -- c36
  _utf8 x'c3a4',                -- c37
  '',                           -- c38
  _utf8 x'c3b6',                -- c39
  REPEAT(_utf8 x'c3bc',255),    -- c40
  _ucs2 x'00e4',                -- c41
  '',                           -- c42
  _ucs2 x'00f6',                -- c43
  REPEAT(_ucs2 x'00fc',255),    -- c44
  #
  '',                           -- c45
  'a',                          -- c46
  REPEAT('e',255),              -- c47
  REPEAT('i',261),              -- c48
  '',                           -- c49
  _utf8 x'c3a4',                -- c50
  REPEAT(_utf8 x'c3b6',255),    -- c51
  REPEAT(_utf8 x'c3bc',261),    -- c52
  '',                           -- c53
  _ucs2 x'00e4',                -- c54
  REPEAT(_ucs2 x'00f6',255),    -- c55
  REPEAT(_ucs2 x'00fc',261),    -- c56
  #
  '0',                          -- c57
  '',                           -- c58
  '1',                          -- c59
  REPEAT('1',255),              -- c60
  #
  '',                           -- c61
  'b',                          -- c62
  REPEAT('c',255),              -- c63
  REPEAT('\'',261),             -- c64
  #
  'tinyblob',                   -- c65
  'tinytext',                   -- c66
  'tinytext-ucs2',              -- c67
  'blob',                       -- c68
  'text',                       -- c69
  'text-ucs2',                  -- c70
  'mediumblob',                 -- c71
  'mediumtext',                 -- c72
  'mediumtext-ucs2',            -- c73
  'longblob',                   -- c74
  'longtext',                   -- c75
  'longtext-ucs2',              -- c76
  #
  'b',                          -- c77
  'b,c',                        -- c78
  #
  4                             -- crn -- row number
  );

--echo #
--echo # Show what we have in the table.
--echo # Do not display bit type output. It's binary and confuses diff.
--echo # Also BINARY with nul-bytes should be avoided.
--echo #
--replace_column 1 # 2 # 57 # 58 # 59 # 60 #
query_vertical SELECT * FROM t1;

--echo #
--echo # NOTE: For matching FLOAT and DOUBLE values in WHERE conditions,
--echo # don't use exact match, but < or > and tweak the numbers a bit.
--echo #
--echo # Show how much rows are affected by each statement.
--echo #
--enable_info

--echo #
--echo # Update min values to max values.
--echo #
UPDATE t1 SET
  c01 = b'1',
  c02 = b'1111111111111111111111111111111111111111111111111111111111111111',
  c03 = 127,
  c04 = 255,
  c05 = 255,
  c06 = true,
  c07 = 32767,
  c08 = 65535,
  c09 = 65535,
  c10 = 8388607,
  c11 = 16777215,
  c12 = 16777215,
  c13 = 2147483647,
  c14 = 4294967295,
  c15 = 4294967295,
  c16 = 9223372036854775807,
  c17 = 18446744073709551615,
  c18 = 18446744073709551615,
  c19 = 3.402823466E+38,
  c20 = 3.402823466E+38,
  c21 = 3.402823466E+38,
  c22 = 1.7976931348623E+308,
  c23 = 1.7976931348623E+308,
  c24 = 1.7976931348623E+308,
  c25 = 9999999999,
  c26 = 9999999999,
  c27 = 9999999999,
  #
  c28 = '9999-12-31',
  c29 = '9999-12-31 23:59:59',
  c30 = '2038-01-08 03:14:07',
  c31 = '838:59:59',
  c32 = '2155',
  #
  c33 = x'ff',
  c34 = '',
  c35 = x'ff',
  c36 = REPEAT(x'ff',255),
  c37 = _utf8 x'efbfbf',
  c38 = '',
  c39 = _utf8 x'efbfbf',
  c40 = REPEAT(_utf8 x'efbfbf',255),
  c41 = _ucs2 x'ffff',
  c42 = '',
  c43 = _ucs2 x'ffff',
  c44 = REPEAT(_ucs2 x'ffff',255),
  #
  c45 = '',
  c46 = x'ff',
  c47 = REPEAT(x'ff',255),
  c48 = REPEAT(x'ff',261),
  c49 = '',
  c50 = _utf8 x'efbfbf',
  c51 = REPEAT(_utf8 x'efbfbf',255),
  c52 = REPEAT(_utf8 x'efbfbf',261),
  c53 = '',
  c54 = _ucs2 x'ffff',
  c55 = REPEAT(_ucs2 x'ffff',255),
  c56 = REPEAT(_ucs2 x'ffff',261),
  #
  c57 = x'ff',
  c58 = '',
  c59 = x'ff',
  c60 = REPEAT(x'ff',255),
  #
  c61 = '',
  c62 = x'ff',
  c63 = REPEAT(x'ff',255),
  c64 = REPEAT(x'ff',261),
  #
  c65 = 'tinyblob',
  c66 = 'tinytext',
  c67 = 'tinytext-ucs2',
  c68 = 'blob',
  c69 = 'text',
  c70 = 'text-ucs2',
  c71 = 'mediumblob',
  c72 = 'mediumtext',
  c73 = 'mediumtext-ucs2',
  c74 = 'longblob',
  c75 = 'longtext',
  c76 = 'longtext-ucs2',
  #
  c77 = 'c',
  c78 = 'a,b,c',
  #
  crn = crn
  #
  WHERE
  #
  c01 = b'0' AND
  c02 = b'0000000000000000000000000000000000000000000000000000000000000000' AND
  c03 = -128 AND
  c04 = 0 AND
  c05 = 000 AND
  c06 = false AND
  c07 = -32768 AND
  c08 = 0 AND
  c09 = 00000 AND
  c10 = -8388608 AND
  c11 = 0 AND
  c12 = 00000000 AND
  c13 = -2147483648 AND
  c14 = 0 AND
  c15 = 0000000000 AND
  c16 = -9223372036854775808 AND
  c17 = 0 AND
  c18 = 00000000000000000000 AND
  c19 < -3.402823465E+38 AND
  c20 < 1.175494352E-38 AND
  c21 = 000000000000 AND
  c22 < -1.7976931348622E+308 AND
  c23 < 2.2250738585073E-308 AND
  c24 = 0000000000000000000000 AND
  c25 = -9999999999 AND
  c26 = 0 AND
  c27 = 0000000000 AND
  #
  c28 = '1000-01-01' AND
  c29 = '1000-01-01 00:00:00' AND
  c30 = '1970-01-02 00:00:01' AND
  c31 = '-838:59:59' AND
  c32 = '1901' AND
  #
  c33 = '' AND
  c34 = '' AND
  c35 = '' AND
  c36 = '' AND
  c37 = '' AND
  c38 = '' AND
  c39 = '' AND
  c40 = '' AND
  c41 = '' AND
  c42 = '' AND
  c43 = '' AND
  c44 = '' AND
  #
  c45 = '' AND
  c46 = '' AND
  c47 = '' AND
  c48 = '' AND
  c49 = '' AND
  c50 = '' AND
  c51 = '' AND
  c52 = '' AND
  c53 = '' AND
  c54 = '' AND
  c55 = '' AND
  c56 = '' AND
  #
  # this does not reproduce the inserted value: c57 = '' AND
  c58 = '' AND
  # this does not reproduce the inserted value: c59 = '' AND
  # this does not reproduce the inserted value: c60 = '' AND
  #
  c61 = '' AND
  c62 = '' AND
  c63 = '' AND
  c64 = '' AND
  #
  c65 = '' AND
  c66 = '' AND
  c67 = '' AND
  c68 = '' AND
  c69 = '' AND
  c70 = '' AND
  c71 = '' AND
  c72 = '' AND
  c73 = '' AND
  c74 = '' AND
  c75 = '' AND
  c76 = '' AND
  #
  c77 = 'a' AND
  c78 = '' AND
  #
  crn = 1;

--echo #
--echo # Update max values to min values.
--echo #
UPDATE t1 SET
  c01 = b'0',
  c02 = b'0000000000000000000000000000000000000000000000000000000000000000',
  c03 = -128,
  c04 = 0,
  c05 = 000,
  c06 = false,
  c07 = -32768,
  c08 = 0,
  c09 = 00000,
  c10 = -8388608,
  c11 = 0,
  c12 = 00000000,
  c13 = -2147483648,
  c14 = 0,
  c15 = 0000000000,
  c16 = -9223372036854775808,
  c17 = 0,
  c18 = 00000000000000000000,
  c19 = -3.402823466E+38,
  c20 = 1.175494351E-38,
  c21 = 000000000000,
  c22 = -1.7976931348623E+308,
  c23 = 2.2250738585072E-308,
  c24 = 0000000000000000000000,
  c25 = -9999999999,
  c26 = 0,
  c27 = 0000000000,
    #
  c28 = '1000-01-01',
  c29 = '1000-01-01 00:00:00',
  c30 = '1970-01-02 00:00:01',
  c31 = '-838:59:59',
  c32 = '1901',
    #
  c33 = '',
  c34 = '',
  c35 = '',
  c36 = '',
  c37 = '',
  c38 = '',
  c39 = '',
  c40 = '',
  c41 = '',
  c42 = '',
  c43 = '',
  c44 = '',
    #
  c45 = '',
  c46 = '',
  c47 = '',
  c48 = '',
  c49 = '',
  c50 = '',
  c51 = '',
  c52 = '',
  c53 = '',
  c54 = '',
  c55 = '',
  c56 = '',
    #
  c57 = '',
  c58 = '',
  c59 = '',
  c60 = '',
    #
  c61 = '',
  c62 = '',
  c63 = '',
  c64 = '',
    #
  c65 = '',
  c66 = '',
  c67 = '',
  c68 = '',
  c69 = '',
  c70 = '',
  c71 = '',
  c72 = '',
  c73 = '',
  c74 = '',
  c75 = '',
  c76 = '',
    #
  c77 = 'a',
  c78 = '',
  #
  crn = crn
  #
  WHERE
  #
  c01 = b'1' AND
  # the below does not reproduce the inserted value:
  #c02 = b'1111111111111111111111111111111111111111111111111111111111111111' AND
  c03 = 127 AND
  c04 = 255 AND
  c05 = 255 AND
  c06 = true AND
  c07 = 32767 AND
  c08 = 65535 AND
  c09 = 65535 AND
  c10 = 8388607 AND
  c11 = 16777215 AND
  c12 = 16777215 AND
  c13 = 2147483647 AND
  c14 = 4294967295 AND
  c15 = 4294967295 AND
  c16 = 9223372036854775807 AND
  c17 = 18446744073709551615 AND
  c18 = 18446744073709551615 AND
  c19 > 3.402823465E+38 AND
  c20 > 3.402823465E+38 AND
  c21 > 3.402823465E+38 AND
  c22 > 1.7976931348622E+308 AND
  c23 > 1.7976931348622E+308 AND
  c24 > 1.7976931348622E+308 AND
  c25 = 9999999999 AND
  c26 = 9999999999 AND
  c27 = 9999999999 AND
  #
  c28 = '9999-12-31' AND
  c29 = '9999-12-31 23:59:59' AND
  c30 = '2038-01-08 03:14:07' AND
  c31 = '838:59:59' AND
  c32 = '2155' AND
  #
  c33 = x'ff' AND
  c34 = '' AND
  c35 = x'ff' AND
  c36 = REPEAT(x'ff',255) AND
  c37 = _utf8 x'efbfbf' AND
  c38 = '' AND
  c39 = _utf8 x'efbfbf' AND
  c40 = REPEAT(_utf8 x'efbfbf',255) AND
  c41 = _ucs2 x'ffff' AND
  c42 = '' AND
  c43 = _ucs2 x'ffff' AND
  c44 = REPEAT(_ucs2 x'ffff',255) AND
  #
  c45 = '' AND
  c46 = x'ff' AND
  c47 = REPEAT(x'ff',255) AND
  c48 = REPEAT(x'ff',261) AND
  c49 = '' AND
  c50 = _utf8 x'efbfbf' AND
  c51 = REPEAT(_utf8 x'efbfbf',255) AND
  c52 = REPEAT(_utf8 x'efbfbf',261) AND
  c53 = '' AND
  c54 = _ucs2 x'ffff' AND
  c55 = REPEAT(_ucs2 x'ffff',255) AND
  c56 = REPEAT(_ucs2 x'ffff',261) AND
  #
  c57 = x'ff' AND
  c58 = '' AND
  c59 = x'ff' AND
  c60 = REPEAT(x'ff',255) AND
  #
  c61 = '' AND
  c62 = x'ff' AND
  c63 = REPEAT(x'ff',255) AND
  c64 = REPEAT(x'ff',261) AND
  #
  c65 = 'tinyblob' AND
  c66 = 'tinytext' AND
  c67 = 'tinytext-ucs2' AND
  c68 = 'blob' AND
  c69 = 'text' AND
  c70 = 'text-ucs2' AND
  c71 = 'mediumblob' AND
  c72 = 'mediumtext' AND
  c73 = 'mediumtext-ucs2' AND
  c74 = 'longblob' AND
  c75 = 'longtext' AND
  c76 = 'longtext-ucs2' AND
  #
  c77 = 'c' AND
  c78 = 'a,b,c' AND
  #
  crn = 2;

--echo #
--echo # Update NULL values to arbitrary values.
--echo #
UPDATE t1 SET
  c01 = b'1',
  c02 = b'1111111111111111111111111111111111111111111111111111111111111111',
  c03 = 127,
  c04 = 0,
  c05 = 001,
  c06 = true,
  c07 = 32767,
  c08 = 0,
  c09 = 00001,
  c10 = 8388607,
  c11 = 0,
  c12 = 00000001,
  c13 = 2147483647,
  c14 = 0,
  c15 = 0000000001,
  c16 = 9223372036854775807,
  c17 = 0,
  c18 = 00000000000000000001,
  c19 = -1.175494351E-38,
  c20 = 1.175494351E-38,
  c21 = 000000000000001,
  c22 = -2.2250738585072E-308,
  c23 = 2.2250738585072E-308,
  c24 = 00000000000000000000001,
  c25 = -9999999999,
  c26 = 9999999999,
  c27 = 0000000001,
  #
  c28 = '2008-08-04',
  c29 = '2008-08-04 16:18:06',
  c30 = '2008-08-04 16:18:24',
  c31 = '16:18:47',
  c32 = '2008',
  #
  c33 = 'a',
  c34 = '',
  c35 = 'e',
  c36 = REPEAT('i',255),
  c37 = _utf8 x'c3a4',
  c38 = '',
  c39 = _utf8 x'c3b6',
  c40 = REPEAT(_utf8 x'c3bc',255),
  c41 = _ucs2 x'00e4',
  c42 = '',
  c43 = _ucs2 x'00f6',
  c44 = REPEAT(_ucs2 x'00fc',255),
  #
  c45 = '',
  c46 = 'a',
  c47 = REPEAT('e',255),
  c48 = REPEAT('i',261),
  c49 = '',
  c50 = _utf8 x'c3a4',
  c51 = REPEAT(_utf8 x'c3b6',255),
  c52 = REPEAT(_utf8 x'c3bc',261),
  c53 = '',
  c54 = _ucs2 x'00e4',
  c55 = REPEAT(_ucs2 x'00f6',255),
  c56 = REPEAT(_ucs2 x'00fc',261),
  #
  c57 = '0',
  c58 = '',
  c59 = '1',
  c60 = REPEAT('1',255),
  #
  c61 = '',
  c62 = 'b',
  c63 = REPEAT('c',255),
  c64 = REPEAT('\'',261),
  #
  c65 = 'tinyblob',
  c66 = 'tinytext',
  c67 = 'tinytext-ucs2',
  c68 = 'blob',
  c69 = 'text',
  c70 = 'text-ucs2',
  c71 = 'mediumblob',
  c72 = 'mediumtext',
  c73 = 'mediumtext-ucs2',
  c74 = 'longblob',
  c75 = 'longtext',
  c76 = 'longtext-ucs2',
  #
  c77 = 'b',
  c78 = 'b,c',
  #
  crn = crn
  #
  WHERE
  #
  c01 IS NULL AND
  c02 IS NULL AND
  c03 IS NULL AND
  c04 IS NULL AND
  c05 IS NULL AND
  c06 IS NULL AND
  c07 IS NULL AND
  c08 IS NULL AND
  c09 IS NULL AND
  c10 IS NULL AND
  c11 IS NULL AND
  c12 IS NULL AND
  c13 IS NULL AND
  c14 IS NULL AND
  c15 IS NULL AND
  c16 IS NULL AND
  c17 IS NULL AND
  c18 IS NULL AND
  c19 IS NULL AND
  c20 IS NULL AND
  c21 IS NULL AND
  c22 IS NULL AND
  c23 IS NULL AND
  c24 IS NULL AND
  c25 IS NULL AND
  c26 IS NULL AND
  c27 IS NULL AND
  #
  c28 IS NULL AND
  c29 IS NULL AND
  # this got a timestamp instead of NULL: c30 IS NULL AND
  c31 IS NULL AND
  c32 IS NULL AND
  #
  c33 IS NULL AND
  c34 IS NULL AND
  c35 IS NULL AND
  c36 IS NULL AND
  c37 IS NULL AND
  c38 IS NULL AND
  c39 IS NULL AND
  c40 IS NULL AND
  c41 IS NULL AND
  c42 IS NULL AND
  c43 IS NULL AND
  c44 IS NULL AND
  #
  c45 IS NULL AND
  c46 IS NULL AND
  c47 IS NULL AND
  c48 IS NULL AND
  c49 IS NULL AND
  c50 IS NULL AND
  c51 IS NULL AND
  c52 IS NULL AND
  c53 IS NULL AND
  c54 IS NULL AND
  c55 IS NULL AND
  c56 IS NULL AND
  #
  c57 IS NULL AND
  c58 IS NULL AND
  c59 IS NULL AND
  c60 IS NULL AND
  #
  c61 IS NULL AND
  c62 IS NULL AND
  c63 IS NULL AND
  c64 IS NULL AND
  #
  c65 IS NULL AND
  c66 IS NULL AND
  c67 IS NULL AND
  c68 IS NULL AND
  c69 IS NULL AND
  c70 IS NULL AND
  c71 IS NULL AND
  c72 IS NULL AND
  c73 IS NULL AND
  c74 IS NULL AND
  c75 IS NULL AND
  c76 IS NULL AND
  #
  c77 IS NULL AND
  c78 IS NULL AND
  #
  crn = 3;

--echo #
--echo # Update arbitrary values to NULL values.
--echo #
UPDATE t1 SET
  c01 = NULL,
  c02 = NULL,
  c03 = NULL,
  c04 = NULL,
  c05 = NULL,
  c06 = NULL,
  c07 = NULL,
  c08 = NULL,
  c09 = NULL,
  c10 = NULL,
  c11 = NULL,
  c12 = NULL,
  c13 = NULL,
  c14 = NULL,
  c15 = NULL,
  c16 = NULL,
  c17 = NULL,
  c18 = NULL,
  c19 = NULL,
  c20 = NULL,
  c21 = NULL,
  c22 = NULL,
  c23 = NULL,
  c24 = NULL,
  c25 = NULL,
  c26 = NULL,
  c27 = NULL,
  #
  c28 = NULL,
  c29 = NULL,
  c30 = NULL,
  c31 = NULL,
  c32 = NULL,
  #
  c33 = NULL,
  c34 = NULL,
  c35 = NULL,
  c36 = NULL,
  c37 = NULL,
  c38 = NULL,
  c39 = NULL,
  c40 = NULL,
  c41 = NULL,
  c42 = NULL,
  c43 = NULL,
  c44 = NULL,
  #
  c45 = NULL,
  c46 = NULL,
  c47 = NULL,
  c48 = NULL,
  c49 = NULL,
  c50 = NULL,
  c51 = NULL,
  c52 = NULL,
  c53 = NULL,
  c54 = NULL,
  c55 = NULL,
  c56 = NULL,
  #
  c57 = NULL,
  c58 = NULL,
  c59 = NULL,
  c60 = NULL,
  #
  c61 = NULL,
  c62 = NULL,
  c63 = NULL,
  c64 = NULL,
  #
  c65 = NULL,
  c66 = NULL,
  c67 = NULL,
  c68 = NULL,
  c69 = NULL,
  c70 = NULL,
  c71 = NULL,
  c72 = NULL,
  c73 = NULL,
  c74 = NULL,
  c75 = NULL,
  c76 = NULL,
  #
  c77 = NULL,
  c78 = NULL,
  #
  crn = crn
  #
  WHERE
  #
  c01 = b'1' AND
  # the below does not reproduce the inserted value:
  #c02 = b'1111111111111111111111111111111111111111111111111111111111111111' AND
  c03 = 127 AND
  c04 = 0 AND
  c05 = 001 AND
  c06 = true AND
  c07 = 32767 AND
  c08 = 0 AND
  c09 = 00001 AND
  c10 = 8388607 AND
  c11 = 0 AND
  c12 = 00000001 AND
  c13 = 2147483647 AND
  c14 = 0 AND
  c15 = 0000000001 AND
  c16 = 9223372036854775807 AND
  c17 = 0 AND
  c18 = 00000000000000000001 AND
  c19 > -1.175494352E-38 AND
  c20 < 1.175494352E-38 AND
  c21 = 000000000000001 AND
  c22 > -2.2250738585073E-308 AND
  c23 < 2.2250738585073E-308 AND
  c24 = 00000000000000000000001 AND
  c25 = -9999999999 AND
  c26 = 9999999999 AND
  c27 = 0000000001 AND
  #
  c28 = '2008-08-04' AND
  c29 = '2008-08-04 16:18:06' AND
  c30 = '2008-08-04 16:18:24' AND
  c31 = '16:18:47' AND
  c32 = '2008' AND
  #
  c33 = 'a' AND
  c34 = '' AND
  c35 = 'e' AND
  c36 = REPEAT('i',255) AND
  c37 = _utf8 x'c3a4' AND
  c38 = '' AND
  c39 = _utf8 x'c3b6' AND
  c40 = REPEAT(_utf8 x'c3bc',255) AND
  c41 = _ucs2 x'00e4' AND
  c42 = '' AND
  c43 = _ucs2 x'00f6' AND
  c44 = REPEAT(_ucs2 x'00fc',255) AND
  #
  c45 = '' AND
  c46 = 'a' AND
  c47 = REPEAT('e',255) AND
  c48 = REPEAT('i',261) AND
  c49 = '' AND
  c50 = _utf8 x'c3a4' AND
  c51 = REPEAT(_utf8 x'c3b6',255) AND
  c52 = REPEAT(_utf8 x'c3bc',261) AND
  c53 = '' AND
  c54 = _ucs2 x'00e4' AND
  c55 = REPEAT(_ucs2 x'00f6',255) AND
  c56 = REPEAT(_ucs2 x'00fc',261) AND
  #
  c57 = '0' AND
  c58 = '' AND
  c59 = '1' AND
  c60 = REPEAT('1',255) AND
  #
  c61 = '' AND
  c62 = 'b' AND
  c63 = REPEAT('c',255) AND
  c64 = REPEAT('\'',261) AND
  #
  c65 = 'tinyblob' AND
  c66 = 'tinytext' AND
  c67 = 'tinytext-ucs2' AND
  c68 = 'blob' AND
  c69 = 'text' AND
  c70 = 'text-ucs2' AND
  c71 = 'mediumblob' AND
  c72 = 'mediumtext' AND
  c73 = 'mediumtext-ucs2' AND
  c74 = 'longblob' AND
  c75 = 'longtext' AND
  c76 = 'longtext-ucs2' AND
  #
  c77 = 'b' AND
  c78 = 'b,c' AND
  #
  crn = 4;

--echo #
--echo # Show what we have in the table.
--echo # Do not display bit type output. It's binary and confuses diff.
--echo # Also BINARY with nul-bytes should be avoided.
--echo #
--replace_column 1 # 2 # 57 # 58 # 59 # 60 #
query_vertical SELECT * FROM t1;

--echo #
--echo # Delete the row that has max values now.
--echo #
DELETE FROM t1 WHERE
  #
  c01 = b'1' AND
  # the below does not reproduce the inserted value:
  #c02 = b'1111111111111111111111111111111111111111111111111111111111111111' AND
  c03 = 127 AND
  c04 = 255 AND
  c05 = 255 AND
  c06 = true AND
  c07 = 32767 AND
  c08 = 65535 AND
  c09 = 65535 AND
  c10 = 8388607 AND
  c11 = 16777215 AND
  c12 = 16777215 AND
  c13 = 2147483647 AND
  c14 = 4294967295 AND
  c15 = 4294967295 AND
  c16 = 9223372036854775807 AND
  c17 = 18446744073709551615 AND
  c18 = 18446744073709551615 AND
  c19 > 3.402823465E+38 AND
  c20 > 3.402823465E+38 AND
  c21 > 3.402823465E+38 AND
  c22 > 1.7976931348622E+308 AND
  c23 > 1.7976931348622E+308 AND
  c24 > 1.7976931348622E+308 AND
  c25 = 9999999999 AND
  c26 = 9999999999 AND
  c27 = 9999999999 AND
  #
  c28 = '9999-12-31' AND
  c29 = '9999-12-31 23:59:59' AND
  c30 = '2038-01-08 03:14:07' AND
  c31 = '838:59:59' AND
  c32 = '2155' AND
  #
  c33 = x'ff' AND
  c34 = '' AND
  c35 = x'ff' AND
  c36 = REPEAT(x'ff',255) AND
  c37 = _utf8 x'efbfbf' AND
  c38 = '' AND
  c39 = _utf8 x'efbfbf' AND
  c40 = REPEAT(_utf8 x'efbfbf',255) AND
  c41 = _ucs2 x'ffff' AND
  c42 = '' AND
  c43 = _ucs2 x'ffff' AND
  c44 = REPEAT(_ucs2 x'ffff',255) AND
  #
  c45 = '' AND
  c46 = x'ff' AND
  c47 = REPEAT(x'ff',255) AND
  c48 = REPEAT(x'ff',261) AND
  c49 = '' AND
  c50 = _utf8 x'efbfbf' AND
  c51 = REPEAT(_utf8 x'efbfbf',255) AND
  c52 = REPEAT(_utf8 x'efbfbf',261) AND
  c53 = '' AND
  c54 = _ucs2 x'ffff' AND
  c55 = REPEAT(_ucs2 x'ffff',255) AND
  c56 = REPEAT(_ucs2 x'ffff',261) AND
  #
  c57 = x'ff' AND
  c58 = '' AND
  c59 = x'ff' AND
  c60 = REPEAT(x'ff',255) AND
  #
  c61 = '' AND
  c62 = x'ff' AND
  c63 = REPEAT(x'ff',255) AND
  c64 = REPEAT(x'ff',261) AND
  #
  c65 = 'tinyblob' AND
  c66 = 'tinytext' AND
  c67 = 'tinytext-ucs2' AND
  c68 = 'blob' AND
  c69 = 'text' AND
  c70 = 'text-ucs2' AND
  c71 = 'mediumblob' AND
  c72 = 'mediumtext' AND
  c73 = 'mediumtext-ucs2' AND
  c74 = 'longblob' AND
  c75 = 'longtext' AND
  c76 = 'longtext-ucs2' AND
  #
  c77 = 'c' AND
  c78 = 'a,b,c' AND
  #
  crn = 1;

--echo #
--echo # Delete the row that has min values now.
--echo #
DELETE FROM t1 WHERE
  #
  c01 = b'0' AND
  c02 = b'0000000000000000000000000000000000000000000000000000000000000000' AND
  c03 = -128 AND
  c04 = 0 AND
  c05 = 000 AND
  c06 = false AND
  c07 = -32768 AND
  c08 = 0 AND
  c09 = 00000 AND
  c10 = -8388608 AND
  c11 = 0 AND
  c12 = 00000000 AND
  c13 = -2147483648 AND
  c14 = 0 AND
  c15 = 0000000000 AND
  c16 = -9223372036854775808 AND
  c17 = 0 AND
  c18 = 00000000000000000000 AND
  c19 < -3.402823465E+38 AND
  c20 < 1.175494352E-38 AND
  c21 = 000000000000 AND
  c22 < -1.7976931348622E+308 AND
  c23 < 2.2250738585073E-308 AND
  c24 = 0000000000000000000000 AND
  c25 = -9999999999 AND
  c26 = 0 AND
  c27 = 0000000000 AND
  #
  c28 = '1000-01-01' AND
  c29 = '1000-01-01 00:00:00' AND
  c30 = '1970-01-02 00:00:01' AND
  c31 = '-838:59:59' AND
  c32 = '1901' AND
  #
  c33 = '' AND
  c34 = '' AND
  c35 = '' AND
  c36 = '' AND
  c37 = '' AND
  c38 = '' AND
  c39 = '' AND
  c40 = '' AND
  c41 = '' AND
  c42 = '' AND
  c43 = '' AND
  c44 = '' AND
  #
  c45 = '' AND
  c46 = '' AND
  c47 = '' AND
  c48 = '' AND
  c49 = '' AND
  c50 = '' AND
  c51 = '' AND
  c52 = '' AND
  c53 = '' AND
  c54 = '' AND
  c55 = '' AND
  c56 = '' AND
  #
  # this does not reproduce the inserted value: c57 = '' AND
  c58 = '' AND
  # this does not reproduce the inserted value: c59 = '' AND
  # this does not reproduce the inserted value: c60 = '' AND
  #
  c61 = '' AND
  c62 = '' AND
  c63 = '' AND
  c64 = '' AND
  #
  c65 = '' AND
  c66 = '' AND
  c67 = '' AND
  c68 = '' AND
  c69 = '' AND
  c70 = '' AND
  c71 = '' AND
  c72 = '' AND
  c73 = '' AND
  c74 = '' AND
  c75 = '' AND
  c76 = '' AND
  #
  c77 = 'a' AND
  c78 = '' AND
  #
  crn = 2;

--echo #
--echo # Delete the row that has arbitrary values now.
--echo #
DELETE FROM t1 WHERE
  #
  c01 = b'1' AND
  # the below does not reproduce the inserted value:
  #c02 = b'1111111111111111111111111111111111111111111111111111111111111111' AND
  c03 = 127 AND
  c04 = 0 AND
  c05 = 001 AND
  c06 = true AND
  c07 = 32767 AND
  c08 = 0 AND
  c09 = 00001 AND
  c10 = 8388607 AND
  c11 = 0 AND
  c12 = 00000001 AND
  c13 = 2147483647 AND
  c14 = 0 AND
  c15 = 0000000001 AND
  c16 = 9223372036854775807 AND
  c17 = 0 AND
  c18 = 00000000000000000001 AND
  c19 > -1.175494352E-38 AND
  c20 < 1.175494352E-38 AND
  c21 = 000000000000001 AND
  c22 > -2.2250738585073E-308 AND
  c23 < 2.2250738585073E-308 AND
  c24 = 00000000000000000000001 AND
  c25 = -9999999999 AND
  c26 = 9999999999 AND
  c27 = 0000000001 AND
  #
  c28 = '2008-08-04' AND
  c29 = '2008-08-04 16:18:06' AND
  c30 = '2008-08-04 16:18:24' AND
  c31 = '16:18:47' AND
  c32 = '2008' AND
  #
  c33 = 'a' AND
  c34 = '' AND
  c35 = 'e' AND
  c36 = REPEAT('i',255) AND
  c37 = _utf8 x'c3a4' AND
  c38 = '' AND
  c39 = _utf8 x'c3b6' AND
  c40 = REPEAT(_utf8 x'c3bc',255) AND
  c41 = _ucs2 x'00e4' AND
  c42 = '' AND
  c43 = _ucs2 x'00f6' AND
  c44 = REPEAT(_ucs2 x'00fc',255) AND
  #
  c45 = '' AND
  c46 = 'a' AND
  c47 = REPEAT('e',255) AND
  c48 = REPEAT('i',261) AND
  c49 = '' AND
  c50 = _utf8 x'c3a4' AND
  c51 = REPEAT(_utf8 x'c3b6',255) AND
  c52 = REPEAT(_utf8 x'c3bc',261) AND
  c53 = '' AND
  c54 = _ucs2 x'00e4' AND
  c55 = REPEAT(_ucs2 x'00f6',255) AND
  c56 = REPEAT(_ucs2 x'00fc',261) AND
  #
  c57 = '0' AND
  c58 = '' AND
  c59 = '1' AND
  c60 = REPEAT('1',255) AND
  #
  c61 = '' AND
  c62 = 'b' AND
  c63 = REPEAT('c',255) AND
  c64 = REPEAT('\'',261) AND
  #
  c65 = 'tinyblob' AND
  c66 = 'tinytext' AND
  c67 = 'tinytext-ucs2' AND
  c68 = 'blob' AND
  c69 = 'text' AND
  c70 = 'text-ucs2' AND
  c71 = 'mediumblob' AND
  c72 = 'mediumtext' AND
  c73 = 'mediumtext-ucs2' AND
  c74 = 'longblob' AND
  c75 = 'longtext' AND
  c76 = 'longtext-ucs2' AND
  #
  c77 = 'b' AND
  c78 = 'b,c' AND
  #
  crn = 3;

--echo #
--echo # Delete the row that has NULL values now.
--echo #
DELETE FROM t1 WHERE
  #
  c01 IS NULL AND
  c02 IS NULL AND
  c03 IS NULL AND
  c04 IS NULL AND
  c05 IS NULL AND
  c06 IS NULL AND
  c07 IS NULL AND
  c08 IS NULL AND
  c09 IS NULL AND
  c10 IS NULL AND
  c11 IS NULL AND
  c12 IS NULL AND
  c13 IS NULL AND
  c14 IS NULL AND
  c15 IS NULL AND
  c16 IS NULL AND
  c17 IS NULL AND
  c18 IS NULL AND
  c19 IS NULL AND
  c20 IS NULL AND
  c21 IS NULL AND
  c22 IS NULL AND
  c23 IS NULL AND
  c24 IS NULL AND
  c25 IS NULL AND
  c26 IS NULL AND
  c27 IS NULL AND
  #
  c28 IS NULL AND
  c29 IS NULL AND
  # this got a timestamp instead of NULL: c30 IS NULL AND
  c31 IS NULL AND
  c32 IS NULL AND
  #
  c33 IS NULL AND
  c34 IS NULL AND
  c35 IS NULL AND
  c36 IS NULL AND
  c37 IS NULL AND
  c38 IS NULL AND
  c39 IS NULL AND
  c40 IS NULL AND
  c41 IS NULL AND
  c42 IS NULL AND
  c43 IS NULL AND
  c44 IS NULL AND
  #
  c45 IS NULL AND
  c46 IS NULL AND
  c47 IS NULL AND
  c48 IS NULL AND
  c49 IS NULL AND
  c50 IS NULL AND
  c51 IS NULL AND
  c52 IS NULL AND
  c53 IS NULL AND
  c54 IS NULL AND
  c55 IS NULL AND
  c56 IS NULL AND
  #
  c57 IS NULL AND
  c58 IS NULL AND
  c59 IS NULL AND
  c60 IS NULL AND
  #
  c61 IS NULL AND
  c62 IS NULL AND
  c63 IS NULL AND
  c64 IS NULL AND
  #
  c65 IS NULL AND
  c66 IS NULL AND
  c67 IS NULL AND
  c68 IS NULL AND
  c69 IS NULL AND
  c70 IS NULL AND
  c71 IS NULL AND
  c72 IS NULL AND
  c73 IS NULL AND
  c74 IS NULL AND
  c75 IS NULL AND
  c76 IS NULL AND
  #
  c77 IS NULL AND
  c78 IS NULL AND
  #
  crn = 4;

--echo #
--echo # Show what we have in the table. Should be empty now.
--echo #
query_vertical SELECT * FROM t1;

--echo #
--echo # Hide how much rows are affected by each statement.
--echo #
--disable_info

--echo #
--echo # Flush all log buffers to the log file.
--echo #
FLUSH LOGS;

--echo #
--echo # Call mysqlbinlog to display the log file contents.
--echo #
let $MYSQLD_DATADIR= `select @@datadir`;
--let $mysqlbinlog_parameters= --base64-output=decode-rows -v -v $MYSQLD_DATADIR/binlog.000001
--source include/mysqlbinlog.inc

--echo #
--echo # Cleanup.
--echo #
DROP TABLE t1;

--echo #
--echo # =========================================
--echo # Test #2 - Multi-row insert/update/delete.
--echo # =========================================
--echo #
--echo # Delete all existing binary logs.
--echo #
RESET MASTER;

--echo #
--echo # Create a test table with selected data types.
--echo #
eval CREATE TABLE t1 (
  c28 DATE,
  c47 VARCHAR(24),
  crn INT -- row number
  ) ENGINE=$engine_type DEFAULT CHARSET latin1;

--echo #
--echo # Show how much rows are affected by each statement.
--echo #
--enable_info

--echo #
--echo # Multi-row insert.
--echo #
INSERT INTO t1 VALUES
  ('2008-08-01','VARCHAR-01',1),
  ('2008-08-02','VARCHAR-02',2),
  ('2008-08-03','VARCHAR-03',3),
  ('2008-08-04','VARCHAR-04',4),
  ('2008-08-05','VARCHAR-05',5),
  ('2008-08-06','VARCHAR-06',6),
  ('2008-08-07','VARCHAR-07',7),
  ('2008-08-08','VARCHAR-08',8),
  ('2008-08-09','VARCHAR-09',9);

--echo #
--echo # Multi-row update.
--echo #
UPDATE t1 SET c28 = ADDDATE(c28,10) WHERE crn < 8;

--echo #
--echo # Show what we have in the table.
--echo #
SELECT * FROM t1;

--echo #
--echo # Multi-row delete.
--echo #
DELETE FROM t1 WHERE crn < 8;

--echo #
--echo # Show what we have in the table.
--echo #
SELECT * FROM t1;

--echo #
--echo # Hide how much rows are affected by each statement.
--echo #
--disable_info

--echo #
--echo # Flush all log buffers to the log file.
--echo #
FLUSH LOGS;

--echo #
--echo # Call mysqlbinlog to display the log file contents.
--echo #
let $MYSQLD_DATADIR= `select @@datadir`;
--let $mysqlbinlog_parameters=  --base64-output=decode-rows -v -v $MYSQLD_DATADIR/binlog.000001
--source include/mysqlbinlog.inc

--echo #
--echo # Cleanup.
--echo #
DROP TABLE t1;

--echo #
--echo # ====================================
--echo # Test #3 - Multi-table update/delete.
--echo # ====================================
--echo #
--echo # Delete all existing binary logs.
--echo #
RESET MASTER;

--echo #
--echo # Create test tables with selected data types.
--echo #
eval CREATE TABLE t1 (
  c_1_1 DATE,
  c_1_2 VARCHAR(255),
  c_1_n INT -- row number
  ) ENGINE=$engine_type DEFAULT CHARSET latin1;
#
eval CREATE TABLE t2 (
  c_2_1 DATE,
  c_2_2 VARCHAR(255),
  c_2_n INT -- row number
  ) ENGINE=$engine_type DEFAULT CHARSET latin1;
#
eval CREATE TABLE t3 (
  c_3_1 DATE,
  c_3_2 VARCHAR(255),
  c_3_n INT -- row number
  ) ENGINE=$engine_type DEFAULT CHARSET latin1;

--echo #
--echo # Show how much rows are affected by each statement.
--echo #
--enable_info

--echo #
--echo # Insert data.
--echo #
INSERT INTO t1 VALUES
  ('2008-01-01','VARCHAR-01-01',11),
  ('2008-01-02','VARCHAR-01-02',2),
  ('2008-01-03','VARCHAR-01-03',3),
  ('2008-01-04','VARCHAR-01-04',4),
  ('2008-01-05','VARCHAR-01-05',5),
  ('2008-01-06','VARCHAR-01-06',6),
  ('2008-01-07','VARCHAR-01-07',7),
  ('2008-01-08','VARCHAR-01-08',18),
  ('2008-01-09','VARCHAR-01-09',19);
#
INSERT INTO t2 VALUES
  ('2008-02-01','VARCHAR-02-01',21),
  ('2008-02-02','VARCHAR-02-02',2),
  ('2008-02-03','VARCHAR-02-03',3),
  ('2008-02-04','VARCHAR-02-04',4),
  ('2008-02-05','VARCHAR-02-05',5),
  ('2008-02-06','VARCHAR-02-06',6),
  ('2008-02-07','VARCHAR-02-07',7),
  ('2008-02-08','VARCHAR-02-08',28),
  ('2008-02-09','VARCHAR-02-09',29);
#
INSERT INTO t3 VALUES
  ('2008-03-01','VARCHAR-03-01',31),
  ('2008-03-02','VARCHAR-03-02',2),
  ('2008-03-03','VARCHAR-03-03',3),
  ('2008-03-04','VARCHAR-03-04',4),
  ('2008-03-05','VARCHAR-03-05',5),
  ('2008-03-06','VARCHAR-03-06',6),
  ('2008-03-07','VARCHAR-03-07',7),
  ('2008-03-08','VARCHAR-03-08',38),
  ('2008-03-09','VARCHAR-03-09',39);

ANALYZE TABLE t1;
ANALYZE TABLE t2;
ANALYZE TABLE t3;

--echo #
--echo # Multi-table update.
--echo #
UPDATE t1,t2,t3 SET
  c_1_1 = ADDDATE(c_1_1,INTERVAL 10 YEAR),
  c_2_1 = ADDDATE(c_2_1,INTERVAL 20 YEAR),
  c_3_1 = ADDDATE(c_3_1,INTERVAL 30 YEAR)
  WHERE c_1_n = c_2_n AND c_2_n = c_3_n;

--echo #
--echo # Show what we have in the tables.
--echo #
SELECT * FROM t1;
SELECT * FROM t2;
SELECT * FROM t3;

--echo #
--echo # Multi-table delete.
--echo #
DELETE FROM t1,t2,t3 USING t1 INNER JOIN t2 INNER JOIN t3
  WHERE c_1_n = c_2_n AND c_2_n = c_3_n;

--echo #
--echo # Show what we have in the tables.
--echo #
SELECT * FROM t1;
SELECT * FROM t2;
SELECT * FROM t3;

--echo #
--echo # Hide how much rows are affected by each statement.
--echo #
--disable_info

--echo #
--echo # Flush all log buffers to the log file.
--echo #
FLUSH LOGS;

--echo #
--echo # Call mysqlbinlog to display the log file contents.
--echo #
let $MYSQLD_DATADIR= `select @@datadir`;
--let $mysqlbinlog_parameters= --base64-output=decode-rows -v -v $MYSQLD_DATADIR/binlog.000001
--source include/mysqlbinlog.inc

--echo #
--echo # Cleanup.
--echo #
DROP TABLE t1, t2, t3;

--echo #
--echo # ===========================
--echo # Test #4 - LOAD DATA INFILE.
--echo # ===========================
--echo #
--echo # Delete all existing binary logs.
--echo #
RESET MASTER;

--echo #
--echo # Create a test table with selected data types.
--echo #
eval CREATE TABLE t1 (
  c1 INT DEFAULT 100,
  c2 INT,
  c3 VARCHAR(60)
  ) ENGINE=$engine_type DEFAULT CHARSET latin1;

--echo #
--echo # Show how much rows are affected by each statement.
--echo #
--enable_info

--echo #
--echo # Load data.
--echo #
LOAD DATA INFILE '../../std_data/loaddata5.dat'
  INTO TABLE t1 FIELDS TERMINATED BY '' ENCLOSED BY '' (c1,c2)
  SET c3 = 'Wow';

--echo #
--echo # Show what we have in the table.
--echo #
SELECT * FROM t1;

--echo #
--echo # Hide how much rows are affected by each statement.
--echo #
--disable_info

--echo #
--echo # Flush all log buffers to the log file.
--echo #
FLUSH LOGS;

--echo #
--echo # Call mysqlbinlog to display the log file contents.
--echo #
let $MYSQLD_DATADIR= `select @@datadir`;
--let $mysqlbinlog_parameters= --base64-output=decode-rows -v -v $MYSQLD_DATADIR/binlog.000001
--source include/mysqlbinlog.inc

--echo #
--echo # Cleanup.
--echo #
DROP TABLE t1;


